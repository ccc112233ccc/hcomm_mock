/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"

#include <limits.h>
#include <unistd.h>

#include <sstream>
#include <string>
#include <vector>

#include "llt_api.h"

namespace {

std::string GetWorkingDirectory()
{
    char buffer[PATH_MAX] = {0};
    if (getcwd(buffer, sizeof(buffer)) == nullptr) {
        return ".";
    }
    return buffer;
}

std::string GetOutputDirectory()
{
    return GetWorkingDirectory() + "/llt_api_smoke_output";
}

std::string BuildUniformJson(const std::string &name,
                             const std::string &type,
                             const std::string &op_mode,
                             const std::string &data_type,
                             unsigned long long data_size,
                             const std::string &algorithm,
                             const std::string &dev_type,
                             int super_pods,
                             int servers_per_pod,
                             int ranks_per_server,
                             int root = -1,
                             const std::vector<std::pair<std::string, std::string> > &env = {})
{
    std::ostringstream json;
    json << "{";
    json << "\"output\":{\"directory\":\"" << GetOutputDirectory() << "\"},";
    json << "\"test_cases\":[{";
    json << "\"name\":\"" << name << "\",";
    json << "\"operation\":{";
    json << "\"type\":\"" << type << "\",";
    json << "\"op_mode\":\"" << op_mode << "\",";
    json << "\"data_type\":\"" << data_type << "\",";
    json << "\"data_size\":" << data_size << ",";
    json << "\"algorithm\":\"" << algorithm << "\",";
    json << "\"dev_type\":\"" << dev_type << "\"";
    if (root >= 0) {
        json << ",\"root\":" << root;
    }
    json << "},";
    json << "\"topology\":{";
    json << "\"kind\":\"uniform\",";
    json << "\"super_pods\":" << super_pods << ",";
    json << "\"servers_per_pod\":" << servers_per_pod << ",";
    json << "\"ranks_per_server\":" << ranks_per_server;
    json << "}";
    if (!env.empty()) {
        json << ",\"env\":{";
        for (size_t i = 0; i < env.size(); ++i) {
            if (i != 0) {
                json << ",";
            }
            json << "\"" << env[i].first << "\":\"" << env[i].second << "\"";
        }
        json << "}";
    }
    json << "}]}";
    return json.str();
}

std::string BuildExplicitJson(const std::string &name,
                              const std::string &type,
                              const std::string &op_mode,
                              const std::string &data_type,
                              unsigned long long data_size,
                              const std::string &algorithm,
                              const std::string &dev_type,
                              const std::string &super_pods_json)
{
    std::ostringstream json;
    json << "{";
    json << "\"output\":{\"directory\":\"" << GetOutputDirectory() << "\"},";
    json << "\"test_cases\":[{";
    json << "\"name\":\"" << name << "\",";
    json << "\"operation\":{";
    json << "\"type\":\"" << type << "\",";
    json << "\"op_mode\":\"" << op_mode << "\",";
    json << "\"data_type\":\"" << data_type << "\",";
    json << "\"data_size\":" << data_size << ",";
    json << "\"algorithm\":\"" << algorithm << "\",";
    json << "\"dev_type\":\"" << dev_type << "\"";
    json << "},";
    json << "\"topology\":{";
    json << "\"kind\":\"explicit\",";
    json << "\"super_pods\":" << super_pods_json;
    json << "}";
    json << "}]}";
    return json.str();
}

std::string BuildSendRecvJson(const std::string &name,
                              const std::string &type,
                              const std::string &op_mode,
                              const std::string &data_type,
                              unsigned long long data_size,
                              const std::string &dev_type,
                              int super_pods,
                              int servers_per_pod,
                              int ranks_per_server,
                              int src_rank,
                              int dst_rank)
{
    std::ostringstream json;
    json << "{";
    json << "\"output\":{\"directory\":\"" << GetOutputDirectory() << "\"},";
    json << "\"test_cases\":[{";
    json << "\"name\":\"" << name << "\",";
    json << "\"operation\":{";
    json << "\"type\":\"" << type << "\",";
    json << "\"op_mode\":\"" << op_mode << "\",";
    json << "\"data_type\":\"" << data_type << "\",";
    json << "\"data_size\":" << data_size << ",";
    json << "\"dev_type\":\"" << dev_type << "\",";
    json << "\"src_rank\":" << src_rank << ",";
    json << "\"dst_rank\":" << dst_rank;
    json << "},";
    json << "\"topology\":{";
    json << "\"kind\":\"uniform\",";
    json << "\"super_pods\":" << super_pods << ",";
    json << "\"servers_per_pod\":" << servers_per_pod << ",";
    json << "\"ranks_per_server\":" << ranks_per_server;
    json << "}";
    json << "}]}";
    return json.str();
}

std::string BuildVCollectiveJson(const std::string &name,
                                 const std::string &type,
                                 const std::string &op_mode,
                                 const std::string &data_type,
                                 const std::string &algorithm,
                                 const std::string &dev_type,
                                 int super_pods,
                                 int servers_per_pod,
                                 int ranks_per_server,
                                 const std::string &counts_json,
                                 const std::string &displs_json)
{
    std::ostringstream json;
    json << "{";
    json << "\"output\":{\"directory\":\"" << GetOutputDirectory() << "\"},";
    json << "\"test_cases\":[{";
    json << "\"name\":\"" << name << "\",";
    json << "\"operation\":{";
    json << "\"type\":\"" << type << "\",";
    json << "\"op_mode\":\"" << op_mode << "\",";
    json << "\"data_type\":\"" << data_type << "\",";
    json << "\"algorithm\":\"" << algorithm << "\",";
    json << "\"dev_type\":\"" << dev_type << "\",";
    json << "\"counts\":" << counts_json << ",";
    json << "\"displs\":" << displs_json;
    json << "},";
    json << "\"topology\":{";
    json << "\"kind\":\"uniform\",";
    json << "\"super_pods\":" << super_pods << ",";
    json << "\"servers_per_pod\":" << servers_per_pod << ",";
    json << "\"ranks_per_server\":" << ranks_per_server;
    json << "}";
    json << "}]}";
    return json.str();
}

std::string BuildAllToAllJson(const std::string &name,
                              const std::string &type,
                              const std::string &op_mode,
                              const std::string &data_type,
                              unsigned long long count,
                              const std::string &algorithm,
                              const std::string &dev_type,
                              int super_pods,
                              int servers_per_pod,
                              int ranks_per_server)
{
    std::ostringstream json;
    json << "{";
    json << "\"output\":{\"directory\":\"" << GetOutputDirectory() << "\"},";
    json << "\"test_cases\":[{";
    json << "\"name\":\"" << name << "\",";
    json << "\"operation\":{";
    json << "\"type\":\"" << type << "\",";
    json << "\"op_mode\":\"" << op_mode << "\",";
    json << "\"data_type\":\"" << data_type << "\",";
    json << "\"count\":" << count << ",";
    json << "\"algorithm\":\"" << algorithm << "\",";
    json << "\"dev_type\":\"" << dev_type << "\"";
    json << "},";
    json << "\"topology\":{";
    json << "\"kind\":\"uniform\",";
    json << "\"super_pods\":" << super_pods << ",";
    json << "\"servers_per_pod\":" << servers_per_pod << ",";
    json << "\"ranks_per_server\":" << ranks_per_server;
    json << "}";
    json << "}]}";
    return json.str();
}

std::string BuildBatchSendRecvJson(const std::string &name,
                                   const std::string &data_type,
                                   unsigned long long count,
                                   const std::string &dev_type,
                                   int super_pods,
                                   int servers_per_pod,
                                   int ranks_per_server)
{
    std::ostringstream json;
    json << "{";
    json << "\"output\":{\"directory\":\"" << GetOutputDirectory() << "\"},";
    json << "\"test_cases\":[{";
    json << "\"name\":\"" << name << "\",";
    json << "\"operation\":{";
    json << "\"type\":\"BATCH_SEND_RECV\",";
    json << "\"op_mode\":\"OPBASE\",";
    json << "\"data_type\":\"" << data_type << "\",";
    json << "\"count\":" << count << ",";
    json << "\"dev_type\":\"" << dev_type << "\"";
    json << "},";
    json << "\"topology\":{";
    json << "\"kind\":\"uniform\",";
    json << "\"super_pods\":" << super_pods << ",";
    json << "\"servers_per_pod\":" << servers_per_pod << ",";
    json << "\"ranks_per_server\":" << ranks_per_server;
    json << "}";
    json << "}]}";
    return json.str();
}

llt::TestResult RunSingleCase(const std::string &json_string,
                              const std::string &test_name,
                              bool expect_success = true,
                              bool expect_dump = true)
{
    llt::LltApi api;
    if (api.LoadConfigFromJsonString(json_string) != HCCL_SUCCESS) {
        llt::TestResult result;
        result.test_name = test_name;
        result.error_message = "LoadConfigFromJsonString failed";
        return result;
    }
    std::vector<std::string> names = api.GetTestCaseNames();
    if (names.size() != 1U) {
        llt::TestResult result;
        result.test_name = test_name;
        result.error_message = "Unexpected testcase count";
        return result;
    }
    if (names[0] != test_name) {
        llt::TestResult result;
        result.test_name = test_name;
        result.error_message = "Unexpected testcase name";
        return result;
    }

    llt::TestResult result = api.Run(test_name);
    EXPECT_EQ(result.dump_generated, expect_dump) << result.error_message;
    if (expect_success) {
        EXPECT_EQ(result.result, HCCL_SUCCESS) << result.error_message;
    } else {
        EXPECT_NE(result.result, HCCL_SUCCESS);
    }
    EXPECT_FALSE(result.text_file.empty());
    EXPECT_FALSE(result.binary_file.empty());
    if (expect_dump) {
        EXPECT_EQ(access(result.text_file.c_str(), F_OK), 0);
        EXPECT_EQ(access(result.binary_file.c_str(), F_OK), 0);
    } else {
        EXPECT_NE(access(result.text_file.c_str(), F_OK), 0);
        EXPECT_NE(access(result.binary_file.c_str(), F_OK), 0);
    }
    return result;
}

}  // namespace

TEST(LltApiSmokeTest, AllGatherMeshOffload)
{
    const std::string json = BuildUniformJson(
        "allgather_mesh_offload",
        "ALLGATHER",
        "OFFLOAD",
        "FP32",
        800,
        "AllGatherMeshExecutor",
        "910B",
        1,
        2,
        8,
        -1,
        {{"HCCL_ALGO", "level0:NA;level1:NB"}});
    RunSingleCase(json, "allgather_mesh_offload");
}

TEST(LltApiSmokeTest, AllReducePipelineOffload)
{
    const std::string json = BuildUniformJson(
        "allreduce_pipeline_offload",
        "ALLREDUCE",
        "OFFLOAD",
        "FP32",
        1024,
        "AllReduceMeshGraphPipelineExecutor",
        "910B",
        1,
        2,
        8);
    RunSingleCase(json, "allreduce_pipeline_offload");
}

TEST(LltApiSmokeTest, BroadcastSuperPodRing)
{
    const std::string json = BuildUniformJson(
        "broadcast_superpod_ring",
        "BROADCAST",
        "OPBASE",
        "FP16",
        200,
        "BroadCastRingFor91093Executor",
        "910_93",
        2,
        1,
        8,
        4);
    RunSingleCase(json, "broadcast_superpod_ring");
}

TEST(LltApiSmokeTest, BroadcastAivSmallCount)
{
    const std::string json = BuildUniformJson(
        "broadcast_aiv_smallcount",
        "BROADCAST",
        "OPBASE",
        "FP16",
        2,
        "BroadcastMeshAivExecutor",
        "910B",
        1,
        1,
        8,
        0,
        {{"HCCL_OP_EXPANSION_MODE", "AIV"}});
    RunSingleCase(json, "broadcast_aiv_smallcount");
}

TEST(LltApiSmokeTest, ReduceMeshOpbase)
{
    const std::string json = BuildUniformJson(
        "reduce_mesh_opbase",
        "REDUCE",
        "OPBASE",
        "FP32",
        800,
        "ReduceMeshExecutor",
        "910B",
        1,
        2,
        8,
        0,
        {{"HCCL_ALGO", "level0:NA;level1:NB"}});
    RunSingleCase(json, "reduce_mesh_opbase");
}

TEST(LltApiSmokeTest, ReduceScatterMeshOffload)
{
    const std::string json = BuildUniformJson(
        "reducescatter_mesh_offload",
        "REDUCE_SCATTER",
        "OFFLOAD",
        "FP32",
        800,
        "ReduceScatterMeshExecutor",
        "910B",
        1,
        2,
        8,
        -1,
        {{"HCCL_ALGO", "level0:NA;level1:ring"}});
    RunSingleCase(json, "reducescatter_mesh_offload");
}

TEST(LltApiSmokeTest, AllReduceRingExplicitTopology)
{
    const std::string json = BuildExplicitJson(
        "allreduce_ring_explicit_topology",
        "ALLREDUCE",
        "OFFLOAD",
        "INT32",
        3200,
        "AllReduceRingExecutor",
        "910B",
        "[[[0,1,2,3,4,5,6,7]]]");
    RunSingleCase(json, "allreduce_ring_explicit_topology");
}

TEST(LltApiSmokeTest, UniformTopologyWithoutKind)
{
    std::ostringstream json;
    json << "{";
    json << "\"output\":{\"directory\":\"" << GetOutputDirectory() << "\"},";
    json << "\"test_cases\":[{";
    json << "\"name\":\"uniform_topology_without_kind\",";
    json << "\"operation\":{";
    json << "\"type\":\"ALLREDUCE\",";
    json << "\"op_mode\":\"OFFLOAD\",";
    json << "\"data_type\":\"INT32\",";
    json << "\"data_size\":400,";
    json << "\"algorithm\":\"AllReduceRingExecutor\",";
    json << "\"dev_type\":\"910B\"";
    json << "},";
    json << "\"topology\":{";
    json << "\"super_pods\":1,";
    json << "\"servers\":1,";
    json << "\"ranks_per_server\":8";
    json << "}";
    json << "}]}";
    RunSingleCase(json.str(), "uniform_topology_without_kind");
}

TEST(LltApiSmokeTest, EnvVarsAlias)
{
    std::ostringstream json;
    json << "{";
    json << "\"output\":{\"directory\":\"" << GetOutputDirectory() << "\"},";
    json << "\"test_cases\":[{";
    json << "\"name\":\"env_vars_alias\",";
    json << "\"operation\":{";
    json << "\"type\":\"ALLREDUCE\",";
    json << "\"op_mode\":\"OFFLOAD\",";
    json << "\"data_type\":\"INT32\",";
    json << "\"data_size\":400,";
    json << "\"algorithm\":\"AllReduceRingExecutor\",";
    json << "\"dev_type\":\"910B\"";
    json << "},";
    json << "\"topology\":{";
    json << "\"super_pods\":1,";
    json << "\"servers\":1,";
    json << "\"ranks_per_server\":8";
    json << "},";
    json << "\"env_vars\":{";
    json << "\"HCCL_ALGO\":\"level0:NA;level1:ring\"";
    json << "}";
    json << "}]}";
    RunSingleCase(json.str(), "env_vars_alias");
}

TEST(LltApiSmokeTest, Mock0526CompatibleConfig)
{
    std::ostringstream json;
    json << "{";
    json << "\"output\":{\"directory\":\"" << GetOutputDirectory() << "\"},";
    json << "\"test_cases\":[{";
    json << "\"name\":\"mock_0526_compatible_config\",";
    json << "\"operation\":{";
    json << "\"type\":\"ALLREDUCE\",";
    json << "\"op_mode\":\"OFFLOAD\",";
    json << "\"data_type\":\"INT32\",";
    json << "\"data_size\":400,";
    json << "\"algorithm\":\"AllReduceRingExecutor\"";
    json << "},";
    json << "\"topology\":{";
    json << "\"super_pods\":1,";
    json << "\"servers\":1,";
    json << "\"ranks_per_server\":8";
    json << "},";
    json << "\"device\":\"DEV_TYPE_910B\",";
    json << "\"env_vars\":{";
    json << "\"HCCL_ALGO\":\"level0:NA;level1:ring\"";
    json << "}";
    json << "}]}";
    RunSingleCase(json.str(), "mock_0526_compatible_config");
}

TEST(LltApiSmokeTest, AllReduceMeshOffload)
{
    const std::string json = BuildUniformJson(
        "allreduce_mesh_offload",
        "ALLREDUCE",
        "OFFLOAD",
        "INT32",
        400,
        "AllReduceMeshExecutor",
        "910B",
        1,
        1,
        4);
    RunSingleCase(json, "allreduce_mesh_offload");
}

TEST(LltApiSmokeTest, ScatterMeshOpbase)
{
    const std::string json = BuildUniformJson(
        "scatter_mesh_opbase",
        "SCATTER",
        "OPBASE",
        "INT32",
        400,
        "ScatterMeshExecutor",
        "910B",
        1,
        2,
        8,
        0);
    RunSingleCase(json, "scatter_mesh_opbase");
}

TEST(LltApiSmokeTest, SendRecvTwoServersOpbase)
{
    const std::string json = BuildSendRecvJson(
        "send_recv_two_servers_opbase",
        "SEND",
        "OPBASE",
        "INT32",
        400,
        "910B",
        1,
        2,
        1,
        0,
        1);
    RunSingleCase(json, "send_recv_two_servers_opbase");
}

TEST(LltApiSmokeTest, AllGatherRingOffload)
{
    const std::string json = BuildUniformJson(
        "allgather_ring_offload",
        "ALLGATHER",
        "OFFLOAD",
        "INT32",
        400,
        "AllGatherRingExecutor",
        "910B",
        1,
        1,
        8);
    RunSingleCase(json, "allgather_ring_offload");
}

TEST(LltApiSmokeTest, AllGatherVOpbase)
{
    const std::string json = BuildVCollectiveJson(
        "allgatherv_opbase",
        "ALLGATHER_V",
        "OPBASE",
        "INT32",
        "AllGatherVMeshExecutor",
        "910B",
        1,
        1,
        4,
        "[1000,2000,3000,4000]",
        "[0,1000,3000,6000]");
    RunSingleCase(json, "allgatherv_opbase");
}

TEST(LltApiSmokeTest, BroadcastMeshOpbase)
{
    const std::string json = BuildUniformJson(
        "broadcast_mesh_opbase",
        "BROADCAST",
        "OPBASE",
        "INT32",
        400,
        "BroadCastMeshExecutor",
        "910B",
        1,
        4,
        4,
        0,
        {{"HCCL_ALGO", "level0:fullmesh;level1:NHR_V1"}});
    RunSingleCase(json, "broadcast_mesh_opbase");
}

TEST(LltApiSmokeTest, ReduceRingOffload)
{
    const std::string json = BuildUniformJson(
        "reduce_ring_offload",
        "REDUCE",
        "OFFLOAD",
        "INT32",
        400,
        "ReduceRingPlusHd",
        "910B",
        1,
        1,
        8,
        0);
    RunSingleCase(json, "reduce_ring_offload");
}

TEST(LltApiSmokeTest, ReduceScatterVOpbase)
{
    const std::string json = BuildVCollectiveJson(
        "reducescatterv_opbase",
        "REDUCE_SCATTER_V",
        "OPBASE",
        "FP32",
        "ReduceScatterVMeshOpbaseExecutor",
        "910B",
        1,
        1,
        4,
        "[100,200,300,400]",
        "[0,100,300,600]");
    RunSingleCase(json, "reducescatterv_opbase");
}

TEST(LltApiSmokeTest, ReduceScatterRingOffload)
{
    const std::string json = BuildUniformJson(
        "reducescatter_ring_offload",
        "REDUCE_SCATTER",
        "OFFLOAD",
        "INT32",
        400,
        "ReduceScatterRingExecutor",
        "910B",
        1,
        1,
        8);
    RunSingleCase(json, "reducescatter_ring_offload");
}

TEST(LltApiSmokeTest, AllToAllOpbase)
{
    const std::string json = BuildAllToAllJson(
        "alltoall_opbase",
        "ALLTOALL",
        "OPBASE",
        "INT32",
        300,
        "",
        "910B",
        1,
        1,
        3);
    RunSingleCase(json, "alltoall_opbase");
}

TEST(LltApiSmokeTest, AllToAllTwoLevelPipeline)
{
    const std::string json = BuildAllToAllJson(
        "alltoall_two_level_pipeline",
        "ALLTOALL",
        "OPBASE",
        "FP16",
        1600,
        "RunAlltoAllVTwoLevelPipeline",
        "910B",
        1,
        2,
        8);
    RunSingleCase(json, "alltoall_two_level_pipeline");
}

TEST(LltApiSmokeTest, AllToAllVOpbase)
{
    const std::string json = BuildAllToAllJson(
        "alltoallv_opbase",
        "ALLTOALLV",
        "OPBASE",
        "INT32",
        300,
        "",
        "910B",
        1,
        1,
        3);
    RunSingleCase(json, "alltoallv_opbase");
}

TEST(LltApiSmokeTest, AllToAllVFullMesh)
{
    const std::string json = BuildAllToAllJson(
        "alltoallv_fullmesh",
        "ALLTOALLV",
        "OPBASE",
        "FP16",
        1600,
        "RunAlltoAllVFullMesh",
        "910B",
        1,
        2,
        8);
    RunSingleCase(json, "alltoallv_fullmesh");
}

TEST(LltApiSmokeTest, AllToAllVCOpbase)
{
    const std::string json = BuildAllToAllJson(
        "alltoallvc_opbase",
        "ALLTOALLVC",
        "OPBASE",
        "INT32",
        300,
        "",
        "910B",
        1,
        1,
        3);
    RunSingleCase(json, "alltoallvc_opbase");
}

TEST(LltApiSmokeTest, AllToAllVCStaged)
{
    const std::string json = BuildAllToAllJson(
        "alltoallvc_staged",
        "ALLTOALLVC",
        "OPBASE",
        "FP16",
        1600,
        "RunAlltoAllVStaged",
        "910B",
        1,
        2,
        8);
    RunSingleCase(json, "alltoallvc_staged");
}

TEST(LltApiSmokeTest, BatchSendRecvOpbase)
{
    const std::string json = BuildBatchSendRecvJson(
        "batch_send_recv_opbase",
        "INT8",
        1024,
        "910B",
        1,
        1,
        8);
    RunSingleCase(json, "batch_send_recv_opbase");
}

TEST(LltApiSmokeTest, ScatterRingOpbase)
{
    const std::string json = BuildUniformJson(
        "scatter_ring_opbase",
        "SCATTER",
        "OPBASE",
        "INT32",
        400,
        "ScatterRingExecutor",
        "910B",
        1,
        2,
        8,
        0);
    RunSingleCase(json, "scatter_ring_opbase");
}
