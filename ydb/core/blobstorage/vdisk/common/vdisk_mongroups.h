#pragma once

#include "defs.h"

#include <ydb/core/base/appdata_fwd.h>
#include <ydb/core/protos/base.pb.h>
#include <ydb/core/protos/blobstorage_base.pb.h>
#include <ydb/core/protos/node_whiteboard.pb.h>
#include <ydb/core/protos/whiteboard_disk_states.pb.h>

namespace NKikimr {
    namespace NMonGroup {
        class TBase {
        public:
            TBase(const TIntrusivePtr<::NMonitoring::TDynamicCounters>& counters,
                  const TString& name,
                  const TString& value)
                : DerivedCounters(counters)
                , GroupCounters(DerivedCounters->GetSubgroup(name, value))
            {}

            TBase(const TIntrusivePtr<::NMonitoring::TDynamicCounters>& counters)
                : DerivedCounters(counters)
                , GroupCounters(DerivedCounters)
            {}

            TIntrusivePtr<::NMonitoring::TDynamicCounters> GetGroup() const { return GroupCounters; }

        protected:
            TIntrusivePtr<::NMonitoring::TDynamicCounters> DerivedCounters;
            TIntrusivePtr<::NMonitoring::TDynamicCounters> GroupCounters;
        };

        bool IsExtendedVDiskCounters();

#define COUNTER_DEF(name)                                                                   \
protected:                                                                                  \
    ::NMonitoring::TDynamicCounters::TCounterPtr name##_;                                     \
public:                                                                                     \
    NMonitoring::TDeprecatedCounter &name() { return *name##_; }                            \
    const NMonitoring::TDeprecatedCounter &name() const { return *name##_; }                \
    const ::NMonitoring::TDynamicCounters::TCounterPtr &name##Ptr() const { return name##_; }

#define COUNTER_INIT(name, derivative)                                                      \
    name##_ = GroupCounters->GetCounter(#name, derivative)

#define COUNTER_INIT_PRIVATE(name, derivative)                                              \
    name##_ = GroupCounters->GetCounter(#name, derivative,                                  \
        NMonitoring::TCountableBase::EVisibility::Private)

#define COUNTER_INIT_IF_EXTENDED(name, derivative)                                          \
    name##_ = GroupCounters->GetCounter(#name, derivative,                                  \
        IsExtendedVDiskCounters() ? NMonitoring::TCountableBase::EVisibility::Public : NMonitoring::TCountableBase::EVisibility::Private)

#define GROUP_CONSTRUCTOR(name)                                                             \
    name(const TIntrusivePtr<::NMonitoring::TDynamicCounters>& counters,                    \
         const TString& name,                                                               \
         const TString& value)                                                              \
    : TBase(counters, name, value) {                                                        \
        InitCounters();                                                                     \
    }                                                                                       \
    name(const TIntrusivePtr<::NMonitoring::TDynamicCounters>& counters)                    \
    : TBase(counters) {                                                                     \
        InitCounters();                                                                     \
    }                                                                                       \
    void InitCounters()


        ///////////////////////////////////////////////////////////////////////////////////
        // TLsmHullGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TLsmHullGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TLsmHullGroup)
            {
                COUNTER_INIT(LsmCompactionBytesRead, true);
                COUNTER_INIT(LsmCompactionBytesWritten, true);
                COUNTER_INIT(LsmCompactionReadRequests, true);
                COUNTER_INIT(LsmCompactionWriteRequests, true);
                COUNTER_INIT(LsmHugeBytesWritten, true);
                COUNTER_INIT(LsmLogBytesWritten, true);
            }

            COUNTER_DEF(LsmCompactionBytesRead)
            COUNTER_DEF(LsmCompactionBytesWritten)
            COUNTER_DEF(LsmCompactionReadRequests)
            COUNTER_DEF(LsmCompactionWriteRequests)
            COUNTER_DEF(LsmHugeBytesWritten)
            COUNTER_DEF(LsmLogBytesWritten)
        };


        ///////////////////////////////////////////////////////////////////////////////////
        // TLsmHullSpaceGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TLsmHullSpaceGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TLsmHullSpaceGroup)
            {
                COUNTER_INIT(DskSpaceCurIndex, false);
                COUNTER_INIT(DskSpaceCurInplacedData, false);
                COUNTER_INIT(DskSpaceCurHugeData, false);
                COUNTER_INIT(DskSpaceCompIndex, false);
                COUNTER_INIT(DskSpaceCompInplacedData, false);
                COUNTER_INIT(DskSpaceCompHugeData, false);
            }

            COUNTER_DEF(DskSpaceCurIndex);
            COUNTER_DEF(DskSpaceCurInplacedData);
            COUNTER_DEF(DskSpaceCurHugeData);
            COUNTER_DEF(DskSpaceCompIndex);
            COUNTER_DEF(DskSpaceCompInplacedData);
            COUNTER_DEF(DskSpaceCompHugeData);
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TSkeletonOverloadGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TSkeletonOverloadGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TSkeletonOverloadGroup)
            {
                COUNTER_INIT_IF_EXTENDED(EmergencyMovedPatchQueueItems, false);
                COUNTER_INIT_IF_EXTENDED(EmergencyPatchStartQueueItems, false);
                COUNTER_INIT_IF_EXTENDED(EmergencyPutQueueItems, false);
                COUNTER_INIT_IF_EXTENDED(EmergencyMultiPutQueueItems, false);

                COUNTER_INIT_IF_EXTENDED(EmergencyMovedPatchQueueBytes, false);
                COUNTER_INIT_IF_EXTENDED(EmergencyPatchStartQueueBytes, false);
                COUNTER_INIT_IF_EXTENDED(EmergencyPutQueueBytes, false);
                COUNTER_INIT_IF_EXTENDED(EmergencyMultiPutQueueBytes, false);

                COUNTER_INIT_IF_EXTENDED(FreshSatisfactionRankPercent, false);
                COUNTER_INIT_IF_EXTENDED(LevelSatisfactionRankPercent, false);

                COUNTER_INIT_IF_EXTENDED(ThrottlingCurrentSpeedLimit, false);
                COUNTER_INIT_IF_EXTENDED(ThrottlingIsActive, false);
                COUNTER_INIT_IF_EXTENDED(ThrottlingDryRun, false);
                COUNTER_INIT_IF_EXTENDED(ThrottlingLevel0SstCount, false);
                COUNTER_INIT_IF_EXTENDED(ThrottlingMinLevel0SstCount, false);
                COUNTER_INIT_IF_EXTENDED(ThrottlingMaxLevel0SstCount, false);
                COUNTER_INIT_IF_EXTENDED(ThrottlingInplacedSize, false);
                COUNTER_INIT_IF_EXTENDED(ThrottlingMinInplacedSizeHDD, false);
                COUNTER_INIT_IF_EXTENDED(ThrottlingMaxInplacedSizeHDD, false);
                COUNTER_INIT_IF_EXTENDED(ThrottlingMinInplacedSizeSSD, false);
                COUNTER_INIT_IF_EXTENDED(ThrottlingMaxInplacedSizeSSD, false);
                COUNTER_INIT_IF_EXTENDED(ThrottlingOccupancyPerMille, false);
                COUNTER_INIT_IF_EXTENDED(ThrottlingMinOccupancyPerMille, false);
                COUNTER_INIT_IF_EXTENDED(ThrottlingMaxOccupancyPerMille, false);
                COUNTER_INIT_IF_EXTENDED(ThrottlingLogChunkCount, false);
                COUNTER_INIT_IF_EXTENDED(ThrottlingMinLogChunkCount, false);
                COUNTER_INIT_IF_EXTENDED(ThrottlingMaxLogChunkCount, false);
            }

            COUNTER_DEF(EmergencyMovedPatchQueueItems);
            COUNTER_DEF(EmergencyPatchStartQueueItems);
            COUNTER_DEF(EmergencyPutQueueItems);
            COUNTER_DEF(EmergencyMultiPutQueueItems);

            COUNTER_DEF(EmergencyMovedPatchQueueBytes);
            COUNTER_DEF(EmergencyPatchStartQueueBytes);
            COUNTER_DEF(EmergencyPutQueueBytes);
            COUNTER_DEF(EmergencyMultiPutQueueBytes);

            COUNTER_DEF(FreshSatisfactionRankPercent);
            COUNTER_DEF(LevelSatisfactionRankPercent);

            COUNTER_DEF(ThrottlingCurrentSpeedLimit);
            COUNTER_DEF(ThrottlingIsActive);
            COUNTER_DEF(ThrottlingDryRun);
            COUNTER_DEF(ThrottlingLevel0SstCount);
            COUNTER_DEF(ThrottlingMinLevel0SstCount);
            COUNTER_DEF(ThrottlingMaxLevel0SstCount);
            COUNTER_DEF(ThrottlingInplacedSize);
            COUNTER_DEF(ThrottlingMinInplacedSizeHDD);
            COUNTER_DEF(ThrottlingMaxInplacedSizeHDD);
            COUNTER_DEF(ThrottlingMinInplacedSizeSSD);
            COUNTER_DEF(ThrottlingMaxInplacedSizeSSD);
            COUNTER_DEF(ThrottlingOccupancyPerMille);
            COUNTER_DEF(ThrottlingMinOccupancyPerMille);
            COUNTER_DEF(ThrottlingMaxOccupancyPerMille);
            COUNTER_DEF(ThrottlingLogChunkCount);
            COUNTER_DEF(ThrottlingMinLogChunkCount);
            COUNTER_DEF(ThrottlingMaxLogChunkCount);
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TDskOutOfSpaceGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TDskOutOfSpaceGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TDskOutOfSpaceGroup)
            {
                COUNTER_INIT(DskOutOfSpace, false);
                COUNTER_INIT(DskTotalBytes, false);
                COUNTER_INIT(DskFreeBytes, false);
                COUNTER_INIT(DskUsedBytes, false);
                COUNTER_INIT(HugeUsedChunks, false);
                COUNTER_INIT_IF_EXTENDED(HugeCanBeFreedChunks, false);
                COUNTER_INIT_IF_EXTENDED(HugeLockedChunks, false);
            }

            COUNTER_DEF(DskOutOfSpace);
            COUNTER_DEF(DskTotalBytes);        // total bytes available on PDisk for this VDisk
            COUNTER_DEF(DskFreeBytes);         // free bytes available on PDisk for this VDisk
            COUNTER_DEF(DskUsedBytes);         // bytes used by this VDisk on PDisk
            // huge heap chunks
            COUNTER_DEF(HugeUsedChunks);       // chunks used by huge heap
            COUNTER_DEF(HugeCanBeFreedChunks); // number of chunks that can be freed after defragmentation
            COUNTER_DEF(HugeLockedChunks);
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TCostGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TCostGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TCostGroup)
            {
                COUNTER_INIT_IF_EXTENDED(DiskTimeAvailableNs, false);
                COUNTER_INIT_IF_EXTENDED(SkeletonFrontUserCostNs, true);
                COUNTER_INIT_IF_EXTENDED(SkeletonFrontInternalCostNs, true);
                COUNTER_INIT_IF_EXTENDED(DefragCostNs, true);
                COUNTER_INIT_IF_EXTENDED(CompactionCostNs, true);
                COUNTER_INIT_IF_EXTENDED(ScrubCostNs, true);
            }
            COUNTER_DEF(DiskTimeAvailableNs);
            COUNTER_DEF(SkeletonFrontUserCostNs);
            COUNTER_DEF(SkeletonFrontInternalCostNs);
            COUNTER_DEF(DefragCostNs);
            COUNTER_DEF(CompactionCostNs);
            COUNTER_DEF(ScrubCostNs);
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TSyncerGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TSyncerGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TSyncerGroup)
            {
                COUNTER_INIT_IF_EXTENDED(SyncerVSyncMessagesSent, true);
                COUNTER_INIT_IF_EXTENDED(SyncerVSyncBytesSent, true);
                COUNTER_INIT_IF_EXTENDED(SyncerVSyncBytesReceived, true);
                COUNTER_INIT_IF_EXTENDED(SyncerVSyncFullMessagesSent, true);
                COUNTER_INIT_IF_EXTENDED(SyncerVSyncFullBytesSent, true);
                COUNTER_INIT_IF_EXTENDED(SyncerVSyncFullBytesReceived, true);
                COUNTER_INIT_IF_EXTENDED(SyncerUnsyncedDisks, false);
                COUNTER_INIT_IF_EXTENDED(SyncerLoggerRecords, true);
                COUNTER_INIT_IF_EXTENDED(SyncerLoggedBytes, true);
            }

            COUNTER_DEF(SyncerVSyncMessagesSent);
            COUNTER_DEF(SyncerVSyncBytesSent);
            COUNTER_DEF(SyncerVSyncBytesReceived);
            COUNTER_DEF(SyncerVSyncFullMessagesSent);
            COUNTER_DEF(SyncerVSyncFullBytesSent);
            COUNTER_DEF(SyncerVSyncFullBytesReceived);
            COUNTER_DEF(SyncerUnsyncedDisks);
            COUNTER_DEF(SyncerLoggerRecords);
            COUNTER_DEF(SyncerLoggedBytes);
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TReplGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TReplGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TReplGroup)
            {
                COUNTER_INIT_IF_EXTENDED(ReplRecoveryGroupTypeErrors, true);
                COUNTER_INIT_IF_EXTENDED(ReplBlobsRecovered, true);
                COUNTER_INIT_IF_EXTENDED(ReplBlobBytesRecovered, true);
                COUNTER_INIT_IF_EXTENDED(ReplHugeBlobsRecovered, true);
                COUNTER_INIT_IF_EXTENDED(ReplHugeBlobBytesRecovered, true);
                COUNTER_INIT_IF_EXTENDED(ReplChunksWritten, true);
                COUNTER_INIT_IF_EXTENDED(ReplUnreplicatedVDisks, false);
                COUNTER_INIT_IF_EXTENDED(ReplVGetBytesReceived, true);
                COUNTER_INIT(ReplPhantomLikeDiscovered, false);
                COUNTER_INIT(ReplPhantomLikeRecovered, false);
                COUNTER_INIT(ReplPhantomLikeUnrecovered, false);
                COUNTER_INIT_IF_EXTENDED(ReplPhantomLikeDropped, false);
                COUNTER_INIT_IF_EXTENDED(ReplWorkUnitsDone, false);
                COUNTER_INIT_IF_EXTENDED(ReplWorkUnitsRemaining, false);
                COUNTER_INIT(ReplItemsDone, false);
                COUNTER_INIT(ReplItemsRemaining, false);
                COUNTER_INIT(ReplUnreplicatedPhantoms, false);
                COUNTER_INIT(ReplUnreplicatedNonPhantoms, false);
                COUNTER_INIT_IF_EXTENDED(ReplSecondsRemaining, false);
                COUNTER_INIT_IF_EXTENDED(ReplTotalBlobsWithProblems, false);
                COUNTER_INIT_IF_EXTENDED(ReplPhantomBlobsWithProblems, false);
                COUNTER_INIT_IF_EXTENDED(ReplMadeNoProgress, false);
            }

            COUNTER_DEF(SyncerVSyncMessagesSent);
            COUNTER_DEF(ReplRecoveryGroupTypeErrors);
            COUNTER_DEF(ReplBlobsRecovered);
            COUNTER_DEF(ReplBlobBytesRecovered);
            COUNTER_DEF(ReplHugeBlobsRecovered);
            COUNTER_DEF(ReplHugeBlobBytesRecovered);
            COUNTER_DEF(ReplChunksWritten);
            COUNTER_DEF(ReplUnreplicatedVDisks);
            COUNTER_DEF(ReplVGetBytesReceived);
            COUNTER_DEF(ReplPhantomLikeDiscovered);
            COUNTER_DEF(ReplPhantomLikeRecovered);
            COUNTER_DEF(ReplPhantomLikeUnrecovered);
            COUNTER_DEF(ReplPhantomLikeDropped);
            COUNTER_DEF(ReplWorkUnitsDone);
            COUNTER_DEF(ReplWorkUnitsRemaining);
            COUNTER_DEF(ReplItemsDone);
            COUNTER_DEF(ReplItemsRemaining);
            COUNTER_DEF(ReplUnreplicatedPhantoms);
            COUNTER_DEF(ReplUnreplicatedNonPhantoms);
            COUNTER_DEF(ReplSecondsRemaining);
            COUNTER_DEF(ReplTotalBlobsWithProblems);
            COUNTER_DEF(ReplPhantomBlobsWithProblems);
            COUNTER_DEF(ReplMadeNoProgress);
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TLocalRecoveryGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TLocalRecoveryGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TLocalRecoveryGroup)
            {
                COUNTER_INIT_PRIVATE(LogoBlobsDbEmpty, false);
                COUNTER_INIT_PRIVATE(BlocksDbEmpty, false);
                COUNTER_INIT_PRIVATE(BarriersDbEmpty, false);
                COUNTER_INIT_IF_EXTENDED(LocalRecovRecsDispatched, true);
                COUNTER_INIT_IF_EXTENDED(LocalRecovBytesDispatched, true);
                COUNTER_INIT_IF_EXTENDED(LocalRecovRecsApplied, true);
                COUNTER_INIT_IF_EXTENDED(LocalRecovBytesApplied, true);
                COUNTER_INIT_IF_EXTENDED(BulkLogoBlobs, true);
            }

            COUNTER_DEF(LogoBlobsDbEmpty);
            COUNTER_DEF(BlocksDbEmpty);
            COUNTER_DEF(BarriersDbEmpty);
            COUNTER_DEF(LocalRecovRecsDispatched);
            COUNTER_DEF(LocalRecovBytesDispatched);
            COUNTER_DEF(LocalRecovRecsApplied);
            COUNTER_DEF(LocalRecovBytesApplied);
            COUNTER_DEF(BulkLogoBlobs);
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TInterfaceGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TInterfaceGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TInterfaceGroup)
            {
                COUNTER_INIT_IF_EXTENDED(PutTotalBytes, true);
                COUNTER_INIT_IF_EXTENDED(GetTotalBytes, true);
            }

            COUNTER_DEF(PutTotalBytes);
            COUNTER_DEF(GetTotalBytes);
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TSyncLogIFaceGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TSyncLogIFaceGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TSyncLogIFaceGroup)
            {
                COUNTER_INIT_IF_EXTENDED(SyncPutMsgs, true);
                COUNTER_INIT_IF_EXTENDED(SyncPutSstMsgs, true);
                COUNTER_INIT_IF_EXTENDED(SyncReadMsgs, true);
                COUNTER_INIT_IF_EXTENDED(SyncReadResMsgs, true);
                COUNTER_INIT_IF_EXTENDED(LocalSyncMsgs, true);
                COUNTER_INIT_IF_EXTENDED(LocalSyncResMsgs, true);
                COUNTER_INIT_IF_EXTENDED(SyncLogGetSnapshot, true);
                COUNTER_INIT_IF_EXTENDED(SyncLogLocalStatus, true);
            }

            COUNTER_DEF(SyncPutMsgs);
            COUNTER_DEF(SyncPutSstMsgs);
            COUNTER_DEF(SyncReadMsgs);
            COUNTER_DEF(SyncReadResMsgs);
            COUNTER_DEF(LocalSyncMsgs);
            COUNTER_DEF(LocalSyncResMsgs);
            COUNTER_DEF(SyncLogGetSnapshot);
            COUNTER_DEF(SyncLogLocalStatus);
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TSyncLogCountersGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TSyncLogCountersGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TSyncLogCountersGroup)
            {
                COUNTER_INIT_IF_EXTENDED(VDiskCheckFailed, true);
                COUNTER_INIT_IF_EXTENDED(UnequalGuid, true);
                COUNTER_INIT_IF_EXTENDED(DiskLocked, true);
                COUNTER_INIT_IF_EXTENDED(ReplyError, true);
                COUNTER_INIT_IF_EXTENDED(FullRecovery, true);
                COUNTER_INIT_IF_EXTENDED(NormalSync, true);
                COUNTER_INIT_IF_EXTENDED(ReadsFromDisk, true);
                COUNTER_INIT_IF_EXTENDED(ReadsFromDiskBytes, true);
            }

            // status of read request:
            COUNTER_DEF(VDiskCheckFailed);
            COUNTER_DEF(UnequalGuid);
            COUNTER_DEF(DiskLocked);
            COUNTER_DEF(ReplyError);
            COUNTER_DEF(FullRecovery);
            COUNTER_DEF(NormalSync);
            // additional counters:
            COUNTER_DEF(ReadsFromDisk);
            COUNTER_DEF(ReadsFromDiskBytes);
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TVDiskStateGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TVDiskStateGroup: public TBase {
            std::array<::NMonitoring::TDynamicCounters::TCounterPtr, NKikimrWhiteboard::EVDiskState_MAX + 1> VDiskStates;
            ::NMonitoring::TDynamicCounters::TCounterPtr CurrentState;

        public:
            GROUP_CONSTRUCTOR(TVDiskStateGroup)
            {
                // depracated, only for compatibility
                TString name = "VDiskState";
                CurrentState = GroupCounters->GetCounter(name, false);
                *CurrentState = NKikimrWhiteboard::Initial;

                for (size_t i = NKikimrWhiteboard::EVDiskState_MIN; i <= NKikimrWhiteboard::EVDiskState_MAX; ++i) {
                    VDiskStates[i] = GroupCounters->GetCounter(name + "_" + NKikimrWhiteboard::EVDiskState_Name(i), false);
                }
                COUNTER_INIT_IF_EXTENDED(VDiskLocalRecoveryState, false);
            }

            void VDiskState(NKikimrWhiteboard::EVDiskState s) {
                *VDiskStates[*CurrentState] = 0;
                *CurrentState = s;
                *VDiskStates[s] = 1;
            }

            NKikimrWhiteboard::EVDiskState VDiskState() const {
                return static_cast<NKikimrWhiteboard::EVDiskState>(CurrentState->Val());
            }

            COUNTER_DEF(VDiskLocalRecoveryState);
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TLsmLevelGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TLsmLevelGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TLsmLevelGroup)
            {
                COUNTER_INIT_PRIVATE(SstNum, false);
                COUNTER_INIT(NumItems, false);
                COUNTER_INIT(NumItemsInplaced, false);
                COUNTER_INIT(NumItemsHuge, false);
                COUNTER_INIT(DataInplaced, false);
                COUNTER_INIT(DataHuge, false);
            }

            COUNTER_DEF(SstNum);
            COUNTER_DEF(NumItems);
            COUNTER_DEF(NumItemsInplaced);
            COUNTER_DEF(NumItemsHuge);
            COUNTER_DEF(DataInplaced);
            COUNTER_DEF(DataHuge);
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TLsmAllLevelsStat
        ///////////////////////////////////////////////////////////////////////////////////
        class TLsmAllLevelsStat {
        public:
            TIntrusivePtr<::NMonitoring::TDynamicCounters> Group;
            // per-level information
            TLsmLevelGroup Level0;
            TLsmLevelGroup Level1to8;
            TLsmLevelGroup Level9to16;
            TLsmLevelGroup Level17;
            TLsmLevelGroup Level18;
            TLsmLevelGroup Level19;

            TLsmAllLevelsStat(const TIntrusivePtr<::NMonitoring::TDynamicCounters>& counters)
                : Group(counters->GetSubgroup("subsystem", "levels"))
                , Level0(Group, "level", "0")
                , Level1to8(Group, "level", "1..8")
                , Level9to16(Group, "level", "9..16")
                , Level17(Group, "level", "17")
                , Level18(Group, "level", "18")
                , Level19(Group, "level", "19")
            {}
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TVDiskIFaceGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TVDiskIFaceGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TVDiskIFaceGroup)
            {
                COUNTER_INIT_IF_EXTENDED(MovedPatchMsgs, true);
                COUNTER_INIT_IF_EXTENDED(PatchStartMsgs, true);
                COUNTER_INIT_IF_EXTENDED(PatchDiffMsgs, true);
                COUNTER_INIT_IF_EXTENDED(PatchXorDiffMsgs, true);
                COUNTER_INIT_IF_EXTENDED(PutMsgs, true);
                COUNTER_INIT_IF_EXTENDED(MultiPutMsgs, true);
                COUNTER_INIT_IF_EXTENDED(GetMsgs, true);
                COUNTER_INIT_IF_EXTENDED(BlockMsgs, true);
                COUNTER_INIT_IF_EXTENDED(GetBlockMsgs, true);
                COUNTER_INIT_IF_EXTENDED(BlockAndGetMsgs, true);
                COUNTER_INIT_IF_EXTENDED(GCMsgs, true);
                COUNTER_INIT_IF_EXTENDED(GetBarrierMsgs, true);
                COUNTER_INIT_IF_EXTENDED(SyncMsgs, true);
                COUNTER_INIT_IF_EXTENDED(SyncFullMsgs, true);
                COUNTER_INIT_IF_EXTENDED(RecoveredHugeBlobMsgs, true);
                COUNTER_INIT_IF_EXTENDED(StatusMsgs, true);
                COUNTER_INIT_IF_EXTENDED(DbStatMsgs, true);
                COUNTER_INIT_IF_EXTENDED(AnubisPutMsgs, true);
                COUNTER_INIT_IF_EXTENDED(OsirisPutMsgs, true);

                COUNTER_INIT_PRIVATE(MovedPatchResMsgs, true);
                COUNTER_INIT_PRIVATE(PatchFoundPartsMsgs, true);
                COUNTER_INIT_PRIVATE(PatchXorDiffResMsgs, true);
                COUNTER_INIT_PRIVATE(PatchResMsgs, true);
                COUNTER_INIT_PRIVATE(PutResMsgs, true);
                COUNTER_INIT_PRIVATE(MultiPutResMsgs, true);
                COUNTER_INIT_PRIVATE(GetResMsgs, true);
                COUNTER_INIT_PRIVATE(BlockResMsgs, true);
                COUNTER_INIT_PRIVATE(GetBlockResMsgs, true);
                COUNTER_INIT_PRIVATE(GCResMsgs, true);
                COUNTER_INIT_PRIVATE(GetBarrierResMsgs, true);
                COUNTER_INIT_PRIVATE(SyncResMsgs, true);
                COUNTER_INIT_PRIVATE(SyncFullResMsgs, true);
                COUNTER_INIT_PRIVATE(RecoveredHugeBlobResMsgs, true);
                COUNTER_INIT_PRIVATE(StatusResMsgs, true);
                COUNTER_INIT_PRIVATE(DbStatResMsgs, true);
                COUNTER_INIT_PRIVATE(AnubisPutResMsgs, true);
                COUNTER_INIT_PRIVATE(OsirisPutResMsgs, true);

                COUNTER_INIT_IF_EXTENDED(PutTotalBytes, true);
                COUNTER_INIT_IF_EXTENDED(GetTotalBytes, true);
            }
                
            void MinHugeBlobInBytes(ui32 size) {
                auto getCounter = [&](ui32 size) {
                    return GroupCounters->GetSubgroup("MinHugeBlobInBytes", ToString(size))->GetCounter("count", 1);
                };
                if (PrevMinHugeBlobInBytes) {
                    *getCounter(PrevMinHugeBlobInBytes) = 0;
                }
                *getCounter(size) = 1;
                PrevMinHugeBlobInBytes = size;
            }

            COUNTER_DEF(MovedPatchMsgs);
            COUNTER_DEF(PatchStartMsgs);
            COUNTER_DEF(PatchDiffMsgs);
            COUNTER_DEF(PatchXorDiffMsgs);
            COUNTER_DEF(PutMsgs);
            COUNTER_DEF(MultiPutMsgs);
            COUNTER_DEF(GetMsgs);
            COUNTER_DEF(BlockMsgs);
            COUNTER_DEF(GetBlockMsgs);
            COUNTER_DEF(BlockAndGetMsgs);
            COUNTER_DEF(GCMsgs);
            COUNTER_DEF(GetBarrierMsgs);
            COUNTER_DEF(SyncMsgs);
            COUNTER_DEF(SyncFullMsgs);
            COUNTER_DEF(RecoveredHugeBlobMsgs);
            COUNTER_DEF(StatusMsgs);
            COUNTER_DEF(DbStatMsgs);
            COUNTER_DEF(AnubisPutMsgs);
            COUNTER_DEF(OsirisPutMsgs);

            COUNTER_DEF(MovedPatchResMsgs);
            COUNTER_DEF(PatchFoundPartsMsgs);
            COUNTER_DEF(PatchXorDiffResMsgs);
            COUNTER_DEF(PatchResMsgs);
            COUNTER_DEF(PutResMsgs);
            COUNTER_DEF(MultiPutResMsgs);
            COUNTER_DEF(GetResMsgs);
            COUNTER_DEF(BlockResMsgs);
            COUNTER_DEF(GetBlockResMsgs);
            COUNTER_DEF(GCResMsgs);
            COUNTER_DEF(GetBarrierResMsgs);
            COUNTER_DEF(SyncResMsgs);
            COUNTER_DEF(SyncFullResMsgs);
            COUNTER_DEF(RecoveredHugeBlobResMsgs);
            COUNTER_DEF(StatusResMsgs);
            COUNTER_DEF(DbStatResMsgs);
            COUNTER_DEF(AnubisPutResMsgs);
            COUNTER_DEF(OsirisPutResMsgs);

            COUNTER_DEF(PutTotalBytes);
            COUNTER_DEF(GetTotalBytes);
        private:
            ui32 PrevMinHugeBlobInBytes = 0;
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TDefragGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TDefragGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TDefragGroup)
            {
                COUNTER_INIT_IF_EXTENDED(DefragBytesRewritten, true);
                COUNTER_INIT_IF_EXTENDED(DefragThreshold, false);
            }

            COUNTER_DEF(DefragBytesRewritten);
            COUNTER_DEF(DefragThreshold);
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TBalancingGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TBalancingGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TBalancingGroup)
            {
                COUNTER_INIT(BalancingIterations, true);
                COUNTER_INIT(EpochTimeouts, true);
                COUNTER_INIT(ReplTokenAquired, true);
                COUNTER_INIT(OnMainByIngressButNotRealy, true);

                COUNTER_INIT(PlannedToSendOnMain, false);
                COUNTER_INIT(CandidatesToDelete, false);

                COUNTER_INIT(ReadFromHandoffBytes, true);
                COUNTER_INIT(ReadFromHandoffResponseBytes, true);
                COUNTER_INIT(ReadFromHandoffBatchTimeout, true);
                COUNTER_INIT(SentOnMain, true);
                COUNTER_INIT(SentOnMainBytes, true);
                COUNTER_INIT(SentOnMainWithResponseBytes, true);
                COUNTER_INIT(SendOnMainBatchTimeout, true);

                COUNTER_INIT(CandidatesToDeleteAskedFromMain, true);
                COUNTER_INIT(CandidatesToDeleteAskedFromMainResponse, true);
                COUNTER_INIT(CandidatesToDeleteAskFromMainBatchTimeout, true);
                COUNTER_INIT(MarkedReadyToDelete, true);
                COUNTER_INIT(MarkedReadyToDeleteBytes, true);
                COUNTER_INIT(MarkedReadyToDeleteResponse, true);
                COUNTER_INIT(MarkedReadyToDeleteWithResponseBytes, true);
                COUNTER_INIT(MarkReadyBatchTimeout, true);
            }

            COUNTER_DEF(BalancingIterations);
            COUNTER_DEF(EpochTimeouts);
            COUNTER_DEF(ReplTokenAquired);
            COUNTER_DEF(OnMainByIngressButNotRealy);

            COUNTER_DEF(PlannedToSendOnMain);
            COUNTER_DEF(ReadFromHandoffBytes);
            COUNTER_DEF(ReadFromHandoffResponseBytes);
            COUNTER_DEF(ReadFromHandoffBatchTimeout);
            COUNTER_DEF(SentOnMain);
            COUNTER_DEF(SentOnMainBytes);
            COUNTER_DEF(SentOnMainWithResponseBytes);
            COUNTER_DEF(SendOnMainBatchTimeout);
            COUNTER_DEF(CandidatesToDelete);
            COUNTER_DEF(CandidatesToDeleteAskedFromMain);
            COUNTER_DEF(CandidatesToDeleteAskedFromMainResponse);
            COUNTER_DEF(CandidatesToDeleteAskFromMainBatchTimeout);
            COUNTER_DEF(MarkedReadyToDelete);
            COUNTER_DEF(MarkedReadyToDeleteBytes);
            COUNTER_DEF(MarkedReadyToDeleteResponse);
            COUNTER_DEF(MarkedReadyToDeleteWithResponseBytes);
            COUNTER_DEF(MarkReadyBatchTimeout);
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TOutOfSpaceGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TOutOfSpaceGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TOutOfSpaceGroup)
            {
                COUNTER_INIT(ResponsesWithDiskSpaceRed, true);
                COUNTER_INIT(ResponsesWithDiskSpaceOrange, true);
                COUNTER_INIT(ResponsesWithDiskSpacePreOrange, true);
                COUNTER_INIT(ResponsesWithDiskSpaceLightOrange, true);
                COUNTER_INIT(ResponsesWithDiskSpaceYellowStop, true);
                COUNTER_INIT(ResponsesWithDiskSpaceLightYellowMove, true);
            }

            COUNTER_DEF(ResponsesWithDiskSpaceRed);
            COUNTER_DEF(ResponsesWithDiskSpaceOrange);
            COUNTER_DEF(ResponsesWithDiskSpacePreOrange);
            COUNTER_DEF(ResponsesWithDiskSpaceLightOrange);
            COUNTER_DEF(ResponsesWithDiskSpaceYellowStop);
            COUNTER_DEF(ResponsesWithDiskSpaceLightYellowMove);
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // THandleClassGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class THandleClassGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(THandleClassGroup)
            {
                COUNTER_INIT_IF_EXTENDED(Undefined, true);
                COUNTER_INIT_IF_EXTENDED(GetDiscover, true);
                COUNTER_INIT_IF_EXTENDED(GetFast, true);
                COUNTER_INIT_IF_EXTENDED(GetAsync, true);
                COUNTER_INIT_IF_EXTENDED(GetLow, true);
                COUNTER_INIT_IF_EXTENDED(PutTabletLog, true);
                COUNTER_INIT_IF_EXTENDED(PutUserData, true);
                COUNTER_INIT_IF_EXTENDED(PutAsyncBlob, true);
            }

            COUNTER_DEF(Undefined);
            COUNTER_DEF(GetDiscover);
            COUNTER_DEF(GetFast);
            COUNTER_DEF(GetAsync);
            COUNTER_DEF(GetLow);
            COUNTER_DEF(PutTabletLog);
            COUNTER_DEF(PutUserData);
            COUNTER_DEF(PutAsyncBlob);
            
            ::NMonitoring::TDeprecatedCounter &GetCounter(const std::optional<NKikimrBlobStorage::EGetHandleClass>& handleClass) {
                if (!handleClass) {
                    return Undefined();
                }
                switch (*handleClass) {
                    case NKikimrBlobStorage::AsyncRead:
                        return GetAsync();
                    case NKikimrBlobStorage::FastRead:
                        return GetFast();
                    case NKikimrBlobStorage::Discover:
                        return GetDiscover();
                    case NKikimrBlobStorage::LowRead:
                        return GetLow();
                    default:
                        return Undefined();
                }
            }
            ::NMonitoring::TDeprecatedCounter &GetCounter(const std::optional<NKikimrBlobStorage::EPutHandleClass>& handleClass) {
                if (!handleClass) {
                    return Undefined();
                }
                switch (*handleClass) {
                    case NKikimrBlobStorage::TabletLog:
                        return PutTabletLog();
                    case NKikimrBlobStorage::AsyncBlob:
                        return PutAsyncBlob();
                    case NKikimrBlobStorage::UserData:
                        return PutUserData();
                    default:
                        return Undefined();
                }
            }

            ::NMonitoring::TDeprecatedCounter &GetCounter() {
                return Undefined();
            }
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TResponseStatusGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TResponseStatusGroup {
        public:
            TIntrusivePtr<::NMonitoring::TDynamicCounters> Group;

            THandleClassGroup Undefined;
            THandleClassGroup ResponsesWithStatusError;
            THandleClassGroup ResponsesWithStatusRace;
            THandleClassGroup ResponsesWithStatusBlocked;
            THandleClassGroup ResponsesWithStatusOutOfSpace;
            THandleClassGroup ResponsesWithStatusDeadline;
            THandleClassGroup ResponsesWithStatusNotReady;
            THandleClassGroup ResponsesWithStatusVdiskErrorState;

            TResponseStatusGroup(const TIntrusivePtr<::NMonitoring::TDynamicCounters>& counters)
                : Group(counters->GetSubgroup("subsystem", "statuses"))
                , Undefined(Group, "status", "UNDEFINED")
                , ResponsesWithStatusError(Group, "status", "ERROR")
                , ResponsesWithStatusRace(Group, "status", "RACE")
                , ResponsesWithStatusBlocked(Group, "status", "BLOCKED")
                , ResponsesWithStatusOutOfSpace(Group, "status", "OUT_OF_SPACE")
                , ResponsesWithStatusDeadline(Group, "status", "DEADLINE")
                , ResponsesWithStatusNotReady(Group, "status", "NOT_READY")
                , ResponsesWithStatusVdiskErrorState(Group, "status", "VDISK_STATUS_ERROR")
            {}

            template <typename THandleClassType>
            ::NMonitoring::TDeprecatedCounter &GetCounterByHandleClass(NKikimrProto::EReplyStatus status, const std::optional<THandleClassType>& handleClass = std::nullopt) {
                switch (status) {
                    case NKikimrProto::ERROR:
                        return ResponsesWithStatusError.GetCounter(handleClass);
                    case NKikimrProto::RACE:
                        return ResponsesWithStatusRace.GetCounter(handleClass);
                    case NKikimrProto::BLOCKED:
                        return ResponsesWithStatusBlocked.GetCounter(handleClass);
                    case NKikimrProto::OUT_OF_SPACE:
                        return ResponsesWithStatusOutOfSpace.GetCounter(handleClass);
                    case NKikimrProto::DEADLINE:
                        return ResponsesWithStatusDeadline.GetCounter(handleClass);
                    case NKikimrProto::NOTREADY:
                        return ResponsesWithStatusNotReady.GetCounter(handleClass);
                    case NKikimrProto::VDISK_ERROR_STATE:
                        return ResponsesWithStatusVdiskErrorState.GetCounter(handleClass);
                    default: return Undefined.GetCounter(handleClass);
                }
            }

            ::NMonitoring::TDeprecatedCounter &GetCounter(NKikimrProto::EReplyStatus status) {
                return GetCounterByHandleClass(status, std::optional<NKikimrBlobStorage::EPutHandleClass>{});
            }
            ::NMonitoring::TDeprecatedCounter &GetCounter(NKikimrProto::EReplyStatus status, const std::optional<NKikimrBlobStorage::EPutHandleClass>& handleClass) {
                return GetCounterByHandleClass(status, handleClass);
            }
            ::NMonitoring::TDeprecatedCounter &GetCounter(NKikimrProto::EReplyStatus status, const std::optional<NKikimrBlobStorage::EGetHandleClass>& handleClass) {
                return GetCounterByHandleClass(status, handleClass);
            }
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TCostTrackerGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TCostTrackerGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TCostTrackerGroup)
            {
                COUNTER_INIT_IF_EXTENDED(UserDiskCost, true);
                COUNTER_INIT_IF_EXTENDED(CompactionDiskCost, true);
                COUNTER_INIT_IF_EXTENDED(ScrubDiskCost, true);
                COUNTER_INIT_IF_EXTENDED(DefragDiskCost, true);
                COUNTER_INIT_IF_EXTENDED(InternalDiskCost, true);
                COUNTER_INIT_IF_EXTENDED(DiskTimeAvailableCtr, false);
            }

            COUNTER_DEF(UserDiskCost);
            COUNTER_DEF(CompactionDiskCost);
            COUNTER_DEF(ScrubDiskCost);
            COUNTER_DEF(DefragDiskCost);
            COUNTER_DEF(InternalDiskCost);
            COUNTER_DEF(DiskTimeAvailableCtr);
        };

        class TScrubGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TScrubGroup)
            {
                COUNTER_INIT(SstProcessed, true);
                COUNTER_INIT(HugeBlobsRead, true);
                COUNTER_INIT(HugeBlobBytesRead, true);
                COUNTER_INIT(SmallBlobIntervalsRead, true);
                COUNTER_INIT(SmallBlobIntervalBytesRead, true);
                COUNTER_INIT(SmallBlobsRead, true);
                COUNTER_INIT(SmallBlobBytesRead, true);
                COUNTER_INIT(UnreadableBlobsFound, false);
                COUNTER_INIT(BlobsFixed, false);
            }

            COUNTER_DEF(SstProcessed);
            COUNTER_DEF(HugeBlobsRead);
            COUNTER_DEF(HugeBlobBytesRead);
            COUNTER_DEF(SmallBlobIntervalsRead);
            COUNTER_DEF(SmallBlobIntervalBytesRead);
            COUNTER_DEF(SmallBlobsRead);
            COUNTER_DEF(SmallBlobBytesRead);
            COUNTER_DEF(UnreadableBlobsFound);
            COUNTER_DEF(BlobsFixed);
        };

        class TMalfunctionGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TMalfunctionGroup)
            {
                COUNTER_INIT(DroppingStuckInternalQueue, false);
            }

            COUNTER_DEF(DroppingStuckInternalQueue);
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TTimerGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TTimerGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TTimerGroup)
            {
                COUNTER_INIT(SkeletonFrontUptimeSeconds, false);
            }

            COUNTER_DEF(SkeletonFrontUptimeSeconds);
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TCompactionStrategyGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TCompactionStrategyGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TCompactionStrategyGroup)
            {
                COUNTER_INIT(BlobsDelSst, true);
                COUNTER_INIT(BlobsPromoteSsts, true);
                COUNTER_INIT(BlobsExplicit, true);
                COUNTER_INIT(BlobsBalance, true);
                COUNTER_INIT(BlobsFreeSpace, true);
                COUNTER_INIT(BlobsSqueeze, true);

                COUNTER_INIT(BlocksPromoteSsts, true);
                COUNTER_INIT(BlocksExplicit, true);
                COUNTER_INIT(BlocksBalance, true);

                COUNTER_INIT(BarriersPromoteSsts, true);
                COUNTER_INIT(BarriersExplicit, true);
                COUNTER_INIT(BarriersBalance, true);
            }

            COUNTER_DEF(BlobsDelSst);
            COUNTER_DEF(BlobsPromoteSsts);
            COUNTER_DEF(BlobsExplicit);
            COUNTER_DEF(BlobsBalance);
            COUNTER_DEF(BlobsFreeSpace);
            COUNTER_DEF(BlobsSqueeze);

            COUNTER_DEF(BlocksPromoteSsts);
            COUNTER_DEF(BlocksExplicit);
            COUNTER_DEF(BlocksBalance);

            COUNTER_DEF(BarriersPromoteSsts);
            COUNTER_DEF(BarriersExplicit);
            COUNTER_DEF(BarriersBalance);
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TDeepScrubbingGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TDeepScrubbingGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TDeepScrubbingGroup)
            {
                COUNTER_INIT(BlobsChecked, true);
                COUNTER_INIT(CheckIntegritySuccesses, true);
                COUNTER_INIT(CheckIntegrityErrors, true);
                COUNTER_INIT(UnknownDataStatus, true);
                COUNTER_INIT(UnknownPlacementStatus, true);
                COUNTER_INIT(DataIssues, true);
                COUNTER_INIT(PlacementIssues, true);
            }

            COUNTER_DEF(BlobsChecked);
            COUNTER_DEF(CheckIntegritySuccesses);
            COUNTER_DEF(CheckIntegrityErrors);
            COUNTER_DEF(UnknownDataStatus);
            COUNTER_DEF(UnknownPlacementStatus);
            COUNTER_DEF(DataIssues);
            COUNTER_DEF(PlacementIssues);
        };

        class TDeepScrubbingSubgroups {
        public:
            TDeepScrubbingSubgroups(TIntrusivePtr<NMonitoring::TDynamicCounters> counters) {
                for (bool isHuge : {true, false}) {
                    for (TErasureType::EErasureSpecies erasure :
                            {TErasureType::ErasureNone, TErasureType::Erasure4Plus2Block,
                            TErasureType::ErasureMirror3of4, TErasureType::ErasureMirror3dc}) {
                        ::NMonitoring::TDynamicCounterPtr subgroup = counters
                                ->GetSubgroup("blobSize", isHuge ? "huge" : "small")
                                ->GetSubgroup("erasure", TErasureType::ErasureSpeciesName(erasure));
                        Subgroups.insert({GetKey(isHuge, erasure), TDeepScrubbingGroup(subgroup)});
                    }
                }
            }

            TDeepScrubbingGroup* GetCounters(bool isHuge, TErasureType::EErasureSpecies erasure) {
                auto it = Subgroups.find(GetKey(isHuge, erasure));
                if (it == Subgroups.end()) {
                    return nullptr;
                }
                return &it->second;
            }

        private:
            std::unordered_map<ui64, TDeepScrubbingGroup> Subgroups;

            ui64 GetKey(bool isHuge, TErasureType::EErasureSpecies erasure) {
                return ((ui64)isHuge << 32) + (ui64)erasure;
            }
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TCounterGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TCounterGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TCounterGroup)
            {
                COUNTER_INIT(VDiskCount, false);
            }

            COUNTER_DEF(VDiskCount);
        };

        ///////////////////////////////////////////////////////////////////////////////////
        // TFullSyncGroup
        ///////////////////////////////////////////////////////////////////////////////////
        class TFullSyncGroup : public TBase {
        public:
            GROUP_CONSTRUCTOR(TFullSyncGroup)
            {
                COUNTER_INIT(UnorderedDataProtocolActorsCreated, false);
                COUNTER_INIT(UnorderedDataProtocolActorsTerminated, false);
            }

            COUNTER_DEF(UnorderedDataProtocolActorsCreated);
            COUNTER_DEF(UnorderedDataProtocolActorsTerminated);
        };

    } // NMonGroup
} // NKikimr
