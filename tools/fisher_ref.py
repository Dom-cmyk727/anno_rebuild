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
corner_grid: width*height HEIGHT samples -> logical (width+1)*(height+1) grid,
copying interior samples and leaving the last row/column zero.
--check-fish-water GAME_DIR is opt-in, reads via insel.py, prints EVERY closed
tile11 and exits1 if any; default expected counts are50 scenarios/2473 placements.
Explicit --expected-scenarios/--expected-placements support bounded synthetic
contract controls, not silently incomplete shipped-corpus acceptance.
trip: cargo is the quantity AFTER a cut. cut_match: selection by goods71 then
conditional harvest; only tile11 matches in the documented data. Nonmatches
are unchanged because no cut is attempted; this is not an unconditional cut
of a replaced or arbitrary resource tile. No ice-fishing chain is simulated.
Float positions/continuations are refused with OQ-603. No engine state or port.
"""
import argparse
import hashlib
import re
import struct
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
    'corner_grid': dict(width=positive_word, height=positive_word,
                        heights=lambda x: [] if x == '-' else [byte(v) for v in x.split(',')]),
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


def corner_grid(width, height, samples):
    """§5: load-side logical grid; allocation has one further zero padding row."""
    if len(samples) != width * height:
        raise Malformed('HEIGHT sample count must equal width*height')
    stride = width + 1
    grid = [0] * (stride * (height + 1))
    for y in range(height):
        grid[y * stride:y * stride + width] = samples[y * width:(y + 1) * width]
    return grid


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
    if kind == 'corner_grid':
        return r.result(stride=r['width'] + 1,
                        grid=corner_grid(r['width'], r['height'], r['heights']))
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
            parsed = {key: numbers(value) if isinstance(actual[key], list) else bounded(value)
                      for key, value in wanted.items()}
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


def check_fish_water(root, expected_scenarios, expected_placements):
    # Explicitly opt-in: the normal vector checker reads no game files.
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import insel
    paths = sorted((root / 'Scenes').rglob('*.sze'))
    if len(paths) != expected_scenarios:
        raise Malformed(f'expected {expected_scenarios} scenarios, found {len(paths)}')
    count = failed = 0
    for path in paths:
        try:
            file_data = path.read_bytes()
            print(f'FILE {path.relative_to(root).as_posix()} '
                  f'sha256={hashlib.sha256(file_data).hexdigest()}')
            for island in insel.islands(file_data):
                width, height = island['width'], island['height']
                samples = insel.grid(island, 'HEIGHT', 1)
                grid = corner_grid(width, height, samples)
                stride = width + 1
                for tag in ('GELAENDE', 'PROD2', 'FLUESSE'):
                    for bx, by, bw, bh, data in insel.blocks(island, tag):
                        if len(data) % 6:
                            raise Malformed('partial placed-object item')
                        for at in range(0, len(data), 6):
                            kind, state, dx, dy = struct.unpack_from('<HHbb', data, at)
                            if kind & 0x3fff != 11:
                                continue
                            x, y = bx + dx, by + dy
                            if not (0 <= x < width and 0 <= y < height):
                                raise Malformed('fish tile outside island')
                            a = y * stride + x
                            corners = [grid[a], grid[a + 1], grid[a + stride],
                                       grid[a + stride + 1]]
                            mean = sum(corners) // 4
                            count += 1
                            if mean >= 32:
                                failed += 1
                                print(f'EXCEPTION {path.relative_to(root).as_posix()} '
                                      f'island={island["number"]} local={x},{y} '
                                      f'world={island["x"] + x},{island["y"] + y} '
                                      f'layer={tag} block={bx},{by} item={at // 6} '
                                      f'corners={",".join(map(str, corners))} mean={mean}')
        except (insel.Bad, OSError, struct.error) as error:
            raise Malformed(f'{path}: {error}') from error
    if count != expected_placements:
        raise Malformed(f'expected {expected_placements} fish placements, found {count}')
    print(f'FISH_WATER scenarios={len(paths)} selected={count} executed={count} '
          f'water={count - failed} failed={failed} empty=0 skipped=0 incomplete=0')
    return int(failed != 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('vectors', nargs='?', type=Path)
    parser.add_argument('--check-fish-water', type=Path, metavar='GAME_DIR')
    parser.add_argument('--expected-scenarios', type=int)
    parser.add_argument('--expected-placements', type=int)
    args = parser.parse_args()
    if (args.vectors is None) == (args.check_fish_water is None):
        parser.error('choose VECTORS or --check-fish-water GAME_DIR')
    if args.vectors and (args.expected_scenarios is not None or
                         args.expected_placements is not None):
        parser.error('expected counts belong to the opt-in corpus mode')
    try:
        if args.vectors:
            return check(args.vectors)
        scenarios = 50 if args.expected_scenarios is None else args.expected_scenarios
        placements = 2473 if args.expected_placements is None else args.expected_placements
        if scenarios <= 0 or placements <= 0:
            raise Malformed('expected corpus counts must be positive')
        return check_fish_water(args.check_fish_water, scenarios, placements)
    except (Malformed, OSError, UnicodeError) as error:
        print(str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
