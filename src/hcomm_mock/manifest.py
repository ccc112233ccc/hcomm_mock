"""Manifest writer."""

from __future__ import annotations

import json
from pathlib import Path

from .scenario import Scenario


def write_manifest(path: Path, scenario: Scenario, artifacts: dict[str, Path | str | int]) -> None:
    payload = {
        "schema_version": 1,
        "case_name": scenario.name,
        "source": str(scenario.source_path),
        "artifacts": {key: str(value) for key, value in artifacts.items()},
    }
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
