# JSON 配置说明

本文档说明 `run_case.py run` 支持的 JSON 输入字段。工具支持两种输入格式：

- 原生 LLT 格式：根节点包含 `test_cases`，推荐使用，和 hcomm ST 请求最接近。
- 简化格式：根节点包含 `name`、`operation`、`topology`，工具会自动转换成原生 LLT 格式。

## 原生 LLT 格式

```json
{
  "output": {
    "directory": "./llt_output"
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
        "op_mode": "OFFLOAD"
      },
      "topology": {
        "super_pods": 1,
        "servers": 2,
        "ranks_per_server": 16
      },
      "device": "DEV_TYPE_910_93",
      "env_vars": {
        "HCCL_ALGO": "level0:NA;level1:NA"
      }
    }
  ]
}
```

执行时 `run_case.py` 会把 `output.directory` 改成当前 case 的输出目录，因此用户通常不需要关心这个字段。

## 简化格式

```json
{
  "name": "allreduce_test_offload_a3",
  "operation": {
    "type": "ALLREDUCE",
    "reduce_op": "SUM",
    "data_type": "FP32",
    "data_size": 25165824
  },
  "algorithm": {
    "mode": "OFFLOAD",
    "name": ""
  },
  "topology": {
    "super_pods": 1,
    "servers": 2,
    "ranks_per_server": 16
  },
  "device": "DEV_TYPE_910_93",
  "env": {
    "HCCL_ALGO": "level0:NA;level1:NA"
  }
}
```

简化格式会映射为：

- `algorithm.mode` -> `operation.op_mode`
- `algorithm.name` -> `operation.algorithm`
- `env` -> test case 的 `env`

## operation 字段

通用字段：

- `type`：必填，集合通信算子类型。
- `op_mode`：必填，`OPBASE` 或 `OFFLOAD`。
- `data_type`：必填，数据类型。
- `data_size`：按字节配置总数据量。若同时配置 `count`，优先使用 `count`。
- `count`：按元素个数配置数据量。
- `algorithm`：可选，指定 hcomm executor/算法名；空字符串表示让 hcomm 按当前环境变量和默认策略选择。
- `reduce_op`：可选，归约类型；未配置时默认 `SUM`。
- `tag`：可选，默认按算子类型生成。
- `dev_type`：可选，也可以使用 test case 顶层 `device`。
- `root`：`BROADCAST`、`REDUCE`、`SCATTER` 等 root 类算子使用。
- `src_rank`、`dst_rank`：`SEND`/`RECEIVE` 使用。

支持的 `type`：

- `ALLREDUCE`
- `ALLGATHER`
- `ALLGATHER_V` 或 `ALLGATHERV`
- `BROADCAST`
- `REDUCE`
- `REDUCE_SCATTER`
- `REDUCE_SCATTER_V` 或 `REDUCESCATTERV`
- `ALLTOALL`
- `ALLTOALLV`
- `ALLTOALLVC`
- `SCATTER`
- `SEND`
- `RECEIVE` 或 `RECV`
- `BATCH_SEND_RECV` 或 `BATCHSENDRECV`

支持的 `op_mode`：

- `OPBASE`
- `OFFLOAD`

支持的 `data_type`：

- `INT8`
- `INT16`
- `INT32`
- `INT64`
- `INT128`
- `UINT8`
- `UINT16`
- `UINT32`
- `UINT64`
- `FP16`
- `FP32`
- `FP64`
- `BFP16`
- `HIF8`
- `FP8E4M3`
- `FP8E5M2`

支持的 `reduce_op`：

- `SUM`
- `PROD`
- `MAX`
- `MIN`

支持的 `device` / `dev_type`：

- `910` 或 `DEV_TYPE_910`
- `310P1` 或 `DEV_TYPE_310P1`
- `310P3` 或 `DEV_TYPE_310P3`
- `910B` 或 `DEV_TYPE_910B`
- `910_93` 或 `DEV_TYPE_910_93`
- `950` 或 `DEV_TYPE_950`

## 算法名

`algorithm` 是传给 hcomm ST 的 executor/算法名，不是本工具内部枚举。它可以为空，也可以指定具体 executor。

已验证示例包括：

- `AllReduceRingFor91093Executor`
- `AllReduceSmallCountAivRdmaExecutor`
- `AllGatherMeshExecutor`
- `AllGatherVMeshExecutor`
- `BroadcastMeshAivExecutor`
- `ScatterMeshExecutor`
- `ReduceScatterVMeshOpbaseExecutor`
- `RunAlltoAllVFullMesh`

如果指定的算法名和当前算子、设备、拓扑或 `op_mode` 不匹配，hcomm ST 可能报错，或者生成的通信 trace 为空。判断方式看输出：

- `analysis_result.txt` 中结果应为 `CHECK_SUCCESS`。
- `trace.csv` 和 `traffic.csv` 不应只有表头。

## V 算子配置

`ALLGATHER_V` 和 `REDUCE_SCATTER_V` 支持：

- `counts`：每个 rank 的元素个数列表，长度必须等于总 rank 数。
- `displs`：每个 rank 的元素偏移列表，长度必须等于总 rank 数。

如果不配置 `counts`，会用 `count` 或 `data_size` 推导出统一 count，并复制到每个 rank。如果不配置 `displs`，会按 `counts` 自动生成连续偏移。

示例：

```json
{
  "type": "ALLGATHER_V",
  "op_mode": "OPBASE",
  "data_type": "FP16",
  "counts": [100, 100, 100, 100],
  "displs": [0, 100, 200, 300]
}
```

## AllToAll 配置

`ALLTOALL` 和 `ALLTOALLVC` 支持：

- `send_type`：可选，发送数据类型；默认使用 `data_type`。
- `recv_type`：可选，接收数据类型；默认使用 `data_type`。
- `send_count_matrix`：可选，长度必须等于 `rank_size * rank_size`。

如果不配置 `send_count_matrix`，会用 `count` 或 `data_size` 推导每个 peer 的统一发送量。

`ALLTOALLV` 支持：

- `send_type`：可选，发送数据类型；默认使用 `data_type`。
- `recv_type`：可选，接收数据类型；默认使用 `data_type`。
- `send_counts`：每个目标 rank 的发送元素个数，长度必须等于总 rank 数。
- `recv_counts`：每个源 rank 的接收元素个数，长度必须等于总 rank 数。
- `sdispls`：发送偏移，长度必须等于总 rank 数。
- `rdispls`：接收偏移，长度必须等于总 rank 数。

如果不配置 `send_counts`/`recv_counts`，会用 `count` 或 `data_size` 推导统一 peer count。如果不配置 `sdispls`/`rdispls`，会按 counts 自动生成连续偏移。

示例：

```json
{
  "type": "ALLTOALLV",
  "op_mode": "OPBASE",
  "data_type": "FP16",
  "send_counts": [100, 100, 100, 100],
  "recv_counts": [100, 100, 100, 100],
  "sdispls": [0, 100, 200, 300],
  "rdispls": [0, 100, 200, 300]
}
```

## Batch Send/Recv 配置

`BATCH_SEND_RECV` 支持可选字段 `all_ranks_send_recv_info`。它是一个按 rank 排列的二维数组，外层长度必须等于总 rank 数。

每个 item 支持：

- `direction`：`SEND`、`RECV` 或 `RECEIVE`。
- `remote_rank`：对端 rank。
- `data_type`：可选，默认使用 operation 的 `data_type`。
- `count`：可选，元素个数。
- `data_size`：可选，未配置 `count` 时按字节推导元素个数。

如果不配置 `all_ranks_send_recv_info`，会使用 hcomm ST 默认 batch send/recv 参数。

## topology 字段

默认拓扑是 uniform，可以省略 `kind`：

```json
{
  "super_pods": 1,
  "servers": 2,
  "ranks_per_server": 16
}
```

字段说明：

- `super_pods`：super pod 数。
- `servers`：每个 super pod 的 server 数。
- `servers_per_pod`：`servers` 的别名。
- `ranks_per_server`：每台 server 的 rank 数。

也支持显式拓扑：

```json
{
  "kind": "explicit",
  "super_pods": [
    [
      [0, 1, 2, 3],
      [4, 5, 6, 7]
    ]
  ]
}
```

显式拓扑结构是三层数组：

- 第一层：super pod。
- 第二层：server。
- 第三层：server 内的物理 device id。

## env / env_vars

`env` 和 `env_vars` 都支持，值必须是字符串。常用配置：

```json
{
  "env_vars": {
    "HCCL_ALGO": "level0:ring;level1:ring",
    "HCCL_OP_EXPANSION_MODE": "AIV"
  }
}
```

说明：

- `HCCL_ALGO` 用于影响 hcomm 算法选择，例如 ring、mesh、fullmesh 等。
- `HCCL_OP_EXPANSION_MODE=AIV` 会启用 AIV 相关路径；本工具会自动设置 mock 环境，避免真实启动 AIV kernel。

## 输出配置

原生 LLT 格式可以写：

```json
{
  "output": {
    "directory": "./llt_output"
  }
}
```

但通过 `run_case.py run` 执行时，实际输出目录由命令行控制：

```bash
python3 run_case.py run examples/llt_allreduce_test_offload_a3.json
python3 run_case.py run examples/llt_allreduce_test_offload_a3.json --out out/my_case
```

默认输出到 `out/<case_name>/`。
