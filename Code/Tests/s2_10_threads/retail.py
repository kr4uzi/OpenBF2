#!/usr/bin/env python3
"""[TEMP] Check production ExcelLexicon against independently decoded cloned game text.

Usage: retail.py <boundary executable> <cloned Localization directory under out/>
Only reads the clone; run with cwd under out/ for the generated unit samples.
"""
from pathlib import Path
import subprocess
import sys

repo = Path(__file__).resolve().parents[3]
content = Path(sys.argv[2]).resolve()
content.relative_to(repo / "out")
args = [str(Path(sys.argv[1]).resolve())]
files = sorted(content.rglob("*.utxt"))
assert files, "no cloned localization files"
for path in files:
    # The first nonempty record is an independent Windows UTF-16 oracle. The
    # production loader also has to scan the remainder without corrupting its stack.
    text = path.read_bytes().decode("utf-16")
    fields = text.split("\x1b\x1b")
    key = fields[0].strip().split(" ", 1)[0]
    value = fields[1]
    assert key and value, path
    args += [str(path), key, value]
samples = repo / "out/s2-10-checks"
samples.mkdir(parents=True, exist_ok=True)
subprocess.run(args, cwd=samples, check=True)
print(f"PASS {len(files)} cloned retail localization files (Python UTF-16 oracle)")
