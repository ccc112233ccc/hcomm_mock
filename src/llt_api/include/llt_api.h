/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCOMM_LLT_API_H
#define HCOMM_LLT_API_H

#include <map>
#include <string>
#include <vector>

#include "config_parser.h"

namespace llt {

class LltApi {
public:
    HcclResult LoadConfig(const std::string &config_path);
    HcclResult LoadConfigFromJsonString(const std::string &json_string);

    TestResult Run(const std::string &test_name);
    std::vector<TestResult> RunAll();

    std::vector<std::string> GetTestCaseNames() const;
    std::string GetOutputDirectory() const;
    void SetOutputDirectory(const std::string &directory);
    std::string GetLastError() const;

private:
    TestResult RunTestCase(const TestCaseConfig &test_case);
    bool EnsureOutputDirectory(std::string *error_message) const;
    void ClearKnownHcclEnv() const;
    void RestoreEnvSnapshot(const std::map<std::string, std::string> &values,
                            const std::vector<std::string> &unset_keys) const;
    static std::string BuildOutputBase(const std::string &directory, const std::string &test_name);
    static std::string SanitizeFileComponent(const std::string &value);
    static bool FileExists(const std::string &path);

    LltConfig config_;
    std::string last_error_;
};

}  // namespace llt

#endif
