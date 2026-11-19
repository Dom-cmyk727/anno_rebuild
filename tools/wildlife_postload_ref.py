#!/usr/bin/env python3
"""Isolated GFX cleanup, docs/sim/wildlife-postload.md §5; raw Terrain only.

Not the ordered postload pipeline. No arbitrary corner grid or physical padding.
Exit 0 agrees, 1 disagrees, 2 malformed/missing/zero vectors.
"""
import re
import sys
from pathlib import Path


def unsigned(text, limit):
    if not re.fullmatch(r'[0-9]+', text):
        raise ValueError('unsigned decimal integer required')
    value = int(text)
    if value > limit:
        raise ValueError('integer out of range')
    return value


def values(text, limit):
    return [] if text == '-' else [unsigned(v, limit) for v in text.split(',')]


def fields(tokens, keys):
    result = {}
    for token in tokens:
        if token.count('=') != 1:
            raise ValueError('key=value required')
        key, value = token.split('=')
        if key in result:
            raise ValueError('duplicate key')
        result[key] = value
    if set(result) != keys:
        raise ValueError('unexpected or missing keys')
    return result


def cleanup(width, height, samples, graphics):
    # Logical border W/H is zero. The outside is also zero: no padding is exposed.
    def vertex(x, y):
        if 0 <= x < width and 0 <= y < height:
            return samples[y * width + x]
        return 0

    output = []
    cleared = 0
    for y in range(height):
        for x in range(width):
            word = graphics[y * width + x]
            if word >> 8 == 0x20:
                maximum = max(vertex(vx, vy) for vy in range(y - 1, y + 3)
                              for vx in range(x - 1, x + 3))
                if maximum == 0:
                    word = 0
                    cleared += 1
            output.append(word)
    return {'gfx': ','.join(map(str, output)), 'cleared': str(cleared)}



def signed(text):
    if not re.fullmatch(r'-?[0-9]+', text):
        raise ValueError('signed decimal integer required')
    value = int(text)
    if not -(1 << 31) <= value < (1 << 31):
        raise ValueError('signed integer out of range')
    return value


def cells(text):
    limits = [0xffffffff, 255, 0xffffffff, 0xffffffff, 65535, 65535,
              65535, 65535, 255, 1]
    output = []
    for row in text.split(';'):
        parts = row.split(',')
        if len(parts) != len(limits):
            raise ValueError('cell needs 10 fields')
        output.append([unsigned(value, limit) for value, limit in zip(parts, limits)])
    return output


def joined(values):
    return ','.join(map(str, values))


def joined_cells(rows):
    return ';'.join(joined(row) for row in rows)


def role(text):
    parts = text.split(',')
    if len(parts) != 4:
        raise ValueError('role needs id,flags,first,shape')
    ident, flags, first, shape = [unsigned(v, limit) for v, limit in
                                zip(parts, [0xffffffff, 65535, 255, 255])]
    if ident == 0 or flags & 3 != 1:
        raise ValueError('unsupported mean profile or zero role id')
    code = 4
    if first or shape:
        code = first
        if first >= 8:
            code = (first - 8) | 0x80
            if shape <= 4:
                code |= (0x50, 0x48, 0x58, 0x78, 0x10)[shape]
    return ident, code


def fill(inputs, expected):
    w, h = (unsigned(inputs[k], 0xffffffff) for k in ('w', 'h'))
    if not w or not h:
        raise ValueError('positive geometry required')
    if unsigned(inputs['complete'], 1) != 1 or unsigned(inputs['ground_done'], 1) != 1:
        raise ValueError('completed local headers and precomputed ground required')
    origin = [signed(v) for v in inputs['origin'].split(',')]
    rectangle = inputs['rect'].split(',')
    if len(origin) != 2 or len(rectangle) != 4:
        raise ValueError('origin/rectangle shape')
    rx, ry = map(signed, rectangle[:2])
    rw, rh = (unsigned(v, 0xffffffff) for v in rectangle[2:])
    columns = {k: values(inputs[k], 65535 if k == 'gfx' else 255)
               for k in ('height', 'gfx', 'ground', 'raw')}
    old = cells(inputs['cells'])
    if any(len(v) != w*h for v in [*columns.values(), old]):
        raise ValueError('plane length must equal w*h')
    if any((a & 15) != (b & 15) for a, b in zip(columns['ground'], columns['raw'])):
        raise ValueError('ground low nibble mismatch')
    grass, code = role(inputs['grass'])
    fish, _ = role(inputs['fish'])
    if grass == fish:
        raise ValueError('role ids must be distinct')
    draws = values(inputs['draws'], 0xffffffff)
    result = [row.copy() for row in old]
    fish_count = grass_count = 0

    def corner(x, y):
        return columns['height'][y*w+x] if 0 <= x < w and 0 <= y < h else 0

    for y in range(max(0, ry-origin[1]), min(h, ry+rh-origin[1])):
        for x in range(max(0, rx-origin[0]), min(w, rx+rw-origin[0])):
            i = y*w+x
            if result[i][0] != 0:
                continue
            mean = (corner(x, y)+corner(x+1, y)+corner(x, y+1)+corner(x+1, y+1))//4
            if mean < 16:
                result[i][0] = fish
                result[i][3] = (result[i][3] & 0xffffff00) | mean
                fish_count += 1
            elif mean >= 32 and (columns['ground'][i] & 15)*10 > 50:
                result[i][0] = grass
                # The runtime word's code/mask project into separate engine columns.
                result[i][3] = (result[i][3] & 0xfe000000) | mean
                result[i][4:6] = [code, 0]
                grass_count += 1
            else:
                continue
            result[i][1] = 0
            result[i][6:9] = [0, 0, 0]
    actual = {k: joined(v) for k, v in columns.items()}
    actual.update(cells=joined_cells(result), fish=str(fish_count), grass=str(grass_count),
                  consumed='0', draws=joined(draws) or '-')
    wanted = {k: joined(values(expected[k], 65535 if k == 'gfx' else 255))
              for k in columns}
    if any(len(values(expected[k], 65535 if k == 'gfx' else 255)) != w*h for k in columns):
        raise ValueError('expected plane length must equal w*h')
    wanted_cells = cells(expected['cells'])
    if len(wanted_cells) != w*h:
        raise ValueError('expected cell length must equal w*h')
    wanted.update(cells=joined_cells(wanted_cells),
                  fish=str(unsigned(expected['fish'], w*h)),
                  grass=str(unsigned(expected['grass'], w*h)),
                  consumed=str(unsigned(expected['consumed'], 0xffffffff)),
                  draws=joined(values(expected['draws'], 0xffffffff)) or '-')
    return actual, wanted


def gfx(inputs, expected):
    w, h = (unsigned(inputs[k], 0xffffffff) for k in ('w', 'h'))
    if w == 0 or h == 0:
        raise ValueError('positive geometry required')
    samples = values(inputs['height'], 255)
    graphics = values(inputs['gfx'], 65535)
    wanted = values(expected['gfx'], 65535)
    cleared = unsigned(expected['cleared'], w*h)
    if any(len(v) != w*h for v in (samples, graphics, wanted)):
        raise ValueError('plane length must equal w*h')
    return cleanup(w, h, samples, graphics), {'gfx': joined(wanted), 'cleared': str(cleared)}


def run(path):
    count = disagreements = 0
    labels = set()
    for number, raw in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        line = raw.split('#', 1)[0].strip()
        if not line:
            continue
        try:
            if line.count('=>') != 1:
                raise ValueError('one => required')
            left, right = line.split('=>')
            tokens = left.split()
            if len(tokens) < 2 or tokens[0] not in ('gfx', 'fill'):
                raise ValueError('unknown record type or missing label')
            identity = tuple(tokens[:2])
            if identity in labels:
                raise ValueError('duplicate label')
            labels.add(identity)
            if tokens[0] == 'gfx':
                inputs = fields(tokens[2:], {'w', 'h', 'height', 'gfx'})
                expected = fields(right.split(), {'gfx', 'cleared'})
                actual, wanted = gfx(inputs, expected)
            else:
                inputs = fields(tokens[2:], {'w', 'h', 'origin', 'rect', 'height', 'gfx',
                                            'ground', 'raw', 'cells', 'grass', 'fish',
                                            'complete', 'ground_done', 'draws'})
                expected = fields(right.split(), {'cells', 'fish', 'grass', 'consumed', 'draws',
                                                 'height', 'gfx', 'ground', 'raw'})
                actual, wanted = fill(inputs, expected)
            for key in wanted:
                if actual[key] != wanted[key]:
                    print(f'{path}:{number} {tokens[1]} {key}: expected '
                          f'{wanted[key]}, got {actual[key]}')
                    disagreements += 1
            count += 1
        except ValueError as error:
            raise ValueError(f'{path}:{number}: {error}') from error
    if count == 0:
        raise ValueError('zero vectors')
    print(f'{count} vectors, {disagreements} disagreements')
    return int(disagreements != 0)


if __name__ == '__main__':
    try:
        if len(sys.argv) != 2:
            raise ValueError('usage: wildlife_postload_ref.py VECTORS')
        sys.exit(run(Path(sys.argv[1])))
    except (ValueError, OSError, UnicodeError) as error:
        print(error, file=sys.stderr)
        sys.exit(2)
