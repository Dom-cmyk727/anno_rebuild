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


def _game_dir(argv):
    if '--game-dir' not in argv:
        return None, argv
    i = argv.index('--game-dir')
    if i + 1 >= len(argv):
        return None, argv
    return argv[i + 1], argv[:i] + argv[i + 2:]


def _templates(game):
    """First-found is not used: the caller looks up the layout's key."""
    import os
    from pathlib import Path
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import insel
    root = Path(game) / 'Scenes' / 'Islands'
    found = {}
    for path in root.rglob('*.scp'):
        key = path.relative_to(root).as_posix().upper().replace('\\', '/')
        found[key] = path
    return found, insel


def _kontor_footprint(game):
    import os
    import struct
    from pathlib import Path
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import figuren
    buf = (Path(game) / 'Anlagen.dat').read_bytes()
    top = figuren.records(buf, 16, 16 + struct.unpack_from('<I', buf, 12)[0])
    anl = next(p for n, p in top if n == 'ANLAGEN')
    for _n, payload in figuren.records(anl, 0, len(anl)):
        ent = figuren.entry(payload)
        if ent['id'] == 1302:
            head = next(rp for rn, rp in ent['records'] if rn == 'HEAD')
            return head[0], head[1]
    raise SystemExit('Anlagen.dat has no entry 1302')


def _island_facts(path, insel):
    isl = insel.islands(path.read_bytes())[0]
    height = insel.grid(isl, 'HEIGHT', 1)
    occupied = set()
    for name in ('GELAENDE', 'FLUESSE', 'PROD2'):
        for x, y, _bw, _bh, data in insel.blocks(isl, name):
            for i in range(len(data) // 6):
                item = data[i * 6:(i + 1) * 6]
                occupied.add((x + item[4], y + item[5]))
    info = next((rp for rn, rp in isl['records'] if rn == 'INFO'), b'')
    climate = info[0] if info else None
    return isl['width'], isl['height'], height, occupied, climate


def _overlaps(a, b):
    ax, ay, aw, ah = a[:4]
    bx, by, bw, bh = b[:4]
    return ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah


def layout_problems(mp, game):
    """What `check` refuses. Empty when the layout can be loaded and its
    Kontore sit on a coast. The footprint is Anlagen entry 1302, HEAD bytes
    0 and 1 (the original's, 4 x 4), not the synthetic registry's 2 x 2."""
    problems = []
    files, insel = _templates(game)
    fw, fh = _kontor_footprint(game)
    boxes = []
    facts = {}
    for island in mp['islands']:
        path = files.get(island['key'])
        if path is None:
            problems.append(f"unknown template '{island['key']}'")
            continue
        if island['x'] % 8 or island['y'] % 8:
            problems.append(f"island slot {island['slot']}: origin is not a multiple of 8")
        try:
            w, h, height, occupied, climate = _island_facts(path, insel)
        except Exception as exc:
            problems.append(f"template '{island['key']}': {exc}")
            continue
        facts[island['slot']] = (w, h, height, occupied, climate, path)
        if island['x'] < 0 or island['y'] < 0 or island['x'] + w > mp['width'] or island['y'] + h > mp['height']:
            problems.append(f"island slot {island['slot']}: outside the world")
        box = (island['x'], island['y'], w, h, island['slot'])
        for earlier in boxes:
            if _overlaps(box, earlier):
                problems.append(
                    f"island slot {box[4]}: overlaps island slot {earlier[4]}")
        boxes.append(box)
    seats = [s['seat'] for s in mp['starts']]
    if seats != list(range(len(seats))):
        problems.append('start seats must be 0..N-1 without gaps')
    for start in mp['starts']:
        fact = facts.get(start['island'])
        if fact is None:
            problems.append(f"start seat {start['seat']}: island slot {start['island']} is not in the layout")
            continue
        w, h, height, occupied, _climate, _path = fact
        x = start['x'] - next(i['x'] for i in mp['islands'] if i['slot'] == start['island'])
        y = start['y'] - next(i['y'] for i in mp['islands'] if i['slot'] == start['island'])
        if x < 0 or y < 0 or x + fw > w or y + fh > h:
            problems.append(
                f"start seat {start['seat']}: the kontor footprint is outside island slot {start['island']}")
            continue
        bad_land = False
        for dy in range(fh):
            for dx in range(fw):
                tile = (x + dx, y + dy)
                if height[(y + dy) * w + (x + dx)] < 16 or tile in occupied:
                    bad_land = True
        if bad_land:
            problems.append(f"start seat {start['seat']}: the kontor is not on free land")
            continue
        water = False
        for dy in range(fh):
            for dx in range(fw):
                for nx, ny in ((x + dx - 1, y + dy), (x + dx + 1, y + dy),
                               (x + dx, y + dy - 1), (x + dx, y + dy + 1)):
                    if nx < 0 or ny < 0 or nx >= w or ny >= h or height[ny * w + nx] < 16:
                        water = True
        if not water:
            problems.append(f"start seat {start['seat']}: the kontor is not on the coast")
    return problems


def check_file(path, game):
    data = open(path, 'rb').read()
    mp, err = parse(data)
    if err:
        print(f'error: line {err[0]}: {err[1]}')
        return 1
    problems = layout_problems(mp, game)
    for problem in problems:
        print(problem)
    if problems:
        return 1
    print(f'ok {path}: {len(mp["islands"])} islands, {len(mp["starts"])} starts, '
          f'edge {mp["width"]}x{mp["height"]}')
    return 0


def _land_count(height, w, h):
    return sum(1 for v in height if v >= 16)


def fairness_text(mp, game):
    files, insel = _templates(game)
    centre = (mp['width'] / 2, mp['height'] / 2)
    islands = []
    for island in mp['islands']:
        w, h, height, _occ, climate = _island_facts(files[island['key']], insel)
        cx = island['x'] + w / 2
        cy = island['y'] + h / 2
        islands.append({**island, 'w': w, 'h': h, 'land': _land_count(height, w, h),
                        'climate': climate, 'cx': cx, 'cy': cy,
                        'size': w if w == h else None})
    starts = []
    for start in mp['starts']:
        home = next(i for i in islands if i['slot'] == start['island'])
        starts.append({**start, 'kx': start['x'] + 2, 'ky': start['y'] + 2, 'home': home})

    def dist(ax, ay, bx, by):
        return ((ax - bx) ** 2 + (ay - by) ** 2) ** 0.5

    lines = ['raw materials: not applied (existing gap)',
             'ores: not applied (existing gap)']
    assigned = {s['seat']: [] for s in starts}
    contested = []
    for island in islands:
        nearest = min(starts, key=lambda s: dist(island['cx'], island['cy'], s['kx'], s['ky']))
        d_start = dist(island['cx'], island['cy'], nearest['kx'], nearest['ky'])
        d_centre = dist(island['cx'], island['cy'], centre[0], centre[1])
        if d_centre < d_start:
            contested.append(island)
        else:
            assigned[nearest['seat']].append(island)
    for start in starts:
        group = assigned[start['seat']]
        lines.append(f"region seat {start['seat']} kontor ({start['x']},{start['y']}) "
                     f"to centre {dist(start['kx'], start['ky'], centre[0], centre[1]):.1f}")
        rivals = [s for s in starts if s['seat'] != start['seat']]
        rival = min(rivals, key=lambda s: dist(start['kx'], start['ky'], s['kx'], s['ky']))
        lines.append(f"  nearest rival seat {rival['seat']} "
                     f"distance {dist(start['kx'], start['ky'], rival['kx'], rival['ky']):.1f}")
        counts = {}
        for island in group:
            key = (island['climate'], island['size'])
            counts[key] = counts.get(key, 0) + island['land']
        for key in sorted(counts, key=lambda k: (k[0] is None, k[0] or 0, k[1] or 0)):
            lines.append(f"  land climate {key[0]} size {key[1]}: {counts[key]}")
    lines.append(f"contested islands: {len(contested)}")
    counts = {}
    for island in contested:
        key = (island['climate'], island['size'])
        counts[key] = counts.get(key, 0) + island['land']
    for key in sorted(counts, key=lambda k: (k[0] is None, k[0] or 0, k[1] or 0)):
        lines.append(f"  land climate {key[0]} size {key[1]}: {counts[key]}")
    return '\n'.join(lines) + '\n'


def _png(width, height, rgb):
    import struct
    import zlib
    def chunk(tag, data):
        return (struct.pack('>I', len(data)) + tag + data +
                struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF))
    raw = bytearray()
    stride = width * 3
    for y in range(height):
        raw.append(0)
        raw.extend(rgb[y * stride:(y + 1) * stride])
    ihdr = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr) +
            chunk(b'IDAT', zlib.compress(bytes(raw), 9)) + chunk(b'IEND', b''))


_CLIMATE_RGB = {
    0: (210, 220, 230),
    1: (130, 155, 145),
    2: (50, 130, 60),
    3: (170, 185, 70),
    4: (20, 90, 45),
    5: (185, 150, 80),
    6: (200, 175, 120),
}
_DIGIT = {
    '0': ('111', '101', '101', '101', '111'),
    '1': ('010', '110', '010', '010', '111'),
    '2': ('111', '001', '111', '100', '111'),
    '3': ('111', '001', '111', '001', '111'),
    '4': ('101', '101', '111', '001', '001'),
    '5': ('111', '100', '111', '001', '111'),
}


def preview_file(path, game, out_path):
    files, insel = _templates(game)
    data = open(path, 'rb').read()
    mp, err = parse(data)
    if err:
        print(f'error: line {err[0]}: {err[1]}')
        return 1
    fw, fh = _kontor_footprint(game)
    legend = 220
    width = mp['width'] + legend
    height = mp['height']
    rgb = bytearray((18, 48, 82) * width * height)  # sea

    def put(x, y, colour):
        if 0 <= x < width and 0 <= y < height:
            i = (y * width + x) * 3
            rgb[i:i + 3] = bytes(colour)

    for island in mp['islands']:
        w, h, grid, _occ, climate = _island_facts(files[island['key']], insel)
        colour = _CLIMATE_RGB.get(climate, (255, 0, 255))
        for ty in range(h):
            for tx in range(w):
                if grid[ty * w + tx] >= 16:
                    put(island['x'] + tx, island['y'] + ty, colour)
    cx, cy = mp['width'] // 2, mp['height'] // 2
    for d in range(-6, 7):
        put(cx + d, cy, (255, 255, 255))
        put(cx, cy + d, (255, 255, 255))
    for start in mp['starts']:
        for dy in range(fh):
            for dx in range(fw):
                put(start['x'] + dx, start['y'] + dy, (220, 30, 30))
        glyph = _DIGIT.get(str(start['seat']), _DIGIT['0'])
        for gy, row in enumerate(glyph):
            for gx, bit in enumerate(row):
                if bit == '1':
                    put(start['x'] + fw + 2 + gx, start['y'] + gy, (255, 255, 255))
    names = ('polar', 'tundra', 'nord', 'praerie', 'dschungel', 'steppe', 'wueste')
    for i, name in enumerate(names):
        y0 = 20 + i * 28
        colour = _CLIMATE_RGB[i]
        for y in range(y0, y0 + 16):
            for x in range(mp['width'] + 16, mp['width'] + 36):
                put(x, y, colour)
        # The name is not drawn; the swatch order is the climate number.
    from pathlib import Path
    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    Path(out_path).write_bytes(_png(width, height, rgb))
    print(out_path)
    return 0


def main(argv):
    sys.stdout.reconfigure(newline='\n')
    if len(argv) < 2:
        print('usage: fixed_map.py dump|check|preview|fairness ...', file=sys.stderr)
        return 2
    game, args = _game_dir(argv)
    cmd = args[1]
    if cmd == 'dump':
        if len(args) < 3:
            print('usage: fixed_map.py dump <file>...', file=sys.stderr)
            return 2
        return dump_files(args[2:])
    if cmd == 'check':
        if game is None or len(args) != 3:
            print('usage: fixed_map.py check <file> --game-dir <dir>', file=sys.stderr)
            return 2
        return check_file(args[2], game)
    if cmd == 'fairness':
        if game is None or len(args) != 3:
            print('usage: fixed_map.py fairness <file> --game-dir <dir>', file=sys.stderr)
            return 2
        data = open(args[2], 'rb').read()
        mp, err = parse(data)
        if err:
            print(f'error: line {err[0]}: {err[1]}')
            return 1
        sys.stdout.write(fairness_text(mp, game))
        return 0
    if cmd == 'preview':
        out = None
        rest = args[2:]
        if '--out' in rest:
            i = rest.index('--out')
            out = rest[i + 1]
            rest = rest[:i] + rest[i + 2:]
        if game is None or len(rest) != 1 or out is None:
            print('usage: fixed_map.py preview <file> --game-dir <dir> --out <png>', file=sys.stderr)
            return 2
        return preview_file(rest[0], game, out)
    print('usage: fixed_map.py dump|check|preview|fairness ...', file=sys.stderr)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
