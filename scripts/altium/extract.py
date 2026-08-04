#!/usr/bin/env python3
"""Extract Altium SchDoc records (ASCII or binary OLE) to a uniform record dump."""
import olefile, struct, sys, os, glob

def records_from_binary(path):
    o = olefile.OleFileIO(path)
    d = o.openstream('FileHeader').read()
    pos = 0
    out = []
    while pos + 4 <= len(d):
        ln = struct.unpack('<I', d[pos:pos+4])[0] & 0x00FFFFFF
        payload = d[pos+4:pos+4+ln]
        if not payload:
            break
        out.append(payload.rstrip(b'\x00'))
        pos += 4 + ln
    return out

def records_from_ascii(path):
    with open(path, 'rb') as f:
        data = f.read()
    return [ln.strip() for ln in data.split(b'\r\n') if ln.strip()]

def parse(rec):
    """pipe-delimited |KEY=VALUE| -> dict with UPPERCASE keys"""
    s = rec.decode('latin-1')
    d = {}
    for field in s.split('|'):
        if not field:
            continue
        if '=' in field:
            k, v = field.split('=', 1)
            d[k.upper()] = v
        else:
            d[field.upper()] = ''
    return d

def load(path):
    with open(path, 'rb') as f:
        magic = f.read(8)
    if magic.startswith(b'\xd0\xcf\x11\xe0'):
        raw = records_from_binary(path)
    else:
        raw = records_from_ascii(path)
    return [parse(r) for r in raw if r]

if __name__ == '__main__':
    src, dst = sys.argv[1], sys.argv[2]
    os.makedirs(dst, exist_ok=True)
    for path in sorted(glob.glob(os.path.join(src, '*.SchDoc'))):
        name = os.path.basename(path).split('-', 1)[1].replace('Copy_of_', '').replace('.SchDoc', '')
        recs = load(path)
        outp = os.path.join(dst, name + '.records.txt')
        with open(outp, 'w') as f:
            for i, r in enumerate(recs):
                f.write('[%d] ' % i + '|'.join('%s=%s' % (k, v) for k, v in r.items()) + '\n')
        types = {}
        for r in recs:
            types[r.get('RECORD', '?')] = types.get(r.get('RECORD', '?'), 0) + 1
        print('%-14s %5d records  types: %s' % (name, len(recs),
              ' '.join('%s:%d' % kv for kv in sorted(types.items(), key=lambda x: int(x[0]) if x[0].isdigit() else 999))))
