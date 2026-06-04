/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCOMM_LLT_API_STRING_TO_ENUM_H
#define HCOMM_LLT_API_STRING_TO_ENUM_H

#include <string>

#include "checker_def.h"

namespace llt {

checker::CheckerOpType ParseOpType(const std::string &value);
checker::CheckerOpMode ParseOpMode(const std::string &value);
checker::CheckerDataType ParseDataType(const std::string &value);
checker::CheckerDevType ParseDevType(const std::string &value);
checker::CheckerReduceOp ParseReduceOp(const std::string &value);
std::string DefaultTagForOp(checker::CheckerOpType op_type);
std::string NormalizeUpper(const std::string &value);

}  // namespace llt

#endif
