#!/usr/bin/env python3
"""Strict Linux/macOS engine-object regression using an existing dedicated CMake build.

The --*-object options accept preserved pre-fix objects independently.
No engine output, game process, listener or map is changed or started.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys


def run(argv, cwd, log):
    log.write("$ " + shlex.join(str(x) for x in argv) + "\n")
    log.flush()
    subprocess.run(argv, cwd=cwd, stdout=log, stderr=subprocess.STDOUT, check=True)


def main(fixture=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-build", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--event-object", type=Path)
    parser.add_argument("--collision-object", type=Path)
    parser.add_argument("--game-server-object", type=Path)
    parser.add_argument("--spawn-manager-object", type=Path)
    args = parser.parse_args()
    if sys.platform not in ("linux", "darwin"):
        parser.error("Run inside the configured Linux or macOS environment")
    build, output = args.engine_build.resolve(), args.output.resolve()
    if output == build or build in output.parents:
        parser.error("Fixture output must be outside the engine build")
    output.mkdir(parents=True, exist_ok=True)
    commands = json.loads((build / "compile_commands.json").read_text())
    entry = next(row for row in commands if row["file"].endswith("/Game/Main/BF2.cpp")
                 and "-DDEDICATED=1" in row["command"])
    compile_argv = shlex.split(entry["command"])
    original_main = (build / compile_argv[compile_argv.index("-o") + 1]).resolve()
    compile_argv[compile_argv.index("-o") + 1] = str(output / "fixture.o")
    fixture = (fixture or Path(__file__).with_suffix(".cpp")).resolve()
    compile_argv[compile_argv.index("-c") + 1] = str(fixture)
    main_copy = output / "BF2.fixture-main.o"
    if sys.platform == "darwin":
        command = (build / "CMakeFiles/bf2.dir/link.txt").read_text().strip()
    else:
        command = subprocess.check_output(["ninja", "-t", "commands", "bf2"], cwd=build, text=True).splitlines()[-1]
    (output / "link-command.txt").write_text(command + "\n")
    link = shlex.split(command)
    if sys.platform == "linux":
        if link[:2] != [":", "&&"] or link[-2:] != ["&&", ":"]:
            parser.error("Expected one CMake Ninja link command with explicit && wrappers")
        link = link[2:-2]
    if any(arg.startswith("-Wl,--unresolved-symbols") or arg == "-Wl,--allow-shlib-undefined" for arg in link):
        parser.error("Input link must not suppress unresolved symbols")
    if any("dynamic_lookup" in arg or arg == "-undefined" for arg in link):
        parser.error("Input link must not suppress unresolved symbols")
    exe = output / fixture.stem
    link[link.index("-o") + 1] = str(exe)
    counts = [0, 0, 0, 0, 0]
    replacements = [
        ("/Game/Main/BF2.cpp.o", main_copy),
        ("/Game/GameEvents/StickyProjectileEvent.cpp.o", args.event_object),
        ("/CollisionComponents/StickyCollisionComp.cpp.o", args.collision_object),
        ("/Game/GameServer/GameServer.cpp.o", args.game_server_object),
        ("/SpawnSystem/SpawnManager.cpp.o", args.spawn_manager_object),
    ]
    for i, arg in enumerate(link):
        if arg.startswith("-Wl,-Map="):
            link[i] = "-Wl,-Map=" + str(output / "fixture.map")
        elif arg.startswith("-Wl,-map,"):
            link[i] = "-Wl,-map," + str(output / "fixture.map")
        elif arg.startswith("-Wl,--dependency-file="):
            link[i] = "-Wl,--dependency-file=" + str(output / "fixture.link.d")
        for index, (suffix, replacement) in enumerate(replacements):
            if arg.endswith(suffix):
                counts[index] += 1
                if index == 0:
                    assert (build / arg).resolve() == original_main
                    link[i] = str(replacement)
                elif replacement:
                    link[i] = str(replacement.resolve(strict=True))
    if counts != [1, 1, 1, 1, 1]:
        parser.error("Expected exactly one main and each selected engine object")
    link.append(str(output / "fixture.o"))
    with (output / "build.log").open("w") as log:
        run(compile_argv, build, log)
        if sys.platform == "darwin":
            # Recompile only the copied entry TU; no Mach-O symbol rewriting or engine output edits.
            renamed_main = shlex.split(entry["command"])
            renamed_main[renamed_main.index("-o") + 1] = str(main_copy)
            renamed_main.append("-Dmain=bf2_fixture_unused_main")
            run(renamed_main, build, log)
        else:
            run(["objcopy", "--redefine-sym", "main=bf2_fixture_unused_main", str(original_main), str(main_copy)], build, log)
        run(link, build, log)
    library_dirs = [str(build / ("bin_macos" if sys.platform == "darwin" else "bin_linux"))]
    for arg in link:
        if not arg.startswith("-") and ".so" in Path(arg).name:
            directory = str((build / arg).resolve().parent)
            if directory not in library_dirs:
                library_dirs.append(directory)
    env = os.environ.copy()
    library_path = "DYLD_LIBRARY_PATH" if sys.platform == "darwin" else "LD_LIBRARY_PATH"
    if env.get(library_path):
        library_dirs.append(env[library_path])
    env[library_path] = os.pathsep.join(library_dirs)
    with (output / "runtime.log").open("w") as log:
        result = subprocess.run([str(exe)], cwd=output, env=env, stdout=log, stderr=subprocess.STDOUT)
    print((output / "runtime.log").read_text(), end="")
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
