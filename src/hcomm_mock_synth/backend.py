"""Backend invocation through the built _llt_api module."""

from __future__ import annotations

import json
import shutil
import sys
from pathlib import Path
from typing import Any

from .scenario import Scenario


class CheckerBackend:
    def __init__(self, module_dir: Path) -> None:
        self.module_dir = module_dir.resolve()

    def run(self, scenario: Scenario, out_dir: Path, test_name: str | None = None) -> dict[str, Path]:
        out_dir.mkdir(parents=True, exist_ok=True)
        request_path = out_dir / "checker_request.json"
        request_path.write_text(json.dumps(scenario.request, indent=2), encoding="utf-8")
        shutil.copy2(scenario.source_path, out_dir / "input.case.json")

        _llt_api = self._import_llt_api()
        api = _llt_api.LltApi()
        rc = api.load_config(str(request_path))
        if int(rc) != int(_llt_api.HCCL_SUCCESS):
            raise RuntimeError(f"load_config failed: {api.get_last_error()}")

        names = api.get_test_case_names()
        if not names:
            raise RuntimeError("no testcase found in checker request")
        selected_name = test_name or scenario.name or names[0]
        result = api.run(selected_name)
        if int(result.result) != int(_llt_api.HCCL_SUCCESS):
            raise RuntimeError(f"checker failed: {result.error_message or api.get_last_error()}")

        binary = self._resolve_runtime_path(result.binary_file)
        text = self._resolve_runtime_path(result.text_file)
        if not binary.exists():
            raise RuntimeError(f"analysis protobuf was not generated: {binary}")

        analysis_pb = out_dir / "analysis_result.pb"
        shutil.copy2(binary, analysis_pb)
        artifacts: dict[str, Path] = {"analysis_pb": analysis_pb}
        if text.exists():
            analysis_txt = out_dir / "analysis_result.txt"
            shutil.copy2(text, analysis_txt)
            artifacts["analysis_txt"] = analysis_txt

        sim_pb = out_dir / "analysis_result_sim.pb"
        self._write_sim_binary(analysis_pb, sim_pb)
        artifacts["analysis_sim_pb"] = sim_pb
        return artifacts

    def _import_llt_api(self):
        if not self.module_dir.exists():
            raise RuntimeError(f"_llt_api module dir does not exist: {self.module_dir}")
        sys.path.insert(0, str(self.module_dir))
        try:
            import _llt_api  # type: ignore
        except ImportError as exc:
            raise RuntimeError(f"failed to import _llt_api from {self.module_dir}") from exc
        return _llt_api

    @staticmethod
    def _resolve_runtime_path(path_str: str) -> Path:
        path = Path(path_str)
        return path if path.is_absolute() else Path.cwd() / path

    @staticmethod
    def _write_sim_binary(input_pb: Path, output_pb: Path) -> None:
        from .analysis_result_pb2 import AnalysisResult

        result = AnalysisResult()
        result.ParseFromString(input_pb.read_bytes())
        result.ClearField("rankStates")
        output_pb.write_bytes(result.SerializeToString())
