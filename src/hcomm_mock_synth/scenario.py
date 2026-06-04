"""Scenario loading and normalization."""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


@dataclass(frozen=True)
class Scenario:
    name: str
    request: dict[str, Any]
    source_path: Path
    raw: dict[str, Any] = field(repr=False)


def load_scenario(path: Path, output_dir: Path) -> Scenario:
    raw = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(raw, dict):
        raise ValueError("scenario root must be an object")
    if "test_cases" in raw:
        return _load_native_llt(raw, path, output_dir)
    return _load_public_dsl(raw, path, output_dir)


def peek_scenario_name(path: Path) -> str:
    raw = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(raw, dict):
        return path.stem
    if "test_cases" in raw:
        test_cases = raw.get("test_cases")
        if isinstance(test_cases, list) and test_cases:
            first = test_cases[0]
            if isinstance(first, dict) and first.get("name"):
                return str(first["name"])
    if raw.get("name"):
        return str(raw["name"])
    return path.stem


def _load_native_llt(raw: dict[str, Any], path: Path, output_dir: Path) -> Scenario:
    test_cases = raw.get("test_cases")
    if not isinstance(test_cases, list) or not test_cases:
        raise ValueError("native LLT config requires a non-empty test_cases list")
    name = str(test_cases[0].get("name", "case"))
    request = dict(raw)
    request["output"] = {"directory": str(output_dir)}
    return Scenario(name=name, request=request, source_path=path.resolve(), raw=raw)


def _load_public_dsl(raw: dict[str, Any], path: Path, output_dir: Path) -> Scenario:
    for key in ("name", "operation", "topology"):
        if key not in raw:
            raise ValueError(f"missing required field: {key}")
    operation = dict(raw["operation"])
    algorithm = dict(raw.get("algorithm", {}))
    if "mode" in algorithm and "op_mode" not in operation:
        operation["op_mode"] = algorithm["mode"]
    if "name" in algorithm and "algorithm" not in operation:
        operation["algorithm"] = algorithm["name"]

    case: dict[str, Any] = {
        "name": str(raw["name"]),
        "operation": operation,
        "topology": dict(raw["topology"]),
    }
    if raw.get("env"):
        case["env"] = {str(key): str(value) for key, value in raw["env"].items()}
    if raw.get("device"):
        case["device"] = raw["device"]

    request = {
        "schema_version": int(raw.get("schema_version", 1)),
        "output": {"directory": str(output_dir)},
        "test_cases": [case],
    }
    return Scenario(name=str(raw["name"]), request=request, source_path=path.resolve(), raw=raw)
