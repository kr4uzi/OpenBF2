#!/usr/bin/env python3
"""[TEMP] Headless content-error checks using strict macOS engine links; no game/audio.

python3 Code/Tests/content_errors_runtime.py --engine-build out/build/client-effects
Reuses completed bf2_client and bf2 builds; writes only to a build-owned fixture directory.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-build", required=True, type=Path)
    parser.add_argument("--image", choices=("client", "dedicated", "both"), default="both")
    args = parser.parse_args()
    build = args.engine_build.resolve()
    output = build / "content-errors-runtime"
    output.mkdir(exist_ok=True)
    commands = json.loads((build / "compile_commands.json").read_text())
    env = os.environ.copy()
    env["DYLD_LIBRARY_PATH"] = str(build / "bin_macos")
    for dedicated, target, objects in ((0, "bf2_client", "bf2_client_objects"),
                                       (1, "bf2", "bf2_engine_objects")):
        if args.image != "both" and (args.image == "dedicated") != bool(dedicated):
            continue
        entry = next(row for row in commands if row["file"].endswith("/Game/Main/BF2.cpp")
                     and f"{objects}.dir" in row["command"])
        compile_command = shlex.split(entry["command"])
        link = shlex.split((build / f"CMakeFiles/{target}.dir/link.txt").read_text())
        if any("undefined" in arg or "multiply_defined" in arg for arg in link):
            parser.error("The fixture requires strict input links")
        renamed_main = output / f"BF2-renamed-{dedicated}.o"
        fixture_object = output / f"fixture-{dedicated}.o"
        replaced = 0
        for index, arg in enumerate(link):
            if arg.endswith("/Game/Main/BF2.cpp.o"):
                link[index] = str(renamed_main)
                replaced += 1
            elif arg.startswith("-Wl,-map,"):
                link[index] = f"-Wl,-map,{output}/fixture-{dedicated}.map"
        assert replaced == 1
        executable = output / f"check-{dedicated}"
        link[link.index("-o") + 1] = str(executable)
        link.append(str(fixture_object))
        with (output / f"build-{dedicated}.log").open("w") as log:
            renamed = compile_command[:]
            renamed[renamed.index("-o") + 1] = str(renamed_main)
            renamed.append("-Dmain=bf2_fixture_unused_main")
            fixture = compile_command[:]
            fixture[fixture.index("-o") + 1] = str(fixture_object)
            fixture[fixture.index("-c") + 1] = str(Path(__file__).with_suffix(".cpp").resolve())
            for command in (renamed, fixture, link):
                subprocess.run(command, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)
        result = subprocess.run([str(executable), "+noSound", "1"], cwd=output, env=env,
                                capture_output=True, text=True, timeout=30)
        (output / f"runtime-{dedicated}.log").write_text(result.stdout + result.stderr)
        print(result.stdout + result.stderr, end="")
        result.check_returncode()
        if f"content errors DEDICATED={dedicated}:" not in result.stdout:
            raise RuntimeError("Fixture did not finish")
        if result.stderr.count("early content error") != 1:
            raise RuntimeError("Early Error fallback was missing or repeated")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
