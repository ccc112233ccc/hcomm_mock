/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "llt_api.h"

namespace py = pybind11;

PYBIND11_MODULE(_llt_api, module)
{
    module.doc() = "Lightweight LLT API for hcomm ST algorithm analysis";

    py::enum_<HcclResult>(module, "HcclResult")
        .value("HCCL_SUCCESS", HCCL_SUCCESS)
        .value("HCCL_E_PARA", HCCL_E_PARA)
        .value("HCCL_E_PTR", HCCL_E_PTR)
        .value("HCCL_E_MEMORY", HCCL_E_MEMORY)
        .value("HCCL_E_INTERNAL", HCCL_E_INTERNAL)
        .value("HCCL_E_NOT_SUPPORT", HCCL_E_NOT_SUPPORT)
        .value("HCCL_E_NOT_FOUND", HCCL_E_NOT_FOUND)
        .value("HCCL_E_UNAVAIL", HCCL_E_UNAVAIL)
        .value("HCCL_E_SYSCALL", HCCL_E_SYSCALL)
        .export_values();

    py::class_<llt::TestResult>(module, "TestResult")
        .def_readonly("test_name", &llt::TestResult::test_name)
        .def_readonly("result", &llt::TestResult::result)
        .def_readonly("dump_generated", &llt::TestResult::dump_generated)
        .def_readonly("error_message", &llt::TestResult::error_message)
        .def_readonly("output_base", &llt::TestResult::output_base)
        .def_readonly("text_file", &llt::TestResult::text_file)
        .def_readonly("binary_file", &llt::TestResult::binary_file);

    py::class_<llt::LltApi>(module, "LltApi")
        .def(py::init<>())
        .def("load_config", &llt::LltApi::LoadConfig)
        .def("load_config_from_json_string", &llt::LltApi::LoadConfigFromJsonString)
        .def("run", &llt::LltApi::Run)
        .def("run_all", &llt::LltApi::RunAll)
        .def("get_test_case_names", &llt::LltApi::GetTestCaseNames)
        .def("get_output_directory", &llt::LltApi::GetOutputDirectory)
        .def("set_output_directory", &llt::LltApi::SetOutputDirectory)
        .def("get_last_error", &llt::LltApi::GetLastError);
}
