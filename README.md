# hcomm mock trace 工具

这个工具用于在没有真实 NPU 的环境下运行 hcomm ST 算法分析，生成可用于网络仿真的通信 trace。

主要能力：

- 通过 JSON 配置集合通信算子、算法、拓扑、数据类型和数据量。
- 调用 hcomm ST 生成 DAG 分析结果。
- 导出 rank-to-rank `trace.csv`。
- 导出 ns3-ub 可用的 `traffic.csv`。
- 导出 `graph.dot`，用于后续生成 SVG/PNG 可视化图。
- 支持 AIV mock 路径，避免无 NPU 环境下真实启动 AIV kernel。

## 目录结构

- `examples/`：示例 case。
- `src/llt_api/`：接入 hcomm ST 的 C++/pybind 模块源码。
- `src/hcomm_mock/`：Python 包，负责运行 case、解析 DAG、导出 trace/traffic。
- `patches/`：应用到官方 hcomm 的 overlay patch。
- `run_case.py`：推荐运行入口。
- `docs/`：开发和 overlay 维护说明。

## 环境准备

需要先准备可编译 hcomm ST 的环境：

- conda 环境中安装 Python 3.11。
- conda 环境中安装 CANN Toolkit。
- conda 环境中安装 `pybind11`。
- 系统中已有 `cmake`、`make`、C++ 编译器等基础编译工具。

示例：

```bash
conda create -n py311 python=3.11 -y
conda activate py311
conda config --add channels https://repo.huaweicloud.com/ascend/repos/conda/
conda install ascend::cann-toolkit==9.1.0.beta.1 pybind11 -y
source "$CONDA_PREFIX/Ascend/ascend-toolkit/set_env.sh"
```

## 编译

```bash
conda activate py311
source "$CONDA_PREFIX/Ascend/ascend-toolkit/set_env.sh"

python3 run_case.py build
```

如果本仓库内没有 `hcomm/` 目录，会默认拉取并准备到 `hcomm/`。

可以先运行 smoke test：

```bash
python3 run_case.py smoke
```

## 运行

普通 AllReduce 示例：

```bash
python3 run_case.py run examples/llt_allreduce_test_offload_a3.json
```

AIV 示例：

```bash
python3 run_case.py run examples/llt_allreduce_aiv_rdma_910b.json
```

## 输出

默认输出到 `out/<case_name>/`，也可以用 `--out` 指定目录。输出内容包括：

- `input.case.json`：原始输入 case。
- `checker_request.json`：实际传给 hcomm ST 的请求。
- `analysis_result.pb`：原始 protobuf DAG。
- `analysis_result.txt`：文本 DAG。
- `analysis_result_sim.pb`：去掉 `rankStates` 的瘦身 protobuf。
- `behavior_graph.json`：统一内部行为图。
- `trace.csv`：rank-to-rank 通信 trace。
- `traffic.csv`：ns3-ub traffic。
- `graph.dot`：DOT 可视化图。
- `manifest.json`：本次运行的产物索引。

判断一个 case 是否可用于网络仿真：

- `analysis_result.txt` 中结果为 `CHECK_SUCCESS`。
- `trace.csv` 不只有表头。
- `traffic.csv` 不只有表头。

## 可视化

`run_case.py` 默认生成 `graph.dot`。如需 SVG：

```bash
dot -Tsvg out/allreduce_test_offload_a3/graph.dot \
  -o out/allreduce_test_offload_a3/graph.svg
```

## 更多说明

- JSON 输入字段、支持的算子类型、数据类型、设备类型和拓扑配置见 [docs/configuration.md](docs/configuration.md)。
- AIV 行为、overlay 同步和维护流程见 [docs/development.md](docs/development.md)。
