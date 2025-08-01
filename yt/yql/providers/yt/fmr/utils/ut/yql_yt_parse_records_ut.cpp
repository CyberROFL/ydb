#include <library/cpp/testing/unittest/registar.h>

#include <yt/cpp/mapreduce/common/helpers.h>
#include <yt/yql/providers/yt/fmr/utils/yql_yt_parse_records.h>
#include <yt/yql/providers/yt/fmr/yt_job_service/mock/yql_yt_job_service_mock.h>
#include <yt/yql/providers/yt/fmr/request_options/yql_yt_request_options.h>

using namespace NYql::NFmr;

Y_UNIT_TEST_SUITE(ParseRecordTests) {
    Y_UNIT_TEST(MockParseRecord) {
        TString inputYsonContent = "{\"key\"=\"075\";\"subkey\"=\"1\";\"value\"=\"abc\"};\n"
                                   "{\"key\"=\"800\";\"subkey\"=\"2\";\"value\"=\"ddd\"};\n";
        auto richPath = NYT::TRichYPath("test_path").Cluster("test_cluster");
        TYtTableRef testYtTable{.RichPath = richPath};
        std::unordered_map<TString, TString> inputTables{{NYT::NodeToCanonicalYsonString(NYT::PathToNode(richPath)), inputYsonContent}};

        std::unordered_map<TString, TString> outputTables;

        auto ytJobService = MakeMockYtJobService(inputTables, outputTables);

        auto reader = ytJobService->MakeReader(testYtTable);
        auto writer = ytJobService->MakeWriter(testYtTable, TClusterConnection());
        auto cancelFlag = std::make_shared<std::atomic<bool>>(false);
        ParseRecords(reader, writer, 1, 10, cancelFlag);
        writer->Flush();
        UNIT_ASSERT_VALUES_EQUAL(outputTables.size(), 1);
        TString serializedRichPath = SerializeRichPath(richPath);
        UNIT_ASSERT(outputTables.contains(serializedRichPath));
        UNIT_ASSERT_NO_DIFF(outputTables[serializedRichPath], inputYsonContent);
    }
}
