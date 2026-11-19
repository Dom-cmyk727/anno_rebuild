#!/usr/bin/env python3
"""Bounded original growth producer, docs/sim/wildlife-growth-load.md.

VECTORS checks authored full-map expectations. --source PATH --write-lookup PATH
writes a private independently computed lookup only after successful parsing.
No original source/table is shipped. Exit 0 agree / 1 differ / 2 invalid.
"""
from pathlib import Path
import hashlib
import json
import re
import sys

DECIMAL = r'[+-]?(?:0|[1-9][0-9]*)'
TOKEN = re.compile(r'[ \t]*('+DECIMAL+r')')
TAIL = re.compile(r'[ \t]*'+DECIMAL+r'(?:[ \t]*,[ \t]*'+DECIMAL+r')*[ \t]*')


def checked(n):
    if not -(1 << 31) <= n < (1 << 31):
        raise ValueError('signed integer out of range')
    return n


def numbers(text, stop=False):
    out = []
    pos = 0
    while True:
        m = TOKEN.match(text, pos)
        if not m:
            raise ValueError('canonical decimal integer required')
        out.append(checked(int(m[1])))
        pos = m.end()
        if pos < len(text) and text[pos] not in ' \t,':
            raise ValueError('unsupported numeric syntax')
        while pos < len(text) and text[pos] in ' \t':
            pos += 1
        if pos == len(text):
            return out, False
        if text[pos] == ',':
            pos += 1
            continue
        if not stop or not TAIL.fullmatch(text[pos:]):
            raise ValueError('unsupported numeric tail')
        numbers(text[pos:])  # Validate every ignored token's int32 bounds; discard the values.
        # The original ignores the remainder, not another whitespace list value.
        return out, True


def load(source):
    if not source or not source.endswith(b'\n') or b'\0' in source:
        raise ValueError('nonempty newline-terminated source without NUL required')
    lookup = bytearray(16384)
    active = ended = False
    sections = bridge = climate = 0
    records = []
    short = []
    stage = [0]*256
    w = h = 6
    ident = None
    row = bridge_row = 0
    complete = False
    size_seen = False
    id_line = 0

    def boundary():
        if ident is not None and (not complete or bridge_row):
            raise ValueError('incomplete record or Bridge batch')

    for line_no, raw in enumerate(source.split(b'\n')[:-1], 1):
        if raw.endswith(b'\r'):
            raw = raw[:-1]
        comment = raw.find(b';')
        if comment >= 0:
            if b'[' in raw[comment:]:
                raise ValueError('bracket in comment unsupported')
            raw = raw[:comment]
        code = raw.decode('ascii').strip(' \t')
        if not code:
            continue
        if '"' in code:
            raise ValueError('quoted code unsupported')
        if ended:
            raise ValueError('content after END')
        if code == '[WACHSTUM]':
            boundary()
            active = True
            sections += 1
            stage = [0]*256
            w = h = 6
            ident = None
            row = bridge_row = 0
            complete = size_seen = False
            continue
        if code == '[END]':
            if not active:
                raise ValueError('WACHSTUM required before END')
            boundary()
            ended = True
            continue
        if not active:
            raise ValueError('WACHSTUM required')
        m = re.fullmatch(r'([A-Za-z_][A-Za-z0-9_]*)[ \t]*:[ \t]*(.*)', code)
        if not m:
            raise ValueError('supported key: value required')
        key, value = m.groups()
        if key == 'Size':
            if ident is not None or size_seen:
                raise ValueError('Size only once before Id')
            dims, _ = numbers(value)
            if len(dims) != 2 or min(dims) <= 0 or dims[0]*dims[1] > 256:
                raise ValueError('dimensions exceed proved staging domain')
            w, h = dims
            size_seen = True
        elif key == 'Id':
            boundary()
            ids, _ = numbers(value)
            if len(ids) != 1:
                raise ValueError('one Id required')
            ident = ids[0]
            id_line = line_no
            row = bridge_row = 0
            complete = False
        elif key in ('Proz', 'Bridge'):
            if ident is None or (key == 'Proz' and complete):
                raise ValueError('Id required or extra Proz row')
            vals, stopped = numbers(value, True)
            if len(vals) > w:
                raise ValueError('too many row values')
            if key == 'Bridge':
                bridge += 1
                bridge_row = (bridge_row+1) % h
                continue
            if len(vals) < w:
                short.append({'line': line_no, 'id': ident, 'row': row,
                              'parsed': len(vals), 'width': w, 'stopped': stopped})
            stage[row*w:row*w+len(vals)] = vals
            row += 1
            if row == h:
                for y in range(h):
                    for x in range(w):
                        index = ((ident & 255) << 6) | ((x & 7) << 3) | (y & 7)
                        lookup[index] = stage[y*w+x] & 255
                records.append({'id': ident, 'width': w, 'height': h, 'line': id_line})
                complete = True
        elif key == 'Klima':
            if ident is None or not re.fullmatch(r'[A-Z_]+(?:[ \t]*,[ \t]*[A-Z_]+)?', value):
                raise ValueError('unsupported Klima sidecar')
            climate += 1
        else:
            raise ValueError('unknown key or section')
    if not ended or not records:
        raise ValueError('END and at least one complete record required')
    return {'lookup': bytes(lookup), 'records': records, 'short': short,
            'sections': sections, 'bridge': bridge, 'climate': climate,
            'source_sha256': hashlib.sha256(source).hexdigest()}


def vectors(path):
    count = failed = 0
    names = set()
    for line in path.read_text(encoding='utf-8').splitlines():
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        if line.count('=>') != 1:
            raise ValueError('one => required')
        left, right = (s.split() for s in line.split('=>'))
        if len(left) != 3 or left[0] != 'growth' or not left[2].startswith('source='):
            raise ValueError('growth name source=path required')
        if left[1] in names:
            raise ValueError('duplicate name')
        names.add(left[1])
        expected = {}
        for token in right:
            if token.count('=') != 1:
                raise ValueError('key=value required')
            k, v = token.split('=')
            if k in expected:
                raise ValueError('duplicate expected key')
            expected[k] = v
        if set(expected) != {'bytes', 'ids', 'sections', 'short', 'bridge', 'climate'}:
            raise ValueError('missing or unknown expected key')
        if any(not expected[k].isdecimal() for k in ('ids', 'sections', 'short', 'bridge', 'climate')):
            raise ValueError('unsigned expected count required')
        want = bytearray(16384)
        seen = set()
        for pair in ([] if expected['bytes'] == '-' else expected['bytes'].split(',')):
            a, b = pair.split(':')
            if not a.isdecimal() or not b.isdecimal():
                raise ValueError('unsigned index:byte required')
            i, v = int(a), int(b)
            if i >= 16384 or v > 255 or i in seen:
                raise ValueError('bad expected byte')
            seen.add(i)
            want[i] = v
        result = load((path.parent/left[2][7:]).read_bytes())
        stats = {'ids': len(result['records']), 'sections': result['sections'],
                 'short': len(result['short']), 'bridge': result['bridge'], 'climate': result['climate']}
        same = result['lookup'] == want and all(str(v) == expected[k] for k, v in stats.items())
        if not same:
            failed += 1
            print(left[1], 'expected lookup/stats differ', stats)
        count += 1
    if count == 0:
        raise ValueError('zero vectors')
    print(f'{count} vectors, {failed} disagreements')
    return 1 if failed else 0


def main():
    if len(sys.argv) == 2:
        return vectors(Path(sys.argv[1]))
    if len(sys.argv) == 5 and sys.argv[1] == '--source' and sys.argv[3] == '--write-lookup':
        if Path(sys.argv[2]).resolve() == Path(sys.argv[4]).resolve():
            raise ValueError('output must differ from source')
        result = load(Path(sys.argv[2]).read_bytes())
        Path(sys.argv[4]).write_bytes(result['lookup'])
        print(json.dumps({k: (len(v) if k in ('records', 'short') else v)
                          for k, v in result.items() if k != 'lookup'}, sort_keys=True))
        print('lookup_sha256='+hashlib.sha256(result['lookup']).hexdigest())
        return 0
    raise ValueError('VECTORS or --source PATH --write-lookup PATH required')


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, OSError, UnicodeError) as error:
        print(error, file=sys.stderr)
        sys.exit(2)
