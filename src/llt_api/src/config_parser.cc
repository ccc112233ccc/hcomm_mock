/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "config_parser.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "nlohmann/json.hpp"
#include "string_to_enum.h"

namespace {

using json = nlohmann::json;

std::string RequireString(const json &obj, const char *key)
{
    if (!obj.contains(key) || !obj.at(key).is_string()) {
        throw std::runtime_error(std::string("missing or invalid string field: ") + key);
    }
    return obj.at(key).get<std::string>();
}

uint64_t RequireUnsigned(const json &obj, const char *key)
{
    if (!obj.contains(key) || !obj.at(key).is_number_integer()) {
        throw std::runtime_error(std::string("missing or invalid integer field: ") + key);
    }
    const int64_t value = obj.at(key).get<int64_t>();
    if (value < 0) {
        throw std::runtime_error(std::string("field must be non-negative: ") + key);
    }
    return static_cast<uint64_t>(value);
}

int RequireInt(const json &obj, const char *key)
{
    if (!obj.contains(key) || !obj.at(key).is_number_integer()) {
        throw std::runtime_error(std::string("missing or invalid integer field: ") + key);
    }
    return obj.at(key).get<int>();
}

int RequireIntAlias(const json &obj, const char *primary_key, const char *fallback_key)
{
    if (obj.contains(primary_key)) {
        return RequireInt(obj, primary_key);
    }
    return RequireInt(obj, fallback_key);
}

bool HasField(const json &obj, const char *key)
{
    return obj.contains(key) && !obj.at(key).is_null();
}

std::vector<u64> RequireUnsignedArray(const json &obj, const char *key)
{
    if (!HasField(obj, key) || !obj.at(key).is_array()) {
        throw std::runtime_error(std::string("missing or invalid integer array field: ") + key);
    }
    std::vector<u64> values;
    const json &array = obj.at(key);
    values.reserve(array.size());
    for (size_t i = 0; i < array.size(); ++i) {
        if (!array.at(i).is_number_integer()) {
            throw std::runtime_error(std::string("array must contain integers: ") + key);
        }
        const int64_t value = array.at(i).get<int64_t>();
        if (value < 0) {
            throw std::runtime_error(std::string("array values must be non-negative: ") + key);
        }
        values.push_back(static_cast<u64>(value));
    }
    return values;
}

std::vector<u64> BuildDisplacements(const std::vector<u64> &counts)
{
    std::vector<u64> displacements;
    displacements.reserve(counts.size());
    u64 offset = 0;
    for (size_t i = 0; i < counts.size(); ++i) {
        displacements.push_back(offset);
        offset += counts[i];
    }
    return displacements;
}

u32 CountRanks(const checker::TopoMeta &topo_meta)
{
    u32 rank_size = 0;
    for (size_t super_pod_idx = 0; super_pod_idx < topo_meta.size(); ++super_pod_idx) {
        const checker::SuperPodMeta &super_pod = topo_meta[super_pod_idx];
        for (size_t server_idx = 0; server_idx < super_pod.size(); ++server_idx) {
            rank_size += static_cast<u32>(super_pod[server_idx].size());
        }
    }
    return rank_size;
}

u64 ResolveElementCount(const json &operation, checker::CheckerDataType data_type)
{
    if (HasField(operation, "count")) {
        return RequireUnsigned(operation, "count");
    }
    const uint64_t data_size = RequireUnsigned(operation, "data_size");
    const uint32_t unit_size = checker::CHECK_SIZE_TABLE[data_type];
    if (unit_size == 0 || data_size % unit_size != 0) {
        throw std::runtime_error("data_size must be divisible by the selected data type size");
    }
    return data_size / unit_size;
}

checker::CheckerSendRecvType ParseSendRecvDirection(const std::string &value)
{
    const std::string normalized = llt::NormalizeUpper(value);
    if (normalized == "SEND") {
        return checker::CHECK_SEND;
    }
    if (normalized == "RECV" || normalized == "RECEIVE") {
        return checker::CHECK_RECV;
    }
    throw std::runtime_error("unsupported send/recv direction: " + normalized);
}

checker::TopoMeta ParseExplicitTopo(const json &topology)
{
    if (!topology.is_array()) {
        throw std::runtime_error("explicit topology must be a three-level nested array");
    }

    checker::TopoMeta topo_meta;
    for (size_t super_pod_idx = 0; super_pod_idx < topology.size(); ++super_pod_idx) {
        const json &super_pod_json = topology.at(super_pod_idx);
        if (!super_pod_json.is_array()) {
            throw std::runtime_error("explicit topology super pod must be an array");
        }
        checker::SuperPodMeta super_pod;
        for (size_t server_idx = 0; server_idx < super_pod_json.size(); ++server_idx) {
            const json &server_json = super_pod_json.at(server_idx);
            if (!server_json.is_array()) {
                throw std::runtime_error("explicit topology server must be an array");
            }
            checker::ServerMeta server;
            for (size_t rank_idx = 0; rank_idx < server_json.size(); ++rank_idx) {
                if (!server_json.at(rank_idx).is_number_unsigned()) {
                    throw std::runtime_error("explicit topology device id must be unsigned");
                }
                server.push_back(server_json.at(rank_idx).get<checker::PhyDeviceId>());
            }
            super_pod.push_back(server);
        }
        topo_meta.push_back(super_pod);
    }
    return topo_meta;
}

checker::TopoMeta ParseTopology(const json &topology)
{
    std::string kind;
    if (HasField(topology, "kind")) {
        kind = llt::NormalizeUpper(RequireString(topology, "kind"));
    } else if (HasField(topology, "super_pods") && topology.at("super_pods").is_array()) {
        kind = "EXPLICIT";
    } else {
        kind = "UNIFORM";
    }

    checker::TopoMeta topo_meta;
    if (kind == "UNIFORM") {
        const int super_pods = RequireInt(topology, "super_pods");
        const int servers_per_pod = RequireIntAlias(topology, "servers_per_pod", "servers");
        const int ranks_per_server = RequireInt(topology, "ranks_per_server");
        const HcclResult ret =
            checker::RankTable_For_LLT::GenTopoMeta(topo_meta, super_pods, servers_per_pod, ranks_per_server);
        if (ret != HCCL_SUCCESS) {
            throw std::runtime_error("failed to generate uniform topology");
        }
        return topo_meta;
    }

    if (kind == "EXPLICIT") {
        if (!topology.contains("super_pods")) {
            throw std::runtime_error("explicit topology requires super_pods array");
        }
        return ParseExplicitTopo(topology.at("super_pods"));
    }

    throw std::runtime_error("unsupported topology kind: " + kind);
}

void ParseSimpleDataDes(checker::CheckerOpParam &op_param, const json &operation)
{
    op_param.DataDes.dataType = llt::ParseDataType(RequireString(operation, "data_type"));
    op_param.DataDes.count = ResolveElementCount(operation, op_param.DataDes.dataType);
}

void ParseVDataDes(checker::CheckerOpParam &op_param, const json &operation, u32 rank_size)
{
    op_param.VDataDes.dataType = llt::ParseDataType(RequireString(operation, "data_type"));
    if (HasField(operation, "counts")) {
        op_param.VDataDes.counts = RequireUnsignedArray(operation, "counts");
    } else {
        const u64 count = ResolveElementCount(operation, op_param.VDataDes.dataType);
        op_param.VDataDes.counts.assign(rank_size, count);
    }

    if (op_param.VDataDes.counts.size() != rank_size) {
        throw std::runtime_error("counts size must match topology rank size");
    }

    if (HasField(operation, "displs")) {
        op_param.VDataDes.displs = RequireUnsignedArray(operation, "displs");
    } else {
        op_param.VDataDes.displs = BuildDisplacements(op_param.VDataDes.counts);
    }

    if (op_param.VDataDes.displs.size() != rank_size) {
        throw std::runtime_error("displs size must match topology rank size");
    }
}

void ParseAllToAllDataDes(checker::CheckerOpParam &op_param, const json &operation, u32 rank_size)
{
    const checker::CheckerDataType fallback_type = llt::ParseDataType(RequireString(operation, "data_type"));
    op_param.All2AllDataDes.sendType = HasField(operation, "send_type")
        ? llt::ParseDataType(RequireString(operation, "send_type"))
        : fallback_type;
    op_param.All2AllDataDes.recvType = HasField(operation, "recv_type")
        ? llt::ParseDataType(RequireString(operation, "recv_type"))
        : fallback_type;

    if (HasField(operation, "send_count_matrix")) {
        op_param.All2AllDataDes.sendCountMatrix = RequireUnsignedArray(operation, "send_count_matrix");
        if (op_param.All2AllDataDes.sendCountMatrix.size() != static_cast<size_t>(rank_size) * rank_size) {
            throw std::runtime_error("send_count_matrix size must equal rank_size * rank_size");
        }
        op_param.All2AllDataDes.sendCount = op_param.All2AllDataDes.sendCountMatrix.empty()
            ? 0
            : op_param.All2AllDataDes.sendCountMatrix[0];
        return;
    }

    const u64 total_count = ResolveElementCount(operation, fallback_type);
    if (rank_size == 0 || total_count % rank_size != 0) {
        throw std::runtime_error("ALLTOALL/ALLTOALLVC count must be divisible by topology rank size");
    }

    const u64 peer_count = total_count / rank_size;
    op_param.All2AllDataDes.sendCount = peer_count;
    op_param.All2AllDataDes.sendCountMatrix.assign(static_cast<size_t>(rank_size) * rank_size, peer_count);
}

void ParseAllToAllVDataDes(checker::CheckerOpParam &op_param, const json &operation, u32 rank_size)
{
    const checker::CheckerDataType fallback_type = llt::ParseDataType(RequireString(operation, "data_type"));
    op_param.All2AllDataDes.sendType = HasField(operation, "send_type")
        ? llt::ParseDataType(RequireString(operation, "send_type"))
        : fallback_type;
    op_param.All2AllDataDes.recvType = HasField(operation, "recv_type")
        ? llt::ParseDataType(RequireString(operation, "recv_type"))
        : fallback_type;

    if (HasField(operation, "send_counts")) {
        op_param.All2AllDataDes.sendCounts = RequireUnsignedArray(operation, "send_counts");
    }
    if (HasField(operation, "recv_counts")) {
        op_param.All2AllDataDes.recvCounts = RequireUnsignedArray(operation, "recv_counts");
    }

    if (op_param.All2AllDataDes.sendCounts.empty() || op_param.All2AllDataDes.recvCounts.empty()) {
        const u64 total_count = ResolveElementCount(operation, fallback_type);
        if (rank_size == 0 || total_count % rank_size != 0) {
            throw std::runtime_error("ALLTOALLV count must be divisible by topology rank size");
        }
        const u64 peer_count = total_count / rank_size;
        op_param.All2AllDataDes.sendCounts.assign(rank_size, peer_count);
        op_param.All2AllDataDes.recvCounts.assign(rank_size, peer_count);
    }

    if (op_param.All2AllDataDes.sendCounts.size() != rank_size ||
        op_param.All2AllDataDes.recvCounts.size() != rank_size) {
        throw std::runtime_error("send_counts/recv_counts size must match topology rank size");
    }

    if (HasField(operation, "sdispls")) {
        op_param.All2AllDataDes.sdispls = RequireUnsignedArray(operation, "sdispls");
    } else {
        op_param.All2AllDataDes.sdispls = BuildDisplacements(op_param.All2AllDataDes.sendCounts);
    }
    if (HasField(operation, "rdispls")) {
        op_param.All2AllDataDes.rdispls = RequireUnsignedArray(operation, "rdispls");
    } else {
        op_param.All2AllDataDes.rdispls = BuildDisplacements(op_param.All2AllDataDes.recvCounts);
    }

    if (op_param.All2AllDataDes.sdispls.size() != rank_size ||
        op_param.All2AllDataDes.rdispls.size() != rank_size) {
        throw std::runtime_error("sdispls/rdispls size must match topology rank size");
    }
}

void ParseBatchSendRecvDataDes(checker::CheckerOpParam &op_param, const json &operation, u32 rank_size)
{
    ParseSimpleDataDes(op_param, operation);
    op_param.allRanksSendRecvInfoVec.resize(rank_size);
    if (!HasField(operation, "all_ranks_send_recv_info")) {
        return;
    }

    const json &all_ranks = operation.at("all_ranks_send_recv_info");
    if (!all_ranks.is_array() || all_ranks.size() != rank_size) {
        throw std::runtime_error("all_ranks_send_recv_info must be an array with rank_size entries");
    }

    for (size_t rank_idx = 0; rank_idx < all_ranks.size(); ++rank_idx) {
        const json &rank_items = all_ranks.at(rank_idx);
        if (!rank_items.is_array()) {
            throw std::runtime_error("each rank send/recv description must be an array");
        }
        for (size_t item_idx = 0; item_idx < rank_items.size(); ++item_idx) {
            const json &item_json = rank_items.at(item_idx);
            if (!item_json.is_object()) {
                throw std::runtime_error("batch send/recv item must be an object");
            }
            checker::CheckerSendRecvItem item;
            item.sendRecvType = ParseSendRecvDirection(RequireString(item_json, "direction"));
            item.buf = nullptr;
            item.dataType = HasField(item_json, "data_type")
                ? llt::ParseDataType(RequireString(item_json, "data_type"))
                : op_param.DataDes.dataType;
            item.count = HasField(item_json, "count")
                ? RequireUnsigned(item_json, "count")
                : ResolveElementCount(item_json, item.dataType);
            item.remoteRank = static_cast<uint32_t>(RequireUnsigned(item_json, "remote_rank"));
            op_param.allRanksSendRecvInfoVec[rank_idx].push_back(item);
        }
    }
}

checker::CheckerOpParam ParseOperation(const json &operation, u32 rank_size, const std::string &device)
{
    checker::CheckerOpParam op_param;
    op_param.opType = llt::ParseOpType(RequireString(operation, "type"));
    op_param.tag = HasField(operation, "tag") ? RequireString(operation, "tag") : llt::DefaultTagForOp(op_param.opType);
    op_param.opMode = llt::ParseOpMode(RequireString(operation, "op_mode"));
    op_param.devtype = operation.contains("dev_type")
        ? llt::ParseDevType(RequireString(operation, "dev_type"))
        : (device.empty() ? checker::CheckerDevType::DEV_TYPE_910B : llt::ParseDevType(device));
    op_param.algName = operation.contains("algorithm") ? RequireString(operation, "algorithm") : "";
    op_param.reduceType = checker::CheckerReduceOp::REDUCE_SUM;
    if (operation.contains("reduce_op")) {
        op_param.reduceType = llt::ParseReduceOp(RequireString(operation, "reduce_op"));
    }
    if (HasField(operation, "root")) {
        op_param.root = RequireInt(operation, "root");
    }
    if (HasField(operation, "src_rank")) {
        op_param.srcRank = RequireInt(operation, "src_rank");
    }
    if (HasField(operation, "dst_rank")) {
        op_param.dstRank = RequireInt(operation, "dst_rank");
    }

    switch (op_param.opType) {
        case checker::CheckerOpType::ALLREDUCE:
        case checker::CheckerOpType::ALLGATHER:
        case checker::CheckerOpType::BROADCAST:
        case checker::CheckerOpType::REDUCE:
        case checker::CheckerOpType::REDUCE_SCATTER:
        case checker::CheckerOpType::SCATTER:
        case checker::CheckerOpType::SEND:
        case checker::CheckerOpType::RECEIVE:
            ParseSimpleDataDes(op_param, operation);
            break;
        case checker::CheckerOpType::ALLGATHER_V:
        case checker::CheckerOpType::REDUCE_SCATTER_V:
            ParseVDataDes(op_param, operation, rank_size);
            break;
        case checker::CheckerOpType::ALLTOALL:
        case checker::CheckerOpType::ALLTOALLVC:
            ParseAllToAllDataDes(op_param, operation, rank_size);
            break;
        case checker::CheckerOpType::ALLTOALLV:
            ParseAllToAllVDataDes(op_param, operation, rank_size);
            break;
        case checker::CheckerOpType::BATCH_SEND_RECV:
            ParseBatchSendRecvDataDes(op_param, operation, rank_size);
            break;
        default:
            throw std::runtime_error("operation type is not yet supported by llt_api");
    }

    return op_param;
}

void AppendEnvVars(const json &test_case, const char *key, llt::TestCaseConfig &config)
{
    if (!test_case.contains(key)) {
        return;
    }
    const json &env_json = test_case.at(key);
    if (!env_json.is_object()) {
        throw std::runtime_error(std::string(key) + " must be an object");
    }
    for (json::const_iterator it = env_json.begin(); it != env_json.end(); ++it) {
        if (!it.value().is_string()) {
            throw std::runtime_error(std::string(key) + " values must be strings");
        }
        config.env_keys.push_back(it.key());
        config.env_values.push_back(it.value().get<std::string>());
    }
}

llt::TestCaseConfig ParseTestCase(const json &test_case)
{
    if (!test_case.is_object()) {
        throw std::runtime_error("test case must be an object");
    }

    llt::TestCaseConfig config;
    config.name = RequireString(test_case, "name");
    if (!test_case.contains("topology") || !test_case.at("topology").is_object()) {
        throw std::runtime_error("test case requires topology object");
    }
    config.topo_meta = ParseTopology(test_case.at("topology"));
    const u32 rank_size = CountRanks(config.topo_meta);

    if (!test_case.contains("operation") || !test_case.at("operation").is_object()) {
        throw std::runtime_error("test case requires operation object");
    }
    const std::string device = HasField(test_case, "device") ? RequireString(test_case, "device") : "";
    config.op_param = ParseOperation(test_case.at("operation"), rank_size, device);

    AppendEnvVars(test_case, "env_vars", config);
    AppendEnvVars(test_case, "env", config);

    return config;
}

llt::LltConfig ParseJson(const json &root)
{
    if (!root.is_object()) {
        throw std::runtime_error("config root must be an object");
    }

    llt::LltConfig config;
    if (root.contains("output")) {
        const json &output = root.at("output");
        if (!output.is_object()) {
            throw std::runtime_error("output must be an object");
        }
        if (output.contains("directory")) {
            config.output.directory = RequireString(output, "directory");
        }
    }

    if (!root.contains("test_cases") || !root.at("test_cases").is_array()) {
        throw std::runtime_error("test_cases must be an array");
    }

    const json &test_cases = root.at("test_cases");
    if (test_cases.empty()) {
        throw std::runtime_error("test_cases must not be empty");
    }

    for (size_t i = 0; i < test_cases.size(); ++i) {
        config.test_cases.push_back(ParseTestCase(test_cases.at(i)));
    }
    return config;
}

}  // namespace

namespace llt {

LltConfig ConfigParser::ParseFile(const std::string &config_path)
{
    std::ifstream input(config_path.c_str());
    if (!input.is_open()) {
        throw std::runtime_error("failed to open config file: " + config_path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return ParseJsonString(buffer.str());
}

LltConfig ConfigParser::ParseJsonString(const std::string &json_string)
{
    return ParseJson(json::parse(json_string));
}

}  // namespace llt
