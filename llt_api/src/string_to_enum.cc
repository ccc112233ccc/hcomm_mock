/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "string_to_enum.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <stdexcept>

namespace {

template <typename T>
T LookupOrThrow(const std::map<std::string, T> &mapping, const std::string &normalized, const std::string &kind)
{
    typename std::map<std::string, T>::const_iterator it = mapping.find(normalized);
    if (it == mapping.end()) {
        throw std::runtime_error("unsupported " + kind + ": " + normalized);
    }
    return it->second;
}

}  // namespace

namespace llt {

std::string NormalizeUpper(const std::string &value)
{
    std::string normalized = value;
    std::replace(normalized.begin(), normalized.end(), '-', '_');
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return normalized;
}

checker::CheckerOpType ParseOpType(const std::string &value)
{
    static const std::map<std::string, checker::CheckerOpType> kOpTypeMap = {
        {"ALLREDUCE", checker::CheckerOpType::ALLREDUCE},
        {"ALLGATHER", checker::CheckerOpType::ALLGATHER},
        {"ALLGATHER_V", checker::CheckerOpType::ALLGATHER_V},
        {"ALLGATHERV", checker::CheckerOpType::ALLGATHER_V},
        {"BROADCAST", checker::CheckerOpType::BROADCAST},
        {"REDUCE", checker::CheckerOpType::REDUCE},
        {"REDUCE_SCATTER", checker::CheckerOpType::REDUCE_SCATTER},
        {"REDUCE_SCATTER_V", checker::CheckerOpType::REDUCE_SCATTER_V},
        {"REDUCESCATTERV", checker::CheckerOpType::REDUCE_SCATTER_V},
        {"ALLTOALL", checker::CheckerOpType::ALLTOALL},
        {"ALLTOALLV", checker::CheckerOpType::ALLTOALLV},
        {"ALLTOALLVC", checker::CheckerOpType::ALLTOALLVC},
        {"SCATTER", checker::CheckerOpType::SCATTER},
        {"SEND", checker::CheckerOpType::SEND},
        {"RECEIVE", checker::CheckerOpType::RECEIVE},
        {"RECV", checker::CheckerOpType::RECEIVE},
        {"BATCH_SEND_RECV", checker::CheckerOpType::BATCH_SEND_RECV},
        {"BATCHSENDRECV", checker::CheckerOpType::BATCH_SEND_RECV},
    };
    return LookupOrThrow(kOpTypeMap, NormalizeUpper(value), "operation type");
}

checker::CheckerOpMode ParseOpMode(const std::string &value)
{
    static const std::map<std::string, checker::CheckerOpMode> kOpModeMap = {
        {"OPBASE", checker::CheckerOpMode::OPBASE},
        {"OFFLOAD", checker::CheckerOpMode::OFFLOAD},
    };
    return LookupOrThrow(kOpModeMap, NormalizeUpper(value), "operation mode");
}

checker::CheckerDataType ParseDataType(const std::string &value)
{
    static const std::map<std::string, checker::CheckerDataType> kDataTypeMap = {
        {"INT8", checker::CheckerDataType::DATA_TYPE_INT8},
        {"INT16", checker::CheckerDataType::DATA_TYPE_INT16},
        {"INT32", checker::CheckerDataType::DATA_TYPE_INT32},
        {"FP16", checker::CheckerDataType::DATA_TYPE_FP16},
        {"FP32", checker::CheckerDataType::DATA_TYPE_FP32},
        {"INT64", checker::CheckerDataType::DATA_TYPE_INT64},
        {"UINT64", checker::CheckerDataType::DATA_TYPE_UINT64},
        {"UINT8", checker::CheckerDataType::DATA_TYPE_UINT8},
        {"UINT16", checker::CheckerDataType::DATA_TYPE_UINT16},
        {"UINT32", checker::CheckerDataType::DATA_TYPE_UINT32},
        {"FP64", checker::CheckerDataType::DATA_TYPE_FP64},
        {"BFP16", checker::CheckerDataType::DATA_TYPE_BFP16},
        {"INT128", checker::CheckerDataType::DATA_TYPE_INT128},
        {"HIF8", checker::CheckerDataType::DATA_TYPE_HIF8},
        {"FP8E4M3", checker::CheckerDataType::DATA_TYPE_FP8E4M3},
        {"FP8E5M2", checker::CheckerDataType::DATA_TYPE_FP8E5M2},
    };
    return LookupOrThrow(kDataTypeMap, NormalizeUpper(value), "data type");
}

checker::CheckerDevType ParseDevType(const std::string &value)
{
    static const std::map<std::string, checker::CheckerDevType> kDevTypeMap = {
        {"910", checker::CheckerDevType::DEV_TYPE_910},
        {"DEV_TYPE_910", checker::CheckerDevType::DEV_TYPE_910},
        {"310P3", checker::CheckerDevType::DEV_TYPE_310P3},
        {"DEV_TYPE_310P3", checker::CheckerDevType::DEV_TYPE_310P3},
        {"910B", checker::CheckerDevType::DEV_TYPE_910B},
        {"DEV_TYPE_910B", checker::CheckerDevType::DEV_TYPE_910B},
        {"310P1", checker::CheckerDevType::DEV_TYPE_310P1},
        {"DEV_TYPE_310P1", checker::CheckerDevType::DEV_TYPE_310P1},
        {"910_93", checker::CheckerDevType::DEV_TYPE_910_93},
        {"DEV_TYPE_910_93", checker::CheckerDevType::DEV_TYPE_910_93},
        {"950", checker::CheckerDevType::DEV_TYPE_950},
        {"DEV_TYPE_950", checker::CheckerDevType::DEV_TYPE_950},
    };
    return LookupOrThrow(kDevTypeMap, NormalizeUpper(value), "device type");
}

checker::CheckerReduceOp ParseReduceOp(const std::string &value)
{
    static const std::map<std::string, checker::CheckerReduceOp> kReduceOpMap = {
        {"SUM", checker::CheckerReduceOp::REDUCE_SUM},
        {"PROD", checker::CheckerReduceOp::REDUCE_PROD},
        {"MAX", checker::CheckerReduceOp::REDUCE_MAX},
        {"MIN", checker::CheckerReduceOp::REDUCE_MIN},
    };
    return LookupOrThrow(kReduceOpMap, NormalizeUpper(value), "reduce op");
}

std::string DefaultTagForOp(checker::CheckerOpType op_type)
{
    switch (op_type) {
        case checker::CheckerOpType::ALLREDUCE:
            return "AllReduce";
        case checker::CheckerOpType::ALLGATHER:
            return "AllGather";
        case checker::CheckerOpType::ALLGATHER_V:
            return "AllGatherV";
        case checker::CheckerOpType::BROADCAST:
            return "Broadcast";
        case checker::CheckerOpType::REDUCE:
            return "Reduce";
        case checker::CheckerOpType::REDUCE_SCATTER:
            return "ReduceScatter";
        case checker::CheckerOpType::REDUCE_SCATTER_V:
            return "ReduceScatterV";
        case checker::CheckerOpType::ALLTOALL:
        case checker::CheckerOpType::ALLTOALLV:
        case checker::CheckerOpType::ALLTOALLVC:
            return "AllToAll";
        case checker::CheckerOpType::SCATTER:
            return "Scatter";
        case checker::CheckerOpType::SEND:
        case checker::CheckerOpType::RECEIVE:
            return "SendRecv";
        case checker::CheckerOpType::BATCH_SEND_RECV:
            return "BatchSendRecv";
        default:
            return "Unknown";
    }
}

}  // namespace llt
