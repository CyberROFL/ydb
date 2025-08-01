#pragma once

#include "propose_tx.h"

#include <ydb/core/tx/columnshard/columnshard_impl.h>
#include <ydb/core/tx/columnshard/transactions/transactions/tx_add_sharding_info.h>
#include <ydb/core/tx/columnshard/common/path_id.h>

namespace NKikimr::NColumnShard {

class TSchemaTransactionOperator: public IProposeTxOperator, public TMonitoringObjectsCounter<TSchemaTransactionOperator> {
private:
    using TBase = IProposeTxOperator;

    using TProposeResult = TTxController::TProposeResult;
    static inline auto Registrator = TFactory::TRegistrator<TSchemaTransactionOperator>(NKikimrTxColumnShard::TX_KIND_SCHEMA);
    std::unique_ptr<NTabletFlatExecutor::ITransaction> TxAddSharding;
    NKikimrTxColumnShard::TSchemaTxBody SchemaTxBody;
    THashSet<TActorId> NotifySubscribers;
    std::shared_ptr<NSubscriber::ISubscriber> WaitOnPropose;

    virtual void DoOnTabletInit(TColumnShard& owner) override;

    template <class TInfoProto>
    THashSet<TInternalPathId> GetNotErasedTableIds(const TColumnShard& owner, const TInfoProto& tables) const {
        THashSet<TInternalPathId> result;
        for (auto&& i : tables) {
            const auto& schemeShardLocalPathId = TSchemeShardLocalPathId::FromProto(i);
            if (const auto internalPathId = owner.TablesManager.ResolveInternalPathId(schemeShardLocalPathId, false)) {
                if (owner.TablesManager.HasTable(*internalPathId, true)) {
                    result.emplace(*internalPathId);
                }
            }
        }
        if (result.size()) {
            AFL_WARN(NKikimrServices::TX_COLUMNSHARD)("event", "async_schema")("reason", JoinSeq(",", result));
        } else {
            AFL_DEBUG(NKikimrServices::TX_COLUMNSHARD)("event", "sync_schema");
        }
        return result;
    }

    virtual TTxController::TProposeResult DoStartProposeOnExecute(TColumnShard& owner, NTabletFlatExecutor::TTransactionContext& txc) override;
    virtual void DoStartProposeOnComplete(TColumnShard& owner, const TActorContext& /*ctx*/) override;
    virtual void DoFinishProposeOnExecute(TColumnShard& /*owner*/, NTabletFlatExecutor::TTransactionContext& /*txc*/) override {
    }
    virtual void DoFinishProposeOnComplete(TColumnShard& /*owner*/, const TActorContext& /*ctx*/) override {
    }
    virtual TString DoGetOpType() const override {
        switch (SchemaTxBody.TxBody_case()) {
            case NKikimrTxColumnShard::TSchemaTxBody::kInitShard:
                return "Scheme:InitShard";
            case NKikimrTxColumnShard::TSchemaTxBody::kEnsureTables:
                return "Scheme:EnsureTables";
            case NKikimrTxColumnShard::TSchemaTxBody::kAlterTable:
                return "Scheme:AlterTable";
            case NKikimrTxColumnShard::TSchemaTxBody::kAlterStore:
                return "Scheme:AlterStore";
            case NKikimrTxColumnShard::TSchemaTxBody::kDropTable:
                return "Scheme:DropTable";
            case NKikimrTxColumnShard::TSchemaTxBody::kMoveTable:
                return "Scheme:MoveTable";
            case NKikimrTxColumnShard::TSchemaTxBody::TXBODY_NOT_SET:
                return "Scheme:TXBODY_NOT_SET";
        }
    }
    virtual bool DoIsAsync() const override {
        return !!WaitOnPropose;
    }
    virtual bool DoParse(TColumnShard& owner, const TString& data) override {
        if (!SchemaTxBody.ParseFromString(data)) {
            return false;
        }
        if (SchemaTxBody.HasGranuleShardingInfo()) {
            NSharding::TGranuleShardingLogicContainer infoContainer;
            if (!infoContainer.DeserializeFromProto(SchemaTxBody.GetGranuleShardingInfo().GetContainer())) {
                AFL_ERROR(NKikimrServices::TX_COLUMNSHARD)("problem", "cannot parse incoming tx message");
                return false;
            }
            TxAddSharding = owner.TablesManager.CreateAddShardingInfoTx(
                owner, TSchemeShardLocalPathId::FromProto(SchemaTxBody.GetGranuleShardingInfo()), SchemaTxBody.GetGranuleShardingInfo().GetVersionId(), infoContainer);
        }
        return true;
    }

public:
    using TBase::TBase;

    virtual bool ProgressOnExecute(
        TColumnShard& owner, const NOlap::TSnapshot& version, NTabletFlatExecutor::TTransactionContext& txc) override {
        if (!!TxAddSharding) {
            auto* tx = dynamic_cast<TTxAddShardingInfo*>(TxAddSharding.get());
            AFL_VERIFY(tx);
            tx->SetSnapshotVersion(version);
            TxAddSharding->Execute(txc, NActors::TActivationContext::AsActorContext());
        }
        if (SchemaTxBody.TxBody_case() != NKikimrTxColumnShard::TSchemaTxBody::TXBODY_NOT_SET) {
            owner.RunSchemaTx(SchemaTxBody, version, txc);
            owner.ProtectSchemaSeqNo(SchemaTxBody.GetSeqNo(), txc);
        }
        return true;
    }

    virtual bool ProgressOnComplete(TColumnShard& owner, const TActorContext& ctx) override {
        if (!!TxAddSharding) {
            TxAddSharding->Complete(ctx);
        }
        for (TActorId subscriber : NotifySubscribers) {
            auto event = MakeHolder<TEvColumnShard::TEvNotifyTxCompletionResult>(owner.TabletID(), GetTxId());
            ctx.Send(subscriber, event.Release(), 0, 0);
        }

        return true;
    }

    virtual bool ExecuteOnAbort(TColumnShard& /*owner*/, NTabletFlatExecutor::TTransactionContext& /*txc*/) override {
        return true;
    }
    virtual bool CompleteOnAbort(TColumnShard& /*owner*/, const TActorContext& /*ctx*/) override {
        return true;
    }

    virtual void RegisterSubscriber(const TActorId& actorId) override {
        NotifySubscribers.insert(actorId);
    }

private:
    TConclusionStatus ValidateTables(::google::protobuf::RepeatedPtrField<::NKikimrTxColumnShard::TCreateTable> tables) const;

    TConclusionStatus ValidateTableSchema(const NKikimrSchemeOp::TColumnTableSchema& schema) const;

    TConclusionStatus ValidateTablePreset(const NKikimrSchemeOp::TColumnTableSchemaPreset& preset) const {
        if (preset.HasName() && preset.GetName() != "default") {
            return TConclusionStatus::Fail("Preset name must be empty or 'default', but '" + preset.GetName() + "' got");
        }
        return ValidateTableSchema(preset.GetSchema());
    }
    virtual TString DoDebugString() const override {
        return "SCHEME:" + SchemaTxBody.DebugString();
    }
};

}   // namespace NKikimr::NColumnShard
