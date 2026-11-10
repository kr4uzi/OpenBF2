#!/usr/bin/env python3
"""[CHOSEN, not recovered] Developer oracle wrapper for the shared native patch utility."""
from pathlib import Path
import runpy

if __name__ == "__main__":
    runpy.run_path(str(Path(__file__).resolve().parents[1] / "nativefx/patch-mojo.py"),
                   run_name="__main__")
