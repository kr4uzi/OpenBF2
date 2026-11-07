#!/usr/bin/env python3
"""[TEMP] Build the S2.10 checks against a completed macOS bf2 build; no engine boot.

Reuses its compile/link manifest, including image defines and sanitizers. Renames only the
production main in a private test object; all other methods and dependencies stay in the link.
Run the emitted executable with cwd under out/ so generated samples stay out of the source tree.
"""
import json
from pathlib import Path
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
build = Path(sys.argv[1]).resolve()
commands = json.loads((build / "compile_commands.json").read_text())
entry = next(c for c in commands if c["file"].endswith("/Game/Main/BF2.cpp")
             and "bf2_engine_objects.dir" in c["command"])
base = shlex.split(entry["command"])
obj = Path(base[base.index("-o") + 1])
private = build / "s2_10_checks"
private.mkdir(exist_ok=True)

def compile_to(source, output, extra=()):
    cmd = list(base)
    cmd[cmd.index("-o") + 1] = str(output)
    cmd[cmd.index("-c") + 1] = str(source)
    subprocess.run(cmd + list(extra), cwd=entry["directory"], check=True)

compile_to(entry["file"], private / "production_main.o", ["-Dmain=s2_10_production_main"])
compile_to(root / "Code/Tests/s2_10_threads/boundaries.cpp", private / "boundaries.o")
link = shlex.split((build / "CMakeFiles/bf2.dir/link.txt").read_text())
link = [str(private / "production_main.o") if arg == str(obj) else arg for arg in link]
output = build / "bin_macos/s2_10_boundaries"
link[link.index("-o") + 1] = str(output)
link.append(str(private / "boundaries.o"))
# Do not overwrite the production link map.
link = ["-Wl,-map," + str(private / "check.map") if arg.startswith("-Wl,-map,") else arg for arg in link]
subprocess.run(link, cwd=build, check=True)
print(output)

# Compile the actual BF2Audio platform wrapper and a worker fixture, without loading OpenAL.
entry = next(c for c in commands if c["file"].endswith("/Sound/SoundEngine/Win32System.cpp")
             and "bf2_bf2audio_static.dir" in c["command"])
base = shlex.split(entry["command"])
compile_to(entry["file"], private / "audio_platform.o")
compile_to(root / "Code/Tests/s2_10_threads/audio_worker.cpp", private / "audio_worker.o")
output = build / "bin_macos/s2_10_audio_worker"
audio_link = shlex.split((build / "CMakeFiles/bf2.dir/link.txt").read_text())
# Retain only compiler, architecture, sanitizer and SDK driver options from the link manifest.
audio_link = audio_link[:next(i for i, arg in enumerate(audio_link) if arg.endswith(".o"))]
audio_link = [arg for arg in audio_link if not arg.startswith("-Wl,")]
audio_link += [str(private / "audio_platform.o"), str(private / "audio_worker.o"), "-o", str(output),
               "-framework", "CoreAudio", "-framework", "CoreFoundation"]
subprocess.run(audio_link, cwd=build, check=True)
print(output)
