#!/usr/bin/env python3
"""[TEMP] [CHOSEN, not recovered] CPU fixture using the macOS client's real compile flags.

Usage: python3 Code/Tests/air_draft_effect_runtime.py out/build/rotor-winddown
No application, window, server or content is opened. Unused engine calls abort by name.
"""
import json
from pathlib import Path
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
build = Path(sys.argv[1]).resolve()
out = build / 'air-draft-check'
out.mkdir(exist_ok=True)
rows = json.loads((build / 'compile_commands.json').read_text())
entry = next(r for r in rows if r['file'].endswith('/AirDraftEffectBundle.cpp')
             and 'bf2_client_objects' in r['command'])
flags = shlex.split(entry['command'])
del flags[flags.index('-o'):flags.index('-o') + 2]
flags.remove('-c')
flags.remove(entry['file'])
sanitizer = ['-fsanitize=address'] if '--sanitize' in sys.argv[2:] else []
flags += sanitizer

sources = ['Code/Tests/air_draft_effect_runtime.cpp',
           'Code/BF2/Game/Objects/Fx/AirDraftEffectBundle.cpp',
           'Code/BF2/Game/Objects/Fx/AirDraftEffectBundleTemplate.cpp',
           'Code/BF2/Game/Objects/Fx/EffectProperties.cpp', 'Code/BF2/Math/Mat4.cpp']
objects = []
for source in sources:
    obj = out / (Path(source).stem + '.o')
    subprocess.run(flags + ['-c', str(root / source), '-o', str(obj)], check=True)
    objects.append(str(obj))
stubs = out / 'unused.cpp'
subprocess.run([sys.executable, str(root / 'tools/macos/link_stubs.py'), str(stubs)] + objects, check=True)
stub_obj = out / 'unused.o'
subprocess.run(flags + ['-c', str(stubs), '-o', str(stub_obj)], check=True)
exe = out / 'air_draft_effect_runtime'
subprocess.run([flags[0], '-arch', 'x86_64', '-Wl,-dead_strip'] + objects
               + [str(stub_obj), '-o', str(exe)] + sanitizer, check=True)
subprocess.run([str(exe)], cwd=out, check=True)
