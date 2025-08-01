#include "test_client.h"

#include <ydb/core/testlib/basics/runtime.h>
#include <ydb/core/base/path.h>
#include <ydb/core/base/appdata.h>
#include <ydb/core/base/hive.h>
#include <ydb/core/base/feature_flags_service.h>
#include <ydb/core/driver_lib/run/auto_config_initializer.h>
#include <ydb/core/driver_lib/run/config_helpers.h>
#include <ydb/core/viewer/viewer.h>
#include <ydb/public/lib/base/msgbus.h>
#include <ydb/core/grpc_services/db_metadata_cache.h>
#include <ydb/core/grpc_services/grpc_request_proxy.h>
#include <ydb/services/auth/grpc_service.h>
#include <ydb/services/fq/grpc_service.h>
#include <ydb/services/fq/private_grpc.h>
#include <ydb/services/cms/grpc_service.h>
#include <ydb/services/datastreams/grpc_service.h>
#include <ydb/services/view/grpc_service.h>
#include <ydb/services/ymq/grpc_service.h>
#include <ydb/services/kesus/grpc_service.h>
#include <ydb/core/grpc_services/grpc_mon.h>
#include <ydb/services/ydb/ydb_clickhouse_internal.h>
#include <ydb/services/ydb/ydb_dummy.h>
#include <ydb/services/ydb/ydb_export.h>
#include <ydb/services/ydb/ydb_import.h>
#include <ydb/services/ydb/ydb_operation.h>
#include <ydb/services/ydb/ydb_object_storage.h>
#include <ydb/services/ydb/ydb_query.h>
#include <ydb/services/ydb/ydb_scheme.h>
#include <ydb/services/ydb/ydb_scripting.h>
#include <ydb/services/ydb/ydb_table.h>
#include <ydb/services/ydb/ydb_logstore.h>
#include <ydb/services/tablet/ydb_tablet.h>
#include <ydb/services/discovery/grpc_service.h>
#include <ydb/services/rate_limiter/grpc_service.h>
#include <ydb/services/persqueue_cluster_discovery/grpc_service.h>
#include <ydb/services/deprecated/persqueue_v0/persqueue.h>
#include <ydb/services/persqueue_v1/persqueue.h>
#include <ydb/services/persqueue_v1/topic.h>
#include <ydb/services/persqueue_v1/grpc_pq_write.h>
#include <ydb/services/replication/grpc_service.h>
#include <ydb/services/monitoring/grpc_service.h>
#include <ydb/core/fq/libs/control_plane_proxy/control_plane_proxy.h>
#include <ydb/core/fq/libs/control_plane_storage/control_plane_storage.h>
#include <ydb/core/fq/libs/db_id_async_resolver_impl/database_resolver.h>
#include <ydb/core/fq/libs/db_id_async_resolver_impl/db_async_resolver_impl.h>
#include <ydb/core/fq/libs/db_id_async_resolver_impl/http_proxy.h>
#include <ydb/core/fq/libs/db_id_async_resolver_impl/mdb_endpoint_generator.h>
#include <ydb/core/blobstorage/base/blobstorage_events.h>
#include <ydb/core/client/metadata/types_metadata.h>
#include <ydb/core/client/metadata/functions_metadata.h>
#include <ydb/core/client/minikql_compile/mkql_compile_service.h>
#include <ydb/core/cms/console/configs_dispatcher.h>
#include <ydb/core/cms/console/console.h>
#include <ydb/core/cms/console/feature_flags_configurator.h>
#include <ydb/core/cms/console/immediate_controls_configurator.h>
#include <ydb/core/cms/console/jaeger_tracing_configurator.h>
#include <ydb/core/formats/clickhouse_block.h>
#include <ydb/core/client/server/msgbus_server.h>
#include <ydb/core/security/ticket_parser.h>
#include <ydb/core/security/ldap_auth_provider/ldap_auth_provider.h>
#include <ydb/core/security/ticket_parser_settings.h>
#include <ydb/core/base/user_registry.h>
#include <ydb/core/health_check/health_check.h>
#include <ydb/core/kafka_proxy/actors/kafka_metrics_actor.h>
#include <ydb/core/kafka_proxy/kafka_listener.h>
#include <ydb/core/kafka_proxy/actors/kafka_metadata_actor.h>
#include <ydb/core/kafka_proxy/kafka_metrics.h>
#include <ydb/core/kafka_proxy/kafka_transactions_coordinator.h>
#include <ydb/core/kqp/common/kqp.h>
#include <ydb/core/kqp/rm_service/kqp_rm_service.h>
#include <ydb/core/kqp/proxy_service/kqp_proxy_service.h>
#include <ydb/core/kqp/finalize_script_service/kqp_finalize_script_service.h>
#include <ydb/core/metering/metering.h>
#include <ydb/core/protos/stream.pb.h>
#include <ydb/core/protos/schemeshard/operations.pb.h>
#include <ydb/library/services/services.pb.h>
#include <ydb/core/tablet_flat/tablet_flat_executed.h>
#include <ydb/core/tx/columnshard/columnshard.h>
#include <ydb/core/tx/coordinator/coordinator.h>
#include <ydb/core/tx/datashard/datashard.h>
#include <ydb/core/tx/long_tx_service/public/events.h>
#include <ydb/core/tx/long_tx_service/long_tx_service.h>
#include <ydb/core/tx/mediator/mediator.h>
#include <ydb/core/tx/replication/controller/controller.h>
#include <ydb/core/tx/schemeshard/schemeshard.h>
#include <ydb/core/tx/sequenceproxy/sequenceproxy.h>
#include <ydb/core/tx/sequenceshard/sequenceshard.h>
#include <ydb/core/tx/tx_allocator/txallocator.h>
#include <ydb/core/tx/tx_proxy/proxy.h>
#include <ydb/core/tx/time_cast/time_cast.h>
#include <ydb/core/mind/address_classification/net_classifier.h>
#include <ydb/core/mind/bscontroller/bsc.h>
#include <ydb/core/mind/hive/hive.h>
#include <ydb/core/mind/labels_maintainer.h>
#include <ydb/core/mind/tenant_pool.h>
#include <ydb/core/mind/tenant_slot_broker.h>
#include <ydb/core/mind/tenant_node_enumeration.h>
#include <ydb/core/mind/node_broker.h>
#include <ydb/core/mon_alloc/monitor.h>
#include <ydb/core/kesus/tablet/events.h>
#include <ydb/core/sys_view/service/sysview_service.h>
#include <yql/essentials/minikql/mkql_function_registry.h>
#include <yql/essentials/minikql/invoke_builtins/mkql_builtins.h>
#include <yql/essentials/public/issue/yql_issue_message.h>
#include <ydb/library/yql/utils/actor_log/log.h>
#include <ydb/core/engine/mkql_engine_flat.h>

#include <library/cpp/testing/unittest/registar.h>
#include <ydb/core/kesus/proxy/proxy.h>
#include <ydb/core/kesus/tablet/tablet.h>
#include <ydb/core/sys_view/processor/processor.h>
#include <ydb/core/statistics/aggregator/aggregator.h>
#include <ydb/core/statistics/service/service.h>
#include <ydb/core/keyvalue/keyvalue.h>
#include <ydb/core/persqueue/pq.h>
#include <ydb/core/persqueue/cluster_tracker.h>
#include <ydb/core/persqueue/dread_cache_service/caching_service.h>
#include <ydb/library/security/ydb_credentials_provider_factory.h>
#include <ydb/core/fq/libs/init/init.h>
#include <ydb/core/fq/libs/mock/yql_mock.h>
#include <ydb/services/metadata/ds_table/service.h>
#include <ydb/services/metadata/service.h>
#include <ydb/services/ext_index/common/config.h>
#include <ydb/services/ext_index/common/service.h>
#include <ydb/services/ext_index/service/executor.h>
#include <ydb/core/tx/conveyor/service/service.h>
#include <ydb/core/tx/conveyor/usage/service.h>
#include <ydb/core/tx/conveyor_composite/usage/service.h>
#include <ydb/core/tx/conveyor_composite/service/service.h>
#include <ydb/core/tx/priorities/usage/service.h>
#include <ydb/core/tx/limiter/grouped_memory/usage/service.h>
#include <ydb/core/tx/columnshard/data_accessor/cache_policy/policy.h>
#include <ydb/core/tx/general_cache/usage/service.h>
#include <ydb/library/folder_service/mock/mock_folder_service_adapter.h>

#include <ydb/core/client/server/ic_nodes_cache_service.h>

#include <ydb/library/actors/http/http_proxy.h>
#include <ydb/library/actors/interconnect/interconnect.h>

#include <ydb/library/grpc/server/actors/logger.h>

#include <util/system/sanitizers.h>
#include <util/system/valgrind.h>
#include <util/system/env.h>

namespace NKikimr {

class TTestMkqlInvoker {

    THolder<NMsgBusProxy::TBusRequest> Request;
    TVector<TString*> WriteResolvedKeysTo;
    TAutoPtr<TEvTxUserProxy::TEvProposeTransaction> Proposal;
    TAutoPtr<NKikimrTxUserProxy::TEvProposeTransactionStatus> ProposalStatus;
    TActorId Sender;
    NActors::TTestActorRuntime* Runtime;

    THashMap<TString, ui64> CompileResolveCookies;
    TString TextProgramForCompilation;
    bool CompilationRetried;

    std::unique_ptr<NMsgBusProxy::TBusResponse> ReplyWithResult(NMsgBusProxy::EResponseStatus status, NKikimrTxUserProxy::TEvProposeTransactionStatus &result);

    std::unique_ptr<NMsgBusProxy::TBusResponse> RetryResolve() {
        if (CompilationRetried || !TextProgramForCompilation)
            return nullptr;

        auto *compEv = new TMiniKQLCompileServiceEvents::TEvCompile(TextProgramForCompilation);
        compEv->ForceRefresh = false;
        compEv->CompileResolveCookies = std::move(CompileResolveCookies);

        Runtime->Send(new IEventHandle(
            GetMiniKQLCompileServiceID(),
            Sender,
            compEv
        ));

        TAutoPtr<IEventHandle> handle;
        auto ev = Runtime->GrabEdgeEventRethrow<TMiniKQLCompileServiceEvents::TEvCompileStatus>(handle);
        CompilationRetried = true;
        return Handle(ev);
    }

    std::unique_ptr<NMsgBusProxy::TBusResponse> Handle(TMiniKQLCompileServiceEvents::TEvCompileStatus* ev) {
        auto* mkqlTx = Proposal->Record.MutableTransaction()->MutableMiniKQLTransaction();

        const auto& result = ev->Result;

        const bool need2CompileProgram = (bool)TextProgramForCompilation;
        const bool need2CompileParams = mkqlTx->HasParams() && mkqlTx->GetParams().HasText();
        const TString& pgm = ev->Program;
        Y_ABORT_UNLESS((need2CompileProgram && TextProgramForCompilation == pgm) // TODO: do not check texts, trust cookies
            || (need2CompileParams && mkqlTx->GetParams().GetText() == pgm));

        if (need2CompileProgram && TextProgramForCompilation == pgm) {
            auto* compileResults = ProposalStatus->MutableMiniKQLCompileResults();
            auto* pgm = mkqlTx->MutableProgram();
            if (result.Errors.Empty()) {
                pgm->ClearText();
                pgm->SetBin(result.CompiledProgram);
                compileResults->SetCompiledProgram(result.CompiledProgram);
            } else {
                NYql::IssuesToMessage(result.Errors, compileResults->MutableProgramCompileErrors());
            }
            CompileResolveCookies = std::move(ev->CompileResolveCookies);
        }

        if (need2CompileParams && mkqlTx->GetParams().GetText() == pgm) {
            auto* compileResults = ProposalStatus->MutableMiniKQLCompileResults();
            auto* params = mkqlTx->MutableParams();
            if (result.Errors.Empty()) {
                params->ClearText();
                params->SetBin(result.CompiledProgram);
                compileResults->SetCompiledParams(result.CompiledProgram);
            } else {
                NYql::IssuesToMessage(result.Errors, compileResults->MutableParamsCompileErrors());
            }
        }

        return AllRequestsCompleted();
    }

public:

    TTestMkqlInvoker(NActors::TTestActorRuntime* runtime, THolder<NMsgBusProxy::TBusRequest> request)
        : Request(std::move(request))
        , Runtime(runtime)
        , CompilationRetried(false)
    {
        Sender = Runtime->AllocateEdgeActor();
    }

    std::unique_ptr<NMsgBusProxy::TBusResponse> AllRequestsCompleted() {
        auto &transaction = Proposal->Record.GetTransaction();
        if (transaction.HasMiniKQLTransaction())
            return AllRequestsCompletedMKQL();
        else
            Y_ABORT("Unexpected transaction type");

        Y_UNREACHABLE();
    }

    std::unique_ptr<NMsgBusProxy::TBusResponse> Handle(TEvTxUserProxy::TEvProposeTransactionStatus* msg) {
        const TEvTxUserProxy::TEvProposeTransactionStatus::EStatus status = static_cast<TEvTxUserProxy::TEvProposeTransactionStatus::EStatus>(msg->Record.GetStatus());
        NKikimrTxUserProxy::TEvProposeTransactionStatus emptyProposeStatus;
        switch (status) {
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::ProxyAccepted:
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::ProxyResolved:
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::ProxyPrepared:
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::CoordinatorPlanned:
        // transitional statuses
            return nullptr;
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::ExecComplete:
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::ExecAlready:
        // completion
            return ReplyWithResult(NMsgBusProxy::EResponseStatus::MSTATUS_OK, msg->Record);
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::ExecTimeout:
            return ReplyWithResult(NMsgBusProxy::EResponseStatus::MSTATUS_INPROGRESS, msg->Record);
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::ProxyNotReady:
            return ReplyWithResult(NMsgBusProxy::EResponseStatus::MSTATUS_NOTREADY, emptyProposeStatus);
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::ExecAborted:
            return ReplyWithResult(NMsgBusProxy::EResponseStatus::MSTATUS_ABORTED, emptyProposeStatus);
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::EmptyAffectedSet:
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::ExecError:
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::AccessDenied:
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::DomainLocalityError:
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::ExecResultUnavailable:
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::ExecCancelled:
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::WrongRequest:
            return ReplyWithResult(NMsgBusProxy::EResponseStatus::MSTATUS_ERROR, msg->Record);
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::ResolveError: {
            auto reply = RetryResolve();
            if (!reply)
                return ReplyWithResult(NMsgBusProxy::EResponseStatus::MSTATUS_ERROR, msg->Record);
            return reply;
        }
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::ProxyShardNotAvailable: {
            auto reply = RetryResolve();
            if (!reply) // TODO: retry if partitioning changed due to split/merge
                return ReplyWithResult(NMsgBusProxy::EResponseStatus::MSTATUS_REJECTED, msg->Record);
            return reply;
        }
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::ProxyShardUnknown:
            return ReplyWithResult(NMsgBusProxy::EResponseStatus::MSTATUS_TIMEOUT, msg->Record);
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::ProxyShardTryLater:
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::ProxyShardOverloaded:
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::CoordinatorDeclined:
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::CoordinatorAborted:
        case TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::CoordinatorOutdated:
            return ReplyWithResult(NMsgBusProxy::EResponseStatus::MSTATUS_REJECTED, msg->Record);
        default:
            return ReplyWithResult(NMsgBusProxy::EResponseStatus::MSTATUS_INTERNALERROR, msg->Record);
        }
    }

    std::unique_ptr<NMsgBusProxy::TBusResponse> AllRequestsCompletedMKQL() {
        auto* mkqlTx = Proposal->Record.MutableTransaction()->MutableMiniKQLTransaction();
        const bool need2CompileProgram = mkqlTx->HasProgram() && mkqlTx->GetProgram().HasText();
        const bool need2CompileParams = mkqlTx->HasParams() && mkqlTx->GetParams().HasText();
        const bool programCompiled = need2CompileProgram
            ? (ProposalStatus->MutableMiniKQLCompileResults()->ProgramCompileErrorsSize() == 0)
            : true;
        const bool paramsCompiled = need2CompileParams
            ? (ProposalStatus->MutableMiniKQLCompileResults()->ParamsCompileErrorsSize() == 0)
            : true;

        const bool allCompiledOk = programCompiled && paramsCompiled;
        if (!allCompiledOk) {
            return ReplyWithResult(NMsgBusProxy::EResponseStatus::MSTATUS_ERROR, *ProposalStatus.Get());
        }

        switch (mkqlTx->GetMode()) {
            case NKikimrTxUserProxy::TMiniKQLTransaction::COMPILE_AND_EXEC: {
                TAutoPtr<TEvTxUserProxy::TEvProposeTransaction> ev = new TEvTxUserProxy::TEvProposeTransaction();
                ev->Record.CopyFrom(Proposal->Record);
                Runtime->Send(new IEventHandle(
                    MakeTxProxyID(), Sender, ev.Release()
                ));

                TAutoPtr<IEventHandle> handle;
                auto ans = Runtime->GrabEdgeEventRethrow<TEvTxUserProxy::TEvProposeTransactionStatus>(handle);
                return Handle(ans);
            }
            case NKikimrTxUserProxy::TMiniKQLTransaction::COMPILE: {
                return ReplyWithResult(NMsgBusProxy::EResponseStatus::MSTATUS_OK, *ProposalStatus.Get());
            }
            default:
                Y_ABORT("Unknown mkqlTxMode.");
        }
    }


    std::unique_ptr<NMsgBusProxy::TBusResponse> InvokeMkql() {
        ProposalStatus.Reset(new NKikimrTxUserProxy::TEvProposeTransactionStatus());
        Proposal.Reset(new TEvTxUserProxy::TEvProposeTransaction());
        NKikimrTxUserProxy::TEvProposeTransaction &record = Proposal->Record;

        // Transaction protobuf structure might be very heavy (if it has a batch of parameters)
        // so we don't want to copy it, just move its contents
        record.MutableTransaction()->Swap(Request->Record.MutableTransaction());

        if (Request->Record.HasProxyFlags())
            record.SetProxyFlags(Request->Record.GetProxyFlags());

        if (Request->Record.HasExecTimeoutPeriod())
            record.SetExecTimeoutPeriod(Request->Record.GetExecTimeoutPeriod());

        record.SetStreamResponse(false);

        auto* transaction = record.MutableTransaction();
        if (transaction->HasMiniKQLTransaction()) {
            auto& mkqlTx = *transaction->MutableMiniKQLTransaction();
            if (!mkqlTx.GetFlatMKQL()) {
                NKikimrTxUserProxy::TEvProposeTransactionStatus status;
                return ReplyWithResult(NMsgBusProxy::EResponseStatus::MSTATUS_ERROR, status);
            }

            if (mkqlTx.HasProgram() && mkqlTx.GetProgram().HasText()) {
                TextProgramForCompilation = mkqlTx.GetProgram().GetText();
                const bool forceRefresh = (mkqlTx.GetMode() == NKikimrTxUserProxy::TMiniKQLTransaction::COMPILE);
                auto* compEv = new TMiniKQLCompileServiceEvents::TEvCompile(TextProgramForCompilation);
                compEv->ForceRefresh = forceRefresh;

                Runtime->Send(new IEventHandle(
                    GetMiniKQLCompileServiceID(),
                    Sender,
                    compEv
                ));

                TAutoPtr<IEventHandle> handle;
                auto ev = Runtime->GrabEdgeEventRethrow<TMiniKQLCompileServiceEvents::TEvCompileStatus>(handle);
                return Handle(ev);
            }

            Y_ABORT_UNLESS(!mkqlTx.HasParams());
            return AllRequestsCompleted();
        }

        Runtime->Send(new IEventHandle(
            MakeTxProxyID(), Sender, Proposal.Release()
        ));

        TAutoPtr<IEventHandle> handle;
        auto ev = Runtime->GrabEdgeEventRethrow<TEvTxUserProxy::TEvProposeTransactionStatus>(handle);
        return Handle(ev);
    }
};

std::unique_ptr<NMsgBusProxy::TBusResponse> TTestMkqlInvoker::ReplyWithResult(NMsgBusProxy::EResponseStatus status,
                                               NKikimrTxUserProxy::TEvProposeTransactionStatus &result)
{
    auto Response = std::make_unique<NMsgBusProxy::TBusResponse>();
    Response.reset(ProposeTransactionStatusToResponse(status, result));

    if (result.HasExecutionEngineEvaluatedResponse()) {
        Response->Record.MutableExecutionEngineEvaluatedResponse()->Swap(result.MutableExecutionEngineEvaluatedResponse());
    }
    if (result.HasSerializedReadTableResponse()) {
        Response->Record.SetSerializedReadTableResponse(result.GetSerializedReadTableResponse());
    }
    if (result.HasStatus()) {
        Response->Record.SetProxyErrorCode(result.GetStatus());
    }

    return Response;
}


namespace Tests {

    TServerSettings& TServerSettings::SetDomainName(const TString& value) {
        StoragePoolTypes.erase("test");
        DomainName = value;
        AddStoragePool("test", "/" + DomainName + ":test");
        return *this;
    }

    const char* ServerRedirectEnvVar = "KIKIMR_SERVER";
    const char* DomainRedirectEnvVar = "KIKIMR_TEST_DOMAIN";
    const TDuration TIMEOUT = NSan::PlainOrUnderSanitizer(
        NValgrind::PlainOrUnderValgrind(TDuration::Seconds(3), TDuration::Seconds(60)),
        TDuration::Seconds(15)
    );
    const ui64 ConnectTimeoutMilliSeconds = NSan::PlainOrUnderSanitizer(
        NValgrind::PlainOrUnderValgrind(TDuration::Seconds(60), TDuration::Seconds(120)),
        TDuration::Seconds(120)
    ).MilliSeconds();

    NMiniKQL::IFunctionRegistry* DefaultFrFactory(const NScheme::TTypeRegistry& typeRegistry) {
        Y_UNUSED(typeRegistry);
        // register test UDFs
        auto freg = NKikimr::NMiniKQL::CreateFunctionRegistry(NKikimr::NMiniKQL::CreateBuiltinRegistry())->Clone();
        NKikimr::NMiniKQL::FillStaticModules(*freg);
        return freg.Release();
    }

    TServer::TServer(TServerSettings::TConstPtr settings, bool init)
        : Settings(settings)
        , UseStoragePools(!Settings->StoragePoolTypes.empty())
    {
        if (Settings->SupportsRedirect && IsServerRedirected()) {
            return;
        }

        const auto nodeCount = StaticNodes() + DynamicNodes();

        if (Settings->AuditLogBackendLines) {
            Runtime = MakeHolder<TTestBasicRuntime>(nodeCount,
                                                    Settings->DataCenterCount ? *Settings->DataCenterCount : nodeCount,
                                                    Settings->UseRealThreads,
                                                    CreateTestAuditLogBackends(*Settings->AuditLogBackendLines));
        } else {
            Runtime = MakeHolder<TTestBasicRuntime>(nodeCount,
                                                    Settings->DataCenterCount ? *Settings->DataCenterCount : nodeCount,
                                                    Settings->UseRealThreads);
        }

        if (init) {
            Initialize();
        }
    }

    TServer::TServer(const TServerSettings &settings, bool init)
        : TServer(new TServerSettings(settings), init)
    {
    }

    void TServer::Initialize() {
        if (Settings->SupportsRedirect && IsServerRedirected())
            return;

        TAppPrepare app; /* will cook TAppData */
        app.SetNetDataSourceUrl(Settings->NetClassifierConfig.GetUpdaterConfig().GetNetDataSourceUrl());
        app.SetEnableKqpSpilling(Settings->EnableKqpSpilling);
        app.SetKeepSnapshotTimeout(Settings->KeepSnapshotTimeout);
        app.SetChangesQueueItemsLimit(Settings->ChangesQueueItemsLimit);
        app.SetChangesQueueBytesLimit(Settings->ChangesQueueBytesLimit);
        app.SetAwsRegion(Settings->AwsRegion);
        app.CompactionConfig = Settings->CompactionConfig;
        app.ImmediateControlsConfig = Settings->Controls;
        app.InitIcb(StaticNodes() + DynamicNodes());
        if (Settings->AppConfig->HasResourceBrokerConfig()) {
            app.ResourceBrokerConfig = Settings->AppConfig->GetResourceBrokerConfig();
        }

        // forcibly turn on some feature flags
        app.FeatureFlags = Settings->FeatureFlags;
        app.FeatureFlags.SetCheckDatabaseAccessPermission(true);

        if (!Settings->UseRealThreads)
            Runtime->SetRegistrationObserverFunc([](TTestActorRuntimeBase& runtime, const TActorId&, const TActorId& actorId) {
                    runtime.EnableScheduleForActor(actorId);
                });

        for (auto& it: Settings->NodeKeys)     {
            ui32 nodeId = it.first;
            const TString& keyValue = it.second;

            TString baseDir = Runtime->GetTempDir();
            TString keyfile = TStringBuilder() << baseDir << "/key-" << nodeId << ".txt";
            {
                TFileOutput file(keyfile);
                file << keyValue;
            }
            app.SetKeyForNode(keyfile, nodeId);
        }

        SetupLogging();

        SetupMessageBus(Settings->Port);
        SetupDomains(app);

        app.AddHive(ChangeStateStorage(Hive, Settings->Domain));
        app.SetFnRegistry(Settings->FrFactory);
        app.SetFormatsFactory(Settings->Formats);

        if (Settings->Formats) {
            NKikHouse::RegisterFormat(*Settings->Formats);
        }

        NKikimr::SetupChannelProfiles(app);

        if (Settings->NeedStatsCollectors) {
            Runtime->SetupStatsCollectors();
        }
        Runtime->SetupMonitoring(Settings->MonitoringPortOffset, Settings->MonitoringTypeAsync);
        Runtime->SetLogBackend(Settings->LogBackend);

        SetupActorSystemConfig();

        Runtime->AddAppDataInit([this](ui32 nodeIdx, NKikimr::TAppData& appData) {
            Y_UNUSED(nodeIdx);

            appData.AuthConfig.MergeFrom(Settings->AuthConfig);
            appData.PQConfig.MergeFrom(Settings->PQConfig);
            appData.PQClusterDiscoveryConfig.MergeFrom(Settings->PQClusterDiscoveryConfig);
            appData.NetClassifierConfig.MergeFrom(Settings->NetClassifierConfig);
            appData.StreamingConfig.MergeFrom(Settings->AppConfig->GetGRpcConfig().GetStreamingConfig());
            auto& securityConfig = Settings->AppConfig->GetDomainsConfig().GetSecurityConfig();
            appData.EnforceUserTokenRequirement = securityConfig.GetEnforceUserTokenRequirement();
            appData.EnforceUserTokenCheckRequirement = securityConfig.GetEnforceUserTokenCheckRequirement();
            TVector<TString> administrationAllowedSIDs(securityConfig.GetAdministrationAllowedSIDs().begin(), securityConfig.GetAdministrationAllowedSIDs().end());
            appData.AdministrationAllowedSIDs = std::move(administrationAllowedSIDs);
            TVector<TString> registerDynamicNodeAllowedSIDs(securityConfig.GetRegisterDynamicNodeAllowedSIDs().cbegin(), securityConfig.GetRegisterDynamicNodeAllowedSIDs().cend());
            appData.RegisterDynamicNodeAllowedSIDs = std::move(registerDynamicNodeAllowedSIDs);
            appData.DomainsConfig.MergeFrom(Settings->AppConfig->GetDomainsConfig());
            appData.ColumnShardConfig.MergeFrom(Settings->AppConfig->GetColumnShardConfig());
            appData.PersQueueGetReadSessionsInfoWorkerFactory = Settings->PersQueueGetReadSessionsInfoWorkerFactory.get();
            appData.DataStreamsAuthFactory = Settings->DataStreamsAuthFactory.get();
            appData.PersQueueMirrorReaderFactory = Settings->PersQueueMirrorReaderFactory.get();
            appData.HiveConfig.MergeFrom(Settings->AppConfig->GetHiveConfig());
            appData.GraphConfig.MergeFrom(Settings->AppConfig->GetGraphConfig());
            appData.SqsConfig.MergeFrom(Settings->AppConfig->GetSqsConfig());
            appData.SharedCacheConfig.MergeFrom(Settings->AppConfig->GetSharedCacheConfig());
            appData.TransferWriterFactory = Settings->TransferWriterFactory;
            appData.WorkloadManagerConfig.MergeFrom(Settings->AppConfig->GetWorkloadManagerConfig());
            appData.QueryServiceConfig.MergeFrom(Settings->AppConfig->GetQueryServiceConfig());

            appData.DynamicNameserviceConfig = new TDynamicNameserviceConfig;
            auto dnConfig = appData.DynamicNameserviceConfig;
            dnConfig->MaxStaticNodeId = 1023;
            dnConfig->MinDynamicNodeId = 1024;
            dnConfig->MaxDynamicNodeId = 1024 + 100;
        });

        const bool mockDisk = (StaticNodes() + DynamicNodes()) == 1 && Settings->EnableMockOnSingleNode;
        if (!Settings->AppConfig->HasSharedCacheConfig()) {
            Settings->AppConfig->MutableSharedCacheConfig()->SetMemoryLimit(32_MB);
        }

        SetupTabletServices(
            *Runtime,
            &app,
            mockDisk,
            Settings->CustomDiskParams,
            &Settings->AppConfig->GetSharedCacheConfig(),
            Settings->EnableForceFollowers,
            Settings->ProxyDSMocks);

        // WARNING: must be careful about modifying app data after actor system starts

        // NOTE: Setup of the static and dynamic nodes is mostly common except for the "local" service,
        // which _must not_ be started up on dynamic nodes.
        //
        // This is because static nodes should be active and must serve root subdomain right from the start.
        // Unlike static nodes, dynamic nodes are vacant. In this testing framework they are intended
        // to serve tenant subdomains that will be created in tests. Dynamic node will be "activated" then
        // by call to SetupDynamicLocalService() which will start "local" service exclusively to serve
        // requested tenant subdomain.
        //
        // And while single "local" service is capable of serving more than one subdomain, there are never
        // should be more than one "local" service on a node. Otherwise two "locals" will be competing
        // and tests might have unexpected flaky behaviour.
        //
        for (ui32 nodeIdx = 0; nodeIdx < StaticNodes(); ++nodeIdx) {
            SetupDomainLocalService(nodeIdx);
            SetupProxies(nodeIdx);
        }

        // Create Observer to catch an event of system views update finished.
        // This doesn't apply in case when UseRealThreads flag is enabled
        // because we can't catch events from other threads
        if (GetSettings().FeatureFlags.GetEnableRealSystemViewPaths()) {
            AddSysViewsRosterUpdateObserver();
        }

        CreateBootstrapTablets();

        // Wait until schemeshard completes creating system view paths,
        // otherwise these transactions may destabilize invariants used in tests
        // (e.g. PathIds values of paths created in tests, general database path count etc.).
        if (GetSettings().FeatureFlags.GetEnableRealSystemViewPaths()) {
            WaitForSysViewsRosterUpdate();
        }

        SetupStorage();
    }

    void TServer::SetupActorSystemConfig() {
        if (!Settings->AppConfig->HasActorSystemConfig()) {
            return;
        }

        auto actorSystemConfig = Settings->AppConfig->GetActorSystemConfig();
        const bool useAutoConfig = actorSystemConfig.GetUseAutoConfig();
        if (useAutoConfig) {
            NAutoConfigInitializer::ApplyAutoConfig(&actorSystemConfig, false);
        }

        TCpuManagerConfig cpuManager;
        for (int poolId = 0; poolId < actorSystemConfig.GetExecutor().size(); poolId++) {
            NActorSystemConfigHelpers::AddExecutorPool(cpuManager, actorSystemConfig.GetExecutor(poolId), actorSystemConfig, poolId, nullptr);
        }

        const NAutoConfigInitializer::TASPools pools = NAutoConfigInitializer::GetASPools(actorSystemConfig, useAutoConfig);

        Runtime->SetupActorSystemConfig(TTestActorRuntime::TActorSystemSetupConfig{
            .CpuManagerConfig = cpuManager,
            .SchedulerConfig = NActorSystemConfigHelpers::CreateSchedulerConfig(actorSystemConfig.GetScheduler()),
            .MonitorStuckActors = actorSystemConfig.GetMonitorStuckActors()
        }, TTestActorRuntime::TActorSystemPools{
            pools.SystemPoolId, pools.UserPoolId, pools.IOPoolId, pools.BatchPoolId,
            NAutoConfigInitializer::GetServicePools(actorSystemConfig, useAutoConfig)
        });
    }

    void TServer::SetupMessageBus(ui16 port) {
        if (port) {
            Bus = NBus::CreateMessageQueue(NBus::TBusQueueConfig());
            BusServer.Reset(NMsgBusProxy::CreateMsgBusServer(
                Bus.Get(),
                BusServerSessionConfig,
                port
            ));
        }
    }

    void TServer::EnableGRpc(const NYdbGrpc::TServerOptions& options, ui32 grpcServiceNodeId, const std::optional<TString>& tenant) {
        auto* grpcInfo = &TenantsGRpc[tenant ? *tenant : Settings->DomainName][grpcServiceNodeId];
        grpcInfo->GRpcServerRootCounters = MakeIntrusive<::NMonitoring::TDynamicCounters>();
        auto& counters = grpcInfo->GRpcServerRootCounters;

        auto& grpcServer = grpcInfo->GRpcServer;
        grpcServer.reset(new NYdbGrpc::TGRpcServer(options));
        auto grpcService = new NGRpcProxy::TGRpcService();

        auto system(Runtime->GetActorSystem(grpcServiceNodeId));

        if (Settings->Verbose) {
            Cerr << "TServer::EnableGrpc on GrpcPort " << options.Port << ", node " << system->NodeId << Endl;
        }

        const size_t proxyCount = Max(ui32{1}, Settings->AppConfig->GetGRpcConfig().GetGRpcProxyCount());
        TVector<TActorId> grpcRequestProxies;
        grpcRequestProxies.reserve(proxyCount);

        auto& appData = Runtime->GetAppData(grpcServiceNodeId);
        for (size_t i = 0; i < proxyCount; ++i) {
            auto grpcRequestProxy = NGRpcService::CreateGRpcRequestProxy(*Settings->AppConfig);
            auto grpcRequestProxyId = system->Register(grpcRequestProxy, TMailboxType::ReadAsFilled, appData.UserPoolId);
            system->RegisterLocalService(NGRpcService::CreateGRpcRequestProxyId(), grpcRequestProxyId);
            grpcRequestProxies.push_back(grpcRequestProxyId);
        }

        system->Register(
            NConsole::CreateJaegerTracingConfigurator(appData.TracingConfigurator, Settings->AppConfig->GetTracingConfig()),
            TMailboxType::ReadAsFilled,
            appData.UserPoolId
        );

        auto grpcMon = system->Register(NGRpcService::CreateGrpcMonService(), TMailboxType::ReadAsFilled, appData.UserPoolId);
        system->RegisterLocalService(NGRpcService::GrpcMonServiceId(), grpcMon);

        // Setup discovery for typically used services on the node
        {
            TIntrusivePtr<NGRpcService::TGrpcEndpointDescription> desc = new NGRpcService::TGrpcEndpointDescription();
            desc->Address = options.Host;
            desc->Port = options.Port;
            desc->Ssl = !options.SslData.Empty();

            if (!tenant) {
                TVector<TString> rootDomains;
                if (const auto& domain = appData.DomainsInfo->Domain) {
                    rootDomains.emplace_back("/" + domain->Name);
                }
                desc->ServedDatabases.insert(desc->ServedDatabases.end(), rootDomains.begin(), rootDomains.end());
            } else {
                desc->ServedDatabases.emplace_back(CanonizePath(*tenant));
            }

            TVector<TString> grpcServices = {"yql", "clickhouse_internal", "datastreams", "table_service", "scripting", "experimental", "discovery", "pqcd", "fds", "pq", "pqv0", "pqv1" };
            desc->ServedServices.insert(desc->ServedServices.end(), grpcServices.begin(), grpcServices.end());

            system->Register(NGRpcService::CreateGrpcEndpointPublishActor(desc.Get()), TMailboxType::ReadAsFilled, appData.UserPoolId);
        }

        auto future = grpcService->Prepare(
            system,
            NMsgBusProxy::CreatePersQueueMetaCacheV2Id(),
            NMsgBusProxy::CreateMsgBusProxyId(),
            counters
        );
        auto startCb = [grpcService] (NThreading::TFuture<void> result) {
            if (result.HasException()) {
                try {
                    result.GetValue();
                } catch (const std::exception& ex) {
                    Y_ABORT("Unable to prepare GRpc service: %s", ex.what());
                }
            } else {
                grpcService->Start();
            }
        };

        future.Subscribe(startCb);

        grpcServer->AddService(grpcService);
        grpcServer->AddService(new NGRpcService::TGRpcYdbExportService(system, counters, grpcRequestProxies[0], true));
        grpcServer->AddService(new NGRpcService::TGRpcYdbImportService(system, counters, grpcRequestProxies[0], true));
        grpcServer->AddService(new NGRpcService::TGRpcYdbSchemeService(system, counters, grpcRequestProxies[0], true));
        grpcServer->AddService(new NGRpcService::TGRpcYdbTableService(system, counters, appData.InFlightLimiterRegistry, grpcRequestProxies, true, 1));
        grpcServer->AddService(new NGRpcService::TGRpcYdbScriptingService(system, counters, grpcRequestProxies[0], true));
        grpcServer->AddService(new NGRpcService::TGRpcOperationService(system, counters, grpcRequestProxies[0], true));
        grpcServer->AddService(new NGRpcService::V1::TGRpcPersQueueService(system, counters, NMsgBusProxy::CreatePersQueueMetaCacheV2Id(), grpcRequestProxies[0], true));
        grpcServer->AddService(new NGRpcService::V1::TGRpcTopicService(system, counters, NMsgBusProxy::CreatePersQueueMetaCacheV2Id(), grpcRequestProxies[0], true));
        grpcServer->AddService(new NGRpcService::TGRpcPQClusterDiscoveryService(system, counters, grpcRequestProxies[0]));
        grpcServer->AddService(new NKesus::TKesusGRpcService(system, counters, appData.InFlightLimiterRegistry, grpcRequestProxies[0], true));
        grpcServer->AddService(new NGRpcService::TGRpcCmsService(system, counters, grpcRequestProxies[0], true));
        grpcServer->AddService(new NGRpcService::TGRpcDiscoveryService(system, counters, grpcRequestProxies[0], true));
        grpcServer->AddService(new NGRpcService::TGRpcYdbClickhouseInternalService(system, counters, appData.InFlightLimiterRegistry, grpcRequestProxies[0], true));
        grpcServer->AddService(new NGRpcService::TGRpcYdbObjectStorageService(system, counters, grpcRequestProxies[0], true));
        grpcServer->AddService(new NQuoter::TRateLimiterGRpcService(system, counters, grpcRequestProxies[0]));
        grpcServer->AddService(new NGRpcService::TGRpcDataStreamsService(system, counters, grpcRequestProxies[0], true));
        grpcServer->AddService(new NGRpcService::TGRpcMonitoringService(system, counters, grpcRequestProxies[0], true));
        grpcServer->AddService(new NGRpcService::TGRpcYdbQueryService(system, counters, grpcRequestProxies, true, 1));
        grpcServer->AddService(new NGRpcService::TGRpcYdbTabletService(system, counters, grpcRequestProxies, true, 1));
        if (Settings->EnableYq || Settings->EnableYqGrpc) {
            grpcServer->AddService(new NGRpcService::TGRpcFederatedQueryService(system, counters, grpcRequestProxies[0]));
            grpcServer->AddService(new NGRpcService::TGRpcFqPrivateTaskService(system, counters, grpcRequestProxies[0]));
        }
        if (const auto& factory = Settings->GrpcServiceFactory) {
            // All services enabled by default for ut
            static const std::unordered_set<TString> dummy;
            for (const auto& service : factory->Create(dummy, dummy, system, counters, grpcRequestProxies[0])) {
                grpcServer->AddService(service);
            }
        }
        grpcServer->AddService(new NGRpcService::TGRpcYdbLogStoreService(system, counters, grpcRequestProxies[0], true));
        grpcServer->AddService(new NGRpcService::TGRpcAuthService(system, counters, grpcRequestProxies[0], true));
        grpcServer->AddService(new NGRpcService::TGRpcReplicationService(system, counters, grpcRequestProxies[0], true));
        grpcServer->AddService(new NGRpcService::TGRpcViewService(system, counters, grpcRequestProxies[0], true));
        grpcServer->Start();
    }

    void TServer::EnableGRpc(ui16 port, ui32 grpcServiceNodeId, const std::optional<TString>& tenant) {
        EnableGRpc(NYdbGrpc::TServerOptions()
            .SetHost("localhost")
            .SetPort(port)
            .SetLogger(NYdbGrpc::CreateActorSystemLogger(*Runtime->GetActorSystem(grpcServiceNodeId), NKikimrServices::GRPC_SERVER)),
            grpcServiceNodeId, tenant
        );
    }

    void TServer::SetupRootStoragePools(const TActorId sender) const {
        if (GetSettings().StoragePoolTypes.empty()) {
            return;
        }

        auto& runtime = *GetRuntime();
        auto& settings = GetSettings();

        auto tid = ChangeStateStorage(SchemeRoot, settings.Domain);
        const TDomainsInfo::TDomain& domain = runtime.GetAppData().DomainsInfo->GetDomain(settings.Domain);

        auto evTx = MakeHolder<NSchemeShard::TEvSchemeShard::TEvModifySchemeTransaction>(1, tid);
        auto transaction = evTx->Record.AddTransaction();
        transaction->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpAlterSubDomain);
        transaction->SetWorkingDir("/");
        auto op = transaction->MutableSubDomain();
        op->SetName(domain.Name);

        for (const auto& [kind, pool] : settings.StoragePoolTypes) {
            auto* p = op->AddStoragePools();
            p->SetKind(kind);
            p->SetName(pool.GetName());
        }

        runtime.SendToPipe(tid, sender, evTx.Release(), 0, GetPipeConfigWithRetries());

        {
            TAutoPtr<IEventHandle> handle;
            auto event = runtime.GrabEdgeEvent<NSchemeShard::TEvSchemeShard::TEvModifySchemeTransactionResult>(handle);
            UNIT_ASSERT_VALUES_EQUAL(event->Record.GetSchemeshardId(), tid);
            UNIT_ASSERT_VALUES_EQUAL(event->Record.GetStatus(), NKikimrScheme::EStatus::StatusAccepted);
        }

        auto evSubscribe = MakeHolder<NSchemeShard::TEvSchemeShard::TEvNotifyTxCompletion>(1);
        runtime.SendToPipe(tid, sender, evSubscribe.Release(), 0, GetPipeConfigWithRetries());

        {
            TAutoPtr<IEventHandle> handle;
            auto event = runtime.GrabEdgeEvent<NSchemeShard::TEvSchemeShard::TEvNotifyTxCompletionResult>(handle);
            UNIT_ASSERT_VALUES_EQUAL(event->Record.GetTxId(), 1);
        }
    }

    void TServer::SetupDomains(TAppPrepare& app) {
        const ui32 domainId = Settings->Domain;
        ui64 planResolution = Settings->DomainPlanResolution;
        if (!planResolution) {
            planResolution = Settings->UseRealThreads ? 7 : 500;
        }
        ui32 timecastBuckets = Settings->DomainTimecastBuckets;
        if (!timecastBuckets) {
            timecastBuckets = TDomainsInfo::TDomain::DefaultTimecastBucketsPerMediator;
        }
        auto domain = TDomainsInfo::TDomain::ConstructDomainWithExplicitTabletIds(Settings->DomainName, domainId, ChangeStateStorage(SchemeRoot, domainId),
                                                                                  planResolution, timecastBuckets,
                                                                                  TVector<ui64>{TDomainsInfo::MakeTxCoordinatorIDFixed(1)},
                                                                                  TVector<ui64>{TDomainsInfo::MakeTxMediatorIDFixed(1)},
                                                                                  TVector<ui64>{TDomainsInfo::MakeTxAllocatorIDFixed(1)},
                                                                                  Settings->StoragePoolTypes);
        app.AddDomain(domain.Release());
    }

    TVector<ui64> TServer::StartPQTablets(ui32 pqTabletsN, bool wait) {
        auto getChannelBind = [](const TString& storagePool) {
            TChannelBind bind;
            bind.SetStoragePoolName(storagePool);
            return bind;
        };
        TVector<ui64> ids;
        ids.reserve(pqTabletsN);
        for (ui32 i = 0; i < pqTabletsN; ++i) {
            auto tabletId = Tests::ChangeStateStorage(Tests::DummyTablet2 + i + 1, Settings->Domain);
            TIntrusivePtr<TTabletStorageInfo> tabletInfo =
                CreateTestTabletInfo(tabletId, TTabletTypes::PersQueue);
            TIntrusivePtr<TTabletSetupInfo> setupInfo =
                new TTabletSetupInfo(&CreatePersQueue, TMailboxType::Simple, 0, TMailboxType::Simple, 0);

            static TString STORAGE_POOL = "/Root:test";
            static TChannelsBindings BINDED_CHANNELS =
                {getChannelBind(STORAGE_POOL), getChannelBind(STORAGE_POOL), getChannelBind(STORAGE_POOL)};

            ui32 nodeIndex = 0;
            auto ev =
                MakeHolder<TEvHive::TEvCreateTablet>(tabletId, 0, TTabletTypes::PersQueue, BINDED_CHANNELS);

            TActorId senderB = Runtime->AllocateEdgeActor(nodeIndex);
            ui64 hive = ChangeStateStorage(Tests::Hive, Settings->Domain);
            Runtime->SendToPipe(hive, senderB, ev.Release(), 0, GetPipeConfigWithRetries());
            TAutoPtr<IEventHandle> handle;
            auto createTabletReply = Runtime->GrabEdgeEventRethrow<TEvHive::TEvCreateTabletReply>(handle);
            UNIT_ASSERT(createTabletReply);
            auto expectedStatus = NKikimrProto::OK;
            UNIT_ASSERT_EQUAL_C(createTabletReply->Record.GetStatus(), expectedStatus,
                                (ui32)createTabletReply->Record.GetStatus() << " != " << (ui32)expectedStatus);
            UNIT_ASSERT_EQUAL_C(createTabletReply->Record.GetOwner(), tabletId,
                                createTabletReply->Record.GetOwner() << " != " << tabletId);
            ui64 id = createTabletReply->Record.GetTabletID();
            while (wait) {
                auto tabletCreationResult =
                    Runtime->GrabEdgeEventRethrow<TEvHive::TEvTabletCreationResult>(handle);
                UNIT_ASSERT(tabletCreationResult);
                if (id == tabletCreationResult->Record.GetTabletID()) {
                    UNIT_ASSERT_EQUAL_C(tabletCreationResult->Record.GetStatus(), NKikimrProto::OK,
                        (ui32)tabletCreationResult->Record.GetStatus() << " != " << (ui32)NKikimrProto::OK);
                    break;
                }
            }
            ids.push_back(id);
        }
        return ids;
    }

    void TServer::CreateBootstrapTablets() {
        const ui32 domainId = Settings->Domain;
        Y_ABORT_UNLESS(TDomainsInfo::MakeTxAllocatorIDFixed(1) == ChangeStateStorage(TxAllocator, domainId));
        CreateTestBootstrapper(*Runtime, CreateTestTabletInfo(ChangeStateStorage(TxAllocator, domainId), TTabletTypes::TxAllocator), &CreateTxAllocator);
        Y_ABORT_UNLESS(TDomainsInfo::MakeTxCoordinatorIDFixed(1) == ChangeStateStorage(Coordinator, domainId));
        CreateTestBootstrapper(*Runtime, CreateTestTabletInfo(ChangeStateStorage(Coordinator, domainId), TTabletTypes::Coordinator), &CreateFlatTxCoordinator);
        Y_ABORT_UNLESS(TDomainsInfo::MakeTxMediatorIDFixed(1) == ChangeStateStorage(Mediator, domainId));
        CreateTestBootstrapper(*Runtime, CreateTestTabletInfo(ChangeStateStorage(Mediator, domainId), TTabletTypes::Mediator), &CreateTxMediator);
        CreateTestBootstrapper(*Runtime, CreateTestTabletInfo(ChangeStateStorage(SchemeRoot, domainId), TTabletTypes::SchemeShard), &CreateFlatTxSchemeShard);
        CreateTestBootstrapper(*Runtime, CreateTestTabletInfo(ChangeStateStorage(Hive, domainId), TTabletTypes::Hive), &CreateDefaultHive);
        CreateTestBootstrapper(*Runtime, CreateTestTabletInfo(MakeBSControllerID(), TTabletTypes::BSController), &CreateFlatBsController);
        CreateTestBootstrapper(*Runtime, CreateTestTabletInfo(MakeTenantSlotBrokerID(), TTabletTypes::TenantSlotBroker), &NTenantSlotBroker::CreateTenantSlotBroker);
        if (Settings->EnableConsole) {
            CreateTestBootstrapper(*Runtime, CreateTestTabletInfo(MakeConsoleID(), TTabletTypes::Console), &NConsole::CreateConsole);
        }
        CreateTestBootstrapper(*Runtime, CreateTestTabletInfo(MakeNodeBrokerID(), TTabletTypes::NodeBroker), &NNodeBroker::CreateNodeBroker);
    }

    void TServer::SetupStorage() {
        TActorId sender = Runtime->AllocateEdgeActor();

        NTabletPipe::TClientConfig pipeConfig;
        pipeConfig.RetryPolicy = NTabletPipe::TClientRetryPolicy::WithRetries();

        //get NodesInfo, nodes hostname and port are interested
        Runtime->Send(new IEventHandle(GetNameserviceActorId(), sender, new TEvInterconnect::TEvListNodes));
        TAutoPtr<IEventHandle> handleNodesInfo;
        auto nodesInfo = Runtime->GrabEdgeEventRethrow<TEvInterconnect::TEvNodesInfo>(handleNodesInfo);

        auto bsConfigureRequest = MakeHolder<TEvBlobStorage::TEvControllerConfigRequest>();

        NKikimrBlobStorage::TDefineBox boxConfig;
        boxConfig.SetBoxId(Settings->BOX_ID);
        boxConfig.SetItemConfigGeneration(Settings->StorageGeneration);

        ui32 nodeId = Runtime->GetNodeId(0);
        Y_ABORT_UNLESS(nodesInfo->Nodes[0].NodeId == nodeId);
        auto& nodeInfo = nodesInfo->Nodes[0];

        NKikimrBlobStorage::TDefineHostConfig hostConfig;
        hostConfig.SetHostConfigId(nodeId);
        hostConfig.SetItemConfigGeneration(Settings->StorageGeneration);
        TString path;
        if (Settings->UseSectorMap) {
            path ="SectorMap:test-client[:2000]";
        } else {
            TString diskPath = Settings->CustomDiskParams.DiskPath;
            path = TStringBuilder() << (diskPath ? diskPath : Runtime->GetTempDir()) << "pdisk_1.dat";
        }
        hostConfig.AddDrive()->SetPath(path);
        if (Settings->Verbose) {
            Cerr << "test_client.cpp: SetPath # " << path << Endl;
        }
        bsConfigureRequest->Record.MutableRequest()->AddCommand()->MutableDefineHostConfig()->CopyFrom(hostConfig);

        auto& host = *boxConfig.AddHost();
        host.MutableKey()->SetFqdn(nodeInfo.Host);
        host.MutableKey()->SetIcPort(nodeInfo.Port);
        host.SetHostConfigId(hostConfig.GetHostConfigId());
        bsConfigureRequest->Record.MutableRequest()->AddCommand()->MutableDefineBox()->CopyFrom(boxConfig);

        std::unordered_map<TString, ui64> poolsConfigGenerations;
        if (Settings->FetchPoolsGeneration) {
            auto bsDescribeRequest = MakeHolder<TEvBlobStorage::TEvControllerConfigRequest>();
            auto& describeCommand = *bsDescribeRequest->Record.MutableRequest()->AddCommand()->MutableReadStoragePool();
            describeCommand.SetBoxId(Settings->BOX_ID);
            for (const auto& [_, storagePool] : Settings->StoragePoolTypes) {
                describeCommand.AddName(storagePool.GetName());
            }

            Runtime->SendToPipe(MakeBSControllerID(), sender, bsDescribeRequest.Release(), 0, pipeConfig);
            TAutoPtr<IEventHandle> handleDescResponse;
            const auto descResponse = Runtime->GrabEdgeEventRethrow<TEvBlobStorage::TEvControllerConfigResponse>(handleDescResponse);

            const auto& response = descResponse->Record.GetResponse();
            if (!response.GetSuccess()) {
                Cerr << "\n\n descResponse is #" << descResponse->Record.DebugString() << "\n\n";
            }
            UNIT_ASSERT(descResponse->Record.GetResponse().GetSuccess());
            UNIT_ASSERT_VALUES_EQUAL(response.StatusSize(), 1);
            const auto& status = response.GetStatus(0);

            poolsConfigGenerations.reserve(status.StoragePoolSize());
            for (const auto& storagePool : status.GetStoragePool()) {
                UNIT_ASSERT(poolsConfigGenerations.emplace(storagePool.GetName(), storagePool.GetItemConfigGeneration()).second);
            }
        }

        for (const auto& [poolKind, storagePool] : Settings->StoragePoolTypes) {
            if (storagePool.GetNumGroups() > 0) {
                auto* command = bsConfigureRequest->Record.MutableRequest()->AddCommand()->MutableDefineStoragePool();
                command->CopyFrom(storagePool);
                const auto poolGenerationIt = poolsConfigGenerations.find(storagePool.GetName());
                command->SetItemConfigGeneration(poolGenerationIt == poolsConfigGenerations.end() ? Settings->StorageGeneration : poolGenerationIt->second);
            }
        }

        Runtime->SendToPipe(MakeBSControllerID(), sender, bsConfigureRequest.Release(), 0, pipeConfig);

        TAutoPtr<IEventHandle> handleConfigureResponse;
        auto configureResponse = Runtime->GrabEdgeEventRethrow<TEvBlobStorage::TEvControllerConfigResponse>(handleConfigureResponse);
        if (!configureResponse->Record.GetResponse().GetSuccess()) {
            Cerr << "\n\n configResponse is #" << configureResponse->Record.DebugString() << "\n\n";
        }
        UNIT_ASSERT(configureResponse->Record.GetResponse().GetSuccess());
    }

    void TServer::SetupDefaultProfiles() {
        NKikimr::Tests::TClient client(*Settings);
        TAutoPtr<NMsgBusProxy::TBusConsoleRequest> request(new NMsgBusProxy::TBusConsoleRequest());
        auto &item = *request->Record.MutableConfigureRequest()->AddActions()->MutableAddConfigItem()->MutableConfigItem();
        item.SetKind((ui32)NKikimrConsole::TConfigItem::TableProfilesConfigItem);
        auto &profiles = *item.MutableConfig()->MutableTableProfilesConfig();
        {
            // Storage policy:
            auto& policy = *profiles.AddStoragePolicies();
            policy.SetName("default");
            auto& family = *policy.AddColumnFamilies();
            family.SetId(0);
            family.MutableStorageConfig()->MutableSysLog()->SetPreferredPoolKind("test");
            family.MutableStorageConfig()->MutableLog()->SetPreferredPoolKind("test");
            family.MutableStorageConfig()->MutableData()->SetPreferredPoolKind("test");
        }
        {
            // Compaction policy:
            NLocalDb::TCompactionPolicyPtr defaultPolicy = NLocalDb::CreateDefaultUserTablePolicy();
            NKikimrCompaction::TCompactionPolicy defaultflatSchemePolicy;
            defaultPolicy->Serialize(defaultflatSchemePolicy);
            auto &defaultCompactionPolicy = *profiles.AddCompactionPolicies();
            defaultCompactionPolicy.SetName("default");
            defaultCompactionPolicy.MutableCompactionPolicy()->CopyFrom(defaultflatSchemePolicy);

            NLocalDb::TCompactionPolicy policy1;
            policy1.Generations.push_back({ 0, 8, 8, 128 * 1024 * 1024, NLocalDb::LegacyQueueIdToTaskName(1), true });
            NKikimrCompaction::TCompactionPolicy flatSchemePolicy1;
            policy1.Serialize(flatSchemePolicy1);
            auto &compactionPolicy1 = *profiles.AddCompactionPolicies();
            compactionPolicy1.SetName("compaction1");
            compactionPolicy1.MutableCompactionPolicy()->CopyFrom(flatSchemePolicy1);

            NLocalDb::TCompactionPolicy policy2;
            policy2.Generations.push_back({ 0, 8, 8, 128 * 1024 * 1024, NLocalDb::LegacyQueueIdToTaskName(1), true });
            policy2.Generations.push_back({ 40 * 1024 * 1024, 5, 16, 512 * 1024 * 1024, NLocalDb::LegacyQueueIdToTaskName(2), false });
            NKikimrCompaction::TCompactionPolicy flatSchemePolicy2;
            policy2.Serialize(flatSchemePolicy2);
            auto &compactionPolicy2 = *profiles.AddCompactionPolicies();
            compactionPolicy2.SetName("compaction2");
            compactionPolicy2.MutableCompactionPolicy()->CopyFrom(flatSchemePolicy2);
        }
        {
            auto& profile = *profiles.AddTableProfiles();
            profile.SetName("default");
            profile.SetStoragePolicy("default");
        }
        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus msgStatus = client.SyncCall(request, reply);
        UNIT_ASSERT_VALUES_EQUAL(msgStatus, NBus::MESSAGE_OK);
        auto resp = dynamic_cast<NMsgBusProxy::TBusConsoleResponse*>(reply.Get())->Record;
        UNIT_ASSERT_VALUES_EQUAL(resp.GetStatus().GetCode(), Ydb::StatusIds::SUCCESS);
    }

    void TServer::SetupDomainLocalService(ui32 nodeIdx) {
        SetupLocalService(nodeIdx, Settings->DomainName);
    }

    void TServer::SetupDynamicLocalService(ui32 nodeIdx, const TString &tenantName) {
        Y_ABORT_UNLESS(nodeIdx >= StaticNodes());
        SetupLocalService(nodeIdx, tenantName);
    }

    void TServer::DestroyDynamicLocalService(ui32 nodeIdx) {
        Y_ABORT_UNLESS(nodeIdx >= StaticNodes());
        TActorId local = MakeLocalID(Runtime->GetNodeId(nodeIdx)); // MakeTenantPoolRootID?
        Runtime->Send(new IEventHandle(local, TActorId(), new TEvents::TEvPoisonPill()));
    }

    void TServer::SetupLocalConfig(TLocalConfig &localConfig, const NKikimr::TAppData &appData) {
        localConfig.TabletClassInfo[TTabletTypes::Dummy] =
            TLocalConfig::TTabletClassInfo(new TTabletSetupInfo(
                &CreateFlatDummyTablet, TMailboxType::Revolving, appData.UserPoolId,
                TMailboxType::Revolving, appData.SystemPoolId));
        localConfig.TabletClassInfo[TTabletTypes::DataShard] =
            TLocalConfig::TTabletClassInfo(new TTabletSetupInfo(
                &CreateDataShard, TMailboxType::Revolving, appData.UserPoolId,
                TMailboxType::Revolving, appData.SystemPoolId));
        localConfig.TabletClassInfo[TTabletTypes::KeyValue] =
            TLocalConfig::TTabletClassInfo(new TTabletSetupInfo(
                &CreateKeyValueFlat, TMailboxType::Revolving, appData.UserPoolId,
                TMailboxType::Revolving, appData.SystemPoolId));
        localConfig.TabletClassInfo[TTabletTypes::ColumnShard] =
            TLocalConfig::TTabletClassInfo(new TTabletSetupInfo(
                &CreateColumnShard, TMailboxType::Revolving, appData.UserPoolId,
                TMailboxType::Revolving, appData.SystemPoolId));
        localConfig.TabletClassInfo[TTabletTypes::PersQueue] =
            TLocalConfig::TTabletClassInfo(new TTabletSetupInfo(
                &CreatePersQueue, TMailboxType::Revolving, appData.UserPoolId,
                TMailboxType::Revolving, appData.SystemPoolId));
        localConfig.TabletClassInfo[TTabletTypes::PersQueueReadBalancer] =
            TLocalConfig::TTabletClassInfo(new TTabletSetupInfo(
                &CreatePersQueueReadBalancer, TMailboxType::Revolving, appData.UserPoolId,
                TMailboxType::Revolving, appData.SystemPoolId));
        localConfig.TabletClassInfo[TTabletTypes::Coordinator] =
            TLocalConfig::TTabletClassInfo(new TTabletSetupInfo(
                &CreateFlatTxCoordinator, TMailboxType::Revolving, appData.UserPoolId,
                TMailboxType::Revolving, appData.SystemPoolId));
        localConfig.TabletClassInfo[TTabletTypes::Mediator] =
            TLocalConfig::TTabletClassInfo(new TTabletSetupInfo(
                &CreateTxMediator, TMailboxType::Revolving, appData.UserPoolId,
                TMailboxType::Revolving, appData.SystemPoolId));
        localConfig.TabletClassInfo[TTabletTypes::Kesus] =
            TLocalConfig::TTabletClassInfo(new TTabletSetupInfo(
                &NKesus::CreateKesusTablet, TMailboxType::Revolving, appData.UserPoolId,
                TMailboxType::Revolving, appData.SystemPoolId));
        localConfig.TabletClassInfo[TTabletTypes::SchemeShard] =
            TLocalConfig::TTabletClassInfo(new TTabletSetupInfo(
                &CreateFlatTxSchemeShard, TMailboxType::Revolving, appData.UserPoolId,
                TMailboxType::Revolving, appData.SystemPoolId));
        localConfig.TabletClassInfo[TTabletTypes::Hive] =
            TLocalConfig::TTabletClassInfo(new TTabletSetupInfo(
                &CreateDefaultHive, TMailboxType::Revolving, appData.UserPoolId,
                TMailboxType::Revolving, appData.SystemPoolId));
        localConfig.TabletClassInfo[TTabletTypes::SysViewProcessor] =
            TLocalConfig::TTabletClassInfo(new TTabletSetupInfo(
                &NSysView::CreateSysViewProcessorForTests, TMailboxType::Revolving, appData.UserPoolId,
                TMailboxType::Revolving, appData.SystemPoolId));
        localConfig.TabletClassInfo[TTabletTypes::SequenceShard] =
            TLocalConfig::TTabletClassInfo(new TTabletSetupInfo(
                &NSequenceShard::CreateSequenceShard, TMailboxType::Revolving, appData.UserPoolId,
                TMailboxType::Revolving, appData.SystemPoolId));
        localConfig.TabletClassInfo[TTabletTypes::ReplicationController] =
            TLocalConfig::TTabletClassInfo(new TTabletSetupInfo(
                &NReplication::CreateController, TMailboxType::Revolving, appData.UserPoolId,
                TMailboxType::Revolving, appData.SystemPoolId));
        localConfig.TabletClassInfo[TTabletTypes::StatisticsAggregator] =
            TLocalConfig::TTabletClassInfo(new TTabletSetupInfo(
                &NStat::CreateStatisticsAggregatorForTests, TMailboxType::Revolving, appData.UserPoolId,
                TMailboxType::Revolving, appData.SystemPoolId));
    }

    void TServer::SetupLocalService(ui32 nodeIdx, const TString &domainName) {
        TLocalConfig::TPtr localConfig = new TLocalConfig();
        auto &appData = Runtime->GetAppData(nodeIdx);
        SetupLocalConfig(*localConfig, appData);

        TTenantPoolConfig::TPtr tenantPoolConfig = new TTenantPoolConfig(localConfig);
        tenantPoolConfig->AddStaticSlot(domainName);
        appData.TenantName = CanonizePath(domainName);

        auto poolId = Runtime->Register(CreateTenantPool(tenantPoolConfig), nodeIdx, appData.SystemPoolId,
                                        TMailboxType::Revolving, 0);
        Runtime->RegisterService(MakeTenantPoolRootID(), poolId, nodeIdx);
        if (Settings->EnableConfigsDispatcher) {
            // We overwrite icb settings here to save behavior when configs dispatcher are enabled
            NKikimrConfig::TAppConfig initial = *Settings->AppConfig;
            if (!initial.HasImmediateControlsConfig()) {
                initial.MutableImmediateControlsConfig()->CopyFrom(Settings->Controls);
            }
            auto *dispatcher = NConsole::CreateConfigsDispatcher(
                    NKikimr::NConfig::TConfigsDispatcherInitInfo {
                        .InitialConfig = initial,
                    });
            auto aid = Runtime->Register(dispatcher, nodeIdx, appData.UserPoolId, TMailboxType::Revolving, 0);
            Runtime->RegisterService(NConsole::MakeConfigsDispatcherID(Runtime->GetNodeId(nodeIdx)), aid, nodeIdx);
            if (Settings->EnableFeatureFlagsConfigurator) {
                Runtime->RegisterService(
                    MakeFeatureFlagsServiceID(),
                    Runtime->Register(NConsole::CreateFeatureFlagsConfigurator(), nodeIdx),
                    nodeIdx);
            }
        }
        if (Settings->IsEnableMetadataProvider()) {
            NKikimrConfig::TMetadataProviderConfig cfgProto;
            cfgProto.SetRefreshPeriodSeconds(1);
            cfgProto.SetEnabled(true);
            cfgProto.MutableRequestConfig()->SetRetryPeriodStartSeconds(1);
            cfgProto.MutableRequestConfig()->SetRetryPeriodFinishSeconds(30);
            NMetadata::NProvider::TConfig cfg;
            cfg.DeserializeFromProto(cfgProto);
            auto* actor = NMetadata::NProvider::CreateService(cfg);
            const auto aid = Runtime->Register(actor, nodeIdx, appData.UserPoolId, TMailboxType::Revolving, 0);
            Runtime->RegisterService(NMetadata::NProvider::MakeServiceId(Runtime->GetNodeId(nodeIdx)), aid, nodeIdx);
        }
        if (Settings->IsEnableExternalIndex()) {
            auto* actor = NCSIndex::CreateService(NCSIndex::TConfig());
            const auto aid = Runtime->Register(actor, nodeIdx, appData.UserPoolId, TMailboxType::Revolving, 0);
            Runtime->RegisterService(NCSIndex::MakeServiceId(Runtime->GetNodeId(nodeIdx)), aid, nodeIdx);
        }
        {
            auto* actor = NOlap::NGroupedMemoryManager::TScanMemoryLimiterOperator::CreateService(NOlap::NGroupedMemoryManager::TConfig(), new ::NMonitoring::TDynamicCounters());
            const auto aid = Runtime->Register(actor, nodeIdx, appData.UserPoolId, TMailboxType::Revolving, 0);
            Runtime->RegisterService(NOlap::NGroupedMemoryManager::TScanMemoryLimiterOperator::MakeServiceId(Runtime->GetNodeId(nodeIdx)), aid, nodeIdx);
        }
        {
            auto* actor = NOlap::NGroupedMemoryManager::TCompMemoryLimiterOperator::CreateService(NOlap::NGroupedMemoryManager::TConfig(), new ::NMonitoring::TDynamicCounters());
            const auto aid = Runtime->Register(actor, nodeIdx, appData.UserPoolId, TMailboxType::Revolving, 0);
            Runtime->RegisterService(NOlap::NGroupedMemoryManager::TCompMemoryLimiterOperator::MakeServiceId(Runtime->GetNodeId(nodeIdx)), aid, nodeIdx);
        }
        {
            auto* actor = NPrioritiesQueue::TCompServiceOperator::CreateService(NPrioritiesQueue::TConfig(), new ::NMonitoring::TDynamicCounters());
            const auto aid = Runtime->Register(actor, nodeIdx, appData.UserPoolId, TMailboxType::Revolving, 0);
            Runtime->RegisterService(NPrioritiesQueue::TCompServiceOperator::MakeServiceId(Runtime->GetNodeId(nodeIdx)), aid, nodeIdx);
        }
        {
            auto* actor = NConveyor::TScanServiceOperator::CreateService(NConveyor::TConfig(), new ::NMonitoring::TDynamicCounters());
            const auto aid = Runtime->Register(actor, nodeIdx, appData.UserPoolId, TMailboxType::Revolving, 0);
            Runtime->RegisterService(NConveyor::TScanServiceOperator::MakeServiceId(Runtime->GetNodeId(nodeIdx)), aid, nodeIdx);
        }
        {
            auto* actor = NConveyorComposite::TServiceOperator::CreateService(NConveyorComposite::NConfig::TConfig::BuildDefault(), new ::NMonitoring::TDynamicCounters());
            const auto aid = Runtime->Register(actor, nodeIdx, appData.UserPoolId, TMailboxType::Revolving, 0);
            Runtime->RegisterService(NConveyorComposite::TServiceOperator::MakeServiceId(Runtime->GetNodeId(nodeIdx)), aid, nodeIdx);
        }
        Runtime->Register(CreateLabelsMaintainer({}), nodeIdx, appData.SystemPoolId, TMailboxType::Revolving, 0);

        auto sysViewService = NSysView::CreateSysViewServiceForTests();
        TActorId sysViewServiceId = Runtime->Register(sysViewService.Release(), nodeIdx, appData.UserPoolId);
        Runtime->RegisterService(NSysView::MakeSysViewServiceID(Runtime->GetNodeId(nodeIdx)), sysViewServiceId, nodeIdx);

        auto tenantPublisher = CreateTenantNodeEnumerationPublisher();
        Runtime->Register(tenantPublisher, nodeIdx);

        if (nodeIdx >= StaticNodes()) {
            SetupProxies(nodeIdx);
        }
    }

    void TServer::SetupConfigurators(ui32 nodeIdx) {
        auto &appData = Runtime->GetAppData(nodeIdx);
        Runtime->Register(NConsole::CreateImmediateControlsConfigurator(appData.Icb, Settings->Controls),
                          nodeIdx, appData.SystemPoolId, TMailboxType::Revolving, 0);
    }

    void TServer::SetupProxies(ui32 nodeIdx) {
        const ui32 userPoolId = Runtime->GetAppData(nodeIdx).UserPoolId;

        Runtime->SetTxAllocatorTabletIds({ChangeStateStorage(TxAllocator, Settings->Domain)});
        {
            if (Settings->AuthConfig.HasLdapAuthentication()) {
                IActor* ldapAuthProvider = NKikimr::CreateLdapAuthProvider(Settings->AuthConfig.GetLdapAuthentication());
                TActorId ldapAuthProviderId = Runtime->Register(ldapAuthProvider, nodeIdx, userPoolId);
                Runtime->RegisterService(MakeLdapAuthProviderID(), ldapAuthProviderId, nodeIdx);
            }
            TTicketParserSettings ticketParserSettings {
                .AuthConfig = Settings->AuthConfig,
                .CertificateAuthValues = {
                    .ClientCertificateAuthorization = Settings->AppConfig->GetClientCertificateAuthorization(),
                    .ServerCertificateFilePath = Settings->ServerCertFilePath,
                    .Domain = Settings->AuthConfig.GetCertificateAuthenticationDomain()
                }
            };
            IActor* ticketParser = Settings->CreateTicketParser(ticketParserSettings);
            TActorId ticketParserId = Runtime->Register(ticketParser, nodeIdx, userPoolId);
            Runtime->RegisterService(MakeTicketParserID(), ticketParserId, nodeIdx);
        }

        {
            IActor* healthCheck = NHealthCheck::CreateHealthCheckService();
            TActorId healthCheckId = Runtime->Register(healthCheck, nodeIdx, userPoolId);
            Runtime->RegisterService(NHealthCheck::MakeHealthCheckID(), healthCheckId, nodeIdx);
        }
        {
            const auto& appData = Runtime->GetAppData(nodeIdx);
            IActor* metadataCache = CreateDatabaseMetadataCache(appData.TenantName, appData.Counters).release();
            TActorId metadataCacheId = Runtime->Register(metadataCache, nodeIdx, userPoolId);
            Runtime->RegisterService(MakeDatabaseMetadataCacheId(Runtime->GetNodeId(nodeIdx)), metadataCacheId, nodeIdx);
        }
        {
            auto kqpProxySharedResources = std::make_shared<NKqp::TKqpProxySharedResources>();

            IActor* kqpRmService = NKqp::CreateKqpResourceManagerActor(
                Settings->AppConfig->GetTableServiceConfig().GetResourceManager(), nullptr, {}, kqpProxySharedResources, Runtime->GetNodeId(nodeIdx));
            TActorId kqpRmServiceId = Runtime->Register(kqpRmService, nodeIdx, userPoolId);
            Runtime->RegisterService(NKqp::MakeKqpRmServiceID(Runtime->GetNodeId(nodeIdx)), kqpRmServiceId, nodeIdx);

            if (!KqpLoggerScope) {
                // We need to keep YqlLoggerScope alive longer than the actor system
                KqpLoggerScope = std::make_shared<NYql::NLog::YqlLoggerScope>(
                    new NYql::NLog::TTlsLogBackend(new TNullLogBackend()));
            }

            NKikimr::NKqp::IKqpFederatedQuerySetupFactory::TPtr federatedQuerySetupFactory = Settings->FederatedQuerySetupFactory;
            if (Settings->InitializeFederatedQuerySetupFactory) {
                const auto& queryServiceConfig = Settings->AppConfig->GetQueryServiceConfig();

                NYql::NConnector::IClient::TPtr connectorClient;
                NYql::IDatabaseAsyncResolver::TPtr databaseAsyncResolver;
                if (queryServiceConfig.HasGeneric()) {
                    const auto& genericGatewayConfig = queryServiceConfig.GetGeneric();

                    connectorClient = NYql::NConnector::MakeClientGRPC(genericGatewayConfig);

                    auto httpProxyActorId = NFq::MakeYqlAnalyticsHttpProxyId();
                    Runtime->RegisterService(
                        httpProxyActorId,
                        Runtime->Register(NHttp::CreateHttpProxy(), nodeIdx, userPoolId),
                        nodeIdx
                    );

                    auto databaseResolverActorId = NFq::MakeDatabaseResolverActorId();
                    Runtime->RegisterService(
                        databaseResolverActorId,
                        Runtime->Register(NFq::CreateDatabaseResolver(httpProxyActorId, Settings->CredentialsFactory), nodeIdx, userPoolId),
                        nodeIdx
                    );

                    if (genericGatewayConfig.HasMdbGateway() || genericGatewayConfig.HasYdbMvpEndpoint()) {
                        databaseAsyncResolver = std::make_shared<NFq::TDatabaseAsyncResolverImpl>(
                            Runtime->GetActorSystem(nodeIdx),
                            databaseResolverActorId,
                            genericGatewayConfig.GetYdbMvpEndpoint(),
                            genericGatewayConfig.GetMdbGateway(),
                            NFq::MakeMdbEndpointGeneratorGeneric(queryServiceConfig.GetMdbTransformHost())
                        );
                    }
                }
                auto driver = std::make_shared<NYdb::TDriver>(NYdb::TDriverConfig());

                federatedQuerySetupFactory = std::make_shared<NKikimr::NKqp::TKqpFederatedQuerySetupFactoryMock>(
                    NKqp::MakeHttpGateway(queryServiceConfig.GetHttpGateway(), Runtime->GetAppData(nodeIdx).Counters),
                    connectorClient,
                    Settings->CredentialsFactory,
                    databaseAsyncResolver,
                    queryServiceConfig.GetS3(),
                    queryServiceConfig.GetGeneric(),
                    queryServiceConfig.GetYt(),
                    Settings->YtGateway ? Settings->YtGateway : NKqp::MakeYtGateway(GetFunctionRegistry(), queryServiceConfig),
                    queryServiceConfig.GetSolomon(),
                    Settings->SolomonGateway ? Settings->SolomonGateway : NYql::CreateSolomonGateway(queryServiceConfig.GetSolomon()),
                    Settings->ComputationFactory,
                    NYql::NDq::CreateReadActorFactoryConfig(queryServiceConfig.GetS3()),
                    Settings->DqTaskTransformFactory,
                    NYql::TPqGatewayConfig{},
                    Settings->PqGateway ? Settings->PqGateway : NKqp::MakePqGateway(driver, NYql::TPqGatewayConfig{}),
                    std::make_shared<NKikimr::TDeferredActorLogBackend::TAtomicActorSystemPtr>(nullptr),
                    driver);
            }

            IActor* kqpProxyService = NKqp::CreateKqpProxyService(Settings->AppConfig->GetLogConfig(),
                                                                  Settings->AppConfig->GetTableServiceConfig(),
                                                                  Settings->AppConfig->GetQueryServiceConfig(),
                                                                  TVector<NKikimrKqp::TKqpSetting>(Settings->KqpSettings),
                                                                  nullptr, std::move(kqpProxySharedResources),
                                                                  federatedQuerySetupFactory, Settings->S3ActorsFactory);
            TActorId kqpProxyServiceId = Runtime->Register(kqpProxyService, nodeIdx, userPoolId);
            Runtime->RegisterService(NKqp::MakeKqpProxyID(Runtime->GetNodeId(nodeIdx)), kqpProxyServiceId, nodeIdx);

            IActor* scriptFinalizeService = NKqp::CreateKqpFinalizeScriptService(
                Settings->AppConfig->GetQueryServiceConfig(),
                federatedQuerySetupFactory,
                Settings->S3ActorsFactory,
                Settings->EnableScriptExecutionBackgroundChecks
            );
            TActorId scriptFinalizeServiceId = Runtime->Register(scriptFinalizeService, nodeIdx, userPoolId);
            Runtime->RegisterService(NKqp::MakeKqpFinalizeScriptServiceId(Runtime->GetNodeId(nodeIdx)), scriptFinalizeServiceId, nodeIdx);
        }

        {
            IActor* txProxy = CreateTxProxy(Runtime->GetTxAllocatorTabletIds());
            TActorId txProxyId = Runtime->Register(txProxy, nodeIdx);
            Runtime->RegisterService(MakeTxProxyID(), txProxyId, nodeIdx);
        }

        {
            IActor* compileService = CreateMiniKQLCompileService(100000);
            TActorId compileServiceId = Runtime->Register(compileService, nodeIdx, userPoolId, TMailboxType::Revolving, 0);
            Runtime->RegisterService(MakeMiniKQLCompileServiceID(), compileServiceId, nodeIdx);
        }

        {
            IActor* longTxService = NLongTxService::CreateLongTxService();
            TActorId longTxServiceId = Runtime->Register(longTxService, nodeIdx, userPoolId);
            Runtime->RegisterService(NLongTxService::MakeLongTxServiceID(Runtime->GetNodeId(nodeIdx)), longTxServiceId, nodeIdx);
        }

        {
            IActor* sequenceProxy = NSequenceProxy::CreateSequenceProxy();
            TActorId sequenceProxyId = Runtime->Register(sequenceProxy, nodeIdx, userPoolId);
            Runtime->RegisterService(NSequenceProxy::MakeSequenceProxyServiceID(), sequenceProxyId, nodeIdx);
        }

        if (BusServer && nodeIdx == 0) { // MsgBus and GRPC are run now only on first node
            {
                IActor* proxy = BusServer->CreateProxy();
                TActorId proxyId = Runtime->Register(proxy, nodeIdx, Runtime->GetAppData(nodeIdx).SystemPoolId, TMailboxType::Revolving, 0);
                Runtime->RegisterService(NMsgBusProxy::CreateMsgBusProxyId(), proxyId, nodeIdx);
            }
        }
        {
            IActor* icNodeCache = NIcNodeCache::CreateICNodesInfoCacheService(Runtime->GetDynamicCounters());
            TActorId icCacheId = Runtime->Register(icNodeCache, nodeIdx, userPoolId);
            Runtime->RegisterService(NIcNodeCache::CreateICNodesInfoCacheServiceId(), icCacheId, nodeIdx);
        }
        {
            auto driverConfig = NYdb::TDriverConfig().SetEndpoint(TStringBuilder() << "localhost:" << Settings->GrpcPort);
            if (!Driver) {
                Driver.Reset(new NYdb::TDriver(driverConfig));
            }
            Runtime->GetAppData(nodeIdx).YdbDriver = Driver.Get();
        }

        {
            IActor* pqClusterTracker = NPQ::NClusterTracker::CreateClusterTracker();
            TActorId pqClusterTrackerId = Runtime->Register(pqClusterTracker, nodeIdx, userPoolId);
            Runtime->RegisterService(NPQ::NClusterTracker::MakeClusterTrackerID(), pqClusterTrackerId, nodeIdx);
        }
        {
            IActor* pqReadCacheService = NPQ::CreatePQDReadCacheService(Runtime->GetDynamicCounters());
            TActorId readCacheId = Runtime->Register(pqReadCacheService, nodeIdx, userPoolId);
            Runtime->RegisterService(NPQ::MakePQDReadCacheServiceActorId(), readCacheId, nodeIdx);
        }

        {
            if (Settings->PQConfig.GetEnabled() == true) {
                IActor *pqMetaCache = NMsgBusProxy::NPqMetaCacheV2::CreatePQMetaCache(
                        new ::NMonitoring::TDynamicCounters(), TDuration::Seconds(1)
                );

                TActorId pqMetaCacheId = Runtime->Register(pqMetaCache, nodeIdx, userPoolId);
                Runtime->RegisterService(NMsgBusProxy::CreatePersQueueMetaCacheV2Id(), pqMetaCacheId, nodeIdx);
            }
        }

        {
            if (Settings->EnableMetering) {
                THolder<TFileLogBackend> fileBackend;
                try {
                    fileBackend = MakeHolder<TFileLogBackend>(Settings->MeteringFilePath);
                    auto meteringActor = NMetering::CreateMeteringWriter(std::move(fileBackend));
                    TActorId meteringId = Runtime->Register(meteringActor.Release(), nodeIdx, Runtime->GetAppData(nodeIdx).IOPoolId);
                    Runtime->RegisterService(NMetering::MakeMeteringServiceID(), meteringId, nodeIdx);

                } catch (const TFileError &ex) {
                    Cerr << "TMeteringWriterInitializer: failed to open file '" << Settings->MeteringFilePath << "': "
                         << ex.what() << Endl;
                }
            }
        }

        {
            if (Settings->NeedStatsCollectors) {
                TString filePathPrefix;
                if (Settings->AppConfig->HasMonitoringConfig()) {
                    filePathPrefix = Settings->AppConfig->GetMonitoringConfig().GetMemAllocDumpPathPrefix();
                }

                const TIntrusivePtr<NMemory::IProcessMemoryInfoProvider> processMemoryInfoProvider(MakeIntrusive<NMemory::TProcessMemoryInfoProvider>());

                IActor* monitorActor = CreateMemProfMonitor(TDuration::Seconds(1), processMemoryInfoProvider,
                    Runtime->GetAppData(nodeIdx).Counters, filePathPrefix);
                const TActorId monitorActorId = Runtime->Register(monitorActor, nodeIdx, Runtime->GetAppData(nodeIdx).BatchPoolId);
                Runtime->RegisterService(MakeMemProfMonitorID(Runtime->GetNodeId(nodeIdx)), monitorActorId, nodeIdx);

                IActor* controllerActor = NMemory::CreateMemoryController(TDuration::Seconds(1), processMemoryInfoProvider,
                    Settings->AppConfig->GetMemoryControllerConfig(), NKikimrConfigHelpers::CreateMemoryControllerResourceBrokerConfig(*Settings->AppConfig),
                    Runtime->GetAppData(nodeIdx).Counters);
                const TActorId controllerActorId = Runtime->Register(controllerActor, nodeIdx, Runtime->GetAppData(nodeIdx).BatchPoolId);
                Runtime->RegisterService(NMemory::MakeMemoryControllerId(0), controllerActorId, nodeIdx);
            }
        }

        {
            auto statActor = NStat::CreateStatService();
            const TActorId statActorId = Runtime->Register(statActor.Release(), nodeIdx, Runtime->GetAppData(nodeIdx).UserPoolId);
            Runtime->RegisterService(NStat::MakeStatServiceID(Runtime->GetNodeId(nodeIdx)), statActorId, nodeIdx);
        }

        {
            IActor* kesusService = NKesus::CreateKesusProxyService();
            TActorId kesusServiceId = Runtime->Register(kesusService, nodeIdx, userPoolId);
            Runtime->RegisterService(NKesus::MakeKesusProxyServiceId(), kesusServiceId, nodeIdx);
        }

        {
            IActor* netClassifier = NNetClassifier::CreateNetClassifier();
            TActorId netClassifierId = Runtime->Register(netClassifier, nodeIdx, userPoolId);
            Runtime->RegisterService(NNetClassifier::MakeNetClassifierID(), netClassifierId, nodeIdx);
        }

        {
            IActor* actor = CreatePollerActor();
            TActorId actorId = Runtime->Register(actor, nodeIdx, Runtime->GetAppData(nodeIdx).SystemPoolId);
            Runtime->RegisterService(MakePollerActorId(), actorId, nodeIdx);
        }
        if (Settings->AppConfig->GetKafkaProxyConfig().GetEnableKafkaProxy()) {

            IActor* discoveryCache = CreateDiscoveryCache(NGRpcService::KafkaEndpointId);
            TActorId discoveryCacheId = Runtime->Register(discoveryCache, nodeIdx, userPoolId);
            Runtime->RegisterService(NKafka::MakeKafkaDiscoveryCacheID(), discoveryCacheId, nodeIdx);

            TActorId kafkaTxnCoordinatorActorId = Runtime->Register(NKafka::CreateTransactionsCoordinator(), nodeIdx, userPoolId);
            Runtime->RegisterService(NKafka::MakeTransactionsServiceID(Runtime->GetNodeId(nodeIdx)), kafkaTxnCoordinatorActorId, nodeIdx);

            NKafka::TListenerSettings settings;
            settings.Port = Settings->AppConfig->GetKafkaProxyConfig().GetListeningPort();
            bool ssl = false;
            if (Settings->AppConfig->GetKafkaProxyConfig().HasSslCertificate()) {
                ssl = true;
                settings.SslCertificatePem = Settings->AppConfig->GetKafkaProxyConfig().GetSslCertificate();
            }

            IActor* actor = NKafka::CreateKafkaListener(MakePollerActorId(), settings, Settings->AppConfig->GetKafkaProxyConfig());
            TActorId actorId = Runtime->Register(actor, nodeIdx, userPoolId);
            Runtime->RegisterService(TActorId{}, actorId, nodeIdx);

            IActor* metricsActor = CreateKafkaMetricsActor(NKafka::TKafkaMetricsSettings{Runtime->GetAppData().Counters->GetSubgroup("counters", "kafka_proxy")});
            TActorId metricsActorId = Runtime->Register(metricsActor, nodeIdx, userPoolId);
            Runtime->RegisterService(NKafka::MakeKafkaMetricsServiceID(), metricsActorId, nodeIdx);

            {
                auto& appData = Runtime->GetAppData(0);

                TIntrusivePtr<NGRpcService::TGrpcEndpointDescription> desc = new NGRpcService::TGrpcEndpointDescription();
                desc->Address = Settings->AppConfig->GetGRpcConfig().GetHost();
                desc->Port = settings.Port;
                desc->Ssl = ssl;
                desc->EndpointId = NGRpcService::KafkaEndpointId;

                TVector<TString> rootDomains;
                if (const auto& domain = appData.DomainsInfo->Domain) {
                    rootDomains.emplace_back("/" + domain->Name);
                }
                desc->ServedDatabases.insert(desc->ServedDatabases.end(), rootDomains.begin(), rootDomains.end());
                Runtime->GetActorSystem(0)->Register(NGRpcService::CreateGrpcEndpointPublishActor(desc.Get()), TMailboxType::ReadAsFilled, appData.UserPoolId);
            }
        }

        if (Settings->EnableYq) {
            NFq::NConfig::TConfig protoConfig;
            protoConfig.SetEnabled(true);

            protoConfig.MutableQuotasManager()->SetEnabled(true);
            protoConfig.MutableRateLimiter()->SetEnabled(false);
            protoConfig.MutableRateLimiter()->SetControlPlaneEnabled(true); // Will answer on creation requests and give empty kesus name
            protoConfig.MutableRateLimiter()->SetDataPlaneEnabled(true);

            protoConfig.MutableCommon()->SetIdsPrefix("id");

            TString endpoint = TStringBuilder() << "localhost:" << Settings->GrpcPort;
            TString prefix = "Root/yq";
            auto port = Runtime->GetPortManager().GetPort();
            TString ydbMvpEndpoint = TStringBuilder()
                << "http://localhost:"
                << port
                << "/yql-mock/abc";

            {
                auto& controlPlaneProxyConfig = *protoConfig.MutableControlPlaneProxy();
                controlPlaneProxyConfig.SetEnabled(true);
            }

            {
                auto& testConnectionConfig = *protoConfig.MutableTestConnection();
                testConnectionConfig.SetEnabled(true);
            }

            {
                auto& controlPlaneStorageConfig = *protoConfig.MutableControlPlaneStorage();
                controlPlaneStorageConfig.SetEnabled(true);
                controlPlaneStorageConfig.SetUseInMemory(Settings->AppConfig->GetFederatedQueryConfig().GetControlPlaneStorage().GetUseInMemory());
                auto& storage = *controlPlaneStorageConfig.MutableStorage();
                storage.SetEndpoint(endpoint);
                storage.SetTablePrefix(prefix);

                controlPlaneStorageConfig.AddAvailableBinding("DATA_STREAMS");
                controlPlaneStorageConfig.AddAvailableBinding("OBJECT_STORAGE");

                controlPlaneStorageConfig.AddAvailableConnection("YDB_DATABASE");
                controlPlaneStorageConfig.AddAvailableConnection("CLICKHOUSE_CLUSTER");
                controlPlaneStorageConfig.AddAvailableConnection("DATA_STREAMS");
                controlPlaneStorageConfig.AddAvailableConnection("OBJECT_STORAGE");
                controlPlaneStorageConfig.AddAvailableConnection("MONITORING");
            }

            {
                auto& commonConfig = *protoConfig.MutableCommon();
                commonConfig.SetYdbMvpCloudEndpoint(ydbMvpEndpoint);
                commonConfig.SetIdsPrefix("ut");
            }

            {
                auto& privateApiConfig = *protoConfig.MutablePrivateApi();
                privateApiConfig.SetEnabled(true);
                privateApiConfig.SetTaskServiceEndpoint(endpoint);
                privateApiConfig.SetTaskServiceDatabase("Root");
            }

            {
                auto& tokenAccessorConfig = *protoConfig.MutableTokenAccessor();
                tokenAccessorConfig.SetEnabled(true);
            }

            {
                auto& dbPoolConfig = *protoConfig.MutableDbPool();
                dbPoolConfig.SetEnabled(true);
                auto& storage = *dbPoolConfig.MutableStorage();
                storage.SetEndpoint(endpoint);
                storage.SetTablePrefix(prefix);
            }

            {
                auto& resourceManagerConfig = *protoConfig.MutableResourceManager();
                resourceManagerConfig.SetEnabled(true);
            }

            {
                auto& privateProxyConfig = *protoConfig.MutablePrivateProxy();
                privateProxyConfig.SetEnabled(true);
            }

            {
                auto& nodesManagerConfig = *protoConfig.MutableNodesManager();
                nodesManagerConfig.SetEnabled(true);
            }

            {
                auto& pendingFetcherConfig = *protoConfig.MutablePendingFetcher();
                pendingFetcherConfig.SetEnabled(true);
            }

            auto& appData = Runtime->GetAppData();

            auto actorRegistrator = [&](NActors::TActorId serviceActorId, NActors::IActor* actor) {
                auto actorId = Runtime->Register(actor, nodeIdx);
                Runtime->RegisterService(serviceActorId, actorId, nodeIdx);
            };

            const auto ydbCredFactory = NKikimr::CreateYdbCredentialsProviderFactory;
            auto counters = MakeIntrusive<::NMonitoring::TDynamicCounters>();
            YqSharedResources = NFq::CreateYqSharedResources(protoConfig, ydbCredFactory, counters);
            NFq::Init(
                protoConfig,
                Runtime->GetNodeId(nodeIdx),
                actorRegistrator,
                &appData,
                "TestTenant",
                nullptr, // MakeIntrusive<NPq::NConfigurationManager::TConnections>(),
                YqSharedResources,
                [](auto& config) { return NKikimr::NFolderService::CreateMockFolderServiceAdapterActor(config, "");},
                /*IcPort = */0,
                {}
                );
            NFq::InitTest(Runtime.Get(), port, Settings->GrpcPort, YqSharedResources);
        }
        {
            using namespace NViewer;
            if (Settings->KikimrRunConfig) {
                IActor* viewer = CreateViewer(*Settings->KikimrRunConfig);
                SetupPQVirtualHandlers(dynamic_cast<IViewer*>(viewer));
                SetupDBVirtualHandlers(dynamic_cast<IViewer*>(viewer));
                TActorId viewerId = Runtime->Register(viewer, nodeIdx, Runtime->GetAppData(nodeIdx).BatchPoolId);
                Runtime->RegisterService(MakeViewerID(nodeIdx), viewerId, nodeIdx);
            }
        }
    }

    void TServer::SetupLogging() {
        Runtime->SetLogPriority(NKikimrServices::FLAT_TX_SCHEMESHARD, NLog::PRI_WARN);
        //Runtime->SetLogPriority(NKikimrServices::SCHEMESHARD_DESCRIBE, NLog::PRI_DEBUG);
        //Runtime->SetLogPriority(NKikimrServices::HIVE, NActors::NLog::PRI_DEBUG);
        //Runtime->SetLogPriority(NKikimrServices::LOCAL, NActors::NLog::PRI_DEBUG);

        Runtime->SetLogPriority(NKikimrServices::BS_CONTROLLER, NLog::PRI_WARN);
        Runtime->SetLogPriority(NKikimrServices::RPC_REQUEST, NLog::PRI_WARN);

        //Runtime->SetLogPriority(NKikimrServices::TX_COORDINATOR, NLog::PRI_DEBUG);
        //Runtime->SetLogPriority(NKikimrServices::TX_MEDIATOR, NLog::PRI_DEBUG);
        //Runtime->SetLogPriority(NKikimrServices::TX_PROXY, NLog::PRI_DEBUG);
        //Runtime->SetLogPriority(NKikimrServices::TX_PROXY_SCHEME_CACHE, NLog::PRI_DEBUG);

        //Runtime->SetLogPriority(NKikimrServices::MINIKQL_ENGINE, NLog::PRI_DEBUG);
        //Runtime->SetLogPriority(NKikimrServices::KQP_PROXY, NLog::PRI_DEBUG);
        //Runtime->SetLogPriority(NKikimrServices::KQP_WORKER, NLog::PRI_DEBUG);

        //Runtime->SetLogPriority(NKikimrServices::TX_PROXY, NActors::NLog::PRI_TRACE);
        //Runtime->SetLogPriority(NKikimrServices::TX_PROXY_SCHEME_CACHE, NActors::NLog::PRI_TRACE);
        //Runtime->SetLogPriority(NKikimrServices::SCHEME_BOARD_REPLICA, NActors::NLog::PRI_DEBUG);
        //Runtime->SetLogPriority(NKikimrServices::SCHEME_BOARD_POPULATOR, NActors::NLog::PRI_DEBUG);
        //Runtime->SetLogPriority(NKikimrServices::SCHEME_BOARD_SUBSCRIBER, NActors::NLog::PRI_TRACE);
        //Runtime->SetLogPriority(NKikimrServices::YQL_PROXY, NActors::NLog::PRI_DEBUG);

        if (Settings->LoggerInitializer) {
            Settings->LoggerInitializer(*Runtime);
        }
    }

    void TServer::AddSysViewsRosterUpdateObserver() {
        if (Runtime && !Runtime->IsRealThreads()) {
            SysViewsRosterUpdateFinished = false;
            SysViewsRosterUpdateObserver = Runtime->AddObserver<NSysView::TEvSysView::TEvRosterUpdateFinished>([this](auto&) {
                SysViewsRosterUpdateFinished = true;
            });
        }
    }

    void TServer::WaitForSysViewsRosterUpdate() {
        if (Runtime && !Runtime->IsRealThreads()) {
            Runtime->WaitFor("SysViewsRoster update finished", [this] {
                return SysViewsRosterUpdateFinished;
            });
            SysViewsRosterUpdateObserver.Remove();
        }
    }

    void TServer::StartDummyTablets() {
        if (!Runtime)
            ythrow TWithBackTrace<yexception>() << "Server is redirected";

        CreateTestBootstrapper(*Runtime, CreateTestTabletInfo(ChangeStateStorage(DummyTablet1, Settings->Domain), TTabletTypes::Dummy), &CreateFlatDummyTablet);
        CreateTestBootstrapper(*Runtime, CreateTestTabletInfo(ChangeStateStorage(DummyTablet2, Settings->Domain), TTabletTypes::Dummy), &CreateFlatDummyTablet);
    }

    TTestActorRuntime* TServer::GetRuntime() const {
        return Runtime.Get();
    }

    const TServerSettings &TServer::GetSettings() const {
        return *Settings;
    }

    const NScheme::TTypeRegistry* TServer::GetTypeRegistry() {
        return Runtime->GetAppData().TypeRegistry;
    }

    const NMiniKQL::IFunctionRegistry* TServer::GetFunctionRegistry() {
        return Runtime->GetAppData().FunctionRegistry;
    }

    const NYdb::TDriver& TServer::GetDriver() const {
        Y_ABORT_UNLESS(Driver);
        return *Driver;
    }

    const NYdbGrpc::TGRpcServer& TServer::GetGRpcServer() const {
        return GetTenantGRpcServer(Settings->DomainName);
    }

    const NYdbGrpc::TGRpcServer& TServer::GetTenantGRpcServer(const TString& tenant) const {
        const auto tenantIt = TenantsGRpc.find(tenant);
        Y_ABORT_UNLESS(tenantIt != TenantsGRpc.end());
        const auto& nodesGRpc = tenantIt->second;
        Y_ABORT_UNLESS(!nodesGRpc.empty());
        Y_ABORT_UNLESS(nodesGRpc.begin()->second.GRpcServer);
        return *nodesGRpc.begin()->second.GRpcServer;
    }

    void TServer::WaitFinalization() {
        for (ui32 nodeIdx = 0; nodeIdx < StaticNodes(); ++nodeIdx) {
            if (!NKqp::WaitHttpGatewayFinalization(Runtime->GetAppData(nodeIdx).Counters)) {
                Cerr << "Http gateway finalization timeout on node " << nodeIdx << "\n";
            }
        }
    }

    TServer::~TServer() {
        ShutdownGRpc();

        if (YqSharedResources) {
            YqSharedResources->Stop();
        }

        if (Runtime) {
            WaitFinalization();
            SysViewsRosterUpdateObserver.Remove();
            Runtime.Destroy();
        }

        if (Bus) {
            Bus->Stop();
            Bus.Drop();
        }
    }


    TClient::TClient(const TServerSettings& settings)
        : Domain(settings.Domain)
        , DomainName(settings.DomainName)
        , SupportsRedirect(settings.SupportsRedirect)
        , StoragePoolTypes(settings.StoragePoolTypes)
        , Verbose(settings.Verbose)
        , FunctionRegistry(NKikimr::NMiniKQL::CreateFunctionRegistry(NKikimr::NMiniKQL::CreateBuiltinRegistry()))
        , LoadedFunctionRegistry(NKikimr::NMiniKQL::CreateFunctionRegistry(NKikimr::NMiniKQL::CreateBuiltinRegistry()))
    {
        TServerSetup serverSetup;
        if (SupportsRedirect && Tests::IsServerRedirected()) {
            serverSetup = GetServerSetup();
        } else {
            serverSetup = TServerSetup("localhost", settings.Port);
        }

        ClientConfig.Ip = serverSetup.IpAddress;
        ClientConfig.Port = serverSetup.Port;
        ClientConfig.BusSessionConfig.TotalTimeout = Max<int>() / 2;
        ClientConfig.BusSessionConfig.ConnectTimeout = ConnectTimeoutMilliSeconds;
        ClientConfig.BusSessionConfig.NumRetries = 10;
        Client.reset(new NMsgBusProxy::TMsgBusClient(ClientConfig));
        Client->Init();

        if (Verbose) {
            Cerr << "TClient is connected to server " << ClientConfig.Ip << ":" << ClientConfig.Port << Endl;
        }
    }

    const NMsgBusProxy::TMsgBusClientConfig& TClient::GetClientConfig() const {
        return ClientConfig;
    }

    TClient::~TClient() {
        Client->Shutdown();
    }


    std::shared_ptr<NMsgBusProxy::TMsgBusClient> TClient::GetClient() const {
        return Client;
    }

    const NScheme::TTypeRegistry& TClient::GetTypeRegistry() const {
        return TypeRegistry;
    }

    bool TClient::LoadTypes() {
        TAutoPtr<NMsgBusProxy::TBusTypesRequest> request(new NMsgBusProxy::TBusTypesRequest());
        if (TypesEtag.Defined()) {
            request->Record.SetETag(*TypesEtag.Get());
        }

        TAutoPtr<NBus::TBusMessage> reply;
        UNIT_ASSERT_VALUES_EQUAL(SyncCall(request, reply), NBus::MESSAGE_OK);
        const NKikimrClient::TTypeMetadataResponse &response = static_cast<NMsgBusProxy::TBusTypesResponse*>(reply.Get())->Record;
        UNIT_ASSERT_VALUES_EQUAL((ui32)NMsgBusProxy::MSTATUS_OK, response.GetStatus());
        if (!response.HasETag()) {
            UNIT_ASSERT(TypesEtag.Defined());
            return false;
        }

        UNIT_ASSERT(response.HasTypeMetadata() && response.HasFunctionMetadata());
        DeserializeMetadata(response.GetTypeMetadata(), &LoadedTypeMetadataRegistry);
        DeserializeMetadata(response.GetFunctionMetadata(), *LoadedFunctionRegistry->GetBuiltins());
        TypesEtag = response.GetETag();
        return true;
    }

    const NScheme::TTypeMetadataRegistry& TClient::GetTypeMetadataRegistry() const {
        if (TypesEtag.Defined())
            return LoadedTypeMetadataRegistry;

        return TypeRegistry.GetTypeMetadataRegistry();
    }

    const NMiniKQL::IFunctionRegistry& TClient::GetFunctionRegistry() const {
        if (TypesEtag.Defined())
            return *LoadedFunctionRegistry;

        return *FunctionRegistry;
    }

    ui64 TClient::GetPatchedSchemeRoot(ui64 schemeRoot, ui32 domain, bool supportsRedirect) {
        if (!supportsRedirect || !IsServerRedirected())
            return ChangeStateStorage(schemeRoot, domain);

        TString domainRedirect = GetEnv(DomainRedirectEnvVar);
        if (!domainRedirect)
            ythrow TWithBackTrace<yexception>() << "Please set domain redirect, format: KIKIMR_TEST_DOMAIN=domain/RootShardTabletId";

        TStringBuf domainUidStr;
        TStringBuf tabletIdStr;
        TStringBuf(domainRedirect).Split('/', domainUidStr, tabletIdStr);
        const ui32 domainUid = FromString<ui32>(domainUidStr);
        if (domainUid != domain) {
            ythrow TWithBackTrace<yexception>() << "Mismatch domain redirect, expected domain: " << domain
                << ", redirected domain: " << domainUid;
        }

        return FromString<ui64>(tabletIdStr);
    }

    void TClient::WaitRootIsUp(const TString& root) {
        while (true) {
            if (Verbose) {
                Cerr << "WaitRootIsUp '" << root << "'..." << Endl;
            }

            TAutoPtr<NMsgBusProxy::TBusResponse> resp = Ls(root);
            UNIT_ASSERT(resp);

            if (resp->Record.GetStatus() == NMsgBusProxy::MSTATUS_OK && resp->Record.GetSchemeStatus() == NKikimrScheme::StatusSuccess) {
                if (Verbose) {
                    Cerr << "WaitRootIsUp '" << root << "' success." << Endl;
                }
                break;
            }
        }
    }

    TAutoPtr<NBus::TBusMessage> TClient::InitRootSchemeWithReply(const TString& root) {
        WaitRootIsUp(root);

        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request = new NMsgBusProxy::TBusSchemeOperation();
        auto* transaction = request->Record.MutableTransaction()->MutableModifyScheme();
        transaction->SetWorkingDir("/");
        transaction->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpAlterSubDomain);
        auto op = transaction->MutableSubDomain();
        op->SetName(root);

        for (const auto& [kind, pool] : StoragePoolTypes) {
            auto* p = op->AddStoragePools();
            p->SetKind(kind);
            p->SetName(pool.GetName());
        }

        TAutoPtr<NBus::TBusMessage> reply;
        SendAndWaitCompletion(request, reply);

        if (Verbose) {
            Cout << PrintToString<NMsgBusProxy::TBusResponse>(reply.Get()) << Endl;
        }
        return reply;
    }

    void TClient::InitRootScheme() {
        InitRootScheme(DomainName);
    }

    void TClient::InitRootScheme(const TString& root) {
        TAutoPtr<NBus::TBusMessage> reply = InitRootSchemeWithReply(root);
        auto resp = dynamic_cast<NMsgBusProxy::TBusResponse*>(reply.Get());
        UNIT_ASSERT(resp);
        UNIT_ASSERT_VALUES_EQUAL(resp->Record.GetStatus(), NMsgBusProxy::MSTATUS_OK);
    }


    NBus::EMessageStatus TClient::WaitCompletion(ui64 txId, ui64 schemeshard, ui64 pathId,
                                        TAutoPtr<NBus::TBusMessage>& reply,
                                        TDuration timeout)
    {
        auto deadline = TInstant::Now() + timeout;

        NBus::EMessageStatus status;
        const NKikimrClient::TResponse* response = nullptr;
        do {
            TAutoPtr<NMsgBusProxy::TBusSchemeOperationStatus> msg = new NMsgBusProxy::TBusSchemeOperationStatus();
            msg->Record.MutableFlatTxId()->SetTxId(txId);
            msg->Record.MutableFlatTxId()->SetSchemeShardTabletId(schemeshard);
            msg->Record.MutableFlatTxId()->SetPathId(pathId);
            msg->Record.MutablePollOptions()->SetTimeout(timeout.MilliSeconds());
            if (Verbose) {
                Cerr << "waiting..." << Endl;
            }
            status = SyncCall(msg, reply);
            if (status != NBus::MESSAGE_OK) {
                const char *description = NBus::MessageStatusDescription(status);
                Cerr << description << Endl;
                return status;
            }
            if (reply->GetHeader()->Type != NMsgBusProxy::MTYPE_CLIENT_RESPONSE) {
                break;
            }
            response = &static_cast<NMsgBusProxy::TBusResponse*>(reply.Get())->Record;
        } while (response->GetStatus() == NMsgBusProxy::MSTATUS_INPROGRESS && deadline >= TInstant::Now());

        return status;
    }

    NBus::EMessageStatus TClient::SendAndWaitCompletion(TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request,
                                                        TAutoPtr<NBus::TBusMessage>& reply,
                                                        TDuration timeout) {
        PrepareRequest(request);

        NBus::EMessageStatus status = SendWhenReady(request, reply, timeout.MilliSeconds());

        if (status != NBus::MESSAGE_OK) {
            return status;
        }

        const NMsgBusProxy::TBusResponse* flatResponse = dynamic_cast<const NMsgBusProxy::TBusResponse*>(reply.Get());
        if (!flatResponse)
            return NBus::MESSAGE_UNKNOWN;

        const NKikimrClient::TResponse* response = &flatResponse->Record;

        if (response->HasErrorReason()) {
            Cerr << "Error " << response->GetStatus() << ": " << response->GetErrorReason() << Endl;
        }

        if (response->GetStatus() != NMsgBusProxy::MSTATUS_INPROGRESS) {
            return status;
        }

        NKikimrClient::TFlatTxId txId = response->GetFlatTxId();
        return WaitCompletion(txId.GetTxId(), txId.GetSchemeShardTabletId(), txId.GetPathId(), reply, timeout);
    }

    NMsgBusProxy::EResponseStatus TClient::MkDir(const TString& parent, const TString& name, const TApplyIf& applyIf) {
        NMsgBusProxy::TBusSchemeOperation* request(new NMsgBusProxy::TBusSchemeOperation());
        auto* mkDirTx = request->Record.MutableTransaction()->MutableModifyScheme();
        mkDirTx->SetWorkingDir(parent);
        mkDirTx->SetOperationType(NKikimrSchemeOp::ESchemeOpMkDir);
        mkDirTx->MutableMkDir()->SetName(name);
        SetApplyIf(*mkDirTx, applyIf);
        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus msgStatus = SendAndWaitCompletion(request, reply);
        Cout << PrintToString<NMsgBusProxy::TBusResponse>(reply.Get()) << Endl;
        UNIT_ASSERT_VALUES_EQUAL(msgStatus, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }
    void TClient::TestMkDir(const TString& parent, const TString& name, const TApplyIf& applyIf) {
        auto status = MkDir(parent, name, applyIf);
        UNIT_ASSERT_VALUES_EQUAL(status, NMsgBusProxy::EResponseStatus::MSTATUS_OK);
    }

    NMsgBusProxy::EResponseStatus TClient::RmDir(const TString& parent, const TString& name, const TApplyIf& applyIf) {
        NMsgBusProxy::TBusSchemeOperation* request(new NMsgBusProxy::TBusSchemeOperation());
        auto* mkDirTx = request->Record.MutableTransaction()->MutableModifyScheme();
        mkDirTx->SetWorkingDir(parent);
        mkDirTx->SetOperationType(NKikimrSchemeOp::ESchemeOpRmDir);
        mkDirTx->MutableDrop()->SetName(name);
        SetApplyIf(*mkDirTx, applyIf);
        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus msgStatus = SendAndWaitCompletion(request, reply);
        Cout << PrintToString<NMsgBusProxy::TBusResponse>(reply.Get()) << Endl;
        UNIT_ASSERT_VALUES_EQUAL(msgStatus, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }
    void TClient::TestRmDir(const TString& parent, const TString& name, const TApplyIf& applyIf) {
        auto status = RmDir(parent, name, applyIf);
        UNIT_ASSERT_VALUES_EQUAL(status, NMsgBusProxy::EResponseStatus::MSTATUS_OK);
    }

    NMsgBusProxy::EResponseStatus TClient::CreateSubdomain(const TString &parent, const TString &description) {
        NKikimrSubDomains::TSubDomainSettings subdomain;
        UNIT_ASSERT(::google::protobuf::TextFormat::ParseFromString(description, &subdomain));
        return CreateSubdomain(parent, subdomain);
    }

    NMsgBusProxy::EResponseStatus TClient::CreateSubdomain(const TString &parent, const NKikimrSubDomains::TSubDomainSettings &subdomain) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto *op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpCreateSubDomain);
        op->SetWorkingDir(parent);
        op->MutableSubDomain()->CopyFrom(subdomain);

        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::CreateExtSubdomain(const TString &parent, const TString &description) {
        NKikimrSubDomains::TSubDomainSettings subdomain;
        UNIT_ASSERT(::google::protobuf::TextFormat::ParseFromString(description, &subdomain));
        return CreateExtSubdomain(parent, subdomain);
    }

    NMsgBusProxy::EResponseStatus TClient::CreateExtSubdomain(const TString &parent, const NKikimrSubDomains::TSubDomainSettings &subdomain) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto *op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpCreateExtSubDomain);
        op->SetWorkingDir(parent);
        op->MutableSubDomain()->CopyFrom(subdomain);

        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::AlterUserAttributes(const TString &parent, const TString &name, const TVector<std::pair<TString, TString>>& addAttrs, const TVector<TString>& dropAttrs, const TApplyIf& applyIf) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto *op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetWorkingDir(parent);
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpAlterUserAttributes);
        auto userAttributes = op->MutableAlterUserAttributes();
        userAttributes->SetPathName(name);
        for (const auto& item: addAttrs) {
            auto attr = userAttributes->AddUserAttributes();
            attr->SetKey(item.first);
            attr->SetValue(item.second);
        }
        for (const auto& item: dropAttrs) {
            auto attr = userAttributes->AddUserAttributes();
            attr->SetKey(item);
        }
        SetApplyIf(*op, applyIf);
        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::AlterSubdomain(const TString &parent, const TString &description, TDuration timeout) {
        NKikimrSubDomains::TSubDomainSettings subdomain;
        UNIT_ASSERT(::google::protobuf::TextFormat::ParseFromString(description, &subdomain));
        return AlterSubdomain(parent, subdomain, timeout);
    }

    NMsgBusProxy::EResponseStatus TClient::AlterSubdomain(const TString &parent, const NKikimrSubDomains::TSubDomainSettings &subdomain, TDuration timeout) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto *op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpAlterSubDomain);
        op->SetWorkingDir(parent);
        op->MutableSubDomain()->CopyFrom(subdomain);

        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply, timeout);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::AlterExtSubdomain(const TString &parent, const NKikimrSubDomains::TSubDomainSettings &subdomain, TDuration timeout) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto *op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpAlterExtSubDomain);
        op->SetWorkingDir(parent);
        op->MutableSubDomain()->CopyFrom(subdomain);

        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply, timeout);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::DeleteSubdomain(const TString &parent, const TString &name) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto *op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpDropSubDomain);
        op->SetWorkingDir(parent);
        op->MutableDrop()->SetName(name);

        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::ForceDeleteSubdomain(const TString &parent, const TString &name) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto *op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpForceDropSubDomain);
        op->SetWorkingDir(parent);
        op->MutableDrop()->SetName(name);

        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::ForceDeleteUnsafe(const TString &parent, const TString &name) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto *op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpForceDropUnsafe);
        op->SetWorkingDir(parent);
        op->MutableDrop()->SetName(name);

        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::CreateUser(const TString& parent, const TCreateUserOption& options, const TString& userToken) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto* op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpAlterLogin);
        op->SetWorkingDir(parent);

        auto* createUser = op->MutableAlterLogin()->MutableCreateUser();
        createUser->SetUser(options.User);
        createUser->SetPassword(options.Password);
        createUser->SetCanLogin(options.CanLogin);
        request->Record.SetSecurityToken(userToken);

        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }
    void TClient::TestCreateUser(const TString& parent, const TCreateUserOption& options, const TString& userToken) {
        auto status = CreateUser(parent, options, userToken);
        UNIT_ASSERT_VALUES_EQUAL(status, NMsgBusProxy::EResponseStatus::MSTATUS_OK);
    }

    NMsgBusProxy::EResponseStatus TClient::CreateUser(const TString& parent, const TString& user, const TString& password, const TString& userToken) {
        return CreateUser(parent, {.User = user, .Password = password}, userToken);
    }
    void TClient::TestCreateUser(const TString& parent, const TString& user, const TString& password, const TString& userToken) {
        auto status = CreateUser(parent, user, password, userToken);
        UNIT_ASSERT_VALUES_EQUAL(status, NMsgBusProxy::EResponseStatus::MSTATUS_OK);
    }

    NMsgBusProxy::EResponseStatus TClient::ModifyUser(const TString& parent, const TModifyUserOption& options, const TString& userToken) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto* op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpAlterLogin);
        op->SetWorkingDir(parent);

        request->Record.SetSecurityToken(userToken);

        auto* modifyUser = op->MutableAlterLogin()->MutableModifyUser();
        modifyUser->SetUser(options.User);

        if (options.Password.has_value()) {
            modifyUser->SetPassword(options.Password.value());
        }

        if (options.CanLogin.has_value()) {
            modifyUser->SetCanLogin(options.CanLogin.value());
        }

        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }
    void TClient::TestModifyUser(const TString& parent, const TModifyUserOption& options, const TString& userToken) {
        auto status = ModifyUser(parent, options, userToken);
        UNIT_ASSERT_VALUES_EQUAL(status, NMsgBusProxy::EResponseStatus::MSTATUS_OK);
    }

    NMsgBusProxy::EResponseStatus TClient::CreateGroup(const TString& parent, const TString& group) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto* op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpAlterLogin);
        op->SetWorkingDir(parent);

        auto* createUser = op->MutableAlterLogin()->MutableCreateGroup();
        createUser->SetGroup(group);

        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }
    void TClient::TestCreateGroup(const TString& parent, const TString& group) {
        auto status = CreateGroup(parent, group);
        UNIT_ASSERT_VALUES_EQUAL(status, NMsgBusProxy::EResponseStatus::MSTATUS_OK);
    }

    NMsgBusProxy::EResponseStatus TClient::AddGroupMembership(const TString& parent, const TString& group, const TString& member) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto* op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpAlterLogin);
        op->SetWorkingDir(parent);

        auto* createUser = op->MutableAlterLogin()->MutableAddGroupMembership();
        createUser->SetGroup(group);
        createUser->SetMember(member);

        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }
    void TClient::TestAddGroupMembership(const TString& parent, const TString& group, const TString& member) {
        auto status = AddGroupMembership(parent, group, member);
        UNIT_ASSERT_VALUES_EQUAL(status, NMsgBusProxy::EResponseStatus::MSTATUS_OK);
    }

    NKikimrScheme::TEvLoginResult TClient::Login(TTestActorRuntime& runtime, const TString& user, const TString& password) {
        TActorId sender = runtime.AllocateEdgeActor();
        TAutoPtr<NSchemeShard::TEvSchemeShard::TEvLogin> evLogin(new NSchemeShard::TEvSchemeShard::TEvLogin());
        evLogin->Record.SetUser(user);
        evLogin->Record.SetPassword(password);

        if (auto ldapDomain = runtime.GetAppData().AuthConfig.GetLdapAuthenticationDomain(); user.EndsWith("@" + ldapDomain)) {
            evLogin->Record.SetExternalAuth(ldapDomain);
        }
        const ui64 schemeRoot = GetPatchedSchemeRoot(SchemeRoot, Domain, SupportsRedirect);
        ForwardToTablet(runtime, schemeRoot, sender, evLogin.Release());
        TAutoPtr<IEventHandle> handle;
        auto event = runtime.GrabEdgeEvent<NSchemeShard::TEvSchemeShard::TEvLoginResult>(handle);
        UNIT_ASSERT(event);
        return event->Record;
    }

    NMsgBusProxy::EResponseStatus TClient::CreateTable(const TString& parent, const NKikimrSchemeOp::TTableDescription &table, TDuration timeout) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto *op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpCreateTable);
        op->SetWorkingDir(parent);
        op->MutableCreateTable()->CopyFrom(table);
        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply, timeout);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::CreateTableWithUniformShardedIndex(const TString& parent,
        const NKikimrSchemeOp::TTableDescription &table, const TString& indexName, const TVector<TString> indexColumns,
        NKikimrSchemeOp::EIndexType type, const TVector<TString> dataColumns, TDuration timeout)
    {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto *op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpCreateIndexedTable);
        op->SetWorkingDir(parent);

        NKikimrSchemeOp::TTableDescription* tableDesc = op->MutableCreateIndexedTable()->MutableTableDescription();
        tableDesc->CopyFrom(table);

        {
            auto indexDesc = op->MutableCreateIndexedTable()->MutableIndexDescription()->Add();
            indexDesc->SetName(indexName);
            for (const auto& c : indexColumns) {
                indexDesc->AddKeyColumnNames(c);
            }
            for (const auto& c : dataColumns) {
                indexDesc->AddDataColumnNames(c);
            }

            indexDesc->SetType(type);
            indexDesc->AddIndexImplTableDescriptions()->SetUniformPartitionsCount(16);
        }

        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply, timeout);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::SplitTable(const TString& table, ui64 datashardId, ui64 border, TDuration timeout) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpSplitMergeTablePartitions);
        auto split = op->MutableSplitMergeTablePartitions();
        split->SetTablePath(table);
        split->AddSourceTabletId(datashardId);
        split->AddSplitBoundary()->MutableKeyPrefix()->AddTuple()->MutableOptional()->SetUint64(border);

        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply, timeout);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::CopyTable(const TString &parent, const TString &name, const TString &src) {
        NKikimrSchemeOp::TTableDescription table;
        table.SetName(name);
        table.SetCopyFromTable(src);
        return CreateTable(parent, table, TDuration::Seconds(5000));
    }

    NMsgBusProxy::EResponseStatus TClient::ConsistentCopyTables(TVector<std::pair<TString, TString>> desc, TDuration timeout) {
        NKikimrSchemeOp::TConsistentTableCopyingConfig coping;
        for (auto& task: desc) {
            auto* item = coping.AddCopyTableDescriptions();
            item->SetSrcPath(std::move(task.first));
            item->SetDstPath(std::move(task.second));
        }

        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto *op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpCreateConsistentCopyTables);
        *op->MutableCreateConsistentCopyTables() = std::move(coping);

        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply, timeout);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::CreateTable(const TString& parent, const TString& scheme, TDuration timeout) {
        NKikimrSchemeOp::TTableDescription table;
        bool parseOk = ::google::protobuf::TextFormat::ParseFromString(scheme, &table);
        UNIT_ASSERT(parseOk);
        return CreateTable(parent, table, timeout);
    }

    NMsgBusProxy::EResponseStatus TClient::CreateKesus(const TString& parent, const TString& name) {
        auto* request = new NMsgBusProxy::TBusSchemeOperation();
        auto* tx = request->Record.MutableTransaction()->MutableModifyScheme();
        tx->SetOperationType(NKikimrSchemeOp::ESchemeOpCreateKesus);
        tx->SetWorkingDir(parent);
        tx->MutableKesus()->SetName(name);
        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus msgStatus = SendAndWaitCompletion(request, reply);
        UNIT_ASSERT_VALUES_EQUAL(msgStatus, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::DeleteKesus(const TString& parent, const TString& name) {
        auto* request = new NMsgBusProxy::TBusSchemeOperation();
        auto* tx = request->Record.MutableTransaction()->MutableModifyScheme();
        tx->SetOperationType(NKikimrSchemeOp::ESchemeOpDropKesus);
        tx->SetWorkingDir(parent);
        tx->MutableDrop()->SetName(name);
        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus msgStatus = SendAndWaitCompletion(request, reply);
        UNIT_ASSERT_VALUES_EQUAL(msgStatus, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::CreateOlapStore(const TString& parent, const TString& scheme) {
        NKikimrSchemeOp::TColumnStoreDescription store;
        bool parseOk = ::google::protobuf::TextFormat::ParseFromString(scheme, &store);
        UNIT_ASSERT(parseOk);
        return CreateOlapStore(parent, store);
    }

    NMsgBusProxy::EResponseStatus TClient::CreateOlapStore(const TString& parent,
                                                           const NKikimrSchemeOp::TColumnStoreDescription& store) {
        auto request = std::make_unique<NMsgBusProxy::TBusSchemeOperation>();
        auto* op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpCreateColumnStore);
        op->SetWorkingDir(parent);
        op->MutableCreateColumnStore()->CopyFrom(store);
        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse& response = dynamic_cast<NMsgBusProxy::TBusResponse*>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::CreateColumnTable(const TString& parent, const TString& scheme) {
        NKikimrSchemeOp::TColumnTableDescription table;
        bool parseOk = ::google::protobuf::TextFormat::ParseFromString(scheme, &table);
        UNIT_ASSERT(parseOk);
        return CreateColumnTable(parent, table);
    }

    NMsgBusProxy::EResponseStatus TClient::CreateColumnTable(const TString& parent,
                                                           const NKikimrSchemeOp::TColumnTableDescription& table) {
        auto request = std::make_unique<NMsgBusProxy::TBusSchemeOperation>();
        auto* op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpCreateColumnTable);
        op->SetWorkingDir(parent);
        op->MutableCreateColumnTable()->CopyFrom(table);
        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse& response = dynamic_cast<NMsgBusProxy::TBusResponse*>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::CreateTopic(const TString& parent,
                                                        const NKikimrSchemeOp::TPersQueueGroupDescription& topic) {
        auto request = std::make_unique<NMsgBusProxy::TBusSchemeOperation>();
        auto* op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpCreatePersQueueGroup);
        op->SetWorkingDir(parent);
        op->MutableCreatePersQueueGroup()->CopyFrom(topic);
        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse& response = dynamic_cast<NMsgBusProxy::TBusResponse*>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::CreateSolomon(const TString& parent, const TString& name, ui32 parts, ui32 channelProfile) {
        auto* request = new NMsgBusProxy::TBusSchemeOperation();
        auto* tx = request->Record.MutableTransaction()->MutableModifyScheme();
        tx->SetOperationType(NKikimrSchemeOp::ESchemeOpCreateSolomonVolume);
        tx->SetWorkingDir(parent);
        tx->MutableCreateSolomonVolume()->SetName(name);
        tx->MutableCreateSolomonVolume()->SetPartitionCount(parts);
        tx->MutableCreateSolomonVolume()->SetChannelProfileId(channelProfile);
        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus msgStatus = SendAndWaitCompletion(request, reply);
        UNIT_ASSERT_VALUES_EQUAL(msgStatus, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    TAutoPtr<NMsgBusProxy::TBusResponse> TClient::AlterTable(const TString& parent, const NKikimrSchemeOp::TTableDescription& alter, const TString& userToken) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto *op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpAlterTable);
        op->SetWorkingDir(parent);
        op->MutableAlterTable()->CopyFrom(alter);
        TAutoPtr<NBus::TBusMessage> reply;
        if (userToken) {
            request->Record.SetSecurityToken(userToken);
        }
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        return dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Release());
    }

    TAutoPtr<NMsgBusProxy::TBusResponse> TClient::MoveIndex(const TString& table, const TString& src, const TString& dst, bool allowOverwrite, const TString& userToken) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto *op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpMoveIndex);
        auto descr = op->MutableMoveIndex();
        descr->SetTablePath(table);
        descr->SetSrcPath(src);
        descr->SetDstPath(dst);
        descr->SetAllowOverwrite(allowOverwrite);
        TAutoPtr<NBus::TBusMessage> reply;
        if (userToken) {
            request->Record.SetSecurityToken(userToken);
        }
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        return dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Release());
    }

    TAutoPtr<NMsgBusProxy::TBusResponse> TClient::AlterTable(const TString& parent, const TString& alter, const TString& userToken) {
        NKikimrSchemeOp::TTableDescription table;
        bool parseOk = ::google::protobuf::TextFormat::ParseFromString(alter, &table);
        UNIT_ASSERT(parseOk);
        return AlterTable(parent, table, userToken);
    }

    NMsgBusProxy::EResponseStatus TClient::AlterTable(const TString& parent, const NKikimrSchemeOp::TTableDescription& alter) {
        TAutoPtr<NMsgBusProxy::TBusResponse> reply = AlterTable(parent, alter, TString());
        const NKikimrClient::TResponse &response = reply->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::AlterTable(const TString& parent, const TString& alter) {
        NKikimrSchemeOp::TTableDescription table;
        bool parseOk = ::google::protobuf::TextFormat::ParseFromString(alter, &table);
        UNIT_ASSERT(parseOk);
        return AlterTable(parent, table);
    }

    NMsgBusProxy::EResponseStatus TClient::StoreTableBackup(const TString& parent, const NKikimrSchemeOp::TBackupTask& task) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto *op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpBackup);
        op->SetWorkingDir(parent);
        op->MutableBackup()->CopyFrom(task);
        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::DeleteTable(const TString& parent, const TString& name) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto *op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpDropTable);
        op->SetWorkingDir(parent);
        op->MutableDrop()->SetName(name);
        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    NMsgBusProxy::EResponseStatus TClient::DeleteTopic(const TString& parent, const TString& name) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto *op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpDropPersQueueGroup);
        op->SetWorkingDir(parent);
        op->MutableDrop()->SetName(name);
        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    TAutoPtr<NMsgBusProxy::TBusResponse> TClient::TryDropPersQueueGroup(const TString& parent, const TString& name) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto * op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpDropPersQueueGroup);
        op->SetWorkingDir(parent);
        op->MutableDrop()->SetName(name);
        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SyncCall(request, reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        NMsgBusProxy::TBusResponse* res = dynamic_cast<NMsgBusProxy::TBusResponse*>(reply.Release());
        UNIT_ASSERT(res);
        return res;
    }

    NMsgBusProxy::EResponseStatus TClient::WaitCreateTx(TTestActorRuntime* runtime, const TString& path, TDuration timeout) {
        TAutoPtr<NSchemeShard::TEvSchemeShard::TEvDescribeScheme> request(new NSchemeShard::TEvSchemeShard::TEvDescribeScheme());
        request->Record.SetPath(path);
        const ui64 schemeRoot = GetPatchedSchemeRoot(SchemeRoot, Domain, SupportsRedirect);
        TActorId sender = runtime->AllocateEdgeActor(0);
        ForwardToTablet(*runtime, schemeRoot, sender, request.Release(), 0);

        TAutoPtr<IEventHandle> handle;
        runtime->GrabEdgeEvent<NSchemeShard::TEvSchemeShard::TEvDescribeSchemeResult>(handle);
        auto& record = handle->Get<NSchemeShard::TEvSchemeShard::TEvDescribeSchemeResult>()->GetRecord();
        //Cerr << record.DebugString() << Endl;

        UNIT_ASSERT_VALUES_EQUAL(record.GetStatus(), NKikimrScheme::StatusSuccess);
        auto& descr = record.GetPathDescription().GetSelf();
        TAutoPtr<NBus::TBusMessage> reply;
        auto msgStatus = WaitCompletion(descr.GetCreateTxId(), descr.GetSchemeshardId(), descr.GetPathId(), reply, timeout);
        Cout << PrintToString<NMsgBusProxy::TBusResponse>(reply.Get()) << Endl;
        UNIT_ASSERT_VALUES_EQUAL(msgStatus, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }

    TAutoPtr<NMsgBusProxy::TBusResponse> TClient::LsImpl(const TString& path) {
        if (Verbose) {
            Cerr << "TClient::Ls request: " << path << Endl;
        }

        TAutoPtr<NMsgBusProxy::TBusSchemeDescribe> request(new NMsgBusProxy::TBusSchemeDescribe());
        request->Record.SetPath(path);
        request->Record.MutableOptions()->SetShowPrivateTable(true);
        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus msgStatus = SendWhenReady(request, reply);
        UNIT_ASSERT_VALUES_EQUAL(msgStatus, NBus::MESSAGE_OK);

        if (Verbose) {
            Cerr << "TClient::Ls response: " << PrintToString<NMsgBusProxy::TBusResponse>(reply.Get()) << Endl;
        }

        return dynamic_cast<NMsgBusProxy::TBusResponse*>(reply.Release());
    }

    TAutoPtr<NMsgBusProxy::TBusResponse> TClient::Ls(const TString& path) {
        return LsImpl(path).Release();
    }

    TClient::TPathVersion TClient::ExtractPathVersion(const TAutoPtr<NMsgBusProxy::TBusResponse>& describe) {
        UNIT_ASSERT(describe.Get());
        auto& record = describe->Record;
        UNIT_ASSERT_VALUES_EQUAL(record.GetStatus(), NMsgBusProxy::MSTATUS_OK);
        UNIT_ASSERT_VALUES_EQUAL(record.GetSchemeStatus(), NKikimrScheme::StatusSuccess);

        UNIT_ASSERT(record.HasPathDescription());
        auto& descr = record.GetPathDescription();

        UNIT_ASSERT(descr.HasSelf());
        auto& self = descr.GetSelf();

        return TPathVersion{self.GetSchemeshardId(), self.GetPathId(), self.GetPathVersion()};
    }

    TVector<ui64> TClient::ExtractTableShards(const TAutoPtr<NMsgBusProxy::TBusResponse>& describe) {
        UNIT_ASSERT(describe.Get());
        NKikimrClient::TResponse& record = describe->Record;
        UNIT_ASSERT_VALUES_EQUAL(record.GetStatus(), NMsgBusProxy::MSTATUS_OK);
        UNIT_ASSERT_VALUES_EQUAL(record.GetSchemeStatus(), NKikimrScheme::StatusSuccess);

        UNIT_ASSERT(record.HasPathDescription());
        auto& descr = record.GetPathDescription();
        UNIT_ASSERT(descr.TablePartitionsSize() > 0);
        auto& parts = descr.GetTablePartitions();

        TVector<ui64> shards;
        for (const auto& part : parts) {
            shards.emplace_back(part.GetDatashardId());
        }
        return shards;
    }

    void TClient::RefreshPathCache(TTestActorRuntime* runtime, const TString& path, ui32 nodeIdx) {
        TActorId sender = runtime->AllocateEdgeActor(nodeIdx);
        auto request = MakeHolder<NSchemeCache::TSchemeCacheNavigate>();
        auto& entry = request->ResultSet.emplace_back();
        entry.Path = SplitPath(path);
        entry.Operation = NSchemeCache::TSchemeCacheNavigate::OpPath;
        runtime->Send(new IEventHandle(
            MakeSchemeCacheID(),
            sender,
            new TEvTxProxySchemeCache::TEvNavigateKeySet(request.Release())),
            nodeIdx);
        auto ev = runtime->GrabEdgeEvent<TEvTxProxySchemeCache::TEvNavigateKeySetResult>(sender);
        Y_ABORT_UNLESS(ev);
    }

    NMsgBusProxy::EResponseStatus TClient::ModifyOwner(const TString& parent, const TString& name, const TString& owner) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto *op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpModifyACL);
        op->SetWorkingDir(parent);
        op->MutableModifyACL()->SetName(name);
        op->MutableModifyACL()->SetNewOwner(owner);
        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }
    void TClient::TestModifyOwner(const TString& parent, const TString& name, const TString& owner) {
        auto status = ModifyOwner(parent, name, owner);
        UNIT_ASSERT_VALUES_EQUAL(status, NMsgBusProxy::EResponseStatus::MSTATUS_OK);
    }

    NMsgBusProxy::EResponseStatus TClient::ModifyACL(const TString& parent, const TString& name, const TString& acl) {
        TAutoPtr<NMsgBusProxy::TBusSchemeOperation> request(new NMsgBusProxy::TBusSchemeOperation());
        auto *op = request->Record.MutableTransaction()->MutableModifyScheme();
        op->SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpModifyACL);
        op->SetWorkingDir(parent);
        op->MutableModifyACL()->SetName(name);
        op->MutableModifyACL()->SetDiffACL(acl);
        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SendAndWaitCompletion(request.Release(), reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        return (NMsgBusProxy::EResponseStatus)response.GetStatus();
    }
    void TClient::TestModifyACL(const TString& parent, const TString& name, const TString& acl) {
        auto status = ModifyACL(parent, name, acl);
        UNIT_ASSERT_VALUES_EQUAL(status, NMsgBusProxy::EResponseStatus::MSTATUS_OK);
    }

    NMsgBusProxy::EResponseStatus TClient::Grant(const TString& parent, const TString& name, const TString& subject, NACLib::EAccessRights rights) {
        NACLib::TDiffACL acl;
        acl.AddAccess(NACLib::EAccessType::Allow, rights, subject);
        return ModifyACL(parent, name, acl.SerializeAsString());
    }
    void TClient::TestGrant(const TString& parent, const TString& name, const TString& subject, NACLib::EAccessRights rights) {
        auto status = Grant(parent, name, subject, rights);
        UNIT_ASSERT_VALUES_EQUAL(status, NMsgBusProxy::EResponseStatus::MSTATUS_OK);
    }

    NMsgBusProxy::EResponseStatus TClient::GrantConnect(const TString& subject) {
        return Grant("/", DomainName, subject, NACLib::EAccessRights::ConnectDatabase);
    }
    void TClient::TestGrantConnect(const TString& subject) {
        auto status = GrantConnect(subject);
        UNIT_ASSERT_VALUES_EQUAL(status, NMsgBusProxy::EResponseStatus::MSTATUS_OK);
    }

    TAutoPtr<NMsgBusProxy::TBusResponse> TClient::HiveCreateTablet(ui32 domainUid, ui64 owner, ui64 owner_index, TTabletTypes::EType tablet_type,
                                                               const TVector<ui32>& allowed_node_ids,
                                                               const TVector<TSubDomainKey>& allowed_domains,
                                                               const TChannelsBindings& bindings)
    {
        TAutoPtr<NMsgBusProxy::TBusHiveCreateTablet> request(new NMsgBusProxy::TBusHiveCreateTablet());
        NKikimrClient::THiveCreateTablet& record = request->Record;
        record.SetDomainUid(domainUid);
        auto *cmdCreate = record.AddCmdCreateTablet();
        cmdCreate->SetOwnerId(owner);
        cmdCreate->SetOwnerIdx(owner_index);
        cmdCreate->SetTabletType(tablet_type);
        for (ui32 node_id: allowed_node_ids) {
            cmdCreate->AddAllowedNodeIDs(node_id);
        }
        for (auto& domain_id: allowed_domains) {
            *cmdCreate->AddAllowedDomains() = domain_id;
        }
        if (!bindings.empty()) {
            for (auto& binding: bindings) {
                *cmdCreate->AddBindedChannels() = binding;
            }
        } else {
            UNIT_ASSERT(!StoragePoolTypes.empty());
            TString storagePool = StoragePoolTypes.begin()->second.GetName();
            cmdCreate->AddBindedChannels()->SetStoragePoolName(storagePool); // 0
            cmdCreate->AddBindedChannels()->SetStoragePoolName(storagePool); // 1
            cmdCreate->AddBindedChannels()->SetStoragePoolName(storagePool); // 2
        }
        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus status = SyncCall(request, reply);
        UNIT_ASSERT_VALUES_EQUAL(status, NBus::MESSAGE_OK);
        NMsgBusProxy::TBusResponse* res = dynamic_cast<NMsgBusProxy::TBusResponse*>(reply.Release());
        UNIT_ASSERT(res);
        return res;
    }

    NKikimrScheme::TEvDescribeSchemeResult TClient::Describe(TTestActorRuntime* runtime, const TString& path, ui64 tabletId, bool showPrivateTable) {
        TAutoPtr<NSchemeShard::TEvSchemeShard::TEvDescribeScheme> request(new NSchemeShard::TEvSchemeShard::TEvDescribeScheme());
        request->Record.SetPath(path);
        request->Record.MutableOptions()->SetShowPrivateTable(showPrivateTable);
        const ui64 schemeRoot = GetPatchedSchemeRoot(tabletId, Domain, SupportsRedirect);
        TActorId sender = runtime->AllocateEdgeActor(0);
        ForwardToTablet(*runtime, schemeRoot, sender, request.Release(), 0);

        TAutoPtr<IEventHandle> handle;
        runtime->GrabEdgeEvent<NSchemeShard::TEvSchemeShard::TEvDescribeSchemeResult>(handle);
        auto& record = handle->Get<NSchemeShard::TEvSchemeShard::TEvDescribeSchemeResult>()->GetRecord();

        if (auto schemeShardId = record.GetPathDescription().GetDomainDescription().GetProcessingParams().GetSchemeShard(); schemeShardId && schemeShardId != tabletId) {
            return Describe(runtime, path, schemeShardId);
        } else {
            return record;
        }
    }

    TString TClient::CreateStoragePool(const TString& poolKind, const TString& partOfName, ui32 groups) {
        Y_ABORT_UNLESS(StoragePoolTypes.contains(poolKind));
        const TString poolName = Sprintf("name_%s_kind_%s", partOfName.c_str(), poolKind.c_str());
        const ui64 poolId = THash<TString>()(poolName);

        NKikimrBlobStorage::TDefineStoragePool storagePool = StoragePoolTypes.at(poolKind);
        Y_ABORT_UNLESS(storagePool.GetKind() == poolKind);
        storagePool.SetStoragePoolId(poolId);
        storagePool.SetName(poolName);
        storagePool.SetNumGroups(groups);

        TAutoPtr<NMsgBusProxy::TBusBlobStorageConfigRequest> request(new NMsgBusProxy::TBusBlobStorageConfigRequest());
        request->Record.MutableRequest()->AddCommand()->MutableDefineStoragePool()->CopyFrom(storagePool);
        request->Record.SetDomain(Domain);
        if (SecurityToken) {
            request->Record.SetSecurityToken(SecurityToken);
        }

        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus msgStatus = SendWhenReady(request, reply);

        UNIT_ASSERT_VALUES_EQUAL(msgStatus, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        Y_ABORT_UNLESS(response.HasBlobStorageConfigResponse() && response.GetBlobStorageConfigResponse().GetSuccess());
        UNIT_ASSERT((NMsgBusProxy::EResponseStatus)response.GetStatus());

        return poolName;
    }

    NKikimrBlobStorage::TDefineStoragePool TClient::DescribeStoragePool(const TString& name) {
        TAutoPtr<NMsgBusProxy::TBusBlobStorageConfigRequest> readRequest(new NMsgBusProxy::TBusBlobStorageConfigRequest());
        readRequest->Record.SetDomain(Domain);
        auto readParam = readRequest->Record.MutableRequest()->AddCommand()->MutableReadStoragePool();
        readParam->SetBoxId(TServerSettings::BOX_ID);
        readParam->AddName(name);

        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus msgStatus = SendWhenReady(readRequest, reply);

        Cerr << PrintToString<NMsgBusProxy::TBusResponse>(reply.Get()) << Endl;
        UNIT_ASSERT_VALUES_EQUAL(msgStatus, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &response = dynamic_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        UNIT_ASSERT(response.HasBlobStorageConfigResponse() && response.GetBlobStorageConfigResponse().GetSuccess());
        UNIT_ASSERT((NMsgBusProxy::EResponseStatus)response.GetStatus());
        UNIT_ASSERT(response.GetBlobStorageConfigResponse().StatusSize() > 0);
        auto status = response.GetBlobStorageConfigResponse().GetStatus(0);
        UNIT_ASSERT(status.StoragePoolSize() > 0);

        auto storagePool = status.GetStoragePool(0);
        UNIT_ASSERT(name == storagePool.GetName());
        UNIT_ASSERT(TServerSettings::BOX_ID == storagePool.GetBoxId());

        return storagePool;
    }

    void TClient::RemoveStoragePool(const TString& name) {
        auto storagePool = DescribeStoragePool(name);

        TAutoPtr<NMsgBusProxy::TBusBlobStorageConfigRequest> deleteRequest(new NMsgBusProxy::TBusBlobStorageConfigRequest());
        deleteRequest->Record.SetDomain(Domain);
        auto deleteParam = deleteRequest->Record.MutableRequest()->AddCommand()->MutableDeleteStoragePool();
        deleteParam->SetBoxId(TServerSettings::BOX_ID);
        deleteParam->SetStoragePoolId(storagePool.GetStoragePoolId());
        deleteParam->SetItemConfigGeneration(storagePool.GetItemConfigGeneration());

        TAutoPtr<NBus::TBusMessage> replyDelete;
        NBus::EMessageStatus msgStatus = SendWhenReady(deleteRequest, replyDelete);

        Cout << PrintToString<NMsgBusProxy::TBusResponse>(replyDelete.Get()) << Endl;
        UNIT_ASSERT_VALUES_EQUAL(msgStatus, NBus::MESSAGE_OK);
        const NKikimrClient::TResponse &responseDelete = dynamic_cast<NMsgBusProxy::TBusResponse *>(replyDelete.Get())->Record;
        UNIT_ASSERT(responseDelete.HasBlobStorageConfigResponse() && responseDelete.GetBlobStorageConfigResponse().GetSuccess());
        UNIT_ASSERT((NMsgBusProxy::EResponseStatus)responseDelete.GetStatus());
    }

    bool TClient::Compile(const TString &mkql, TString &compiled) {
        TAutoPtr<NMsgBusProxy::TBusRequest> request = new NMsgBusProxy::TBusRequest();
        auto* mkqlTx = request->Record.MutableTransaction()->MutableMiniKQLTransaction();
        mkqlTx->MutableProgram()->SetText(mkql);
        mkqlTx->SetFlatMKQL(true);
        mkqlTx->SetMode(NKikimrTxUserProxy::TMiniKQLTransaction::COMPILE);

        TAutoPtr<NBus::TBusMessage> reply;
        NBus::EMessageStatus msgStatus = SyncCall(request, reply);
        UNIT_ASSERT_EQUAL(msgStatus, NBus::MESSAGE_OK);

        const NKikimrClient::TResponse &response = static_cast<NMsgBusProxy::TBusResponse *>(reply.Get())->Record;
        if (!response.HasMiniKQLCompileResults())
            return false;

        const auto &compileRes = response.GetMiniKQLCompileResults();
        if (compileRes.ProgramCompileErrorsSize()) {
            NYql::TIssues issues;
            NYql::IssuesFromMessage(compileRes.GetProgramCompileErrors(), issues);
            TStringStream err;
            issues.PrintTo(err);
            Cerr << "error: " << err.Str() << Endl;

            return false;
        }

        compiled = compileRes.GetCompiledProgram();
        return true;
    }

    ui32 TClient::FlatQueryRaw(TTestActorRuntime* runtime, const TString &query, TFlatQueryOptions& opts, NKikimrClient::TResponse& response, int retryCnt) {
        while (retryCnt--) {
            THolder<NMsgBusProxy::TBusRequest> request = MakeHolder<NMsgBusProxy::TBusRequest>();
            {
                auto* mkqlTx = request->Record.MutableTransaction()->MutableMiniKQLTransaction();
                if (opts.IsQueryCompiled)
                    mkqlTx->MutableProgram()->SetBin(query);
                else
                    mkqlTx->MutableProgram()->SetText(query);

                if (opts.Params)
                    mkqlTx->MutableParams()->SetText(opts.Params);
                mkqlTx->SetFlatMKQL(true);
                if (opts.CollectStats)
                    mkqlTx->SetCollectStats(true);
            }

            TAutoPtr<NBus::TBusMessage> reply;
            std::unique_ptr<NMsgBusProxy::TBusResponse> result = TTestMkqlInvoker(runtime, std::move(request)).InvokeMkql();

            NMsgBusProxy::TBusResponse * ret = result.get();
            ui32 responseStatus = ret->Record.GetStatus();
            if (responseStatus == NMsgBusProxy::MSTATUS_NOTREADY ||
                responseStatus == NMsgBusProxy::MSTATUS_TIMEOUT ||
                responseStatus == NMsgBusProxy::MSTATUS_INPROGRESS)
                continue;

            response.Swap(&result.get()->Record);
            break;
        }

        UNIT_ASSERT(retryCnt > 0);
        return response.GetStatus();
    }

    bool TClient::FlatQuery(TTestActorRuntime* runtime, const TString &query, TFlatQueryOptions& opts, NKikimrMiniKQL::TResult &result, const NKikimrClient::TResponse& expectedResponse) {
        NKikimrClient::TResponse response;
        if (expectedResponse.HasStatus() && expectedResponse.GetStatus() == NMsgBusProxy::MSTATUS_OK) {
            // Client is expecting OK, retry REJECTED replies during restarts and splits
            for (int i = 0; i < 5; ++i) {
                if (i != 0) {
                    response.Clear();
                    Cerr << "Retrying rejected query..." << Endl;
                }
                FlatQueryRaw(runtime, query, opts, response);
                if (response.GetStatus() != NMsgBusProxy::MSTATUS_REJECTED) {
                    break;
                }
            }
        } else {
            FlatQueryRaw(runtime, query, opts, response);
        }

        if (!response.GetDataShardErrors().empty()) {
            Cerr << "DataShardErrors:" << Endl << response.GetDataShardErrors() << Endl;
        }
        if (!response.GetMiniKQLErrors().empty()) {
            Cerr << "MiniKQLErrors:" << Endl << response.GetMiniKQLErrors() << Endl;
        }
        if (response.HasProxyErrorCode()) {
            if (response.GetProxyErrorCode() != TEvTxUserProxy::TResultStatus::ExecComplete)
                Cerr << "proxy error code: " << static_cast<TEvTxUserProxy::TResultStatus::EStatus>(response.GetProxyErrorCode()) << Endl;
            if (expectedResponse.HasProxyErrorCode()) {
                UNIT_ASSERT_VALUES_EQUAL(response.GetProxyErrorCode(), expectedResponse.GetProxyErrorCode());
            }
        }
        if (response.HasProxyErrors()) {
            Cerr << "proxy errors: " << response.GetProxyErrors() << Endl;
        }
        if (response.UnresolvedKeysSize() > 0) {
            for (size_t i = 0, end = response.UnresolvedKeysSize(); i < end; ++i) {
                Cerr << response.GetUnresolvedKeys(i) << Endl;
            }
        }
        if (response.HasMiniKQLCompileResults()) {
            const auto &compileRes = response.GetMiniKQLCompileResults();
            if (compileRes.ProgramCompileErrorsSize()) {
                NYql::TIssues issues;
                NYql::IssuesFromMessage(compileRes.GetProgramCompileErrors(), issues);
                TStringStream err;
                issues.PrintTo(err);
                Cerr << "error: " << err.Str() << Endl;
            }
            if (compileRes.ParamsCompileErrorsSize()) {
                NYql::TIssues issues;
                NYql::IssuesFromMessage(compileRes.GetParamsCompileErrors(), issues);
                TStringStream err;
                issues.PrintTo(err);
                Cerr << "error: " << err.Str() << Endl;
            }
        }
        if (response.HasHadFollowerReads() && response.GetHadFollowerReads()) {
            Cerr << "had follower reads" << Endl;
        }

        if (expectedResponse.HasStatus()) {
            UNIT_ASSERT_VALUES_EQUAL(response.GetStatus(), expectedResponse.GetStatus());
        }
        if (expectedResponse.GetStatus() != NMsgBusProxy::MSTATUS_OK)
            return false;

        UNIT_ASSERT(response.HasTxId());
        UNIT_ASSERT(response.GetExecutionEngineResponseStatus() == ui32(NMiniKQL::IEngineFlat::EStatus::Complete)
            || response.GetExecutionEngineResponseStatus() == ui32(NMiniKQL::IEngineFlat::EStatus::Aborted));

        if (response.HasExecutionEngineEvaluatedResponse()) {
            result.Swap(response.MutableExecutionEngineEvaluatedResponse());
        }

        return response.GetExecutionEngineResponseStatus() == ui32(NMiniKQL::IEngineFlat::EStatus::Complete);
    }

    bool TClient::FlatQuery(TTestActorRuntime* runtime, const TString &query, TFlatQueryOptions& opts, NKikimrMiniKQL::TResult &result, ui32 expectedStatus) {
        NKikimrClient::TResponse expectedResponse;
        expectedResponse.SetStatus(expectedStatus);
        return FlatQuery(runtime, query, opts, result, expectedResponse);
    }

    bool TClient::FlatQuery(TTestActorRuntime* runtime, const TString& mkql, NKikimrMiniKQL::TResult& result) {
        TFlatQueryOptions opts;
        return FlatQuery(runtime, mkql, opts, result);
    }

    TString TClient::SendTabletMonQuery(TTestActorRuntime* runtime, ui64 tabletId, TString query) {
        TActorId sender = runtime->AllocateEdgeActor(0);
        ForwardToTablet(*runtime, tabletId, sender, new NActors::NMon::TEvRemoteHttpInfo(query), 0);
        TAutoPtr<IEventHandle> handle;
        // Timeout for DEBUG purposes only
        runtime->GrabEdgeEvent<NMon::TEvRemoteJsonInfoRes>(handle);
        TString res = handle->Get<NMon::TEvRemoteJsonInfoRes>()->Json;
        Cerr << res << Endl;
        return res;
    }

    TString TClient::MarkNodeInHive(TTestActorRuntime* runtime, ui32 nodeIdx, bool up) {
        ui32 nodeId = runtime->GetNodeId(nodeIdx);
        ui64 hive = ChangeStateStorage(Tests::Hive, Domain);
        TInstant deadline = TInstant::Now() + TIMEOUT;
        while (TInstant::Now() <= deadline) {
            TString res = SendTabletMonQuery(runtime, hive, TString("/app?page=SetDown&node=") + ToString(nodeId) + "&down=" + (up ? "0" : "1"));
            if (!res.empty() && !res.Contains("Error"))
                return res;

        }
        UNIT_ASSERT_C(false, "Failed to mark node in hive");
        return TString();
    }

    TString TClient::KickNodeInHive(TTestActorRuntime* runtime, ui32 nodeIdx) {
        ui32 nodeId = runtime->GetNodeId(nodeIdx);
        ui64 hive = ChangeStateStorage(Tests::Hive, Domain);
        return SendTabletMonQuery(runtime, hive, TString("/app?page=KickNode&node=") + ToString(nodeId));
    }

    bool TClient::WaitForTabletAlive(TTestActorRuntime* runtime, ui64 tabletId, bool leader, TDuration timeout) {
        TActorId edge = runtime->AllocateEdgeActor();
        NTabletPipe::TClientConfig clientConfig;
        clientConfig.AllowFollower = !leader;
        clientConfig.ForceFollower = !leader;
        clientConfig.RetryPolicy = NTabletPipe::TClientRetryPolicy::WithRetries();
        TActorId pipeClient = runtime->Register(NTabletPipe::CreateClient(edge, tabletId, clientConfig));
        TAutoPtr<IEventHandle> handle;
        const TInstant deadline = TInstant::Now() + timeout;
        bool res = false;

        try {
            while (TInstant::Now() <= deadline) {
                TEvTabletPipe::TEvClientConnected* ev = runtime->GrabEdgeEvent<TEvTabletPipe::TEvClientConnected>(handle, deadline - TInstant::Now());
                if (!ev) {
                    break;
                }
                if (ev->ClientId == pipeClient && ev->TabletId == tabletId) {
                    res = (ev->Status == NKikimrProto::OK);
                    break;
                }
            }
        } catch (TEmptyEventQueueException &) {}

        runtime->Send(new IEventHandle(pipeClient, TActorId(), new TEvents::TEvPoisonPill()));
        return res;
    }

    bool TClient::WaitForTabletDown(TTestActorRuntime* runtime, ui64 tabletId, bool leader, TDuration timeout) {
        TActorId edge = runtime->AllocateEdgeActor();
        NTabletPipe::TClientConfig clientConfig;
        clientConfig.AllowFollower = !leader;
        clientConfig.ForceFollower = !leader;
        clientConfig.RetryPolicy = {
            .RetryLimitCount = 5,
            .MinRetryTime = TDuration::MilliSeconds(500),
            .MaxRetryTime = TDuration::Seconds(1),
            .BackoffMultiplier = 2,
        };
        TActorId pipeClient = runtime->Register(NTabletPipe::CreateClient(edge, tabletId, clientConfig));
        TInstant deadline = TInstant::Now() + timeout;

        bool res = false;

        try {
            while (TInstant::Now() <= deadline) {
                TAutoPtr<IEventHandle> handle;
                auto result = runtime->GrabEdgeEvents<TEvTabletPipe::TEvClientConnected, TEvTabletPipe::TEvClientDestroyed>(handle, deadline - TInstant::Now());
                if (handle && handle->Recipient == edge && handle->Sender == pipeClient) {
                    if (std::get<TEvTabletPipe::TEvClientDestroyed*>(result) != nullptr)
                    {
                        TEvTabletPipe::TEvClientDestroyed* event = std::get<TEvTabletPipe::TEvClientDestroyed*>(result);
                        if (event->TabletId == tabletId) {
                            res = true;
                            break;
                        }
                    }
                    if (std::get<TEvTabletPipe::TEvClientConnected*>(result) != nullptr)
                    {
                        TEvTabletPipe::TEvClientConnected* event = std::get<TEvTabletPipe::TEvClientConnected*>(result);
                        if (event->TabletId == tabletId && event->Status != NKikimrProto::OK) {
                            res = true;
                            break;
                        }
                    }
                }
            }
        } catch (TEmptyEventQueueException &) {}

        runtime->Send(new IEventHandle(pipeClient, edge, new TEvents::TEvPoisonPill()));
        return res;
    }

    void TClient::GetTabletInfoFromHive(TTestActorRuntime* runtime, ui64 tabletId, bool returnFollowers, NKikimrHive::TEvResponseHiveInfo& res) {
        TAutoPtr<TEvHive::TEvRequestHiveInfo> ev(new TEvHive::TEvRequestHiveInfo);
        ev->Record.SetTabletID(tabletId);
        ev->Record.SetReturnFollowers(returnFollowers);

        ui64 hive = ChangeStateStorage(Tests::Hive, Domain);
        TActorId edge = runtime->AllocateEdgeActor();
        runtime->SendToPipe(hive, edge, ev.Release());
        TAutoPtr<IEventHandle> handle;
        TEvHive::TEvResponseHiveInfo* response = runtime->GrabEdgeEventRethrow<TEvHive::TEvResponseHiveInfo>(handle);
        res.Swap(&response->Record);
    }

    ui32 TClient::GetLeaderNode(TTestActorRuntime* runtime, ui64 tabletId) {
        NKikimrHive::TEvResponseHiveInfo res;
        GetTabletInfoFromHive(runtime, tabletId, false, res);
        // Cerr << res << Endl;

        for (const NKikimrHive::TTabletInfo& tablet : res.GetTablets()) {
            if (tablet.GetTabletID() == tabletId && tablet.GetNodeID() != 0) {
                return NodeIdToIndex(runtime, tablet.GetNodeID());
            }
        }

        return Max<ui32>();
    }

    bool TClient::TabletExistsInHive(TTestActorRuntime* runtime, ui64 tabletId, bool evenInDeleting) {
        NKikimrHive::TEvResponseHiveInfo res;
        GetTabletInfoFromHive(runtime, tabletId, false, res);
        // Cerr << res << Endl;

        for (const NKikimrHive::TTabletInfo& tablet : res.GetTablets()) {
            if (tablet.GetTabletID() == tabletId) {
                return evenInDeleting || tablet.GetState() != (ui32)NHive::ETabletState::Deleting;
            }
        }

        return false;
    }

    void TClient::GetTabletStorageInfoFromHive(TTestActorRuntime* runtime, ui64 tabletId, NKikimrHive::TEvGetTabletStorageInfoResult& res) {
        TAutoPtr<TEvHive::TEvGetTabletStorageInfo> ev(new TEvHive::TEvGetTabletStorageInfo(tabletId));

        ui64 hive = ChangeStateStorage(Tests::Hive, Domain);
        TActorId edge = runtime->AllocateEdgeActor();
        runtime->SendToPipe(hive, edge, ev.Release());
        TAutoPtr<IEventHandle> handle;
        TEvHive::TEvGetTabletStorageInfoResult* response = runtime->GrabEdgeEventRethrow<TEvHive::TEvGetTabletStorageInfoResult>(handle);

        res.Swap(&response->Record);
        Cerr << response->Record.DebugString() << "\n";

        if (res.GetStatus() == NKikimrProto::OK) {
            auto& info = res.GetInfo();
            Y_ABORT_UNLESS(res.GetTabletID() == info.GetTabletID());
            Y_ABORT_UNLESS(info.ChannelsSize() > 0);

            auto& channel = info.GetChannels(0);
            Y_ABORT_UNLESS(channel.GetChannel() == 0);
            Y_ABORT_UNLESS(channel.HistorySize() > 0);
        }
    }

    TVector<ui32> TClient::GetFollowerNodes(TTestActorRuntime* runtime, ui64 tabletId) {
        NKikimrHive::TEvResponseHiveInfo res;
        GetTabletInfoFromHive(runtime, tabletId, true, res);
        // Cerr << res << Endl;

        TVector<ui32> followerNodes;
        for (const NKikimrHive::TTabletInfo& tablet : res.GetTablets()) {
            if (tablet.GetTabletID() == tabletId && tablet.HasFollowerID() && tablet.GetNodeID() != 0) {
                followerNodes.push_back(NodeIdToIndex(runtime, tablet.GetNodeID()));
            }
        }

        return followerNodes;
    }

    ui64 TClient::GetKesusTabletId(const TString& kesusPath) {
        auto describeResult = Ls(kesusPath);
        UNIT_ASSERT_C(describeResult->Record.GetPathDescription().HasKesus(), describeResult->Record);
        return describeResult->Record.GetPathDescription().GetKesus().GetKesusTabletId();
    }

    Ydb::StatusIds::StatusCode TClient::AddQuoterResource(TTestActorRuntime* runtime, const TString& kesusPath, const TString& resourcePath, const TMaybe<double> maxUnitsPerSecond) {
        NKikimrKesus::THierarchicalDRRResourceConfig cfg;
        if (maxUnitsPerSecond) {
            cfg.SetMaxUnitsPerSecond(*maxUnitsPerSecond);
        }
        return AddQuoterResource(runtime, kesusPath, resourcePath, cfg);
    }

    Ydb::StatusIds::StatusCode TClient::AddQuoterResource(TTestActorRuntime* runtime, const TString& kesusPath, const TString& resourcePath, const NKikimrKesus::THierarchicalDRRResourceConfig& props) {
        THolder<NKesus::TEvKesus::TEvAddQuoterResource> request = MakeHolder<NKesus::TEvKesus::TEvAddQuoterResource>();
        request->Record.MutableResource()->SetResourcePath(resourcePath);
        *request->Record.MutableResource()->MutableHierarchicalDRRResourceConfig() = props;

        TActorId sender = runtime->AllocateEdgeActor(0);
        ForwardToTablet(*runtime, GetKesusTabletId(kesusPath), sender, request.Release(), 0);

        TAutoPtr<IEventHandle> handle;
        runtime->GrabEdgeEvent<NKesus::TEvKesus::TEvAddQuoterResourceResult>(handle);
        auto& record = handle->Get<NKesus::TEvKesus::TEvAddQuoterResourceResult>()->Record;
        return record.GetError().GetStatus();
    }

    THolder<NKesus::TEvKesus::TEvGetConfigResult> TClient::GetKesusConfig(TTestActorRuntime* runtime, const TString& kesusPath) {
        THolder<NKesus::TEvKesus::TEvGetConfig> request = MakeHolder<NKesus::TEvKesus::TEvGetConfig>();

        TActorId sender = runtime->AllocateEdgeActor(0);
        ForwardToTablet(*runtime, GetKesusTabletId(kesusPath), sender, request.Release(), 0);

        TAutoPtr<IEventHandle> handle;
        runtime->GrabEdgeEvent<NKesus::TEvKesus::TEvGetConfigResult>(handle);
        return THolder<NKesus::TEvKesus::TEvGetConfigResult>(handle->Release<NKesus::TEvKesus::TEvGetConfigResult>());
    }

    bool IsServerRedirected() {
        return !!GetEnv(ServerRedirectEnvVar);
    }

    TServerSetup GetServerSetup() {
        if (!IsServerRedirected()) {
            return TServerSetup("localhost", 0);
        }

        const auto& envValue = GetEnv(ServerRedirectEnvVar);
        TStringBuf str(envValue);
        TStringBuf address;
        TStringBuf port;
        str.Split('/', address, port);
        ui64 portValue = 0;
        if (address.empty() || !TryFromString(port, portValue))
            ythrow TWithBackTrace<yexception>() << "Incorrect server redirect, expected 'IpAddress/Port'";

        return TServerSetup(TString(address), portValue);
    }

    TTenants::TTenants(TServer::TPtr server)
        : Server(server)
    {
        ui32 dynamicNodeStartsAt = Server->StaticNodes();
        ui32 dynamicNodeEndsAt = dynamicNodeStartsAt + Server->DynamicNodes();
        for (ui32 nodeIdx = dynamicNodeStartsAt; nodeIdx < dynamicNodeEndsAt; ++nodeIdx) {
            VacantNodes.push_back(nodeIdx);
        }
        MakeHeap(VacantNodes.begin(), VacantNodes.end());
    }

    TTenants::~TTenants() {
        Stop();
    }

    void TTenants::Run(const TString &name, ui32 nodes) {
        Y_ABORT_UNLESS(!Tenants.contains(name));
        Y_ABORT_UNLESS(Availabe() >= nodes);

        Tenants[name] = {};
        RunNodes(name, nodes);
    }

    void TTenants::Stop(const TString &name) {
        Y_ABORT_UNLESS(Tenants.contains(name));

        Free(name, Size(name));
        Tenants.erase(name);
    }

    void TTenants::Stop() {
        for (auto &it: Tenants) {
            const TString &name = it.first;
            Free(name, Size(name));
        }
        Tenants.clear();
    }

    void TTenants::Add(const TString &name, ui32 nodes) {
        Y_ABORT_UNLESS(Tenants.contains(name));
        Y_ABORT_UNLESS(Availabe() >= nodes);

        return RunNodes(name, nodes);
    }

    void TTenants::Free(const TString &name, ui32 nodes) {
        Y_ABORT_UNLESS(Tenants.contains(name));
        Y_ABORT_UNLESS(Size(name) >= nodes);

        return StopNodes(name, nodes);
    }

    void TTenants::FreeNode(const TString &name, ui32 nodeIdx) {
        Y_ABORT_UNLESS(Tenants.contains(name));
        Y_ABORT_UNLESS(Size(name) >= 1);

        return StopPaticularNode(name, nodeIdx);
    }

    bool TTenants::IsStaticNode(ui32 nodeIdx) const {
        return nodeIdx < Server->StaticNodes();
    }

    bool TTenants::IsActive(const TString &name, ui32 nodeIdx) const {
        const TVector<ui32>& nodes = List(name);
        Cerr << "IsActive: " << name << " -- " << nodeIdx << Endl;
        for (auto& x: nodes) {
            Cerr << " -- " << x;
        }
        Cerr << Endl;
        return std::find(nodes.begin(), nodes.end(), nodeIdx) != nodes.end();
    }

    const TVector<ui32> &TTenants::List(const TString &name) const {
        Y_ABORT_UNLESS(Tenants.contains(name));

        return Tenants.at(name);
    }

    ui32 TTenants::Size(const TString &name) const {
        if (!Tenants.contains(name))
            return 0;
        return List(name).size();
    }

    ui32 TTenants::Size() const {
        return Capacity() - Availabe();
    }

    ui32 TTenants::Availabe() const {
        return VacantNodes.size();
    }

    ui32 TTenants::Capacity() const {
        return Server->DynamicNodes();
    }

    void TTenants::CreateTenant(Ydb::Cms::CreateDatabaseRequest request, ui32 nodes, TDuration timeout, bool acceptAlreadyExist) {
        const TString path = request.path();
        const bool serverless = request.has_serverless_resources();

        // Create new tenant
        auto& runtime = *Server->GetRuntime();
        const auto result = NKikimr::NRpcService::DoLocalRpc<NKikimr::NGRpcService::TGrpcRequestOperationCall<Ydb::Cms::CreateDatabaseRequest, Ydb::Cms::CreateDatabaseResponse>>(
            std::move(request), "", "", runtime.GetActorSystem(0), true
        ).ExtractValueSync();

        if (result.operation().status() != Ydb::StatusIds::SUCCESS && (!acceptAlreadyExist || result.operation().status() != Ydb::StatusIds::ALREADY_EXISTS)) {
            NYql::TIssues issues;
            NYql::IssuesFromMessage(result.operation().issues(), issues);
            ythrow yexception() << "Failed to create tenant " << path << ", " << result.operation().status() << ", reason:\n" << issues.ToString();
        }

        // Run new tenant
        if (!serverless) {
            Run(path, nodes);
        }

        // Wait tenant is up
        Ydb::Cms::GetDatabaseStatusResult getTenantResult;
        const TActorId edgeActor = runtime.AllocateEdgeActor();
        const TInstant start = TInstant::Now();
        while (TInstant::Now() - start <= timeout) {
            auto getTenantRequest = std::make_unique<NConsole::TEvConsole::TEvGetTenantStatusRequest>();
            getTenantRequest->Record.MutableRequest()->set_path(path);
            runtime.SendToPipe(MakeConsoleID(), edgeActor, getTenantRequest.release(), 0, GetPipeConfigWithRetries());

            auto response = runtime.GrabEdgeEvent<NConsole::TEvConsole::TEvGetTenantStatusResponse>(edgeActor, timeout);
            if (!response) {
                ythrow yexception() << "Waiting CMS get tenant response timeout. Last tenant description:\n" << getTenantResult.DebugString();
            }
            response->Get()->Record.GetResponse().operation().result().UnpackTo(&getTenantResult);
            if (getTenantResult.state() == Ydb::Cms::GetDatabaseStatusResult::RUNNING) {
                return;
            }

            Sleep(TDuration::MilliSeconds(100));
        }
        ythrow yexception() << "Waiting tenant status RUNNING timeout. Spent time " << TInstant::Now() - start << " exceeds limit " << timeout << ". Last tenant description:\n" << getTenantResult.DebugString();
    }

    TVector<ui32> &TTenants::Nodes(const TString &name) {
        return Tenants[name];
    }

    void TTenants::StopNode(const TString, ui32 nodeIdx) {
        Server->DestroyDynamicLocalService(nodeIdx);
    }

    void TTenants::RunNode(const TString &name, ui32 nodeIdx) {
        Server->SetupDynamicLocalService(nodeIdx, name);
    }

    void TTenants::StopPaticularNode(const TString &name, ui32 nodeIdx) {
        TVector<ui32>& nodes = Nodes(name);

        auto subj = std::find(nodes.begin(), nodes.end(), nodeIdx);
        Y_ABORT_UNLESS(subj != nodes.end());

        StopNode(name, nodeIdx);

        std::swap(*subj, nodes.back());
        nodes.pop_back();
        FreeNodeIdx(nodeIdx);
    }

    void TTenants::StopNodes(const TString &name, ui32 count) {
        TVector<ui32>& nodes = Nodes(name);

        for (ui32 num = 0; num < count && nodes; ++num) {
            ui32 nodeIdx = nodes.back();
            StopNode(name, nodeIdx);
            nodes.pop_back();
            FreeNodeIdx(nodeIdx);
        }
    }

    void TTenants::RunNodes(const TString &name, ui32 count) {
        TVector<ui32>& nodes = Nodes(name);

        for (ui32 num = 0; num < count; ++num) {
            ui32 nodeIdx = AllocNodeIdx();
            RunNode(name, nodeIdx);
            nodes.push_back(nodeIdx);
        }
    }

    ui32 TTenants::AllocNodeIdx() {
        Y_ABORT_UNLESS(VacantNodes);
        PopHeap(VacantNodes.begin(), VacantNodes.end());
        ui32 node = VacantNodes.back();
        VacantNodes.pop_back();
        return node;
    }

    void TTenants::FreeNodeIdx(ui32 nodeIdx) {
        VacantNodes.push_back(nodeIdx);
        PushHeap(VacantNodes.begin(), VacantNodes.end());
    }

    TServerSettings& TServerSettings::AddStoragePool(const TString& poolKind, const TString& poolName, ui32 numGroups, ui32 encryptionMode) {
        NKikimrBlobStorage::TDefineStoragePool& hddPool = StoragePoolTypes[poolKind];
        hddPool.SetBoxId(BOX_ID);
        hddPool.SetStoragePoolId(POOL_ID++);
        hddPool.SetErasureSpecies("none");
        hddPool.SetVDiskKind("Default");
        hddPool.AddPDiskFilter()->AddProperty()->SetType(NKikimrBlobStorage::ROT);
        hddPool.SetKind(poolKind);
        if (poolName) {
            hddPool.SetName(poolName);
        } else {
            hddPool.SetName(poolKind);
        }
        if (encryptionMode) {
            hddPool.SetEncryptionMode(encryptionMode);
        }
        hddPool.SetNumGroups(numGroups);
        return *this;
    }

    TServerSettings& TServerSettings::AddStoragePoolType(const TString& poolKind, ui32 encryptionMode) {
        NKikimrBlobStorage::TDefineStoragePool hddPool;
        hddPool.SetBoxId(BOX_ID);
        hddPool.SetStoragePoolId(POOL_ID++);
        hddPool.SetErasureSpecies("none");
        hddPool.SetVDiskKind("Default");
        hddPool.AddPDiskFilter()->AddProperty()->SetType(NKikimrBlobStorage::ROT);
        hddPool.SetKind(poolKind);
        if (encryptionMode) {
            hddPool.SetEncryptionMode(encryptionMode);
        }
        StoragePoolTypes[poolKind] = hddPool;
        return *this;
    }


}
}
