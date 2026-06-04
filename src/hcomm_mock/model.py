"""Behavior graph IR derived from HCOMM analysis_result protobuf."""

from __future__ import annotations

import json
import re
from collections import deque
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any

from google.protobuf.message import DecodeError

from .analysis_result_pb2 import AnalysisResult


DATA_NODE_TYPES = {
    "READ",
    "READ_REDUCE",
    "WRITE",
    "WRITE_REDUCE",
    "BEING_READ",
    "BEING_READ_REDUCE",
    "BEING_WRITTEN",
    "BEING_WRITTEN_REDUCE",
}

SLICE_PATTERN = re.compile(
    r"(srcSlice|dstSlice|localSlice|remoteSlice)=DataSlice\[BufferType::[A-Z_]+, offset=(\d+), size=(\d+)\]"
)
REMOTE_RANK_PATTERN = re.compile(r"remoteRank=(\d+)")


@dataclass
class Endpoint:
    rank: int | None = None
    offset: int = 0
    size: int = 0


@dataclass
class BehaviorNode:
    node_id: int
    rank_id: int
    node_type: str
    global_step: int
    local_step: int
    parents: list[int] = field(default_factory=list)
    children: list[int] = field(default_factory=list)
    link_type: str = ""
    data_size: int = 0
    src: Endpoint | None = None
    dst: Endpoint | None = None


@dataclass
class BehaviorGraph:
    graph_name: str
    rank_size: int
    nodes: list[BehaviorNode]

    @classmethod
    def from_protobuf(cls, binary_path: Path, graph_name: str = "whole") -> "BehaviorGraph":
        result = _load_analysis_result(binary_path)
        graph_container = _choose_graph_container(result, graph_name)
        nodes: list[BehaviorNode] = []
        for rank_graph in graph_container.rankGraphs:
            for node in rank_graph.nodes:
                node_type = _node_type_name(node)
                node_describe = getattr(node, "nodeDescribe", "")
                data_size_raw, src_offset_raw, dst_offset_raw = _parse_transfer_fields(node_type, node_describe)
                if _optional_field_is_set(node, "dataSize"):
                    data_size_raw = str(int(node.dataSize))
                src_rank_raw, dst_rank_raw = _parse_rank_endpoints(node, node_type, node_describe)
                data_size = int(data_size_raw or 0)
                behavior = BehaviorNode(
                    node_id=int(node.nodeId),
                    rank_id=int(node.rankId),
                    node_type=node_type,
                    global_step=int(node.globalStep),
                    local_step=int(node.localStep.localStep),
                    parents=[int(parent) for parent in node.parents],
                    children=[int(child) for child in node.children],
                    link_type=_link_type_name(node),
                    data_size=data_size,
                    src=_endpoint(src_rank_raw, src_offset_raw, data_size),
                    dst=_endpoint(dst_rank_raw, dst_offset_raw, data_size),
                )
                nodes.append(behavior)
        return cls(graph_name=graph_name, rank_size=int(result.rankSize), nodes=nodes)

    def write_json(self, path: Path) -> None:
        payload = {
            "graph_name": self.graph_name,
            "rank_size": self.rank_size,
            "nodes": [asdict(node) for node in self.nodes],
        }
        path.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    def node_by_id(self) -> dict[int, BehaviorNode]:
        return {node.node_id: node for node in self.nodes}

    def data_nodes(self, selected_ranks: set[int] | None = None) -> list[BehaviorNode]:
        selected_ranks = selected_ranks or set()
        nodes = [
            node for node in self.nodes
            if node.node_type in DATA_NODE_TYPES
            and node.data_size > 0
            and node.src is not None
            and node.dst is not None
            and (not selected_ranks or node.rank_id in selected_ranks)
        ]
        return sorted(nodes, key=lambda node: (node.global_step, node.rank_id, node.node_id))

    def phase_maps(self, emitted_nodes: list[BehaviorNode]) -> tuple[dict[int, int], dict[int, list[int]]]:
        node_by_id = self.node_by_id()
        emitted_ids = {node.node_id for node in emitted_nodes}
        phase_by_node_id: dict[int, int] = {}
        predecessors_by_node_id: dict[int, list[int]] = {}
        for node in emitted_nodes:
            predecessors = _find_emitted_predecessors(node, node_by_id, emitted_ids)
            predecessors_by_node_id[node.node_id] = predecessors
            phase_by_node_id[node.node_id] = 0 if not predecessors else max(phase_by_node_id[p] for p in predecessors) + 1
        return phase_by_node_id, predecessors_by_node_id


def _endpoint(rank_raw: str, offset_raw: str, size: int) -> Endpoint | None:
    if rank_raw == "":
        return None
    return Endpoint(rank=int(rank_raw), offset=int(offset_raw or 0), size=size)


def _load_analysis_result(binary_path: Path) -> AnalysisResult:
    result = AnalysisResult()
    try:
        result.ParseFromString(binary_path.read_bytes())
    except DecodeError as exc:
        raise RuntimeError(f"failed to parse protobuf file {binary_path}: {exc}") from exc
    return result


def _choose_graph_container(result: AnalysisResult, graph_name: str):
    field_name = "wholeGraph" if graph_name == "whole" else "bilateralGraph"
    if not result.HasField(field_name):
        raise RuntimeError(f"{field_name} is not present in the protobuf result")
    return getattr(result, field_name)


def _node_type_name(node) -> str:
    return node.DESCRIPTOR.fields_by_name["nodeType"].enum_type.values_by_number[node.nodeType].name


def _optional_field_is_set(message, field_name: str) -> bool:
    try:
        return message.HasField(field_name)
    except ValueError:
        return False


def _link_type_name(node) -> str:
    if not _optional_field_is_set(node, "linkType"):
        return ""
    return node.DESCRIPTOR.fields_by_name["linkType"].enum_type.values_by_number[node.linkType].name.replace("LINK_", "")


def _parse_transfer_fields(node_type: str, node_describe: str) -> tuple[str, str, str]:
    slices = {match.group(1): (match.group(2), match.group(3)) for match in SLICE_PATTERN.finditer(node_describe)}
    if node_type in {"LOCAL_COPY", "LOCAL_REDUCE"}:
        src = slices.get("srcSlice")
        dst = slices.get("dstSlice")
    elif node_type.startswith("READ") or node_type.startswith("BEING_READ"):
        src = slices.get("remoteSlice")
        dst = slices.get("localSlice")
    elif node_type.startswith("WRITE") or node_type.startswith("BEING_WRITTEN"):
        src = slices.get("localSlice")
        dst = slices.get("remoteSlice")
    else:
        src = None
        dst = None
    if src is None and dst is None:
        return "", "", ""
    src_offset = src[0] if src is not None else ""
    dst_offset = dst[0] if dst is not None else ""
    data_size = src[1] if src is not None else dst[1] if dst is not None else ""
    return data_size, src_offset, dst_offset


def _parse_remote_rank(node_describe: str) -> str:
    match = REMOTE_RANK_PATTERN.search(node_describe)
    return "" if match is None else match.group(1)


def _parse_rank_endpoints(node, node_type: str, node_describe: str) -> tuple[str, str]:
    local_rank = str(int(node.rankId))
    remote_rank = str(int(node.remoteRank)) if _optional_field_is_set(node, "remoteRank") else _parse_remote_rank(node_describe)
    if not remote_rank:
        return "", ""
    if node_type.startswith("READ") or node_type.startswith("BEING_WRITTEN") or node_type == "WAIT":
        return remote_rank, local_rank
    if node_type.startswith("WRITE") or node_type.startswith("BEING_READ") or node_type == "POST":
        return local_rank, remote_rank
    return "", ""


def _find_emitted_predecessors(
    node: BehaviorNode,
    node_by_id: dict[int, BehaviorNode],
    emitted_ids: set[int],
) -> list[int]:
    queue = deque(node.parents)
    visited: set[int] = set()
    predecessors: set[int] = set()
    while queue:
        node_id = queue.popleft()
        if node_id in visited:
            continue
        visited.add(node_id)
        if node_id in emitted_ids:
            predecessors.add(node_id)
            continue
        parent = node_by_id.get(node_id)
        if parent is not None:
            queue.extend(parent.parents)
    return sorted(predecessors)
