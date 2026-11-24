#!/usr/bin/env python3
"""[TEMP] Build/run headless control-map and console checks from a configured native client.

Usage: python3 Code/Tests/control_maps/run.py out/build/<tag>
Pass --sanitize for AddressSanitizer. No game, window, input hardware or audio device is opened.
"""
import json
from pathlib import Path
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
build = Path(sys.argv[1]).resolve()
sanitize = "--sanitize" in sys.argv[2:]
out = build / ("control_maps_check_asan" if sanitize else "control_maps_check")
out.mkdir(exist_ok=True)
database = json.loads((build / "compile_commands.json").read_text())
sources = ["IO/Input/ControlMap.cpp", "IO/Input/ControlMapManager.cpp",
           "IO/Input/ControlMapConsole.cpp", "IO/Input/InputEnums.cpp",
           "IO/Input/InputMapStream.cpp", "Core/ConsoleObjectBase.cpp",
           "Core/ConsoleObjects.cpp", "Core/StringUtil.cpp"]
objects = []
compiler = None
base = None
for source in sources + [None]:
    if source:
        path = root / "Code/BF2" / source
        entry = next(e for e in database if Path(e["file"]) == path
                     and "bf2_client_objects.dir" in e["command"])
        command = shlex.split(entry["command"])
        if base is None:
            base = command[:]
            compiler = command[0]
    else:
        path = Path(__file__).with_name("runtime.cpp")
        command = base[:]
        command[-1] = str(path)
    obj = out / (path.stem + ".o")
    command[command.index("-o") + 1] = str(obj)
    if sanitize:
        command[1:1] = ["-fsanitize=address", "-fno-omit-frame-pointer"]
    subprocess.run(command, cwd=build, check=True)
    objects.append(str(obj))
exe = out / "control_maps_check"
link_flags = ["-fsanitize=address"] if sanitize else []
subprocess.run([compiler, *link_flags, "-Wl,-dead_strip", *objects, "-o", str(exe)], check=True)
subprocess.run([str(exe), str(out / "Controls.con")], check=True)
