"""Command line entrypoint."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any

from .scenario import load_scenario, peek_scenario_name

REPO_ROOT = Path(__file__).resolve().parents[2]
CONFIG_PATH = REPO_ROOT / ".hcomm-mock.json"
DEFAULT_BUILD_DIR = "build_st"
DEFAULT_HCOMM_DIR = REPO_ROOT / "hcomm"
DEFAULT_BASE_COMMIT = "9029b29b2aa457c4a84799afd67891cde362579a"
DEFAULT_HCOMM_URL = "https://gitcode.com/cann/hcomm.git"
PATCHED_HCOMM_FILES = [
    "src/algorithm/impl/hccl_aiv.cc",
    "test/st/algorithm/CMakeLists.txt",
    "test/st/algorithm/utils/checker/semantics_check/task_check_op_semantics.cc",
    "test/st/algorithm/utils/checker/ui/data_dumper/data_dumper.cc",
    "test/st/algorithm/utils/checker/ui/proto/analysis_result.proto",
]


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    argv = list(sys.argv[1:] if argv is None else argv)
    commands = {"init", "prepare", "build", "smoke", "run", "sync", "-h", "--help"}
    if argv and argv[0] not in commands:
        argv.insert(0, "run")

    parser = argparse.ArgumentParser(description="Run HCOMM mock trace tooling.")
    subparsers = parser.add_subparsers(dest="command")

    init_parser = subparsers.add_parser("init", help="Initialize local hcomm config.")
    add_hcomm_args(init_parser)
    add_prepare_args(init_parser, suppress_help=True)

    prepare_parser = subparsers.add_parser("prepare", help="Prepare a patched hcomm repository.")
    add_prepare_args(prepare_parser)

    build_parser = subparsers.add_parser("build", help="Build _llt_api in hcomm.")
    add_hcomm_args(build_parser)
    build_parser.add_argument("-j", "--jobs", default="6", help="Parallel build jobs.")

    smoke_parser = subparsers.add_parser("smoke", help="Run llt_api_smoke_test.")
    add_hcomm_args(smoke_parser)

    run_parser = subparsers.add_parser("run", help="Run a scenario and export artifacts.")
    run_parser.add_argument("case", type=Path, help="Scenario JSON or native LLT JSON config.")
    run_parser.add_argument("--out", type=Path, help="Output directory. Default: out/<case_name>.")
    add_hcomm_args(run_parser)
    run_parser.add_argument("--module-dir", type=Path, help="Directory containing built _llt_api module.")
    run_parser.add_argument("--test-name", default="", help="Override testcase name.")
    run_parser.add_argument("--graph", choices=("whole", "bilateral"), default="whole")
    run_parser.add_argument("--rank", default="", help="Comma-separated rank filter.")
    run_parser.add_argument("--no-dot", action="store_true")
    run_parser.add_argument("--no-trace", action="store_true")
    run_parser.add_argument("--no-traffic", action="store_true")
    run_parser.add_argument("--base-address", default="0x80000000")

    sync_parser = subparsers.add_parser("sync", help="Sync this repository from a patched hcomm tree.")
    add_hcomm_args(sync_parser)
    sync_parser.add_argument("--base", default=DEFAULT_BASE_COMMIT)
    sync_parser.add_argument("--target", default="HEAD")

    args = parser.parse_args(argv)
    if args.command is None:
        parser.print_help()
        raise SystemExit(2)
    return args


def add_hcomm_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--hcomm", type=Path, help="hcomm repository path.")
    parser.add_argument("--build-dir", default="", help=f"Build directory. Default: {DEFAULT_BUILD_DIR}.")


def add_prepare_args(parser: argparse.ArgumentParser, suppress_help: bool = False) -> None:
    help_value = argparse.SUPPRESS if suppress_help else None
    parser.add_argument("--repo", type=Path, help=help_value or "Existing local hcomm git repository.")
    parser.add_argument("--dest", type=Path, default=None, help=help_value or "Clone destination.")
    parser.add_argument("--url", default=DEFAULT_HCOMM_URL, help=help_value or "hcomm git URL.")
    parser.add_argument("--base", default=DEFAULT_BASE_COMMIT, help=help_value or "Base hcomm commit.")
    parser.add_argument("--branch", default="", help=help_value or "Branch to create.")
    parser.add_argument("--commit", action="store_true", help=help_value or "Create one commit after applying patch.")


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if args.command == "init":
        return init_config(args)
    if args.command == "prepare":
        return prepare_hcomm(args)
    if args.command == "build":
        return build_hcomm(args)
    if args.command == "smoke":
        return run_smoke(args)
    if args.command == "run":
        return run_case(args)
    if args.command == "sync":
        return sync_from_hcomm(args)
    raise RuntimeError(f"unsupported command: {args.command}")


def init_config(args: argparse.Namespace) -> int:
    if getattr(args, "repo", None) or getattr(args, "dest", None):
        return prepare_hcomm(args)
    hcomm = resolve_hcomm(args, required=False)
    if not is_hcomm_repo(hcomm):
        hcomm = prompt_hcomm_path()
    build_dir = resolve_build_dir(args)
    write_config({"hcomm": str(hcomm), "build_dir": build_dir})
    print(f"hcomm={hcomm}")
    print(f"build_dir={build_dir}")
    print(f"config={CONFIG_PATH}")
    return 0


def build_hcomm(args: argparse.Namespace) -> int:
    hcomm = resolve_hcomm_or_prepare(args)
    build_dir = resolve_build_dir(args)
    ensure_compatible_cmake_cache(hcomm, build_dir)
    remove_stale_third_party_cmake_caches(hcomm)
    env = build_env()
    cmd = [
        "cmake",
        "-S",
        str(hcomm),
        "-B",
        str(hcomm / build_dir),
        "-DENABLE_TEST=ON",
        "-DENABLE_ST=ON",
        f"-DASCEND_CANN_PACKAGE_PATH={env['ASCEND_HOME_PATH']}",
        f"-DHCOMM_MOCK_LLT_API_ROOT={REPO_ROOT / 'src' / 'llt_api'}",
    ]
    subprocess.run(cmd, check=True, env=env)
    subprocess.run(
        ["cmake", "--build", str(hcomm / build_dir), "--target", "_llt_api", "llt_api_smoke_test", "-j", str(args.jobs)],
        check=True,
        env=env,
    )
    write_config({"hcomm": str(hcomm), "build_dir": build_dir})
    return 0


def prepare_hcomm(args: argparse.Namespace) -> int:
    dest = args.dest or DEFAULT_HCOMM_DIR
    hcomm = args.repo.resolve() if args.repo else dest.resolve()
    if args.repo is None:
        if hcomm.exists():
            raise RuntimeError(f"clone destination already exists: {hcomm}")
        subprocess.run(["git", "clone", args.url, str(hcomm)], check=True)

    ensure_git_repo(hcomm)
    ensure_clean_git(hcomm)

    branch = args.branch or f"hcomm_mock_from_{args.base[:8]}"
    run_git(hcomm, "rev-parse", "--verify", f"{args.base}^{{commit}}")
    branch_exists = subprocess.run(
        ["git", "-C", str(hcomm), "show-ref", "--verify", "--quiet", f"refs/heads/{branch}"],
        check=False,
    ).returncode == 0
    if branch_exists:
        raise RuntimeError(f"branch already exists: {branch}")

    run_git(hcomm, "checkout", "-b", branch, args.base)
    patch = REPO_ROOT / "patches" / "modified_files.patch"
    subprocess.run(["git", "-C", str(hcomm), "apply", "--check", str(patch)], check=True)
    subprocess.run(["git", "-C", str(hcomm), "apply", str(patch)], check=True)
    run_git(hcomm, "add", "-A")
    if args.commit:
        run_git(hcomm, "commit", "-m", "Port hcomm mock LLT trace tooling")
    write_config({"hcomm": str(hcomm), "build_dir": DEFAULT_BUILD_DIR})
    print(f"hcomm={hcomm}")
    print(f"branch={branch}")
    print(f"config={CONFIG_PATH}")
    return 0


def run_smoke(args: argparse.Namespace) -> int:
    hcomm = resolve_hcomm_or_prompt(args, allow_prepare=False)
    build_dir = resolve_build_dir(args)
    smoke = hcomm / build_dir / "test" / "st" / "algorithm" / "llt_api" / "llt_api_smoke_test"
    if not smoke.exists():
        raise RuntimeError(f"smoke test binary does not exist: {smoke}. Run build first.")
    subprocess.run([str(smoke)], check=True)
    write_config({"hcomm": str(hcomm), "build_dir": build_dir})
    return 0


def sync_from_hcomm(args: argparse.Namespace) -> int:
    hcomm = resolve_hcomm(args, required=True)
    ensure_git_repo(hcomm)
    target_commit = run_git_capture(hcomm, "rev-parse", "--short=9", args.target).strip()

    diff_cmd = [
        "git",
        "-C",
        str(hcomm),
        "diff",
        "--binary",
        "--diff-filter=M",
        f"{args.base}..{args.target}",
        "--",
        *PATCHED_HCOMM_FILES,
    ]
    patch = subprocess.run(diff_cmd, check=True, capture_output=True).stdout
    (REPO_ROOT / "patches" / "modified_files.patch").write_bytes(patch)

    sync_llt_api(hcomm)
    sync_examples(hcomm)

    metadata = "\n".join(
        [
            f"base_commit={args.base}",
            f"target_commit={target_commit}",
            f"source_branch={run_git_capture(hcomm, 'branch', '--show-current').strip()}",
            "base_ref=master",
            "",
        ]
    )
    (REPO_ROOT / "metadata.txt").write_text(metadata, encoding="utf-8")
    (REPO_ROOT / "patches" / "metadata.txt").write_text(metadata, encoding="utf-8")
    print(f"synced_from={hcomm}")
    print(f"target_commit={target_commit}")
    return 0


def run_case(args: argparse.Namespace) -> int:
    from .backend import CheckerBackend
    from .exporters import export_dot, export_trace_csv, export_traffic_csv, parse_rank_filter
    from .manifest import write_manifest
    from .model import BehaviorGraph

    case_path = args.case.resolve()
    out_dir = (args.out or default_output_dir(case_path)).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    module_dir = resolve_module_dir(args)
    scenario = load_scenario(case_path, out_dir)
    backend = CheckerBackend(module_dir)
    artifacts = backend.run(scenario, out_dir, test_name=args.test_name or None)

    graph = BehaviorGraph.from_protobuf(Path(artifacts["analysis_sim_pb"]), graph_name=args.graph)
    behavior_graph_path = out_dir / "behavior_graph.json"
    graph.write_json(behavior_graph_path)
    artifacts["behavior_graph"] = behavior_graph_path

    selected_ranks = parse_rank_filter(args.rank)
    if not args.no_dot:
        dot_path = out_dir / "graph.dot"
        export_dot(graph, dot_path, selected_ranks)
        artifacts["graph_dot"] = dot_path
    if not args.no_trace:
        trace_path = out_dir / "trace.csv"
        artifacts["trace_rows"] = export_trace_csv(graph, trace_path, selected_ranks)
        artifacts["trace_csv"] = trace_path
    if not args.no_traffic:
        traffic_path = out_dir / "traffic.csv"
        artifacts["traffic_rows"] = export_traffic_csv(
            graph,
            traffic_path,
            selected_ranks,
            base_address=int(args.base_address, 0),
        )
        artifacts["traffic_csv"] = traffic_path

    manifest_path = out_dir / "manifest.json"
    write_manifest(manifest_path, scenario, artifacts)
    artifacts["manifest"] = manifest_path

    for key, value in artifacts.items():
        print(f"{key}={value}")
    return 0


def default_output_dir(case_path: Path) -> Path:
    return REPO_ROOT / "out" / sanitize_name(peek_scenario_name(case_path))


def sanitize_name(name: str) -> str:
    return "".join(ch if ch.isalnum() or ch in "._-" else "_" for ch in name).strip("_") or "case"


def resolve_module_dir(args: argparse.Namespace) -> Path:
    if args.module_dir:
        return args.module_dir.resolve()
    hcomm = resolve_hcomm_or_prompt(args, allow_prepare=False)
    build_dir = resolve_build_dir(args)
    return hcomm / build_dir / "test" / "st" / "algorithm" / "llt_api"


def resolve_hcomm(args: argparse.Namespace, required: bool = False) -> Path:
    if getattr(args, "hcomm", None):
        return args.hcomm.resolve()

    if is_hcomm_repo(DEFAULT_HCOMM_DIR):
        return DEFAULT_HCOMM_DIR.resolve()
    if required:
        raise RuntimeError(f"hcomm repo not found: {DEFAULT_HCOMM_DIR}")
    return DEFAULT_HCOMM_DIR.resolve()


def resolve_hcomm_or_prepare(args: argparse.Namespace) -> Path:
    hcomm = resolve_hcomm(args, required=False)
    if is_hcomm_repo(hcomm):
        return hcomm
    prepared = prepare_hcomm(default_prepare_args())
    if prepared != 0:
        raise RuntimeError("failed to prepare hcomm repository")
    return DEFAULT_HCOMM_DIR.resolve()


def resolve_hcomm_or_prompt(args: argparse.Namespace, allow_prepare: bool = True) -> Path:
    hcomm = resolve_hcomm(args, required=False)
    if is_hcomm_repo(hcomm):
        return hcomm
    if allow_prepare:
        return resolve_hcomm_or_prepare(args)
    return prompt_hcomm_path()


def default_prepare_args() -> argparse.Namespace:
    return argparse.Namespace(
        repo=None,
        dest=DEFAULT_HCOMM_DIR,
        url=DEFAULT_HCOMM_URL,
        base=DEFAULT_BASE_COMMIT,
        branch="",
        commit=False,
    )


def prompt_hcomm_path() -> Path:
    if not sys.stdin.isatty():
        raise RuntimeError(f"hcomm repo not found: {DEFAULT_HCOMM_DIR}")
    while True:
        value = input("hcomm repo path: ").strip()
        if not value:
            continue
        path = Path(value).expanduser().resolve()
        if is_hcomm_repo(path):
            return path
        print(f"not a hcomm repository: {path}", file=sys.stderr)


def resolve_build_dir(args: argparse.Namespace) -> str:
    if getattr(args, "build_dir", ""):
        return str(args.build_dir)
    config = read_config()
    if config.get("build_dir"):
        return str(config["build_dir"])
    return DEFAULT_BUILD_DIR


def ensure_compatible_cmake_cache(hcomm: Path, build_dir: str) -> None:
    build_path = hcomm / build_dir
    cache_path = build_path / "CMakeCache.txt"
    if not cache_path.exists():
        return
    expected_source = str(hcomm.resolve())
    for line in cache_path.read_text(encoding="utf-8", errors="ignore").splitlines():
        if not line.startswith("CMAKE_HOME_DIRECTORY:INTERNAL="):
            continue
        cached_source = line.split("=", 1)[1]
        if Path(cached_source).resolve() != Path(expected_source):
            shutil.rmtree(build_path)
        return


def remove_stale_third_party_cmake_caches(hcomm: Path) -> None:
    third_party = hcomm / "third_party"
    if not third_party.exists():
        return
    for cache_path in third_party.glob("*-build/CMakeCache.txt"):
        source = read_cmake_home_directory(cache_path)
        if source and not source.exists():
            shutil.rmtree(cache_path.parent)


def read_cmake_home_directory(cache_path: Path) -> Path | None:
    for line in cache_path.read_text(encoding="utf-8", errors="ignore").splitlines():
        if line.startswith("CMAKE_HOME_DIRECTORY:INTERNAL="):
            return Path(line.split("=", 1)[1]).resolve()
    return None


def is_hcomm_repo(path: Path) -> bool:
    return (path / "CMakeLists.txt").exists() and (path / "test" / "st" / "algorithm").exists()


def read_config() -> dict[str, Any]:
    if not CONFIG_PATH.exists():
        return {}
    return json.loads(CONFIG_PATH.read_text(encoding="utf-8"))


def write_config(config: dict[str, Any]) -> None:
    CONFIG_PATH.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")


def build_env() -> dict[str, str]:
    env = dict(os.environ)
    if env.get("ASCEND_HOME_PATH") and has_cann_headers(Path(env["ASCEND_HOME_PATH"])):
        return env
    if env.get("CONDA_PREFIX"):
        cann_path = find_conda_cann_root(Path(env["CONDA_PREFIX"]))
        if cann_path:
            env["ASCEND_HOME_PATH"] = str(cann_path)
    if not env.get("ASCEND_HOME_PATH"):
        raise RuntimeError("ASCEND_HOME_PATH is not set. Activate CANN environment first.")
    return env


def find_conda_cann_root(conda_prefix: Path) -> Path | None:
    ascend_root = conda_prefix / "Ascend"
    candidates = sorted(ascend_root.glob("cann-*")) + [ascend_root / "ascend-toolkit"]
    for candidate in candidates:
        if has_cann_headers(candidate):
            return candidate
    return None


def has_cann_headers(path: Path) -> bool:
    return (
        (path / "aarch64-linux" / "include" / "securec.h").exists()
        and (path / "aarch64-linux" / "include" / "acl" / "acl_rt.h").exists()
        and (path / "aarch64-linux" / "pkg_inc" / "base" / "dlog_pub.h").exists()
    )


def ensure_git_repo(repo: Path) -> None:
    subprocess.run(["git", "-C", str(repo), "rev-parse", "--git-dir"], check=True, capture_output=True)


def ensure_clean_git(repo: Path) -> None:
    if subprocess.run(["git", "-C", str(repo), "diff", "--quiet"], check=False).returncode != 0:
        raise RuntimeError(f"target repository has unstaged changes: {repo}")
    if subprocess.run(["git", "-C", str(repo), "diff", "--cached", "--quiet"], check=False).returncode != 0:
        raise RuntimeError(f"target repository has staged changes: {repo}")


def run_git(repo: Path, *args: str) -> None:
    subprocess.run(["git", "-C", str(repo), *args], check=True)


def run_git_capture(repo: Path, *args: str) -> str:
    return subprocess.run(["git", "-C", str(repo), *args], check=True, text=True, capture_output=True).stdout


def sync_llt_api(hcomm: Path) -> None:
    src = hcomm / "test" / "st" / "algorithm" / "llt_api"
    dst = REPO_ROOT / "src" / "llt_api"
    if dst.exists():
        shutil.rmtree(dst)
    for rel in ("CMakeLists.txt", "include", "src"):
        src_item = src / rel
        dst_item = dst / rel
        if src_item.is_dir():
            shutil.copytree(src_item, dst_item)
        elif src_item.exists():
            dst_item.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src_item, dst_item)


def sync_examples(hcomm: Path) -> None:
    src = hcomm / "test" / "st" / "algorithm" / "examples"
    dst = REPO_ROOT / "examples"
    dst.mkdir(parents=True, exist_ok=True)
    for old in dst.glob("llt_*.json"):
        old.unlink()
    for item in sorted(src.glob("llt_*.json")):
        shutil.copy2(item, dst / item.name)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
