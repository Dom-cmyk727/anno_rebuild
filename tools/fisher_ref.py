#!/usr/bin/env python3
"""Integer fisher reference, from docs/re/fisher.md at accepted 6d57bb12 only.

Usage: fisher_ref.py VECTORS; exit0 agree,1 disagree,2 malformed/missing/empty.
TYPE LABEL key=value ... => expected_key=value ... (comments start with #).
Every schema requires draws=- or a comma-separated raw u32 draw stream; unused
suffix draws are allowed. Every output key, including consumed, is required.
No PRNG is implemented. Direct draws only; transitive_draws is OQ-604.

farm_tick: elapsed is an already wrapped u16 difference, advanced is0 or500,
stamp wraps, flags is the building flag byte. made is a redundant caller fact
validated against this visit's calculated added quantity, not prior production.
dispatched means an eligible creation ATTEMPT, not a successful worker spawn.
converted: t0 and i0 are nonnegative exact numerator/denominator fractions;
t0 is positive. This is the documented rational formula, not an x87 emulator.
search_rect: integer farm origin/dimensions, no float position/continuation.
sea_cell: heights is exactly four unsigned corner-height bytes.
trip: cargo is the quantity AFTER a cut. cut_match: selection by goods71 then
conditional harvest; only tile11 matches in the documented data. Nonmatches
are unchanged because no cut is attempted; this is not an unconditional cut
of a replaced or arbitrary resource tile. No ice-fishing chain is simulated.
Float positions/continuations are refused with OQ-603. No engine state or port.
"""
import re
import sys
from pathlib import Path


class Malformed(ValueError):
    pass


def bounded(value, low=0, high=0xffffffff):
    if not re.fullmatch(r'-?(?:0[xX][0-9a-fA-F]+|[0-9]+)', value):
        raise Malformed('expected integer: ' + value)
    try:
        number = int(value, 16 if 'x' in value.lower() else 10)
    except ValueError as error:
        raise Malformed('integer representation is too long') from error
    if not low <= number <= high:
        raise Malformed('integer out of range: ' + value)
    return number


def word(x):
    return bounded(x, 0, 65535)


def coordinate(x):
    if not re.fullmatch(r'-?(?:0[xX][0-9a-fA-F]+|[0-9]+)', x):
        raise Malformed('OQ-603: float positions are not settled; expected integer: ' + x)
    return word(x)


def positive_word(x):
    return bounded(x, 1, 65535)


def byte(x):
    return bounded(x, 0, 255)


def boolean(x):
    return bounded(x, 0, 1)


def numbers(x):
    return [] if x == '-' else [bounded(v) for v in x.split(',')]


def fraction(x):
    parts = x.split('/')
    if len(parts) != 2:
        raise Malformed('expected exact fraction numerator/denominator')
    return bounded(parts[0]), bounded(parts[1], 1)


def heights(x):
    parts = x.split(',')
    if len(parts) != 4:
        raise Malformed('expected four corner heights')
    return [byte(v) for v in parts]


SCHEMAS = {
    'farm_tick': dict(elapsed=word, stamp=word, switch=boolean, flags=byte,
                      made=word, workers=lambda x: bounded(x, 0, 5), maximum=byte,
                      raw=word, need=positive_word, rate=positive_word,
                      output=word, capacity=word, activity=word),
    'converted': dict(t0=fraction, i0=fraction),
    'search_rect': dict(x=coordinate, y=coordinate, width=lambda x: bounded(x, 1, 255),
                        height=lambda x: bounded(x, 1, 255), radius=byte),
    'sea_cell': dict(heights=heights),
    'trip': dict(cargo=word, capacity=word),
    'cut_match': dict(tile=word),
}
for _schema in SCHEMAS.values():
    _schema['draws'] = numbers


class Record:
    def __init__(self, fields, schema):
        if set(fields) != set(schema):
            raise Malformed('fields: missing=' + ','.join(sorted(set(schema) - set(fields)))
                            + ' unknown=' + ','.join(sorted(set(fields) - set(schema))))
        self.values = {key: convert(fields[key]) for key, convert in schema.items()}
        self.used = 0

    def __getitem__(self, key):
        return self.values[key]

    def draw(self):
        if self.used == len(self['draws']):
            raise Malformed('draw stream exhausted')
        value = self['draws'][self.used]
        self.used += 1
        return value

    def result(self, **values):
        return dict(values, consumed=self.used)


def farm_tick(r):
    # §2, AnnoWelt6ea90–6ee5f: gate, decay, conversion, THEN dispatch admission.
    advanced = 500 if r['elapsed'] >= 500 else 0
    ratio = debit = added = dispatched = 0
    raw, output, activity = r['raw'], r['output'], r['activity']
    if advanced and r['switch']:
        activity = activity * 255 // 256
        if not r['flags'] & 0x30:
            if not (r['maximum'] == 1 and r['workers']) and output < r['capacity']:
                ratio = min(128, raw * 128 // r['need'])
                if ratio >= 64:
                    debit = r['need'] * ratio // 128
                    added = r['rate'] * ratio // 128
                    raw = (raw - debit) & 65535
                    output = (output + added) & 65535
                    activity = (activity + 16 * added) & 65535
            # §4: output-full is not a dispatch veto. The value drawn is discarded.
            dispatched = int((r['maximum'] > 1 or added == 0)
                             and r['workers'] < r['maximum']
                             and 384 // r['rate'] >= raw // r['need'])
            if dispatched:
                r.draw()
    if r['made'] != added:
        raise Malformed('made fact disagrees with calculated added quantity')
    return r.result(advanced=advanced, stamp=(r['stamp'] + advanced) & 65535,
                    r=ratio, debit=debit, added=added, raw=raw, output=output,
                    activity=activity, dispatched=dispatched)


def evaluate(kind, fields):
    if kind in ('float_position', 'continuation'):
        raise Malformed('OQ-603: float positions and continuations are not settled')
    if kind == 'transitive_draws':
        raise Malformed('OQ-604: transitive draw total is not settled')
    if kind not in SCHEMAS:
        raise Malformed('unknown record type: ' + kind)
    r = Record(fields, SCHEMAS[kind])
    if kind == 'farm_tick':
        return farm_tick(r)
    if kind == 'converted':
        # §1: floor(256/T0 + .5); need uses the UNROUNDED quotient times I0.
        tn, td = r['t0']
        inn, ind = r['i0']
        if not tn:
            raise Malformed('t0 must be positive')
        rate = (512 * td + tn) // (2 * tn)
        need = 256 * td * inn // (tn * ind)
        if max(rate, need) > 65535:
            raise Malformed('converted values outside documented u16 domain')
        return r.result(need=need, rate=rate)
    if kind == 'search_rect':
        # §3, 59210–59398: origin words and extent bytes, not widened/clamped.
        x = (r['x'] + (r['width'] - 1) // 2 - r['radius']) & 65535
        y = (r['y'] + (r['height'] - 1) // 2 - r['radius']) & 65535
        width = (2 * r['radius'] + 1 + (r['width'] - 1) % 2) & 255
        height = (2 * r['radius'] + 1 + (r['height'] - 1) % 2) & 255
        return r.result(x=x, y=y, width=width, height=height)
    if kind == 'sea_cell':
        # §3, 90560/9226b: integer four-corner mean; equality32 is closed.
        opened = int(sum(r['heights']) // 4 < 32)
        return r.result(open=opened, free=511 * opened)
    if kind == 'trip':
        # §3, 5c260: repeat below capacity, home at equality or above.
        repeat = int(r['cargo'] < r['capacity'])
        return r.result(repeat=repeat, home=1 - repeat)
    # §1/3: goods71 selects only11; conditional harvest leaves young10.
    match = int(r['tile'] == 11)
    return r.result(match=match, remaining=10 if match else r['tile'])


def pairs(tokens):
    values = {}
    for token in tokens:
        if token.count('=') != 1:
            raise Malformed('expected key=value: ' + token)
        key, value = token.split('=')
        if not key or not value or key in values:
            raise Malformed('empty or duplicate field: ' + token)
        values[key] = value
    return values


def check(path):
    count = disagreements = 0
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
            if set(wanted) != set(actual):
                raise Malformed('expected all output keys including consumed, and no unknown keys')
            parsed = {key: bounded(value) for key, value in wanted.items()}
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
        print('usage: fisher_ref.py VECTORS', file=sys.stderr)
        return 2
    try:
        return check(Path(sys.argv[1]))
    except (Malformed, OSError, UnicodeError) as error:
        print(str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
