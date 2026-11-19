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
            if len(tokens) < 2 or tokens[0] != 'gfx':
                raise ValueError('unknown record type or missing label')
            if tokens[1] in labels:
                raise ValueError('duplicate label')
            labels.add(tokens[1])
            inputs = fields(tokens[2:], {'w', 'h', 'height', 'gfx'})
            expected = fields(right.split(), {'gfx', 'cleared'})
            w, h = (unsigned(inputs[k], 0xffffffff) for k in ('w', 'h'))
            if w == 0 or h == 0:
                raise ValueError('positive geometry required')
            samples = values(inputs['height'], 255)
            graphics = values(inputs['gfx'], 65535)
            wanted = values(expected['gfx'], 65535)
            cleared = unsigned(expected['cleared'], w * h)
            if any(len(v) != w * h for v in (samples, graphics, wanted)):
                raise ValueError('plane length must equal w*h')
            actual = cleanup(w, h, samples, graphics)
            expected = {'gfx': ','.join(map(str, wanted)), 'cleared': str(cleared)}
            for key in expected:
                if actual[key] != expected[key]:
                    print(f'{path}:{number} {tokens[1]} {key}: expected '
                          f'{expected[key]}, got {actual[key]}')
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
