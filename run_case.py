#!/usr/bin/env python3
"""Run an HCOMM mock synthesis case."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SRC = ROOT / "src"
if str(SRC) not in sys.path:
    sys.path.insert(0, str(SRC))

from hcomm_mock.cli import main


if __name__ == "__main__":
    raise SystemExit(main())
