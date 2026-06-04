/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "llt_api.h"

#include <cerrno>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "checker.h"

namespace {

const char *kKnownEnvKeys[] = {
    "HCCL_HIGH_PERF_ENABLE",
    "HCCL_DETERMINISTIC",
    "HCCL_INTRA_PCIE_ENABLE",
    "HCCL_INTRA_ROCE_ENABLE",
    "HCCL_ALGO",
    "HCCL_BUFFSIZE",
    "HCCL_INTER_HCCS_DISABLE",
    "HCCL_OP_EXPANSION_MODE",
    "HCCL_CONCURRENT_ENABLE",
    "HCCL_DEBUG_CONFIG",
    "HCOMM_MOCK_AIV_KERNEL",
};

bool CreateDirectoryRecursively(const std::string &directory, std::string *error_message)
{
    if (directory.empty()) {
        if (error_message != nullptr) {
            *error_message = "output directory must not be empty";
        }
        return false;
    }

    if (access(directory.c_str(), F_OK) == 0) {
        return true;
    }

    std::string current;
    if (directory[0] == '/') {
        current = "/";
    }

    std::stringstream path_stream(directory);
    std::string segment;
    while (std::getline(path_stream, segment, '/')) {
        if (segment.empty()) {
            continue;
        }
        if (!current.empty() && current[current.size() - 1] != '/') {
            current += "/";
        }
        current += segment;
        if (access(current.c_str(), F_OK) == 0) {
            continue;
        }
        if (mkdir(current.c_str(), 0755) != 0 && errno != EEXIST) {
            if (error_message != nullptr) {
                *error_message = "failed to create output directory: " + current;
            }
            return false;
        }
    }
    return true;
}

std::string ToUpperAscii(std::string value)
{
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] >= 'a' && value[i] <= 'z') {
            value[i] = static_cast<char>(value[i] - 'a' + 'A');
        }
    }
    return value;
}

bool ContainsCaseInsensitive(const std::string &value, const std::string &needle)
{
    return ToUpperAscii(value).find(ToUpperAscii(needle)) != std::string::npos;
}

bool IsAivTestCase(const llt::TestCaseConfig &test_case)
{
    for (size_t i = 0; i < test_case.env_keys.size(); ++i) {
        if (test_case.env_keys[i] == "HCCL_OP_EXPANSION_MODE" &&
            ToUpperAscii(test_case.env_values[i]) == "AIV") {
            return true;
        }
    }
    return ContainsCaseInsensitive(test_case.op_param.algName, "AIV");
}

}  // namespace

namespace llt {

HcclResult LltApi::LoadConfig(const std::string &config_path)
{
    try {
        config_ = ConfigParser::ParseFile(config_path);
        last_error_.clear();
        return HCCL_SUCCESS;
    } catch (const std::exception &ex) {
        last_error_ = ex.what();
        return HCCL_E_PARA;
    }
}

HcclResult LltApi::LoadConfigFromJsonString(const std::string &json_string)
{
    try {
        config_ = ConfigParser::ParseJsonString(json_string);
        last_error_.clear();
        return HCCL_SUCCESS;
    } catch (const std::exception &ex) {
        last_error_ = ex.what();
        return HCCL_E_PARA;
    }
}

TestResult LltApi::Run(const std::string &test_name)
{
    for (size_t i = 0; i < config_.test_cases.size(); ++i) {
        if (config_.test_cases[i].name == test_name) {
            return RunTestCase(config_.test_cases[i]);
        }
    }

    TestResult result;
    result.test_name = test_name;
    result.result = HCCL_E_NOT_FOUND;
    result.error_message = "test case not found: " + test_name;
    last_error_ = result.error_message;
    return result;
}

std::vector<TestResult> LltApi::RunAll()
{
    std::vector<TestResult> results;
    for (size_t i = 0; i < config_.test_cases.size(); ++i) {
        results.push_back(RunTestCase(config_.test_cases[i]));
    }
    return results;
}

std::vector<std::string> LltApi::GetTestCaseNames() const
{
    std::vector<std::string> names;
    for (size_t i = 0; i < config_.test_cases.size(); ++i) {
        names.push_back(config_.test_cases[i].name);
    }
    return names;
}

std::string LltApi::GetOutputDirectory() const
{
    return config_.output.directory;
}

void LltApi::SetOutputDirectory(const std::string &directory)
{
    config_.output.directory = directory;
}

std::string LltApi::GetLastError() const
{
    return last_error_;
}

TestResult LltApi::RunTestCase(const TestCaseConfig &test_case)
{
    TestResult result;
    result.test_name = test_case.name;
    result.output_base = BuildOutputBase(config_.output.directory, test_case.name);
    result.text_file = result.output_base + ".txt";
    result.binary_file = result.output_base + "_binary.txt";

    std::string mkdir_error;
    if (!EnsureOutputDirectory(&mkdir_error)) {
        result.result = HCCL_E_SYSCALL;
        result.error_message = mkdir_error;
        last_error_ = result.error_message;
        return result;
    }

    std::set<std::string> env_key_set(kKnownEnvKeys, kKnownEnvKeys + sizeof(kKnownEnvKeys) / sizeof(kKnownEnvKeys[0]));
    for (size_t i = 0; i < test_case.env_keys.size(); ++i) {
        env_key_set.insert(test_case.env_keys[i]);
    }

    std::map<std::string, std::string> saved_values;
    std::vector<std::string> unset_keys;
    for (std::set<std::string>::const_iterator it = env_key_set.begin(); it != env_key_set.end(); ++it) {
        const char *current = getenv(it->c_str());
        if (current == nullptr) {
            unset_keys.push_back(*it);
        } else {
            saved_values[*it] = current;
        }
    }

    ClearKnownHcclEnv();
    checker::CheckerReset();

    for (size_t i = 0; i < test_case.env_keys.size(); ++i) {
        setenv(test_case.env_keys[i].c_str(), test_case.env_values[i].c_str(), 1);
    }
    if (IsAivTestCase(test_case)) {
        setenv("HCOMM_MOCK_AIV_KERNEL", "1", 1);
    }

    checker::Checker::SetDumpFileName(result.output_base);
    {
        checker::CheckerOpParam op_param = test_case.op_param;
        checker::TopoMeta topo_meta = test_case.topo_meta;
        checker::Checker checker;
        checker.EnableGraphicDump();
        checker.CloseRankMemCheck();
        result.result = checker.Check(op_param, topo_meta);
    }

    RestoreEnvSnapshot(saved_values, unset_keys);
    checker::CheckerReset();

    result.dump_generated = FileExists(result.text_file) && FileExists(result.binary_file);
    if (!result.dump_generated) {
        result.error_message = "analysis result files were not generated";
        if (result.result == HCCL_SUCCESS) {
            result.result = HCCL_E_INTERNAL;
        }
    } else if (result.result != HCCL_SUCCESS) {
        result.error_message = "checker finished with non-success status but generated dump files";
    }

    last_error_ = result.error_message;
    return result;
}

bool LltApi::EnsureOutputDirectory(std::string *error_message) const
{
    return CreateDirectoryRecursively(config_.output.directory, error_message);
}

void LltApi::ClearKnownHcclEnv() const
{
    for (size_t i = 0; i < sizeof(kKnownEnvKeys) / sizeof(kKnownEnvKeys[0]); ++i) {
        unsetenv(kKnownEnvKeys[i]);
    }
}

void LltApi::RestoreEnvSnapshot(const std::map<std::string, std::string> &values,
                                const std::vector<std::string> &unset_keys) const
{
    for (size_t i = 0; i < unset_keys.size(); ++i) {
        unsetenv(unset_keys[i].c_str());
    }
    for (std::map<std::string, std::string>::const_iterator it = values.begin(); it != values.end(); ++it) {
        setenv(it->first.c_str(), it->second.c_str(), 1);
    }
}

std::string LltApi::BuildOutputBase(const std::string &directory, const std::string &test_name)
{
    std::string base = directory;
    if (!base.empty() && base[base.size() - 1] != '/') {
        base += "/";
    }
    return base + "analysis_result_" + SanitizeFileComponent(test_name);
}

std::string LltApi::SanitizeFileComponent(const std::string &value)
{
    std::string sanitized = value;
    for (size_t i = 0; i < sanitized.size(); ++i) {
        const char ch = sanitized[i];
        const bool is_alpha_num = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
        if (!is_alpha_num && ch != '_' && ch != '-') {
            sanitized[i] = '_';
        }
    }
    return sanitized;
}

bool LltApi::FileExists(const std::string &path)
{
    return access(path.c_str(), F_OK) == 0;
}

}  // namespace llt
