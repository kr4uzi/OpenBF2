#!/usr/bin/env python3
"""[TEMP] [CHOSEN, not recovered] Run the fixture against an already built macOS client.

Usage: python3 Code/Tests/tutorial_messages_runtime.py out/build/feat-tutoff
Reuses the client's compile/link commands; invokes no application, server or game content.
"""
import json
import os
import pathlib
import shlex
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[2]
build = pathlib.Path(sys.argv[1]).resolve()
source = root / 'Code/Tests/tutorial_messages_runtime.cpp'
obj = build / 'tutorial_messages_runtime.o'
exe = build / 'bin_macos/tutorial_messages_runtime'
entry = next(row for row in json.loads((build / 'compile_commands.json').read_text())
             if row['file'].endswith('/Game/GeneralSettings.cpp') and 'bf2_client_objects' in row['command'])
compile_cmd = shlex.split(entry['command'])
compile_cmd[compile_cmd.index('-o') + 1] = str(obj)
compile_cmd[compile_cmd.index(entry['file'])] = str(source)
subprocess.run(compile_cmd, cwd=entry['directory'], check=True)
link_cmd = shlex.split((build / 'CMakeFiles/bf2_client.dir/link.txt').read_text())
link_cmd[link_cmd.index('-o') + 1] = str(exe)
link_cmd = [arg for arg in link_cmd if not arg.startswith('-Wl,-map,')]
link_cmd += [str(obj), '-Wl,-e,_bf2TutorialMessagesMain']
subprocess.run(link_cmd, cwd=build, check=True)
# Constructing BF2Engine acquires the macOS client lock. Keep that file and its
# directories inside this fixture's build tree, independent of the owner's client.
fixture_home = build / 'tutorial-messages-home'
fixture_home.mkdir(exist_ok=True)
subprocess.run([str(exe)], cwd=build, env=dict(os.environ, HOME=str(fixture_home)), check=True)
