#!/usr/bin/env python3
"""[CHOSEN, not recovered] Vtable-order check for the AI behaviour system (S0 skeleton).

Compares the native declarations with the shipped libaidll.so vtables recorded in
tools/macos/ai_behaviour_vtables.json (written by ai_behaviour_skeleton.py from the ELF).
Clang computes the native slots (`-Xclang -fdump-vtable-layouts` on a probe TU that makes one
virtual call per class); this tool never predicts slots from declaration order. Each slot is
compared as implementing class + method (destructors as [complete]/[deleting], pure slots as
`pure`, secondary-base thunks as `thunk`). The unnamed pure direction slots have chosen
declarations under AGENTS.md's unimplemented-slot exception and are checked as pure slots.

The probe uses the compile command of a bf2_aidll source from <build-dir>/compile_commands.json
and is cached against the flags and the contents of every header it includes.
Exit status 0 when every class matches.
"""
import argparse, hashlib, json, os, re, shlex, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
AI = ROOT / 'Code/BF2/AI/AIDLL/AI'
NS = 'dice::hfe::ai::'

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build-dir', required=True)
    ap.add_argument('--verbose', action='store_true')
    a = ap.parse_args()
    bd = Path(a.build_dir).resolve()
    man = json.load(open(ROOT / 'tools/macos/ai_behaviour_vtables.json'))
    classes = man['classes']
    cc = bd / 'compile_commands.json'
    if not cc.exists():
        print('ai_behaviour_vtables: no compile_commands.json in', bd); return 1
    ref = None
    for e in json.load(open(cc)):
        if e['file'].endswith('AIBotActionPlan/BAPStatement.cpp'):
            ref = e; break
    if ref is None:
        print('ai_behaviour_vtables: BAPStatement.cpp not in the compilation database'); return 1
    args = shlex.split(ref['command']) if 'command' in ref else list(ref['arguments'])
    flags, skip = [], False
    for x in args[1:]:
        if skip: skip = False; continue
        if x in ('-o', '-MF', '-MT', '-MQ'): skip = True; continue
        if x in ('-c', '-MD', '-MMD') or x == ref['file'] or x.endswith('BAPStatement.cpp'): continue
        flags.append(x)
    headers = sorted({c['header'] for c in classes.values() if c.get('header')})
    lines = [f'#include "{AI / h}"' for h in headers]
    for i, name in enumerate(sorted(classes)):
        short = name.split('::')[-1]
        lines.append(f'void bf2_ai_vtable_probe_{i}({NS}{name}* p) {{ p->~{short}(); }}')
    probe = '\n'.join(lines) + '\n'
    h = hashlib.sha256(probe.encode() + ' '.join(flags).encode())
    h.update(Path(__file__).read_bytes())
    h.update((ROOT / 'tools/macos/ai_behaviour_vtables.json').read_bytes())
    for hd in headers:
        h.update((AI / hd).read_bytes())
    for root, _, fs in os.walk(AI):
        for f in sorted(fs):
            if f.endswith('.h'): h.update((Path(root) / f).read_bytes())
    stamp = bd / 'ai_behaviour_vtables.ok'
    if stamp.exists() and stamp.read_text() == h.hexdigest():
        print('ai_behaviour_vtables: unchanged, OK (cached)'); return 0
    src = bd / 'ai_behaviour_vtables_probe.cpp'
    src.write_text(probe)
    # [CHOSEN, not recovered] Clang cannot emit assembly for multiple -arch flags.
    # Audit universal slices independently and require identical slot maps before
    # comparing with the ELF manifest. Keep the thin-build command unchanged.
    archs = [flags[i + 1] for i, flag in enumerate(flags) if flag == '-arch']
    slices = [(None, flags)]
    if len(archs) > 1:
        common, skip = [], False
        for flag in flags:
            if skip: skip = False; continue
            if flag == '-arch': skip = True; continue
            common.append(flag)
        slices = [(arch, common + ['-arch', arch]) for arch in archs]
    native = None
    for arch, slice_flags in slices:
        r = subprocess.run([args[0]] + slice_flags + ['-S', '-o', os.devnull, '-Xclang', '-fdump-vtable-layouts', str(src)],
                           cwd=ref['directory'], capture_output=True, text=True)
        if r.returncode:
            print(r.stderr[-4000:]); print(f'ai_behaviour_vtables: probe did not compile ({arch or "native"})'); return 1
        current = parse(r.stdout + r.stderr)
        if native is not None and current != native:
            different = sorted(name for name in current.keys() | native.keys() if current.get(name) != native.get(name))
            print(f'ai_behaviour_vtables: {arch} differs from {slices[0][0]}: {different}'); return 1
        native = current
        if arch: print(f'ai_behaviour_vtables: {arch} slice compiled; {len(current)} native tables')
    skips = set()
    for u in man.get('unresolved_pure_slots', []):
        c, off = u.split('+0x'); skips.add((c, int(off, 16) // 4))
    def derived_from(c, b):
        while c:
            if c == b: return True
            bl = classes.get(c, {}).get('bases') or []
            c = bl[0] if bl else None
        return False
    bad = 0
    for name, c in sorted(classes.items()):
        elf = c['slots']
        nat = native.get(name)
        if nat is None:
            print(f'MISSING {name}: no native vtable dumped'); bad += 1; continue
        idx = {i for (b, i) in skips if derived_from(name, b)}
        e2 = [re.sub(r'\(.*\)$', '', s) if '(' in s and '[' not in s else s
              for i, s in enumerate(elf) if i not in idx]
        if e2 != nat:
            bad += 1
            print(f'MISMATCH {name} (ELF {c["vtable"]})')
            for i in range(max(len(e2), len(nat))):
                x = e2[i] if i < len(e2) else '(none)'; y = nat[i] if i < len(nat) else '(none)'
                if x != y or a.verbose: print(f'  [{i:2d}] ELF {x:55s} native {y}')
    print(f'ai_behaviour_vtables: {len(classes)} classes, {bad} mismatches')
    if bad == 0: stamp.write_text(h.hexdigest())
    return 1 if bad else 0

ENT = re.compile(r'^\s*\d+ \| (.*)$')
def parse(text):
    out, cur, ents = {}, None, None
    for line in text.splitlines():
        m = re.match(r"^Vtable for '(.+)' \(\d+ entries\)\.", line)
        if m:
            cur = m[1].replace(NS, ''); ents = []; out[cur] = ents; continue
        if cur is None: continue
        if not line.strip():
            cur = None; continue
        if '[this adjustment' in line and ents:
            ents[-1] = 'thunk ' + ents[-1]; continue
        m = ENT.match(line)
        if not m: continue
        e = m[1]
        if e.startswith('offset_to_top') or e.endswith(' RTTI') or e.startswith('vbase_offset') or e.startswith('vcall_offset'):
            ents.append('-'); continue
        pure = '[pure]' in e
        kind = ' [complete]' if '[complete]' in e else ' [deleting]' if '[deleting]' in e else ''
        e2 = e.replace('operator<<', 'operatorLSHIFT')
        q = e2.split('(')[0].split(' ')[-1].lstrip('*&').replace(NS, '').replace('operatorLSHIFT', 'operator<<')
        ents.append('pure' if pure else q + kind)
    # the dump starts every vtable with offset_to_top and RTTI; the ELF list starts at +8
    for k, v in out.items():
        if v[:2] == ['-', '-']: out[k] = v[2:]
    return out

if __name__ == '__main__':
    sys.exit(main())
