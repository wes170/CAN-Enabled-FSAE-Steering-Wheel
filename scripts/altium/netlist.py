#!/usr/bin/env python3
"""Build a human-readable netlist + component report from Altium SchDoc records.

Connectivity is geometric: wire segments are unioned where they touch, then pins,
net labels and power ports are attached by coordinate. Net names come from power
ports / net labels; unnamed nets get N$<n>.
"""
import sys, os, glob
from collections import defaultdict
from extract import load

# ---------- union-find ----------
class UF:
    def __init__(self): self.p = {}
    def find(self, x):
        self.p.setdefault(x, x)
        while self.p[x] != x:
            self.p[x] = self.p[self.p[x]]; x = self.p[x]
        return x
    def union(self, a, b):
        ra, rb = self.find(a), self.find(b)
        if ra != rb: self.p[ra] = rb

def num(d, k, default=0):
    v = d.get(k)
    if v is None or v == '': return default
    try: return int(v)
    except ValueError:
        try: return int(float(v))
        except ValueError: return default

def loc(d, prefix='LOCATION'):
    """Altium coords: LOCATION.X plus optional LOCATION.X_FRAC (1/100000 sub-unit)."""
    x = num(d, prefix + '.X'); y = num(d, prefix + '.Y')
    return (x, y)

# pin orientation: low 2 bits of PINCONGLOMERATE -> 0=right(0deg) 1=up 2=left 3=down
DIRS = {0: (1, 0), 1: (0, 1), 2: (-1, 0), 3: (0, -1)}
ELEC = {0: 'Input', 1: 'IO', 2: 'Output', 3: 'OpenCollector', 4: 'Passive',
        5: 'HiZ', 6: 'OpenEmitter', 7: 'Power'}

def on_segment(p, a, b, tol=0):
    """Is point p on axis-aligned-or-diagonal segment a-b?"""
    (px, py), (ax, ay), (bx, by) = p, a, b
    cross = (bx-ax)*(py-ay) - (by-ay)*(px-ax)
    if cross != 0: return False
    if min(ax,bx)-tol <= px <= max(ax,bx)+tol and min(ay,by)-tol <= py <= max(ay,by)+tol:
        return True
    return False

def build(recs):
    comps = {}       # record idx -> info
    pins = []        # dict
    segs = []        # (a, b)
    labels = []      # (pt, text)
    powers = []      # (pt, text)
    junctions = []
    noerc = []
    params_by_owner = defaultdict(list)
    impl_by_owner = defaultdict(list)

    for i, r in enumerate(recs):
        t = r.get('RECORD')
        owner = num(r, 'OWNERINDEX', -1) + 1 if 'OWNERINDEX' in r else None
        if t == '1':
            comps[i] = {'idx': i, 'libref': r.get('LIBREFERENCE', ''),
                        'desc': r.get('COMPONENTDESCRIPTION', ''),
                        'loc': loc(r), 'designator': None, 'params': {},
                        'footprint': None, 'partcount': num(r, 'PARTCOUNT', 1),
                        'designitemid': r.get('DESIGNITEMID', '')}
        elif t == '2':
            pc = num(r, 'PINCONGLOMERATE')
            d = DIRS[pc & 3]
            L = num(r, 'PINLENGTH')
            base = loc(r)
            pins.append({'owner': owner, 'name': r.get('NAME', ''),
                         'desig': r.get('DESIGNATOR', ''),
                         'elec': ELEC.get(num(r, 'ELECTRICAL', 4), '?'),
                         'base': base, 'len': L, 'dir': d,
                         'partid': num(r, 'OWNERPARTID', 1)})
        elif t == '27':
            n = num(r, 'LOCATIONCOUNT')
            pts = [(num(r, 'X%d' % k), num(r, 'Y%d' % k)) for k in range(1, n+1)]
            for k in range(len(pts)-1):
                segs.append((pts[k], pts[k+1]))
        elif t == '25':
            labels.append((loc(r), r.get('TEXT', '')))
        elif t == '17':
            powers.append((loc(r), r.get('TEXT', '')))
        elif t == '29':
            junctions.append(loc(r))
        elif t == '22':
            noerc.append(loc(r))
        elif t == '34' and owner in comps:
            comps[owner]['designator'] = r.get('TEXT', '')
        elif t == '41' and owner is not None:
            params_by_owner[owner].append((r.get('NAME', ''), r.get('TEXT', '')))
        elif t == '45' and owner is not None:
            impl_by_owner[owner].append(r)

    # dedupe pins duplicated across display modes / part records
    seen = set()
    dedup = []
    for p in pins:
        key = (p['owner'], p['desig'], p['base'], p['dir'], p['len'])
        if key in seen: continue
        seen.add(key); dedup.append(p)
    pins = dedup

    for ci, c in comps.items():
        for name, text in params_by_owner.get(ci, []):
            if name and not name.startswith('%'):
                c['params'][name] = text
        for im in impl_by_owner.get(ci, []):
            if im.get('MODELTYPE') == 'PCBLIB' or 'FOOTPRINT' in (im.get('DESCRIPTION','').upper()):
                c['footprint'] = im.get('MODELNAME', c['footprint'])
            elif im.get('MODELNAME') and c['footprint'] is None and im.get('MODELTYPE') not in ('SI','SIM','PCB3DLib'):
                pass

    # decide pin connection end empirically: base vs base+len*dir
    wire_pts = set()
    for a, b in segs:
        wire_pts.add(a); wire_pts.add(b)
    def score(which):
        s = 0
        for p in pins:
            pt = pin_pt(p, which)
            if pt in wire_pts: s += 1
            else:
                for a, b in segs:
                    if on_segment(pt, a, b): s += 1; break
        return s
    hyp = 'far' if score('far') >= score('base') else 'base'
    return dict(comps=comps, pins=pins, segs=segs, labels=labels, powers=powers,
                junctions=junctions, noerc=noerc, hyp=hyp,
                score_far=score('far'), score_base=score('base'))

def pin_pt(p, which):
    if which == 'base': return p['base']
    return (p['base'][0] + p['dir'][0]*p['len'], p['base'][1] + p['dir'][1]*p['len'])

def nets(model):
    segs, pins = model['segs'], model['pins']
    uf = UF()
    # union wire segments that share endpoints or T-touch
    for i, (a, b) in enumerate(segs):
        uf.find(('s', i))
    for i, (a, b) in enumerate(segs):
        for j in range(i+1, len(segs)):
            c, d = segs[j]
            touch = False
            if a in (c, d) or b in (c, d):
                touch = True
            elif on_segment(a, c, d) or on_segment(b, c, d) \
                 or on_segment(c, a, b) or on_segment(d, a, b):
                touch = True   # Altium auto-junctions a T
            if touch:
                uf.union(('s', i), ('s', j))
    # attach pins
    pin_nodes = {}
    for k, p in enumerate(pins):
        pt = pin_pt(p, model['hyp'])
        node = ('p', k)
        uf.find(node)
        pin_nodes[k] = pt
        for i, (a, b) in enumerate(segs):
            if on_segment(pt, a, b):
                uf.union(node, ('s', i))
    # pin-to-pin direct abutment
    bypt = defaultdict(list)
    for k, pt in pin_nodes.items(): bypt[pt].append(k)
    for pt, ks in bypt.items():
        for k in ks[1:]: uf.union(('p', ks[0]), ('p', k))
    # attach labels / power ports
    named = {}
    for kind, items in (('L', model['labels']), ('P', model['powers'])):
        for n, (pt, text) in enumerate(items):
            node = (kind, n)
            uf.find(node)
            hit = False
            for i, (a, b) in enumerate(segs):
                if on_segment(pt, a, b):
                    uf.union(node, ('s', i)); hit = True
            for k, ppt in pin_nodes.items():
                if ppt == pt:
                    uf.union(node, ('p', k)); hit = True
            named[node] = (text, pt, hit)
    # merge islands that carry the same net name (power ports are always global;
    # net labels are global in a flat project with no ports/sheet symbols)
    byname = defaultdict(list)
    for node, (text, pt, hit) in named.items():
        if hit and text: byname[text].append(node)
    islands = {}
    for text, nodes in byname.items():
        roots = {uf.find(n) for n in nodes}
        islands[text] = len(roots)
        for n in nodes[1:]: uf.union(nodes[0], n)
    # group
    groups = defaultdict(list)
    for node in list(uf.p.keys()):
        groups[uf.find(node)].append(node)
    out = []
    for root, members in groups.items():
        pnames, lnames, pinlist = [], [], []
        for m in members:
            if m[0] == 'p': pinlist.append(pins[m[1]])
            elif m[0] == 'P': pnames.append(named[m][0])
            elif m[0] == 'L': lnames.append(named[m][0])
        out.append({'power': sorted(set(pnames)), 'labels': sorted(set(lnames)),
                    'pins': pinlist, 'members': members})
    return out, named, islands

def report(path, outdir):
    recs = load(path)
    name = os.path.basename(path).split('-', 1)[1].replace('Copy_of_', '').replace('.SchDoc', '')
    m = build(recs)
    ns, named, islands = nets(m)
    comps = m['comps']
    desig = {}
    for ci, c in comps.items():
        desig[ci] = c['designator'] or ('?%d' % ci)

    lines = []
    A = lines.append
    A('SHEET: %s' % name)
    A('Source: %s' % os.path.basename(path))
    A('Coordinates are in 10-mil units. Pin connection end resolved as: %s '
      '(score far=%d base=%d)' % (m['hyp'], m['score_far'], m['score_base']))
    A('')
    A('=' * 78)
    A('COMPONENTS (%d)' % len(comps))
    A('=' * 78)
    for ci in sorted(comps, key=lambda i: desig[i]):
        c = comps[ci]
        A('')
        A('%s  @(%d,%d)' % (desig[ci], c['loc'][0], c['loc'][1]))
        A('    Description : %s' % c['desc'])
        A('    LibRef      : %s' % c['libref'])
        if c['designitemid']: A('    DesignItemID: %s' % c['designitemid'])
        if c['footprint']:    A('    Footprint   : %s' % c['footprint'])
        interesting = [(k, v) for k, v in sorted(c['params'].items())
                       if v and v != '*' and k.lower() not in ('currenttime','currentdate','time','date')]
        for k, v in interesting:
            A('    %-12s: %s' % (k[:12], v))
        cp = [p for p in m['pins'] if p['owner'] == ci]
        for p in sorted(cp, key=lambda p: (len(p['desig']), p['desig'])):
            pt = pin_pt(p, m['hyp'])
            A('      pin %-4s %-22s %-13s @(%d,%d)' % (p['desig'], p['name'], p['elec'], pt[0], pt[1]))
    A('')
    A('=' * 78)
    A('NETS')
    A('=' * 78)
    auto = 0
    rows = []
    for n in ns:
        conns = []
        for p in n['pins']:
            conns.append('%s.%s(%s)%s' % (desig.get(p['owner'], '?'), p['desig'],
                                          p['name'], '' if p['elec'] == 'Passive' else ' [%s]' % p['elec']))
        if not conns and not n['power'] and not n['labels']:
            continue
        nm = n['power'][0] if n['power'] else (n['labels'][0] if n['labels'] else None)
        if nm is None:
            auto += 1; nm = 'N$%d' % auto
        alias = sorted(set(n['power'] + n['labels']))
        rows.append((nm, alias, sorted(conns), n))
    for nm, alias, conns, n in sorted(rows):
        extra = ''
        if len(alias) > 1:
            extra = '   << MULTIPLE NAMES ON ONE NET: %s >>' % ', '.join(alias)
        isl = islands.get(nm, 1)
        if isl > 1:
            extra += '   [joined by name across %d separate wire islands on this sheet]' % isl
        A('')
        A('%s  (%d pin%s)%s' % (nm, len(conns), '' if len(conns) == 1 else 's', extra))
        for c in conns:
            A('    %s' % c)
        if len(conns) == 1:
            A('    ** single-pin net on this sheet -- verify it is driven from another sheet **')
        if len(conns) == 0:
            A('    ** named net with NO pins attached **')
    # dangling / unconnected
    A('')
    A('=' * 78)
    A('UNCONNECTED PINS (no wire, no label, no power port)')
    A('=' * 78)
    attached = set()
    for n in ns:
        if len(n['pins']) + len(n['power']) + len(n['labels']) > 1:
            for p in n['pins']: attached.add(id(p))
    lone = [p for p in m['pins'] if id(p) not in attached]
    if not lone: A('  (none)')
    for p in sorted(lone, key=lambda p: (desig.get(p['owner'], ''), p['desig'])):
        pt = pin_pt(p, m['hyp'])
        A('  %-6s pin %-4s %-22s %-13s @(%d,%d)' % (desig.get(p['owner'], '?'), p['desig'],
                                                    p['name'], p['elec'], pt[0], pt[1]))
    A('')
    A('Floating net labels / power ports (not touching any wire or pin):')
    orph = [(k, v) for k, v in named.items() if not v[2]]
    if not orph: A('  (none)')
    for k, (text, pt, hit) in orph:
        A('  %-4s %-20s @(%d,%d)' % ('LABEL' if k[0] == 'L' else 'POWER', text, pt[0], pt[1]))
    A('')
    A('No-ERC markers placed: %d %s' % (len(m['noerc']), m['noerc'] if m['noerc'] else ''))
    A('Junctions: %d' % len(m['junctions']))

    outp = os.path.join(outdir, name + '.netlist.txt')
    with open(outp, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    print('%-14s %3d comps  %3d pins  %3d nets  %2d unconnected pins  (%s)' %
          (name, len(comps), len(m['pins']), len(rows), len(lone), m['hyp']))
    return name

if __name__ == '__main__':
    src, outdir = sys.argv[1], sys.argv[2]
    os.makedirs(outdir, exist_ok=True)
    for p in sorted(glob.glob(os.path.join(src, '*.SchDoc'))):
        report(p, outdir)
