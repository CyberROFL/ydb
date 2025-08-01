#include "dq_output_channel.h"
#include "dq_arrow_helpers.h"

#include <util/generic/buffer.h>
#include <util/generic/size_literals.h>
#include <util/stream/buffer.h>

#include <yql/essentials/minikql/computation/mkql_computation_node_pack.h>
#include <yql/essentials/utils/yql_panic.h>

namespace NYql::NDq {

#define LOG(s) do { if (Y_UNLIKELY(LogFunc)) { LogFunc(TStringBuilder() << "channelId: " << PopStats.ChannelId << ". " << s); } } while (0)

#ifndef NDEBUG
#define DLOG(s) LOG(s)
#else
#define DLOG(s)
#endif

namespace {

using namespace NKikimr;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<bool FastPack>
class TDqOutputChannel : public IDqOutputChannel {
public:
    TDqOutputStats PushStats;
    TDqOutputChannelStats PopStats;

    TDqOutputChannel(ui64 channelId, ui32 dstStageId, NMiniKQL::TType* outputType,
        const NMiniKQL::THolderFactory& holderFactory, const TDqOutputChannelSettings& settings, const TLogFunc& logFunc,
        NDqProto::EDataTransportVersion transportVersion)
        : OutputType(outputType)
        , Packer(OutputType, settings.BufferPageAllocSize)
        , Width(OutputType->IsMulti() ? static_cast<NMiniKQL::TMultiType*>(OutputType)->GetElementsCount() : 1u)
        , Storage(settings.ChannelStorage)
        , HolderFactory(holderFactory)
        , TransportVersion(transportVersion)
        , MaxStoredBytes(settings.MaxStoredBytes)
        , MaxChunkBytes(std::min(settings.MaxChunkBytes, settings.ChunkSizeLimit / 2))
        , ChunkSizeLimit(settings.ChunkSizeLimit)
        , ArrayBufferMinFillPercentage(settings.ArrayBufferMinFillPercentage)
        , LogFunc(logFunc)
    {
        PopStats.Level = settings.Level;
        PushStats.Level = settings.Level;
        PopStats.ChannelId = channelId;
        PopStats.DstStageId = dstStageId;
        UpdateSettings(settings.MutableSettings);

        if (Packer.IsBlock() && ArrayBufferMinFillPercentage && *ArrayBufferMinFillPercentage > 0) {
            BlockSplitter = NArrow::CreateBlockSplitter(OutputType, (ChunkSizeLimit - MaxChunkBytes) * *ArrayBufferMinFillPercentage / 100);
        }
    }

    ui64 GetChannelId() const override {
        return PopStats.ChannelId;
    }

    ui64 GetValuesCount() const override {
        return SpilledChunkCount + PackedChunkCount + PackerCurrentChunkCount;
    }

    const TDqOutputStats& GetPushStats() const override {
        return PushStats;
    }

    const TDqOutputChannelStats& GetPopStats() const override {
        return PopStats;
    }

    EDqFillLevel CalcFillLevel() const {
        if (Storage) {
            return FirstStoredId < NextStoredId ? (Storage->IsFull() ? HardLimit : SoftLimit) : NoLimit;
        } else {
            return PackedDataSize + Packer.PackedSizeEstimate() >= MaxStoredBytes ? HardLimit : NoLimit;
        }
    }

    EDqFillLevel GetFillLevel() const override {
        return FillLevel;
    }

    EDqFillLevel UpdateFillLevel() override {
        auto result = CalcFillLevel();
        if (FillLevel != result) {
            if (Aggregator) {
                Aggregator->UpdateCount(FillLevel, result);
            }
            FillLevel = result;
        }
        return result;
    }

    void SetFillAggregator(std::shared_ptr<TDqFillAggregator> aggregator) override {
        Aggregator = aggregator;
        Aggregator->AddCount(FillLevel);
    }

    void Push(NUdf::TUnboxedValue&& value) override {
        YQL_ENSURE(!OutputType->IsMulti());
        DoPushSafe(&value, 1);
    }

    void WidePush(NUdf::TUnboxedValue* values, ui32 width) override {
        YQL_ENSURE(OutputType->IsMulti());
        YQL_ENSURE(Width == width);
        DoPushSafe(values, width);
    }

    // Try to split data before push to fulfill ChunkSizeLimit
    void DoPushSafe(NUdf::TUnboxedValue* values, ui32 width) {
        YQL_ENSURE(GetFillLevel() != HardLimit);

        if (Finished) {
            return;
        }

        ui32 rows = Packer.IsBlock() ?
            NKikimr::NMiniKQL::TArrowBlock::From(values[width - 1]).GetDatum().scalar_as<arrow::UInt64Scalar>().value
            : 1;

        if (PushStats.CollectBasic()) {
            PushStats.Rows += rows;
            PushStats.Chunks++;
            PushStats.Resume();
        }

        PackerCurrentRowCount += rows;

        if (!IsLocalChannel && BlockSplitter && BlockSplitter->ShouldSplitItem(values, width)) {
            for (auto&& block : BlockSplitter->SplitItem(values, width)) {
                DoPushBlock(std::move(block));
            }
        } else {
            DoPush(values, width);
        }
    }

    // Push data as single chunk
    void DoPush(NUdf::TUnboxedValue* values, ui32 width) {
        if (OutputType->IsMulti()) {
            Packer.AddWideItem(values, width);
        } else {
            Packer.AddItem(*values);
        }
        for (ui32 i = 0; i < width; ++i) {
            values[i] = {};
        }

        PackerCurrentChunkCount++;
        TryPack();
    }

    void DoPushBlock(std::vector<arrow::Datum>&& data) {
        NKikimr::NMiniKQL::TUnboxedValueVector outputValues;
        outputValues.reserve(data.size());
        for (auto& datum : data) {
            outputValues.emplace_back(HolderFactory.CreateArrowBlock(std::move(datum)));
        }
        Packer.AddWideItem(outputValues.data(), outputValues.size());

        PackerCurrentChunkCount++;
        TryPack();
    }

    // Pack and spill data batch if enough data (>= Max Chunk Bytes) or force = true
    void TryPack() {
        size_t packerSize = Packer.PackedSizeEstimate();
        if (packerSize >= MaxChunkBytes) {
            Data.emplace_back();
            Data.back().Buffer = FinishPackAndCheckSize();
            if (PushStats.CollectBasic()) {
                PushStats.Bytes += Data.back().Buffer.Size();
            }
            PackedDataSize += Data.back().Buffer.Size();
            PackedChunkCount += PackerCurrentChunkCount;
            PackedRowCount += PackerCurrentRowCount;
            Data.back().ChunkCount = PackerCurrentChunkCount;
            Data.back().RowCount = PackerCurrentRowCount;
            PackerCurrentChunkCount = 0;
            PackerCurrentRowCount = 0;
            packerSize = 0;
        }

        while (Storage && PackedDataSize && PackedDataSize + packerSize > MaxStoredBytes) {
            auto& head = Data.front();
            size_t bufSize = head.Buffer.Size();
            YQL_ENSURE(PackedDataSize >= bufSize);

            TDqSerializedBatch data;
            data.Proto.SetTransportVersion(TransportVersion);
            data.Proto.SetChunks(head.ChunkCount);
            data.Proto.SetRows(head.RowCount);
            data.SetPayload(std::move(head.Buffer));
            Storage->Put(NextStoredId++, SaveForSpilling(std::move(data)));

            PackedDataSize -= bufSize;
            PackedChunkCount -= head.ChunkCount;
            PackedRowCount -= head.RowCount;

            SpilledChunkCount += head.ChunkCount;

            if (PopStats.CollectFull()) {
                PopStats.SpilledRows += head.ChunkCount; // FIXME with RowCount
                PopStats.SpilledBytes += bufSize + sizeof(head.ChunkCount);
                PopStats.SpilledBlobs++;
            }

            Data.pop_front();
            LOG("Data spilled. Total rows spilled: " << SpilledChunkCount << ", bytesInMemory: " << (PackedDataSize + packerSize)); // FIXME with RowCount
        }

        UpdateFillLevel();

        if (PopStats.CollectFull()) {
            if (FillLevel != NoLimit) {
                PopStats.TryPause();
            }
            PopStats.MaxMemoryUsage = std::max(PopStats.MaxMemoryUsage, PackedDataSize + packerSize);
            PopStats.MaxRowsInMemory = std::max(PopStats.MaxRowsInMemory, PackedChunkCount);
        }
    }

    void Push(NDqProto::TWatermark&& watermark) override {
        YQL_ENSURE(!Watermark);
        Watermark.ConstructInPlace(std::move(watermark));
    }

    void Push(NDqProto::TCheckpoint&& checkpoint) override {
        YQL_ENSURE(!Checkpoint);
        Checkpoint.ConstructInPlace(std::move(checkpoint));
    }

    [[nodiscard]]
    bool Pop(TDqSerializedBatch& data) override {
        if (!HasData()) {
            PushStats.TryPause();
            if (Finished) {
                data.Clear();
                data.Proto.SetTransportVersion(TransportVersion);
            }
            return false;
        }

        data.Clear();
        data.Proto.SetTransportVersion(TransportVersion);
        if (FirstStoredId < NextStoredId) {
            YQL_ENSURE(Storage);
            LOG("Loading spilled blob. BlobId: " << FirstStoredId);
            TBuffer blob;
            if (!Storage->Get(FirstStoredId, blob)) {
                LOG("BlobId " << FirstStoredId << " not ready yet");
                return false;
            }
            ++FirstStoredId;
            data = LoadSpilled(std::move(blob));
            SpilledChunkCount -= data.ChunkCount();
        } else if (!Data.empty()) {
            auto& packed = Data.front();
            PackedChunkCount -= packed.ChunkCount;
            PackedRowCount -= packed.RowCount;
            PackedDataSize -= packed.Buffer.Size();
            data.Proto.SetChunks(packed.ChunkCount);
            data.Proto.SetRows(packed.RowCount);
            data.SetPayload(std::move(packed.Buffer));
            Data.pop_front();
        } else {
            data.Proto.SetChunks(PackerCurrentChunkCount);
            data.Proto.SetRows(PackerCurrentRowCount);
            data.SetPayload(FinishPackAndCheckSize());
            PackerCurrentChunkCount = 0;
            PackerCurrentRowCount = 0;
        }

        DLOG("Took " << data.RowCount() << " rows");

        UpdateFillLevel();

        if (PopStats.CollectBasic()) {
            PopStats.Bytes += data.Size();
            PopStats.Rows += data.RowCount();
            PopStats.Chunks++; // pop chunks do not match push chunks
            if (FillLevel == NoLimit) {
                PopStats.Resume();
            }
        }

        return true;
    }

    [[nodiscard]]
    bool Pop(NDqProto::TWatermark& watermark) override {
        if (!HasData() && Watermark) {
            watermark = std::move(*Watermark);
            Watermark = Nothing();
            return true;
        }
        return false;
    }

    [[nodiscard]]
    bool Pop(NDqProto::TCheckpoint& checkpoint) override {
        if (!HasData() && Checkpoint) {
            checkpoint = std::move(*Checkpoint);
            Checkpoint = Nothing();
            return true;
        }
        return false;
    }

    [[nodiscard]]
    bool PopAll(TDqSerializedBatch& data) override {
        if (!HasData()) {
            if (Finished) {
                data.Clear();
                data.Proto.SetTransportVersion(TransportVersion);
            }
            return false;
        }

        data.Clear();
        data.Proto.SetTransportVersion(TransportVersion);
        if (SpilledChunkCount == 0 && PackedChunkCount == 0) {
            data.Proto.SetChunks(PackerCurrentChunkCount);
            data.Proto.SetRows(PackerCurrentRowCount);
            data.SetPayload(FinishPackAndCheckSize());
            if (PushStats.CollectBasic()) {
                PushStats.Bytes += data.Payload.Size();
            }
            PackerCurrentChunkCount = 0;
            PackerCurrentRowCount = 0;
            return true;
        }

        // Repack all - thats why PopAll should never be used
        if (PackerCurrentChunkCount) {
            Data.emplace_back();
            Data.back().Buffer = FinishPackAndCheckSize();
            if (PushStats.CollectBasic()) {
                PushStats.Bytes += Data.back().Buffer.Size();
            }
            PackedDataSize += Data.back().Buffer.Size();
            PackedChunkCount += PackerCurrentChunkCount;
            PackedRowCount += PackerCurrentRowCount;
            Data.back().ChunkCount = PackerCurrentChunkCount;
            Data.back().RowCount = PackerCurrentRowCount;
            PackerCurrentChunkCount = 0;
            PackerCurrentRowCount = 0;
        }

        NKikimr::NMiniKQL::TUnboxedValueBatch rows(OutputType);
        size_t repackedChunkCount = 0;
        size_t repackedRowCount = 0;
        for (;;) {
            TDqSerializedBatch batch;
            if (!this->Pop(batch)) {
                break;
            }
            repackedChunkCount += batch.ChunkCount();
            repackedRowCount += batch.RowCount();
            Packer.UnpackBatch(batch.PullPayload(), HolderFactory, rows);
        }

        if (OutputType->IsMulti()) {
            rows.ForEachRowWide([this](const auto* values, ui32 width) {
                Packer.AddWideItem(values, width);
            });
        } else {
            rows.ForEachRow([this](const auto& value) {
                Packer.AddItem(value);
            });
        }

        data.Proto.SetChunks(repackedChunkCount);
        data.Proto.SetRows(repackedRowCount);
        data.SetPayload(FinishPackAndCheckSize());
        if (PopStats.CollectBasic()) {
            PopStats.Bytes += data.Size();
            PopStats.Rows += data.RowCount();
            PopStats.Chunks++;
            if (UpdateFillLevel() == NoLimit) {
                PopStats.Resume();
            }
        }
        YQL_ENSURE(!HasData());
        return true;
    }

    void Finish() override {
        LOG("Finish request");
        Finished = true;
    }

    TChunkedBuffer FinishPackAndCheckSize() {
        TChunkedBuffer result = Packer.Finish();
        if (!IsLocalChannel && result.Size() > ChunkSizeLimit) {
            // TODO: may relax requirement if OOB transport is enabled
            ythrow TDqOutputChannelChunkSizeLimitExceeded() << "Row data size is too big: "
                << result.Size() << " bytes, exceeds limit of " << ChunkSizeLimit << " bytes";
        }
        return result;
    }

    bool HasData() const override {
        return GetValuesCount() != 0;
    }

    bool IsFinished() const override {
        return Finished && !HasData();
    }

    ui64 Drop() override { // Drop channel data because channel was finished. Leave checkpoint because checkpoints keep going through channel after finishing channel data transfer.
        ui64 chunks = GetValuesCount();
        Data.clear();
        Packer.Clear();
        PackedDataSize = 0;
        PackedChunkCount = 0;
        PackedRowCount = 0;
        SpilledChunkCount = 0;
        PackerCurrentChunkCount = 0;
        PackerCurrentRowCount = 0;
        FirstStoredId = NextStoredId;
        UpdateFillLevel();
        return chunks;
    }

    NKikimr::NMiniKQL::TType* GetOutputType() const override {
        return OutputType;
    }

    void Terminate() override {
    }

    void UpdateSettings(const TDqOutputChannelSettings::TMutable& settings) override {
        IsLocalChannel = settings.IsLocalChannel;
        if (Packer.IsBlock()) {
            Packer.SetMinFillPercentage(IsLocalChannel ? Nothing() : ArrayBufferMinFillPercentage);
        }
    }

private:
    NKikimr::NMiniKQL::TType* OutputType;
    NKikimr::NMiniKQL::TValuePackerTransport<FastPack> Packer;
    const ui32 Width;
    const IDqChannelStorage::TPtr Storage;
    const NMiniKQL::THolderFactory& HolderFactory;
    const NDqProto::EDataTransportVersion TransportVersion;
    const ui64 MaxStoredBytes;
    const ui64 MaxChunkBytes;
    const ui64 ChunkSizeLimit;
    const TMaybe<ui8> ArrayBufferMinFillPercentage;
    NArrow::IBlockSplitter::TPtr BlockSplitter;
    bool IsLocalChannel = false;
    TLogFunc LogFunc;

    struct TSerializedBatch {
        TChunkedBuffer Buffer;
        ui64 ChunkCount = 0;
        ui64 RowCount = 0;
    };
    std::deque<TSerializedBatch> Data;

    size_t SpilledChunkCount = 0;
    ui64 FirstStoredId = 0;
    ui64 NextStoredId = 0;

    size_t PackedDataSize = 0;
    size_t PackedChunkCount = 0;
    size_t PackedRowCount = 0;

    size_t PackerCurrentChunkCount = 0;
    size_t PackerCurrentRowCount = 0;

    bool Finished = false;

    TMaybe<NDqProto::TWatermark> Watermark;
    TMaybe<NDqProto::TCheckpoint> Checkpoint;
    std::shared_ptr<TDqFillAggregator> Aggregator;
    EDqFillLevel FillLevel = NoLimit;
};

} // anonymous namespace


IDqOutputChannel::TPtr CreateDqOutputChannel(ui64 channelId, ui32 dstStageId, NKikimr::NMiniKQL::TType* outputType,
    const NKikimr::NMiniKQL::THolderFactory& holderFactory,
    const TDqOutputChannelSettings& settings, const TLogFunc& logFunc)
{
    auto transportVersion = settings.TransportVersion;
    switch(transportVersion) {
        case NDqProto::EDataTransportVersion::DATA_TRANSPORT_VERSION_UNSPECIFIED:
            transportVersion = NDqProto::EDataTransportVersion::DATA_TRANSPORT_UV_PICKLE_1_0;
            [[fallthrough]];
        case NDqProto::EDataTransportVersion::DATA_TRANSPORT_UV_PICKLE_1_0:
        case NDqProto::EDataTransportVersion::DATA_TRANSPORT_OOB_PICKLE_1_0:
            return new TDqOutputChannel<false>(channelId, dstStageId, outputType, holderFactory, settings, logFunc, transportVersion);
        case NDqProto::EDataTransportVersion::DATA_TRANSPORT_UV_FAST_PICKLE_1_0:
        case NDqProto::EDataTransportVersion::DATA_TRANSPORT_OOB_FAST_PICKLE_1_0:
            return new TDqOutputChannel<true>(channelId, dstStageId, outputType, holderFactory, settings, logFunc, transportVersion);
        default:
            YQL_ENSURE(false, "Unsupported transport version " << (ui32)transportVersion);
    }
}

} // namespace NYql::NDq
