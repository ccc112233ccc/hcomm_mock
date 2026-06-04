# 开发说明

这份文档记录仓库内部结构、hcomm overlay 维护流程和 JSON 配置。普通使用只需要看根目录 `README.md`。

## 架构边界

本仓库不是完全脱离 hcomm 的独立算法实现。它复用 hcomm ST 的算法实现、checker、protobuf schema 和 `hccl_alg_analyzer`，在此基础上增加：

- JSON 运行入口。
- DAG 结构化通信元数据。
- 瘦身 protobuf 输出。
- rank-to-rank trace 导出。
- ns3-ub traffic 导出。
- AIV mock 分析路径。

`src/llt_api/` 保留为 C++/pybind 模块源码，编译时由 `run_case.py build` 接入 hcomm 的 CMake 构建。

## 基线

- 官方 hcomm 基线 commit：`9029b29b2aa457c4a84799afd67891cde362579a`
- 当前 mock 目标 commit：`c9e348143`
- 默认官方仓库：`https://gitcode.com/cann/hcomm.git`

## 关键目录

- `patches/modified_files.patch`：对 hcomm 已有文件的 patch。
- `src/llt_api/`：C++/pybind 模块源码，只保留构建必要文件。
- `src/hcomm_mock/`：Python 正式工具链。
- `examples/`：输入 case。
- `run_case.py prepare`：拉取或修改 hcomm 仓库并应用 patch。
- `run_case.py build`：在 hcomm 构建目录中编译 `_llt_api` 和 smoke test。
- `run_case.py sync`：从已修改的 hcomm 工作树更新本仓库的 overlay、`llt_api` 和 examples。

## AIV 支持边界

当前 AIV 是 mock 分析路径：

- 指定 AIV executor，或设置 `HCCL_OP_EXPANSION_MODE=AIV` 时，会自动启用 `HCOMM_MOCK_AIV_KERNEL=1`。
- 不会真实查找或启动 AIV kernel。
- 只要 ST DAG 中暴露了 `READ`、`WRITE`、`READ_REDUCE`、`WRITE_REDUCE` 等跨 rank task，就可以导出 rank-to-rank trace/traffic。
- 如果 AIV 通信完全封装在 kernel 内部且 ST DAG 没有跨 rank task，case 可能成功，但 `trace.csv` 和 `traffic.csv` 可能只有表头。

已验证：

- `AllReduceSmallCountAivRdmaExecutor`：可导出跨 rank traffic。
- `BroadcastMeshAivExecutor`：可跑通，但当前 traffic 为空。

## Overlay 同步流程

本仓库是独立工具仓库，不是旧式迁移包。它仍然需要维护一份 `patches/modified_files.patch`，用于在自动拉取的官方 hcomm 上应用 proto、data dumper、AIV mock、CMake 等必要改动。

`sync` 只用于开发维护：当你在某个 hcomm 工作树中继续修改了这些 overlay 相关代码后，可以把差异重新同步回本仓库：

```bash
python3 run_case.py sync \
  --hcomm /path/to/hcomm \
  --base 9029b29b2aa457c4a84799afd67891cde362579a \
  --target HEAD
```

同步内容包括：

- `src/llt_api/CMakeLists.txt`
- `src/llt_api/include/`
- `src/llt_api/src/`
- `examples/llt_*.json`
- `patches/modified_files.patch`
- `metadata.txt`

`src/llt_api` 下旧 Python 工具不会再同步。运行、导出和可视化统一由 `src/hcomm_mock/` 和根目录 `run_case.py` 负责。

## JSON 输入

示例输入：

```json
{
  "output": {
    "directory": "./llt_output/allreduce_test_offload_a3_bundle"
  },
  "test_cases": [
    {
      "name": "allreduce_test_offload_a3",
      "operation": {
        "type": "ALLREDUCE",
        "algorithm": "",
        "reduce_op": "SUM",
        "data_type": "FP32",
        "data_size": 25165824,
        "op_mode": "OFFLOAD",
        "dev_type": "910_93"
      },
      "topology": {
        "super_pods": 1,
        "servers": 2,
        "ranks_per_server": 16
      },
      "env_vars": {
        "HCCL_ALGO": "level0:NA;level1:NA"
      }
    }
  ]
}
```

兼容字段：

- `topology.servers` 和 `topology.servers_per_pod` 都支持。
- `env_vars` 和 `env` 都支持。
- 顶层 `device` 和 `operation.dev_type` 都支持。
- `topology.kind` 可省略，默认按 uniform 拓扑处理。
