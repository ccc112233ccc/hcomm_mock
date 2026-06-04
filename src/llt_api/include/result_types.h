/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCOMM_LLT_API_RESULT_TYPES_H
#define HCOMM_LLT_API_RESULT_TYPES_H

#include <string>
#include <vector>

#include "hccl_types.h"
#include "checker_def.h"
#include "topo_meta.h"

namespace llt {

struct OutputConfig {
    std::string directory = "./llt_output";
};

struct TestCaseConfig {
    std::string name;
    checker::CheckerOpParam op_param;
    checker::TopoMeta topo_meta;
    std::vector<std::string> env_keys;
    std::vector<std::string> env_values;
};

struct LltConfig {
    OutputConfig output;
    std::vector<TestCaseConfig> test_cases;
};

struct TestResult {
    std::string test_name;
    HcclResult result = HCCL_E_INTERNAL;
    bool dump_generated = false;
    std::string error_message;
    std::string output_base;
    std::string text_file;
    std::string binary_file;
};

}  // namespace llt

#endif
