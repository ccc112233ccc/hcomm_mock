set(LLT_API_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

set(LLT_API_SOURCES
    ${LLT_API_ROOT}/src/config_parser.cc
    ${LLT_API_ROOT}/src/llt_api.cc
    ${LLT_API_ROOT}/src/string_to_enum.cc
)

set(LLT_API_TEST_SOURCES
    ${LLT_API_ROOT}/src/llt_api_smoke_test.cc
)

add_library(llt_api STATIC ${LLT_API_SOURCES})

target_include_directories(llt_api PRIVATE
    ${LLT_API_ROOT}/include
    ${HCOMM_CODE_ROOT}/third_party/json/single_include
    ${UTILS_PUBLIC_INCLUDE_LIST}
    ${UTILS_INNER_INCLUDE_LIST}
    ${HCOMM_PUBLIC_INCLUDE_LIST}
    ${HCOMM_PLATFORM_INCLUDE_LIST}
    ${HCOMM_ALGORITHM_INCLUDE_LIST}
    ${HCOMM_COMMON_INCLUDE_LIST}
    ${HCOMM_FRAMEWORK_INCLUDE_LIST}
    ${HCOMM_LEGACY_INCLUDE_LIST}
    ${CMAKE_BINARY_DIR}/proto/alg_analyzer_proto/proto
    ${ASCEND_CANN_INCLUDE_LIST}
)

target_compile_definitions(llt_api PRIVATE
    google=ascend_private
    -DOPEN_HCCL_TEST
    _GLIBCXX_USE_CXX11_ABI=0
)

target_compile_options(llt_api PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O2
    -pipe
)

target_link_libraries(llt_api PRIVATE
    hccl_alg_analyzer
    -pthread
)

add_executable(llt_api_smoke_test ${LLT_API_TEST_SOURCES})
target_include_directories(llt_api_smoke_test PRIVATE
    ${LLT_API_ROOT}/include
    ${HCOMM_CODE_ROOT}/third_party/json/single_include
    ${UTILS_PUBLIC_INCLUDE_LIST}
    ${UTILS_INNER_INCLUDE_LIST}
    ${HCOMM_PUBLIC_INCLUDE_LIST}
    ${HCOMM_PLATFORM_INCLUDE_LIST}
    ${HCOMM_ALGORITHM_INCLUDE_LIST}
    ${HCOMM_COMMON_INCLUDE_LIST}
    ${HCOMM_FRAMEWORK_INCLUDE_LIST}
    ${HCOMM_LEGACY_INCLUDE_LIST}
    ${CMAKE_BINARY_DIR}/proto/alg_analyzer_proto/proto
    ${ASCEND_CANN_INCLUDE_LIST}
)
target_compile_definitions(llt_api_smoke_test PRIVATE
    google=ascend_private
    -DOPEN_HCCL_TEST
    _GLIBCXX_USE_CXX11_ABI=0
)
target_compile_options(llt_api_smoke_test PRIVATE
    -U_FORTIFY_SOURCE
    -Os
    -O0 -g
)
target_link_libraries(llt_api_smoke_test PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    llt_api
    GTest::gtest_main
    -pthread
)
add_test(NAME llt_api_smoke_test COMMAND llt_api_smoke_test)

if(DEFINED ENV{CONDA_PREFIX})
    file(GLOB _pybind11_config_dirs "$ENV{CONDA_PREFIX}/lib/python*/site-packages/pybind11/share/cmake/pybind11")
    if(_pybind11_config_dirs)
        list(APPEND CMAKE_PREFIX_PATH ${_pybind11_config_dirs})
    endif()
endif()

find_package(pybind11 CONFIG QUIET)

if(pybind11_FOUND)
    pybind11_add_module(_llt_api MODULE ${LLT_API_ROOT}/src/pybind_llt_api.cc)
    target_include_directories(_llt_api PRIVATE
        ${LLT_API_ROOT}/include
        ${HCOMM_CODE_ROOT}/third_party/json/single_include
        ${UTILS_PUBLIC_INCLUDE_LIST}
        ${UTILS_INNER_INCLUDE_LIST}
        ${HCOMM_PUBLIC_INCLUDE_LIST}
        ${HCOMM_PLATFORM_INCLUDE_LIST}
        ${HCOMM_ALGORITHM_INCLUDE_LIST}
        ${HCOMM_COMMON_INCLUDE_LIST}
        ${HCOMM_FRAMEWORK_INCLUDE_LIST}
        ${HCOMM_LEGACY_INCLUDE_LIST}
        ${CMAKE_BINARY_DIR}/proto/alg_analyzer_proto/proto
        ${ASCEND_CANN_INCLUDE_LIST}
    )
    target_compile_definitions(_llt_api PRIVATE
        _GLIBCXX_USE_CXX11_ABI=0
    )
    target_link_libraries(_llt_api PRIVATE
        llt_api
    )
endif()
