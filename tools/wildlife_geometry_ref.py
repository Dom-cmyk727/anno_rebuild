#!/usr/bin/env python3
"""Conditional rational geometry reference, docs/re/wildlife-ordinary-geometry.md.

JSON array of {kind, inputs, expected} records. Exit 0 agrees, 1 differs, 2
malformed/empty/unsupported. No host float, world PRNG or native x87 execution.
Separate table-initialization and callback profiles; none is selected as original.
"""
from fractions import Fraction as F
import json
from pathlib import Path
import sys

MODES = {'nearest', 'down', 'up', 'zero'}


def profile(p, rc):
    if type(p) is not int or p not in (24, 53, 64) or rc not in MODES:
        raise ValueError('unsupported numeric profile')


def integer(n, upper):
    if type(n) is not int or not 0 <= n <= upper:
        raise ValueError('unsigned integer out of source domain')
    return n


def power(e):
    return F(2) ** e


def rounded(v, p, rc):
    if not v:
        return F(0)
    negative = v < 0
    a = abs(v)
    e = a.numerator.bit_length() - a.denominator.bit_length()
    if a < power(e):
        e -= 1
    if e < -126 or e > 100:
        raise ValueError('numeric domain: normal finite values only')
    step = power(e - p + 1)
    n, r = divmod((a / step).numerator, (a / step).denominator)
    if rc == 'nearest':
        n += 2*r > (a / step).denominator or (
            2*r == (a / step).denominator and n % 2 == 1)
    elif r and ((rc == 'up' and not negative) or (rc == 'down' and negative)):
        n += 1
    return (-1 if negative else 1) * n * step


def value(word):
    word = integer(word, 0xffffffff)
    sign = -1 if word >> 31 else 1
    exponent = (word >> 23) & 255
    fraction = word & 0x7fffff
    if exponent == 0 and fraction == 0:
        return F(0)
    if exponent in (0, 255):
        raise ValueError('numeric domain: normal finite words or zero only')
    return sign * F((1 << 23) + fraction) * power(exponent - 150)


def bits(v):
    if not v:
        return 0
    sign = 0x80000000 if v < 0 else 0
    a = abs(v)
    e = a.numerator.bit_length() - a.denominator.bit_length()
    if a < power(e):
        e -= 1
    sig = a / power(e - 23)
    if sig.denominator != 1 or not -126 <= e <= 127:
        raise ValueError('not an exact normal binary32 result')
    return sign | ((e + 127) << 23) | (int(sig) - (1 << 23))


class Arithmetic:
    def __init__(self, p, rc):
        profile(p, rc)
        self.p, self.rc = p, rc

    def q(self, v):
        return rounded(v, self.p, self.rc)

    def s(self, v):
        return rounded(v, 24, self.rc)

    def add(self, a, b):
        return self.q(a+b)

    def sub(self, a, b):
        return self.q(a-b)

    def mul(self, a, b):
        return self.q(a*b)

    def div(self, a, b):
        if b == 0:
            raise ValueError('numeric domain: zero divisor')
        return self.q(a/b)


def table_word(n, pc, rc):
    integer(n, 255)
    a = Arithmetic(pc, rc)
    acc = F(0)
    for _ in range(n):
        acc = a.add(acc, F(5368709, 1 << 27))
    return bits(a.s(acc))


def position(ox, oz, cx, cz, packed, draw, pc, rc):
    a = Arithmetic(pc, rc)
    integer(ox, 65535)
    integer(oz, 65535)
    for n in (cx, cz, packed):
        integer(n, 255)
    integer(draw, 0xffffffff)
    tx, tz = ox + 8*cx + (packed & 7), oz + 8*cz + ((packed >> 3) & 7)
    choice = draw & 3
    def coordinate(tile, high):
        offset = F(11184811, 1 << 26)
        if high:
            return bits(a.s(a.sub(a.mul(a.add(F(tile), F(1)), F(1)),
                                 a.mul(F(1), offset))))
        return bits(a.s(a.mul(a.add(F(tile), offset), F(1))))
    return {'x': coordinate(tx, choice in (1, 2)),
            'z': coordinate(tz, choice in (2, 3)), 'consumed': 1}


def height(x, z, corners, pc, rc):
    a = Arithmetic(pc, rc)
    x, z = value(x), value(z)
    if x < 0 or z < 0 or x >= 67583 or z >= 67583:
        raise ValueError('height: outside ordinary source-width profile')
    if not isinstance(corners, list) or len(corners) != 4:
        raise ValueError('height: four explicit table words required')
    ha, hb, hc, hd = map(value, corners)
    if any(h < 0 for h in (ha, hb, hc, hd)):
        raise ValueError('height: source table heights must be nonnegative')
    x0, z0 = a.s(F(int(x))), a.s(F(int(z)))
    x1, z1 = a.s(a.add(x0, F(1))), a.s(a.add(z0, F(1)))
    if not (x0 < x < x1 and z0 < z < z1):
        raise ValueError('height: ordinary interior coordinates required')
    def edge(h0, h1, delta, span):
        return a.add(a.div(a.mul(a.sub(h1, h0), delta), span), h0)
    d, e = a.s(a.sub(z1, z0)), a.s(a.sub(z1, z0))
    t, u = a.s(a.sub(z, z0)), a.s(a.sub(z, z0))
    left = a.s(edge(x0, x0, t, d))
    right = a.s(edge(x1, x0, u, e))
    if left <= x <= right:
        low = a.s(edge(ha, hc, t, d))
        high = edge(hb, hc, u, e)
        triangle = 1
    else:
        right = a.s(edge(x1, x1, t, d))
        left = a.s(edge(x1, x0, t, e))
        if not left <= x <= right:
            return {'y': 0, 'triangle': 0, 'consumed': 0}
        high = edge(hb, hd, t, d)
        low = edge(hb, hc, t, e)
        triangle = 2
    y = high if left == right else a.add(
        a.div(a.mul(a.sub(high, low), a.sub(x, left)), a.sub(right, left)), low)
    if y == 0 and rc == 'down':
        raise ValueError('numeric domain: directed signed zero is unmodelled')
    return {'y': bits(a.s(y)), 'triangle': triangle, 'consumed': 0}


def evaluate(kind, inputs):
    schemas = {'position': ('ox', 'oz', 'cx', 'cz', 'packed', 'draw', 'pc', 'rc'),
               'height': ('x', 'z', 'corners', 'pc', 'rc'), 'table': ('n', 'pc', 'rc')}
    if kind not in schemas or not isinstance(inputs, dict) or set(inputs) != set(schemas[kind]):
        raise ValueError('unknown kind or input schema')
    if kind == 'position':
        return position(**inputs)
    if kind == 'height':
        return height(**inputs)
    return {'word': table_word(**inputs), 'consumed': 0}


def main(argv):
    try:
        if len(argv) != 1:
            raise ValueError('one vectors path required')
        records = json.loads(Path(argv[0]).read_text(encoding='utf-8'))
        if not isinstance(records, list) or not records:
            raise ValueError('zero vectors or invalid array')
        failures = 0
        for i, row in enumerate(records):
            if not isinstance(row, dict) or set(row) != {'kind', 'inputs', 'expected', 'derivation'}:
                raise ValueError('record schema')
            got = evaluate(row['kind'], row['inputs'])
            expected = row['expected']
            if not isinstance(expected, dict) or set(expected) != set(got):
                raise ValueError('expected output schema')
            for key, want in expected.items():
                if type(want) is not int:
                    raise ValueError('expected outputs must be integers')
                if want != got[key]:
                    failures += 1
                    print(f'{i} {key}: expected {want}, got {got[key]}')
        print(f'{len(records)} vectors, {failures} disagreements; conditional profiles only')
        return int(failures != 0)
    except (OSError, ValueError, TypeError, KeyError, ZeroDivisionError) as error:
        print('malformed: ' + str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))
