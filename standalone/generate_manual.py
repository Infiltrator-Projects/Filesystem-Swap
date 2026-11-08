#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Produce the current SWAP manual for the direct CMake install path."""
from pathlib import Path
import re
import sys

from manual import build


def main() -> None:
    if len(sys.argv) != 4:
        raise SystemExit("Usage: generate_manual.py VERSION_FILE PAGES_FILE OUTPUT_PDF")
    version = Path(sys.argv[1]).read_text(encoding="utf-8").strip()
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version):
        raise SystemExit("Invalid filesystem release version")
    destination = Path(sys.argv[3])
    destination.parent.mkdir(parents=True, exist_ok=True)
    build(destination, Path(sys.argv[2]), version)


if __name__ == "__main__":
    main()
