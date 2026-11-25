#!/usr/bin/env python3
# [TEMP] [RETIRE-WHEN: terrain-black-hexagons] Pairs the BF2_TERRAIN_TRACE lines of each traced
# frame: every drawFinal terrain draw with the pass-0 (drawMain:p0) draw of the same patch and the
# same constants LOD (curLod), and prints each field that differs. Constants are compared by name,
# since the two vertex shaders keep them in different registers. Draws without a partner and the
# other systems' depth-writing draws between the passes are listed too.
# Usage: python3 -I pair.py ~/Desktop/OpenBF2-terrain-trace.log
import re
import sys

TOKEN = re.compile(r'(\S+?)=(\([^)]*\)|\S+)')
CONST = re.compile(r'^(\w+)\[(\d+)\]@c\d+$')
# Fields that differ between pass 0 and the colour passes by design (other shaders, other
# index lists, other depth state): shown only with -v.
EXPECTED = {'draw', 'label', 'tech', 'pass', 'vs', 'ps', 'ib', 'prim', 'start', 'zw', 'zf', 'cw'}

def parse(line):
    kind = line.split()[1]
    fields = {}
    for key, value in TOKEN.findall(line):
        match = CONST.match(key)
        if match:
            key = 'const:%s[%s]' % (match.group(1), match.group(2))
        fields[key] = value
    return kind, fields

def main():
    if len(sys.argv) < 2:
        print(__doc__ or 'usage: pair.py LOG [-v]')
        return 2
    verbose = '-v' in sys.argv[2:]
    frames = {}
    order = []
    with open(sys.argv[1], 'r', errors='replace') as log:
        for line in log:
            if not line.startswith('[TT] '):
                continue
            if line.startswith('[TT] frame='):
                frame = line.split()[1].split('=')[1]
                frames[frame] = {'header': line.strip(), 'T': [], 'F': []}
                order.append(frame)
                continue
            if line.startswith('[TT] end'):
                continue
            kind, fields = parse(line)
            frame = fields.get('frame')
            if frame in frames and kind in ('T', 'F'):
                frames[frame][kind].append(fields)
    if not order:
        print('no [TT] frames in the log')
        return 1
    for frame in order:
        data = frames[frame]
        print('=' * 100)
        print(data['header'])
        pass0 = {}
        for t in data['T']:
            if t.get('label') == 'drawMain:p0':
                pass0.setdefault((t.get('idx'), t.get('curLod')), []).append(t)
        finals = [t for t in data['T'] if t.get('label') == 'drawFinal']
        used = set()
        mismatched = 0
        for f in finals:
            key = (f.get('idx'), f.get('curLod'))
            partners = pass0.get(key)
            if not partners:
                print('  NO PASS-0 PARTNER: drawFinal draw=%s idx=%s grid=%s curLod=%s pass=%s prim=%s start=%s'
                      % (f.get('draw'), key[0], f.get('grid'), key[1], f.get('pass'), f.get('prim'), f.get('start')))
                continue
            p = partners[0]
            used.add(key)
            keys = sorted(set(p) | set(f))
            diffs = [k for k in keys if p.get(k) != f.get(k) and (verbose or k not in EXPECTED)]
            if diffs:
                mismatched += 1
                print('  DIFF idx=%s grid=%s curLod=%s  p0 draw=%s pass=%s | final draw=%s pass=%s'
                      % (key[0], f.get('grid'), key[1], p.get('draw'), p.get('pass'), f.get('draw'), f.get('pass')))
                for k in diffs:
                    print('      %-34s p0=%-48s final=%s' % (k, p.get(k, '-'), f.get(k, '-')))
        for key, partners in sorted(pass0.items()):
            if key not in used:
                p = partners[0]
                print('  NO DRAWFINAL PARTNER: p0 idx=%s grid=%s curLod=%s (%d draws)' % (key[0], p.get('grid'), key[1], len(partners)))
        print('  pass-0 draws %d, drawFinal draws %d, drawFinal draws with a differing field %d'
              % (sum(len(v) for v in pass0.values()), len(finals), mismatched))
        # Other systems that wrote depth after the terrain's light pass and before its colour pass.
        between = [x for x in data['F'] if x.get('label') not in ('RSMTZOnly', 'drawMain', 'drawMain:p7', 'drawMain:p0', 'drawMain:lights', 'drawFinal', 'surrounding')]
        counts = {}
        for x in between:
            k = (x.get('label'), x.get('zf'), x.get('bias'), x.get('slope'), x.get('cw'), x.get('ds'))
            counts[k] = counts.get(k, 0) + 1
        for (label, zf, bias, slope, cw, ds), n in sorted(counts.items(), key=lambda kv: -kv[1]):
            print('  depth write between the passes: %-12s %4d draws zf=%s bias=%s slope=%s cw=%s ds=%s' % (label, n, zf, bias, slope, cw, ds))
        early = [x for x in data['F'] if x.get('label') in ('RSMTZOnly', 'drawMain', 'drawMain:p7', 'drawMain:p0', 'drawMain:lights')]
        if early:
            print('  depth writes before or inside the light pass (not terrain ranges): %d' % len(early))
    return 0

if __name__ == '__main__':
    sys.exit(main())
