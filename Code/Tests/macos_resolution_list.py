#!/usr/bin/env python3
"""[TEMP] Run real renderer mode/option code without engine startup or a window.

Usage: python3 Code/Tests/macos_resolution_list.py out/build/client-effects
Requires a completed bf2_client build. Reuses its compiler/link settings and copies its renderer
archive, substituting a TU that includes production VideoOptions.cpp and the headless entry point.
Only the adapter metadata and capabilities are fixtures; no Metal device is required.
All temporary objects stay in the given build directory; production artifacts are read-only.
"""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
build = Path(sys.argv[1]).resolve()
output = build / "resolution-check"
output.mkdir(exist_ok=True)
commands = json.loads((build / "compile_commands.json").read_text())
entry = next(c for c in commands if c["file"].endswith("/RendDX9/VideoOptions.cpp")
             and "-DBF2_IMAGE_RENDDX9" in c["command"])
command = shlex.split(entry["command"])
command[command.index("-o") + 1] = str(output / "VideoOptions.cpp.o")
command[command.index("-c") + 1] = str(root / "Code/Tests/macos_resolution_list.cpp")
subprocess.run(command, cwd=entry["directory"], check=True)

archive = output / "renderer.a"
archive.write_bytes((build / "libbf2_renddx9_static.a").read_bytes())
subprocess.run(["ar", "-r", str(archive), str(output / "VideoOptions.cpp.o")], check=True)
subprocess.run(["ranlib", str(archive)], check=True)
prelink = (build / "CMakeFiles/bf2_client.dir/build.make").read_text().splitlines()
command = next(line.strip() for line in prelink
               if " -r -nostdlib " in line and "bf2_renddx9_static_prelinked.o" in line)
command = shlex.split(command)
command[command.index("-o") + 1] = str(output / "renderer.o")
for i, arg in enumerate(command):
    if arg.endswith("libbf2_renddx9_static.a"):
        command[i] = "-Wl,-force_load," + str(archive)
subprocess.run(command, cwd=build, check=True)

command = shlex.split((build / "CMakeFiles/bf2_client.dir/link.txt").read_text())
command[command.index("StaticPlugins/bf2_renddx9_static_prelinked.o")] = str(output / "renderer.o")
command[command.index("-o") + 1] = str(output / "resolution-check")
command = [arg for arg in command if not arg.startswith("-Wl,-map,")]
command.append("-Wl,-e,_resolutionListCheck")
subprocess.run(command, cwd=build, check=True)
env = dict(os.environ, BF2_TEST_DESKTOP_MODE="1680x1050@60",
           BF2_TEST_DISPLAY_MODES="720x450@60,800x600@60,825x525@60,1024x640@60,1024x768@60,"
                                  "1152x720@60,1280x800@60,1440x900@60,1650x1050@60,1680x1050@60")
env["DYLD_LIBRARY_PATH"] = str(build / "bin_macos")
subprocess.run([str(output / "resolution-check"), "+noSound", "1"], env=env, check=True)
env["BF2_TEST_DISPLAY_MODES"] += ",640x480@60"
subprocess.run([str(output / "resolution-check"), "+noSound", "1"], env=env, check=True)
