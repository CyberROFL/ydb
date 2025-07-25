#include "read_context.h"

#include <ydb/core/tx/columnshard/engines/reader/common_reader/constructor/resolver.h>
#include <ydb/core/tx/conveyor_composite/usage/service.h>

namespace NKikimr::NOlap::NReader {

IDataReader::IDataReader(const std::shared_ptr<TReadContext>& context)
    : Context(context) {
}

TReadContext::TReadContext(const std::shared_ptr<IStoragesManager>& storagesManager,
    const std::shared_ptr<NDataAccessorControl::IDataAccessorsManager>& dataAccessorsManager,
    const NColumnShard::TConcreteScanCounters& counters, const TReadMetadataBase::TConstPtr& readMetadata, const TActorId& scanActorId,
    const TActorId& resourceSubscribeActorId, const TActorId& readCoordinatorActorId, const TComputeShardingPolicy& computeShardingPolicy,
    const ui64 scanId, const NConveyorComposite::TCPULimitsConfig& cpuLimits, NKqp::NScheduler::TSchedulableTaskPtr schedulableTask)
    : StoragesManager(storagesManager)
    , DataAccessorsManager(dataAccessorsManager)
    , SchedulableTask(std::move(schedulableTask))
    , Counters(counters)
    , ReadMetadata(readMetadata)
    , ResourcesTaskContext("CS::SCAN_READ", counters.ResourcesSubscriberCounters)
    , ScanId(scanId)
    , ScanActorId(scanActorId)
    , ResourceSubscribeActorId(resourceSubscribeActorId)
    , ReadCoordinatorActorId(readCoordinatorActorId)
    , ComputeShardingPolicy(computeShardingPolicy)
    , ConveyorProcessGuard(NConveyorComposite::TScanServiceOperator::StartProcess(ScanId, ::ToString(ScanId), cpuLimits)) {
    Y_ABORT_UNLESS(ReadMetadata);
    if (ReadMetadata->HasResultSchema()) {
        Resolver = std::make_shared<NCommon::TIndexColumnResolver>(ReadMetadata->GetResultSchema()->GetIndexInfo());
    }
}

}   // namespace NKikimr::NOlap::NReader
