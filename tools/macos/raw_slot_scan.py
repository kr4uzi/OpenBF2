#!/usr/bin/env python3
# [CHOSEN, not recovered] Raw vtable-slot dispatch gate for native (non-MSVC) code.
#
# Cross-image calls on macOS go through a real native interface declaration or a
# named, audited adapter (e.g. Game/Objects/GeometryPartAccess.h over
# Geom/NativeGeometrySlots.h). This scan finds every expression that loads a
# function table from an object (`*reinterpret_cast<void***>(obj)`, `(*(void***)obj)`)
# in code that compiles on non-MSVC builds, and compares the set with the reviewed
# baseline tools/macos/raw_slot_baseline.tsv. A new site, or a baseline row whose
# site disappeared, fails the gate. MSVC-only arms (`#if defined(_MSC_VER)`,
# `#ifdef _MSC_VER` and their `#else` complements) are evaluated, not guessed.
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASELINE = Path(__file__).with_name('raw_slot_baseline.tsv')
PATTERN = re.compile(r'(?:reinterpret_cast|static_cast)<\s*(?:const\s+)?void\s*\*\s*(?:const\s*)?\*\s*(?:volatile\s*)?\*\s*>'
                     r'|\(\s*\*\s*\(\s*void\s*\*\*\*\s*\)')
DIRECTIVE = re.compile(r'#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)')


def classify(kind, condition):
    """'msvc' when the branch compiles only with MSVC, 'native' when only without, else 'any'."""
    c = condition.replace(' ', '')
    if kind == 'ifdef':
        return 'msvc' if c == '_MSC_VER' else 'any'
    if kind == 'ifndef':
        return 'native' if c == '_MSC_VER' else 'any'
    if '_MSC_VER' not in c:
        return 'any'
    if re.fullmatch(r'!defined\(_MSC_VER\)(&&.*)?|.*&&!defined\(_MSC_VER\)', c):
        return 'native'
    # A conjunction that requires MSVC: no `||` and a non-negated defined(_MSC_VER).
    if '||' not in c and re.search(r'(?<!!)defined\(_MSC_VER\)', c):
        return 'msvc'
    return 'any'


def scan(root=ROOT):
    sites = []
    for path in sorted((root / 'Code/BF2').rglob('*')):
        if path.suffix not in ('.cpp', '.h', '.inl') or 'External' in path.parts:
            continue
        stack = []
        text = path.read_text(errors='replace')
        for number, line in enumerate(text.splitlines(), 1):
            stripped = line.strip()
            match = DIRECTIVE.match(stripped)
            if match:
                kind, condition = match.group(1), match.group(2).split('//')[0].strip()
                if kind in ('if', 'ifdef', 'ifndef'):
                    stack.append(classify(kind, condition))
                elif kind == 'elif' and stack:
                    stack[-1] = 'native' if stack[-1] == 'msvc' and classify('if', condition) != 'msvc' else 'any'
                elif kind == 'else' and stack:
                    stack[-1] = {'msvc': 'native', 'native': 'msvc'}.get(stack[-1], 'any')
                elif kind == 'endif' and stack:
                    stack.pop()
                continue
            if 'msvc' in stack:
                continue
            code = line.split('//')[0]
            if PATTERN.search(code):
                sites.append((str(path.relative_to(root)), ' '.join(code.split())))
    return sites


def load_baseline():
    rows = {}
    for line in BASELINE.read_text().splitlines():
        if not line or line.startswith('#'):
            continue
        path, code, label = line.split('\t')[:3]
        rows.setdefault((path, code), []).append(label)
    return rows


def check():
    sites = scan()
    baseline = load_baseline()
    seen = {}
    errors = []
    for site in sites:
        seen[site] = seen.get(site, 0) + 1
    for site, count in seen.items():
        if count > len(baseline.get(site, [])):
            errors.append(f'new raw slot dispatch in non-MSVC code: {site[0]}: {site[1]}')
    for site, labels in baseline.items():
        if seen.get(site, 0) < len(labels):
            errors.append(f'stale raw-slot baseline row (site changed or removed): {site[0]}: {site[1]}')
    return sites, errors


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--list':
        for path, code in scan():
            print(f'{path}\t{code}')
        sys.exit(0)
    sites, errors = check()
    for error in errors:
        print(error, file=sys.stderr)
    print(f'raw-slot scan: {len(sites)} reviewed sites, {len(errors)} errors')
    sys.exit(1 if errors else 0)
