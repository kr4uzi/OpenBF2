#!/usr/bin/env python3
"""[TEMP] [CHOSEN, not recovered] Link the fixture with production client objects.

[RETIRE-WHEN: n1-loopback] Retire with the fixture when G6 automates this check.
Run after building bf2_client; reuse its image defines, forced headers and strict link.
"""
import json
from pathlib import Path
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
build = Path(sys.argv[1]).resolve()
entries = json.loads((build / 'compile_commands.json').read_text())
entry = next(e for e in entries if e['file'].endswith('/Game/Main/BF2.cpp')
             and 'bf2_client_objects.dir' in e['command'])
base = shlex.split(entry['command'])
main_obj = base[base.index('-o') + 1]
private = build / 'n1_checks'
private.mkdir(exist_ok=True)
if '--python-only' in sys.argv[2:]:
    # Run the same inet_ntop checks while the full client is still building.
    cmd = list(base)
    cmd[cmd.index('-c') + 1] = str(root / 'Code/Tests/n1_posix_net/loopback.cpp')
    output_obj = private / 'inet_ntop.o'
    cmd[cmd.index('-o') + 1] = str(output_obj)
    subprocess.run(cmd + ['-I' + str(root / 'Code/BF2'), '-DN1_PYTHON_ONLY=1'],
                   cwd=entry['directory'], check=True)
    link = shlex.split((build / 'dice_py/CMakeFiles/dice_py_check.dir/link.txt').read_text())
    link = [str(output_obj) if arg.endswith('.o') else arg for arg in link]
    output = build / 'bin_macos/n1_inet_ntop'
    link[link.index('-o') + 1] = str(output)
    subprocess.run(link, cwd=build / 'dice_py', check=True)
    print(output)
    sys.exit(0)
for source, output, extra in [
    (entry['file'], private / 'main.o', ['-Dmain=n1_production_main']),
    (root / 'Code/Tests/n1_posix_net/loopback.cpp', private / 'loopback.o', []),
]:
    cmd = list(base)
    cmd[cmd.index('-c') + 1] = str(source)
    cmd[cmd.index('-o') + 1] = str(output)
    subprocess.run(cmd + ['-I' + str(root / 'Code/BF2')] + extra,
                   cwd=entry['directory'], check=True)
link = shlex.split((build / 'CMakeFiles/bf2_client.dir/link.txt').read_text())
assert main_obj in link, 'production main object missing from link manifest'
link = [str(private / 'main.o') if arg == main_obj else arg for arg in link]
link = ['-Wl,-map,' + str(private / 'loopback.map') if arg.startswith('-Wl,-map,')
        else arg for arg in link]
output = build / 'bin_macos/n1_loopback'
link[link.index('-o') + 1] = str(output)
subprocess.run(link + [str(private / 'loopback.o')], cwd=build, check=True)
print(output)
