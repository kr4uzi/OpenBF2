#!/usr/bin/env python3
"""[TEMP] [CHOSEN, not recovered] Run console fixtures against built macOS images.

Usage: python3 Code/Tests/console_commands_runtime.py out/build/<directory>
Selects a fixture entry point; never enters the game/server main or opens a window.
"""
import json
from pathlib import Path
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
build = Path(sys.argv[1]).resolve()
database = json.loads((build / "compile_commands.json").read_text())
source = root / "Code/Tests/console_commands_runtime.cpp"
for target, dedicated in (("bf2_client", 0), ("bf2", 1)):
    entry = next(r for r in database if r["file"].endswith("/Game/GameLogic/GameLogicConsole.cpp")
                 and f"-DDEDICATED={dedicated}" in r["command"])
    obj = build / f"console_commands_{target}.o"
    exe = build / f"bin_macos/console_commands_{target}"
    command = shlex.split(entry["command"])
    command[command.index("-o") + 1] = str(obj)
    command[command.index(entry["file"])] = str(source)
    result = subprocess.run(command, cwd=entry["directory"], capture_output=True, text=True)
    (build / f"console_commands_{target}.compile.log").write_text(result.stdout + result.stderr)
    if result.returncode:
        sys.exit(result.stderr)
    link = shlex.split((build / f"CMakeFiles/{target}.dir/link.txt").read_text())
    link[link.index("-o") + 1] = str(exe)
    link = [arg for arg in link if not arg.startswith("-Wl,-map,")]
    link += [str(obj), "-Wl,-e,_bf2ConsoleCommandsMain"]
    subprocess.run(link, cwd=build, check=True)
    result = subprocess.run([str(exe)], cwd=build, capture_output=True, text=True, timeout=30)
    (build / f"console_commands_{target}.run.log").write_text(result.stdout + result.stderr)
    print(result.stdout, end="", flush=True)
    if result.returncode:
        sys.exit(result.stderr or f"{target} fixture exited {result.returncode}")
