#!/usr/bin/env python3
"""Cross-sheet net cross-reference for the wheel project."""
import glob, os, sys, re
from collections import defaultdict

src, out = sys.argv[1], sys.argv[2]
sheets = {}
for p in sorted(glob.glob(os.path.join(src, '*.netlist.txt'))):
    name = os.path.basename(p).replace('.netlist.txt', '')
    txt = open(p).read()
    body = txt.split('NETS\n' + '=' * 78)[1].split('=' * 78 + '\nUNCONNECTED')[0]
    nets = {}
    cur = None
    for line in body.splitlines():
        m = re.match(r'^(\S+)\s+\((\d+) pins?\)', line)
        if m:
            cur = m.group(1); nets[cur] = []
        elif cur and line.startswith('    ') and not line.strip().startswith('**'):
            nets[cur].append(line.strip())
    sheets[name] = nets

allnets = defaultdict(dict)
for sh, nets in sheets.items():
    for n, conns in nets.items():
        if n.startswith('N$'): continue
        allnets[n][sh] = conns

lines = []
A = lines.append
A('GLOBAL NET CROSS-REFERENCE - FSAE WHEEL')
A('=' * 78)
A('')
A('This project uses NO ports and NO sheet-symbol entries. Every inter-sheet')
A('connection is made purely by matching net-label / power-port NAME. A net that')
A('appears on only one sheet therefore connects to nothing else in the design.')
A('')
A('Sheets: %s' % ', '.join(sorted(sheets)))
A('')
A('-' * 78)
A('NETS SPANNING MULTIPLE SHEETS')
A('-' * 78)
for n in sorted(allnets):
    if len(allnets[n]) < 2: continue
    A('')
    A('%s   [on %d sheets]' % (n, len(allnets[n])))
    for sh in sorted(allnets[n]):
        A('   %-14s %s' % (sh, ', '.join(allnets[n][sh]) or '(no pins)'))

A('')
A('-' * 78)
A('NETS APPEARING ON ONLY ONE SHEET (named, but no cross-sheet partner)')
A('-' * 78)
for n in sorted(allnets):
    if len(allnets[n]) != 1: continue
    sh = list(allnets[n])[0]
    conns = allnets[n][sh]
    flag = '   <<< ONLY ONE PIN, AND NO OTHER SHEET USES THIS NAME' if len(conns) <= 1 else ''
    A('%-22s %-14s %s%s' % (n, sh, ', '.join(conns) or '(no pins)', flag))

A('')
A('-' * 78)
A('NEAR-MISS NET NAMES (possible typos - names differing only by case/underscore)')
A('-' * 78)
norm = defaultdict(list)
for n in allnets: norm[re.sub(r'[^a-z0-9]', '', n.lower())].append(n)
found = False
for k, v in sorted(norm.items()):
    if len(set(v)) > 1:
        found = True
        A('  %s' % '  vs  '.join(sorted(set(v))))
        for n in sorted(set(v)):
            A('      %-20s on %s' % (n, ', '.join(sorted(allnets[n]))))
if not found: A('  (none)')

open(out, 'w').write('\n'.join(lines) + '\n')
print('\n'.join(lines[:6]))
print('...')
print('multi-sheet nets: %d   single-sheet named nets: %d' %
      (sum(1 for n in allnets if len(allnets[n]) > 1),
       sum(1 for n in allnets if len(allnets[n]) == 1)))
