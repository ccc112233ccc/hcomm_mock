"""Export behavior graph artifacts."""

from __future__ import annotations

import csv
from pathlib import Path

from .model import BehaviorGraph, BehaviorNode


TRACE_COLUMNS = [
    "taskID",
    "sourceRank",
    "destRank",
    "dataSize(Byte)",
    "opType",
    "linkType",
    "priority",
    "delay",
    "phaseId",
    "dependOnPhases",
    "srcOffset",
    "dstOffset",
]

TRAFFIC_COLUMNS = [
    "taskId",
    "sourceNode",
    "destNode",
    "dataSize(Byte)",
    "PhysicalAddress",
    "opType",
    "priority",
    "delay",
    "phaseId",
    "dependOnPhases",
]


def parse_rank_filter(raw: str) -> set[int]:
    if not raw.strip():
        return set()
    return {int(part.strip()) for part in raw.split(",") if part.strip()}


def export_trace_csv(graph: BehaviorGraph, path: Path, selected_ranks: set[int]) -> int:
    nodes = graph.data_nodes(selected_ranks)
    phase_by_node_id, predecessors_by_node_id = graph.phase_maps(nodes)
    nodes.sort(key=lambda node: (phase_by_node_id[node.node_id], node.global_step, node.rank_id, node.node_id))
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=TRACE_COLUMNS)
        writer.writeheader()
        for task_id, node in enumerate(nodes):
            writer.writerow(_trace_row(task_id, node, phase_by_node_id, predecessors_by_node_id))
    return len(nodes)


def export_traffic_csv(
    graph: BehaviorGraph,
    path: Path,
    selected_ranks: set[int],
    base_address: int = 0x8000_0000,
    priority: int = 7,
    delay: str = "0ns",
) -> int:
    nodes = graph.data_nodes(selected_ranks)
    phase_by_node_id, predecessors_by_node_id = graph.phase_maps(nodes)
    nodes.sort(key=lambda node: (phase_by_node_id[node.node_id], node.global_step, node.rank_id, node.node_id))
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=TRAFFIC_COLUMNS)
        writer.writeheader()
        for task_id, node in enumerate(nodes):
            writer.writerow(_traffic_row(task_id, node, phase_by_node_id, predecessors_by_node_id, base_address, priority, delay))
    return len(nodes)


def export_dot(graph: BehaviorGraph, path: Path, selected_ranks: set[int]) -> None:
    included = {
        node.node_id for node in graph.nodes
        if not selected_ranks or node.rank_id in selected_ranks
    }
    lines = ["digraph behavior_graph {", "  rankdir=LR;"]
    for node in graph.nodes:
        if node.node_id not in included:
            continue
        label = f"r{node.rank_id} n{node.node_id}\\n{node.node_type}\\nstep {node.global_step}"
        lines.append(f'  n{node.node_id} [label="{label}"];')
    for node in graph.nodes:
        if node.node_id not in included:
            continue
        for child in node.children:
            if child in included:
                lines.append(f"  n{node.node_id} -> n{child};")
    lines.append("}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _trace_row(
    task_id: int,
    node: BehaviorNode,
    phase_by_node_id: dict[int, int],
    predecessors_by_node_id: dict[int, list[int]],
) -> dict[str, str | int]:
    dependency_phases = _dependency_phases(node, phase_by_node_id, predecessors_by_node_id)
    return {
        "taskID": task_id,
        "sourceRank": "" if node.src is None else node.src.rank,
        "destRank": "" if node.dst is None else node.dst.rank,
        "dataSize(Byte)": node.data_size,
        "opType": node.node_type,
        "linkType": node.link_type,
        "priority": 0,
        "delay": 0,
        "phaseId": phase_by_node_id[node.node_id],
        "dependOnPhases": ",".join(str(phase) for phase in dependency_phases),
        "srcOffset": "" if node.src is None else node.src.offset,
        "dstOffset": "" if node.dst is None else node.dst.offset,
    }


def _traffic_row(
    task_id: int,
    node: BehaviorNode,
    phase_by_node_id: dict[int, int],
    predecessors_by_node_id: dict[int, list[int]],
    base_address: int,
    priority: int,
    delay: str,
) -> dict[str, str | int]:
    dependency_phases = _dependency_phases(node, phase_by_node_id, predecessors_by_node_id)
    src_offset = 0 if node.src is None else node.src.offset
    return {
        "taskId": task_id,
        "sourceNode": "" if node.src is None else node.src.rank,
        "destNode": "" if node.dst is None else node.dst.rank,
        "dataSize(Byte)": node.data_size,
        "PhysicalAddress": hex(base_address + src_offset),
        "opType": "URMA_WRITE",
        "priority": priority,
        "delay": delay,
        "phaseId": phase_by_node_id[node.node_id],
        "dependOnPhases": " ".join(str(phase) for phase in dependency_phases),
    }


def _dependency_phases(
    node: BehaviorNode,
    phase_by_node_id: dict[int, int],
    predecessors_by_node_id: dict[int, list[int]],
) -> list[int]:
    return sorted({phase_by_node_id[p] for p in predecessors_by_node_id[node.node_id]})
