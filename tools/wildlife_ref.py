#!/usr/bin/env python3
"""Integer decision oracle from docs/re/wildlife.md §§1–10, not an engine port.

Usage: wildlife_ref.py VECTORS. Exit 0 agree, 1 disagree, 2 unusable input.
Records: TYPE LABEL key=value ... => expected=value ... ; # begins a comment.
Lists are comma separated; '-' is empty. All records require draws (raw u32
world outputs); unused trailing draws are allowed. Every expected side requires
consumed. Expected keys may otherwise be a nonempty subset of the result.

Schemas are deliberately local: habitat receives resolved object facts, create
receives the climate's 32-entry types/goods tables, load receives an already
divided timestamp base, and completion receives the documented callee outcome.
They do not certify transitive draws (OQ-578), float positions (OQ-582), or a
whole tick. Each record starts at its named decision; quantities are those at
that decision (after the completed action's effects), not pre-action cargo.
See the schema definitions below and the hand-derived fixtures for all fields.
"""
import re
import sys
from pathlib import Path


class Malformed(ValueError):
    pass


def integer(value, maximum=0xffffffff):
    if not re.fullmatch(r'(?:0[xX][0-9a-fA-F]+|[0-9]+)', value):
        raise Malformed('expected unsigned integer: ' + value)
    try:
        number = int(value, 16 if value.lower().startswith('0x') else 10)
    except ValueError as error:
        raise Malformed('integer representation is too long') from error
    if number > maximum:
        raise Malformed('integer out of range: ' + value)
    return number


def numbers(value, maximum=0xffffffff):
    return [] if value == '-' else [integer(x, maximum) for x in value.split(',')]


class Record:
    def __init__(self, fields, schema):
        if set(fields) != set(schema):
            raise Malformed('fields: missing=' + ','.join(sorted(set(schema) - set(fields)))
                            + ' unknown=' + ','.join(sorted(set(fields) - set(schema))))
        self.values = {key: convert(fields[key]) for key, convert in schema.items()}
        self.draws = self.values['draws']
        self.used = 0

    def __getitem__(self, key):
        return self.values[key]

    def draw(self):
        if self.used == len(self.draws):
            raise Malformed('draw stream exhausted')
        result = self.draws[self.used]
        self.used += 1
        return result

    def result(self, **values):
        return dict(values, consumed=self.used)


def byte(x):
    return integer(x, 255)


def word(x):
    return integer(x, 65535)


def boolean(x):
    return integer(x, 1)


def choice(*options):
    def parse(x):
        if x not in options:
            raise Malformed('expected one of ' + ','.join(options))
        return x
    return parse


def facts(x):
    """Exactly eight id:goods0:kind triples; zero ids denote empty cells."""
    result = [tuple(word(part) for part in item.split(':')) for item in x.split(',')]
    if len(result) != 8 or any(len(row) != 3 for row in result):
        raise Malformed('neighbors must contain eight id:goods0:kind triples')
    return result


SCHEMAS = {
    'density': dict(n=byte),
    'habitat': dict(center=word, goods=word, kind=word, neighbors=facts, property7=word),
    'replenish': dict(candidates=numbers, deficit=byte, occupied=numbers),
    'create': dict(cave=boolean, entrances=word, climate=byte, types=numbers,
                   goods=numbers, saved=word),
    'load': dict(entry4=byte, base=lambda x: integer(x, 65), mode=byte, goods=word),
    'animal': dict(event=choice('complete', 'fallback', 'idle'), state=byte,
                   success=boolean, quantity=word, capacity=word, variants=byte),
    'walk': dict(attempt=byte, behavior=byte, facing=byte),
    'route_old': dict(quantity=word),
    'wolf': dict(event=choice('complete', 'fallback', 'idle'), state=byte,
                 success=boolean, quantity=word, capacity=word, variants=byte),
    'prey': dict(predator=word, prey=word, kind=word),
}
for _schema in SCHEMAS.values():
    _schema['draws'] = numbers


def transition(r, wolf=False):
    event, state = r['event'], r['state']
    if event == 'fallback':
        mapping = {27: 28, 30: 27, 31: 30} if wolf else {26: 27, 27: 28}
        return r.result(next=mapping.get(state, 0))
    if event == 'idle':
        if state != 28 or not 1 <= r['variants'] <= 3:
            raise Malformed('idle needs state=28 and variants=1..3')
        return r.result(next=28, variant=r.draw() % r['variants'])
    supported = (25, 27, 28, 30, 31) if wolf else (25, 26, 27, 28)
    if state not in supported:
        raise Malformed('unsupported completion state')
    # The document identifies New 26/27/28 and Wolf 27/28 completion
    # helpers as succeeding; success is a callee outcome, not an invented gate.
    if state in ((27, 28) if wolf else (26, 27, 28)) and not r['success']:
        raise Malformed('documented completion helper cannot fail here')
    if wolf:
        if state == 25:
            next_state = 27 if r['success'] else 25
        elif state == 30:
            next_state = 31 if r['success'] else 30
        elif state == 31:
            next_state = 25 if r['success'] else 31
        elif state == 27:
            remainder = r.draw() % 7
            next_state = 28 if remainder == 0 else (30 if remainder <= 3 else 27)
        else:
            next_state = 30 if r.draw() & 1 else 27
        search = 30
    else:
        if state == 25 and not r['success']:
            next_state = 25
        elif state == 26:
            next_state = 25
        elif state == 27:
            next_state = 26 if r.draw() % 5 else 28
        else:
            next_state = (~r.draw() & 1) | 26
        search = 26
    if next_state == search and r['quantity'] > 2 * (r['capacity'] // 3):
        next_state = 27
    return r.result(next=next_state)


def evaluate(kind, fields):
    if kind not in SCHEMAS:
        raise Malformed('unknown record type: ' + kind)
    r = Record(fields, SCHEMAS[kind])
    if kind == 'density':
        if r['n'] > 16:
            raise Malformed('only the documented 16-cell sweep is supported')
        return r.result(desired=[0, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 5, 5, 5][r['n']])
    if kind == 'habitat':
        center_ok = r['center'] != 0 and (r['goods'] == 0x46 or r['kind'] == 0x145)
        score = 1 + sum((goods == 0x46) + (kind_id == 0x145)
                        for obj, goods, kind_id in r['neighbors'] if obj != 0)
        return r.result(passed=int(center_ok and score >= (1 if r['property7'] == 5 else 4)))
    if kind == 'replenish':
        candidates = list(r['candidates'])
        if len(set(candidates)) != len(candidates) or len(candidates) > 16:
            raise Malformed('compacted candidates must be unique, at most 16')
        occupied = set(r['occupied'])
        chosen = []
        while len(chosen) < r['deficit']:
            if not candidates:
                raise Malformed('OQ-572: candidate exhaustion is unresolved')
            index = r.draw() % len(candidates)
            cell = candidates[index]
            candidates[index] = candidates[-1]
            candidates.pop()
            if cell not in occupied:
                chosen.append(cell)
        return r.result(chosen=chosen, remaining=candidates)
    if kind == 'create':
        types, goods = r['types'], r['goods']
        if len(types) != 32 or len(goods) != 32 or any(x > 65535 for x in types + goods):
            raise Malformed('types/goods need 32 u16 entries each for the supplied climate')
        if r['cave'] and not r['entrances']:
            raise Malformed('OQ-574: cave without entrance')
        position = -1 if r['cave'] else r.draw() & 3
        index = r.draw() & 31
        inspected = 1
        if r['saved'] != 255:
            while inspected < 18 and goods[index] != r['saved']:
                index = (index + 1) & 31
                inspected += 1
        return r.result(position=position, index=index, species=types[index], inspected=inspected)
    if kind == 'load':
        timestamp = (r['base'] + (0 if r['entry4'] & 1 else r.draw() & 31)) & 255
        return r.result(timestamp=timestamp, saved=255 if r['mode'] & 1 else r['goods'])
    if kind in ('animal', 'wolf'):
        return transition(r, wolf=kind == 'wolf')
    if kind == 'walk':
        doubled = r['behavior'] == 7 and r.draw() % 6 == 1
        minimum, modulus = (80, 64) if doubled else (40, 32)
        half_span = 128 if r['attempt'] < 2 else 256
        base = ((r['facing'] + 2) << 6) & 255
        radii, angles = [], []
        for _ in range(3):
            radii.append(minimum + r.draw() % modulus)
            angles.append((base + r.draw() % (2 * half_span + 1) - half_span) & 511)
        return r.result(radii=radii, angles=angles, attempt=(r['attempt'] + 1) & 255)
    if kind == 'route_old':
        return r.result(next=12 if r['quantity'] else (7 if r.draw() & 1 else 12))
    if r['prey'] == 130:
        raise Malformed('OQ-585: zero-HP prey is not a certified corpse/kill case')
    short = {117, 122, 129, 135, 502, 504, 505}
    extra = {118, 119, 128, 130, 120, 123, 127, 132, 137, 141, 501}
    allowed = short | extra if r['predator'] in {116, 126, 131, 201, 529} else short
    return r.result(admitted=int(49 <= r['kind'] <= 64 and r['prey'] in allowed))


def pairs(tokens):
    result = {}
    for token in tokens:
        if token.count('=') != 1:
            raise Malformed('expected key=value: ' + token)
        key, value = token.split('=')
        if not key or not value or key in result:
            raise Malformed('empty or duplicate field: ' + token)
        result[key] = value
    return result


def expected(value, actual):
    # Only position uses -1; all other scalar/list results are nonnegative.
    if isinstance(actual, list):
        return numbers(value)
    return -1 if value == '-1' else integer(value)


def check(path):
    count, disagreements = 0, 0
    for line_no, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        line = line.split('#', 1)[0].strip()
        if not line:
            continue
        try:
            if line.count('=>') != 1:
                raise Malformed('expected exactly one =>')
            left, right = line.split('=>')
            tokens = left.split()
            if len(tokens) < 2 or '=' in tokens[1]:
                raise Malformed('expected TYPE LABEL')
            actual = evaluate(tokens[0], pairs(tokens[2:]))
            wanted = pairs(right.split())
            if 'consumed' not in wanted or set(wanted) - set(actual):
                raise Malformed('expected consumed and only supported output keys')
            parsed = {key: expected(value, actual[key]) for key, value in wanted.items()}
            for key, value in parsed.items():
                if actual[key] != value:
                    disagreements += 1
                    print(f'{path}:{line_no}: {tokens[1]} {key}: expected {value}, got {actual[key]}',
                          file=sys.stderr)
            count += 1
        except Malformed as error:
            raise Malformed(f'{path}:{line_no}: {error}') from error
    if not count:
        raise Malformed('zero vectors')
    print(f'{count} vectors, {disagreements} disagreements')
    return int(disagreements != 0)


def main():
    if len(sys.argv) != 2:
        print('usage: wildlife_ref.py VECTORS', file=sys.stderr)
        return 2
    try:
        return check(Path(sys.argv[1]))
    except (Malformed, OSError, UnicodeError) as error:
        print(str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
