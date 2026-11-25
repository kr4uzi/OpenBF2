#!/usr/bin/env python3
# [TEMP] [RETIRE-WHEN: terrain-black-hexagons] Per traced frame and patch: the pass-0 draws, the
# colour-pass draws in order (pass, primitive range, index buffer, the pixel shader's
# vComponentsel and the texture on the component-map sampler sampler2Clamp), and the coverage
# line ([TT] C): triangles pass 0 drew that no colour pass drew stay black.
# Usage: python3 -I layers.py ~/Desktop/OpenBF2-terrain-trace.log [patch-idx ...]
import re
import sys

TOKEN = re.compile(r'(\S+?)=(\([^)]*\)|\S+)')

def fields(line):
    return dict(TOKEN.findall(line))

def pick(f, prefix):
    for k, v in f.items():
        if k.startswith(prefix):
            return '%s=%s' % (k.split('@')[-1], v)
    return '-'

def main():
    if len(sys.argv) < 2:
        print('usage: layers.py LOG [patch-idx ...]')
        return 2
    only = set(sys.argv[2:])
    frames = []
    current = None
    with open(sys.argv[1], 'r', errors='replace') as log:
        for line in log:
            if line.startswith('[TT] frame='):
                current = {'header': line.strip(), 'draws': [], 'coverage': {}}
                frames.append(current)
            elif current and line.startswith('[TT] T '):
                current['draws'].append(fields(line))
            elif current and line.startswith('[TT] C '):
                f = fields(line)
                current['coverage'][f.get('idx')] = f
    for frame in frames:
        print('=' * 100)
        print(frame['header'])
        patches = []
        for d in frame['draws']:
            if d.get('idx') not in patches:
                patches.append(d.get('idx'))
        for idx in patches:
            if only and idx not in only:
                continue
            draws = [d for d in frame['draws'] if d.get('idx') == idx and d.get('label') in ('drawMain:p0', 'drawFinal')]
            cov = frame['coverage'].get(idx, {})
            flag = ' <-- UNCOVERED' if cov.get('uncovered', '0') != '0' else ''
            print('patch idx=%s grid=%s coverage: pass0=%s colour=%s uncovered=%s colourOnly=%s first=%s%s'
                  % (idx, draws[0].get('grid') if draws else '-', cov.get('pass0Triangles'), cov.get('colourTriangles'),
                     cov.get('uncovered'), cov.get('colourOnly'), cov.get('firstUncovered'), flag))
            for d in draws:
                print('   %-12s pass=%-2s curLod=%s prim=%-5s start=%-7s ib=%s %s %s'
                      % (d.get('label'), d.get('pass'), d.get('curLod'), d.get('prim'), d.get('start'), d.get('ib'),
                         pick(d, 'ps:vComponentsel'), pick(d, 's:sampler2Clamp')))
    return 0

if __name__ == '__main__':
    sys.exit(main())
