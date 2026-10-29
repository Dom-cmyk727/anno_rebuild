"""The fixed-map layout (docs/sim/fixed-map.md).

  python -I tools/fixed_map.py dump <file>...

Prints the same text as `anno-dump fixed-map`. Stdlib only.
"""
import struct
import sys

FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
EDGE_BOUND = 1 << 14
MAX_RECORDS = 65536
MAX_VALUES = 256
U32_MAX = 0xFFFFFFFF
I32_MIN = -2147483648
I32_MAX = 2147483647


def fnv64(data):
    h = FNV_OFFSET
    for b in data:
        h ^= b
        h = (h * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return h


def lines_of(data):
    rows = []
    number = 1
    i = 0
    n = len(data)
    while i < n:
        end = data.find(b'\n', i)
        if end < 0:
            last = n
            nxt = n
        else:
            last = end
            nxt = end + 1
        if last > i and data[last - 1] == 0x0D:
            last -= 1
        rows.append((number, data[i:last]))
        i = nxt
        number += 1
    return rows


def ascii_line(raw):
    for b in raw:
        if b != 0x09 and (b < 0x20 or b > 0x7E):
            return False
    return True


def fields_of(text):
    return text.split()


def normalise_key(key):
    out = []
    for c in key:
        if c == '\\':
            out.append('/')
        elif 'a' <= c <= 'z':
            out.append(chr(ord(c) - 32))
        else:
            out.append(c)
    return ''.join(out)


def field_error(line, record, field, message):
    return line, f"field {field} of '{record}' {message}"


def parse_int(token, line, record, field, lo, hi, signed):
    if token == '' or token[0] == '+':
        return None, field_error(line, record, field, 'is not an integer')
    neg = token[0] == '-'
    digits = token[1:] if neg else token
    if neg and not signed:
        return None, field_error(line, record, field, 'is not an integer')
    if digits == '' or any(c < '0' or c > '9' for c in digits):
        return None, field_error(line, record, field, 'is not an integer')
    try:
        value = int(digits, 10)
    except ValueError:
        return None, field_error(line, record, field, 'is out of range')
    if neg:
        # A magnitude past the positive range of the field, including the
        # extra one that two's complement spends on the minimum.
        if value > (hi + 1 if signed else hi):
            return None, field_error(line, record, field, 'is out of range')
        value = -value
    if value < lo or value > hi:
        return None, field_error(line, record, field, 'is out of range')
    return value, None


def need(fields, n, line, record):
    if len(fields) != n:
        need_n = n - 1
        word = 'field' if need_n == 1 else 'fields'
        return line, f"record '{record}' needs {need_n} {word}, has {len(fields) - 1}"
    return None


def parse(data):
    saw = {'name': False, 'edge': False, 'base': False, 'seed': False, 'money': False}
    saw_version = False
    name = ''
    width = height = seed = 0
    base = ''
    money = None
    islands = []
    starts = []
    extras = []
    for number, raw in lines_of(data):
        if not ascii_line(raw):
            return None, (number, 'line is not ASCII')
        try:
            text = raw.decode('ascii')
        except UnicodeError:
            return None, (number, 'line is not ASCII')
        body = text.lstrip(' \t')
        if body == '' or body[0] == '#':
            continue
        fields = fields_of(body)
        kind = fields[0]
        if not saw_version:
            if kind != 'anno1503-fixed-map' or len(fields) != 2 or fields[1] != '1':
                return None, (number, "version line must be 'anno1503-fixed-map 1'")
            saw_version = True
            continue
        if kind in ('name', 'edge', 'base', 'seed', 'money'):
            if saw[kind]:
                return None, (number, f"duplicate record '{kind}'")
            saw[kind] = True
        if kind == 'name':
            bad = need(fields, 2, number, kind)
            if bad:
                return None, bad
            name = fields[1]
        elif kind == 'edge':
            bad = need(fields, 3, number, kind)
            if bad:
                return None, bad
            width, err = parse_int(fields[1], number, kind, 1, 1, EDGE_BOUND, False)
            if err:
                return None, err
            height, err = parse_int(fields[2], number, kind, 2, 1, EDGE_BOUND, False)
            if err:
                return None, err
        elif kind == 'base':
            bad = need(fields, 2, number, kind)
            if bad:
                return None, bad
            base = fields[1]
        elif kind == 'seed':
            bad = need(fields, 2, number, kind)
            if bad:
                return None, bad
            seed, err = parse_int(fields[1], number, kind, 1, 0, U32_MAX, False)
            if err:
                return None, err
        elif kind == 'money':
            bad = need(fields, 2, number, kind)
            if bad:
                return None, bad
            money, err = parse_int(fields[1], number, kind, 1, I32_MIN, I32_MAX, True)
            if err:
                return None, err
        elif kind == 'island':
            bad = need(fields, 5, number, kind)
            if bad:
                return None, bad
            if len(islands) >= MAX_RECORDS:
                return None, (number, 'too many island records')
            slot, err = parse_int(fields[1], number, kind, 1, 0, U32_MAX, False)
            if err:
                return None, err
            x, err = parse_int(fields[2], number, kind, 2, I32_MIN, I32_MAX, True)
            if err:
                return None, err
            y, err = parse_int(fields[3], number, kind, 3, I32_MIN, I32_MAX, True)
            if err:
                return None, err
            islands.append({
                'slot': slot, 'x': x, 'y': y, 'key': normalise_key(fields[4]),
                'raw': [], 'ore': [], 'line': number,
            })
        elif kind == 'start':
            bad = need(fields, 6, number, kind)
            if bad:
                return None, bad
            if len(starts) >= MAX_RECORDS:
                return None, (number, 'too many start records')
            seat, err = parse_int(fields[1], number, kind, 1, 0, U32_MAX, False)
            if err:
                return None, err
            island, err = parse_int(fields[2], number, kind, 2, 0, U32_MAX, False)
            if err:
                return None, err
            x, err = parse_int(fields[3], number, kind, 3, I32_MIN, I32_MAX, True)
            if err:
                return None, err
            y, err = parse_int(fields[4], number, kind, 4, I32_MIN, I32_MAX, True)
            if err:
                return None, err
            rot, err = parse_int(fields[5], number, kind, 5, 0, 3, False)
            if err:
                return None, err
            starts.append({
                'seat': seat, 'island': island, 'x': x, 'y': y,
                'rotation': rot, 'line': number,
            })
        elif kind in ('raw', 'ore'):
            if len(fields) < 2:
                return None, (number, f"record '{kind}' needs 1 field, has 0")
            if len(fields) - 2 > MAX_VALUES:
                return None, (number,
                              f"record '{kind}' needs at most {MAX_VALUES} values, "
                              f"has {len(fields) - 2}")
            slot, err = parse_int(fields[1], number, kind, 1, 0, U32_MAX, False)
            if err:
                return None, err
            values = []
            for i in range(2, len(fields)):
                value, err = parse_int(fields[i], number, kind, i, 0, U32_MAX, False)
                if err:
                    return None, err
                values.append(value)
            extras.append({'slot': slot, 'values': values, 'line': number, 'raw': kind == 'raw'})
        else:
            return None, (number, f"unknown record '{kind}'")
    if not saw_version:
        return None, (1, 'missing version line')
    if not (saw['name'] and saw['edge'] and saw['base'] and saw['seed']):
        which = ('name' if not saw['name'] else 'edge' if not saw['edge'] else
                 'base' if not saw['base'] else 'seed')
        return None, (1, f"missing record '{which}'")
    islands.sort(key=lambda item: item['slot'])
    for i in range(1, len(islands)):
        if islands[i]['slot'] == islands[i - 1]['slot']:
            return None, (islands[i]['line'], f"duplicate island slot {islands[i]['slot']}")
    starts.sort(key=lambda item: item['seat'])
    for i in range(1, len(starts)):
        if starts[i]['seat'] == starts[i - 1]['seat']:
            return None, (starts[i]['line'], f"duplicate start seat {starts[i]['seat']}")
    raw_set = [False] * len(islands)
    ore_set = [False] * len(islands)
    by_slot = {item['slot']: i for i, item in enumerate(islands)}
    for extra in extras:
        index = by_slot.get(extra['slot'])
        if index is None:
            which = 'raw' if extra['raw'] else 'ore'
            return None, (extra['line'],
                          f"{which} names island slot {extra['slot']}, which is not in the layout")
        seen = raw_set if extra['raw'] else ore_set
        if seen[index]:
            which = 'raw' if extra['raw'] else 'ore'
            return None, (extra['line'],
                          f"duplicate {which} for island slot {extra['slot']}")
        seen[index] = True
        islands[index]['raw' if extra['raw'] else 'ore'] = extra['values']
    return {
        'version': 1, 'name': name, 'width': width, 'height': height, 'base': base,
        'seed': seed, 'money': money, 'islands': islands, 'starts': starts,
    }, None


def put_u32(out, value):
    out.extend(struct.pack('<I', value & 0xFFFFFFFF))


def put_i32(out, value):
    out.extend(struct.pack('<i', value))


def put_str(out, text):
    raw = text.encode('ascii')
    put_u32(out, len(raw))
    out.extend(raw)


def canonical(mp):
    out = bytearray()
    put_u32(out, mp['version'])
    put_str(out, mp['name'])
    put_u32(out, mp['width'])
    put_u32(out, mp['height'])
    put_str(out, mp['base'])
    put_u32(out, mp['seed'])
    out.append(1 if mp['money'] is not None else 0)
    put_i32(out, 0 if mp['money'] is None else mp['money'])
    put_u32(out, len(mp['islands']))
    for island in mp['islands']:
        put_u32(out, island['slot'])
        put_i32(out, island['x'])
        put_i32(out, island['y'])
        put_str(out, island['key'])
        put_u32(out, len(island['raw']))
        for value in island['raw']:
            put_u32(out, value)
        put_u32(out, len(island['ore']))
        for value in island['ore']:
            put_u32(out, value)
    put_u32(out, len(mp['starts']))
    for start in mp['starts']:
        put_u32(out, start['seat'])
        put_u32(out, start['island'])
        put_i32(out, start['x'])
        put_i32(out, start['y'])
        put_u32(out, start['rotation'])
    return bytes(out)


def digest(mp):
    return fnv64(canonical(mp))


def summary(mp):
    lines = [
        f"fixed-map {mp['version']}",
        f"name {mp['name']}",
        f"edge {mp['width']} {mp['height']}",
        f"base {mp['base']}",
        f"seed {mp['seed']}",
        f"money {mp['money']}" if mp['money'] is not None else 'money default',
    ]
    for island in mp['islands']:
        lines.append(f"island {island['slot']} {island['x']} {island['y']} {island['key']}")
        if island['raw']:
            lines.append('raw ' + ' '.join(str(v) for v in [island['slot'], *island['raw']]))
        if island['ore']:
            lines.append('ore ' + ' '.join(str(v) for v in [island['slot'], *island['ore']]))
    for start in mp['starts']:
        lines.append(
            f"start {start['seat']} {start['island']} {start['x']} {start['y']} {start['rotation']}")
    lines.append(f"digest {digest(mp):016x}")
    return '\n'.join(lines) + '\n'


def dump_files(paths):
    status = 0
    for path in paths:
        try:
            data = open(path, 'rb').read()
        except OSError:
            print(f'{path}: cannot read', file=sys.stderr)
            status = 1
            continue
        print(f'# {path}')
        mp, err = parse(data)
        if err:
            print(f'error: line {err[0]}: {err[1]}')
            status = 1
        else:
            sys.stdout.write(summary(mp))
    return status


def main(argv):
    sys.stdout.reconfigure(newline='\n')
    if len(argv) < 2 or argv[1] != 'dump' or len(argv) < 3:
        print('usage: fixed_map.py dump <file>...', file=sys.stderr)
        return 2
    return dump_files(argv[2:])


if __name__ == '__main__':
    sys.exit(main(sys.argv))
