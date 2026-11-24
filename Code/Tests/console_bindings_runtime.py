#!/usr/bin/env python3
"""Content-free console binding checks using a completed macOS client build.

python3 Code/Tests/console_bindings_runtime.py --engine-build out/build/local --output out/console-check
No game, window, sound, socket or production startup is run. Files stay in the output directory.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
from template_save_runtime import descriptors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-build", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    build, output = args.engine_build.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    descriptors(Path(__file__).resolve().parents[2], output)
    entries = json.loads((build / "compile_commands.json").read_text())
    row = next(row for row in entries if row["file"].endswith("/Game/Main/BF2.cpp")
               and "bf2_client_objects.dir" in row["command"])
    compile = shlex.split(row["command"])
    link = shlex.split((build / "CMakeFiles/bf2_client.dir/link.txt").read_text())
    if any("undefined" in arg or "multiply_defined" in arg for arg in link):
        parser.error("The fixture requires a strict input link")
    original = (build / compile[compile.index("-o") + 1]).resolve()
    renamed = output / "BF2.renamed-main.o"
    matches = [i for i, arg in enumerate(link) if arg.endswith("/Game/Main/BF2.cpp.o")]
    assert len(matches) == 1 and (build / link[matches[0]]).resolve() == original
    link[matches[0]] = str(renamed)
    exe = output / "console_bindings_runtime"
    link[link.index("-o") + 1] = str(exe)
    link.append(str(output / "fixture.o"))
    with (output / "build.log").open("w") as log:
        cmd = compile[:]
        cmd[cmd.index("-o") + 1] = str(renamed)
        cmd.append("-Dmain=bf2_fixture_unused_main")
        subprocess.run(cmd, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)
        cmd = compile[:]
        cmd[cmd.index("-o") + 1] = str(output / "fixture.o")
        cmd[cmd.index("-c") + 1] = str(Path(__file__).with_suffix(".cpp").resolve())
        cmd.append("-I" + str(output))
        subprocess.run(cmd, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)
        subprocess.run(link, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)
    env = os.environ.copy()
    env["DYLD_LIBRARY_PATH"] = str(build / "bin_macos")
    with (output / "runtime.log").open("w") as log:
        result = subprocess.run([str(exe), "+noSound", "1"], cwd=output, env=env, stdout=log, stderr=subprocess.STDOUT)
    print((output / "runtime.log").read_text(), end="")
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
