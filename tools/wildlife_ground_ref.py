#!/usr/bin/env python3
"""Supplied-fact full-island integer leaf; docs/sim/wildlife-preprocess.md.

No original scene, city, climate or catalogue provenance is certified. N = null
descriptor, A = absent property, F = unsuccessful query, V = successful byte.
Missing fact refuses, including object zero. Exit 0 agree / 1 differ / 2 invalid.
"""
from pathlib import Path
import re
import sys


def integer(text, low, high):
    if not re.fullmatch(r'-?[0-9]+', text):
        raise ValueError('decimal integer required')
    n = int(text)
    if not low <= n <= high:
        raise ValueError('integer out of range')
    return n


def values(text, high=255):
    return [] if text == '-' else [integer(x, 0, high) for x in text.split(',')]


def fields(tokens, keys):
    out = {}
    for token in tokens:
        if token.count('=') != 1:
            raise ValueError('key=value required')
        k, v = token.split('=')
        if k in out:
            raise ValueError('duplicate key')
        out[k] = v
    if set(out) != keys:
        raise ValueError('missing or unknown key')
    return out


def compute(p):
    w, h = [integer(p[k], 1, 0xffffffff) for k in ('w', 'h')]
    origin = p['origin'].split(',')
    if len(origin) != 2:
        raise ValueError('origin shape')
    x0, y0 = [integer(x, -(1 << 31), (1 << 31)-1) for x in origin]
    if x0+w-1 > (1 << 31)-1 or y0+h-1 > (1 << 31)-1:
        raise ValueError('geometry exceeds tile coordinates')
    gfx = values(p['gfx'], 65535)
    objects = values(p['objects'], 0xffffffff)
    old = values(p['ground'])
    lookup = values(p['lookup'])
    kernel = values(p['kernel'])
    if len(kernel) != 25:
        raise ValueError('kernel needs 25 bytes')
    if any(len(a) != w*h for a in (gfx, objects, old)):
        raise ValueError('plane length')
    facts = {}
    for row in ([] if p['facts'] == '-' else p['facts'].split(';')):
        parts = row.split(',')
        if len(parts) != 4:
            raise ValueError('fact shape')
        word = integer(parts[0], 0, 0xffffffff)
        state = parts[1]
        kind = integer(parts[2], 0, 0xffffffff)
        value = integer(parts[3], 0, 255)
        if word in facts:
            raise ValueError('duplicate object fact')
        if state not in ('N', 'A', 'F', 'V') or (state == 'N' and kind) or (state != 'V' and value):
            raise ValueError('malformed object fact')
        facts[word] = state, kind, value
    scratch = []
    for graphic, word in zip(gfx, objects):
        if word not in facts:
            raise ValueError('missing object fact')
        if graphic >> 2 >= len(lookup):
            raise ValueError('lookup coverage')
        p0 = lookup[graphic >> 2]
        state, kind, v = facts[word]
        if state == 'V' and 225 <= kind <= 256 and v:
            p0 = (p0+3*v)//4 if p0 < v else (v+3*p0)//4
        scratch.append((p0+5)//10)
    result = old.copy()
    for i, centre in enumerate(scratch):
        if centre == 0:
            continue
        x, y = i % w, i // w
        total = 0
        for dy in range(-2, 3):
            for dx in range(-2, 3):
                nx, ny = x+dx, y+dy
                if 0 <= nx < w and 0 <= ny < h:
                    total += (scratch[ny*w+nx]*kernel[(dy+2)*5+dx+2])//12
        result[i] = (old[i] & 240) | min(total, 10)
    return {'ground': ','.join(map(str, result))}


def main():
    if len(sys.argv) != 2:
        raise ValueError('one vectors path required')
    count = disagree = 0
    names = set()
    for line in Path(sys.argv[1]).read_text(encoding='utf-8').splitlines():
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        if line.count('=>') != 1:
            raise ValueError('one => required')
        left, right = line.split('=>')
        tokens = left.split()
        if len(tokens) < 2 or tokens[0] != 'ground':
            raise ValueError('ground record required')
        name = tokens[1]
        if name in names:
            raise ValueError('duplicate name')
        names.add(name)
        p = fields(tokens[2:], {'w', 'h', 'origin', 'gfx', 'objects', 'ground', 'lookup', 'kernel', 'facts'})
        expected = fields(right.split(), {'ground'})
        values(expected['ground'])
        got = compute(p)
        count += 1
        if got != expected:
            disagree += 1
            print(f'{name}: expected {expected}, got {got}')
    if count == 0:
        raise ValueError('zero vectors')
    print(f'{count} vectors, {disagree} disagreements')
    return 1 if disagree else 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, OSError, UnicodeError) as error:
        print(error, file=sys.stderr)
        sys.exit(2)
