#!/usr/bin/env python3
"""Integer decision reference from docs/re/whaler.md §§3–9, not an engine port.

Usage: whale_ref.py VECTORS. TYPE LABEL key=value ... => expected=value ...
Exit 0 agree / 1 disagree / 2 unusable input, including zero vectors.
Comments start with #; comma-separated lists use '-' for empty. Every record
requires draws (raw u32 world outputs); trailing unused draws are permitted.
Expected keys may be a subset of the result, but must include consumed.

All schemas below are required fields. Elapsed is an already wrapping-u16
elapsed difference; stock/activity/progress fields are original u16 units.
HP inputs are signed16; coordinates are integer-only samples (no float model).
Replenish lists contain exactly three slot facts in slot order0,1,2.
whale_route selection=initial|later selects walk, then failed=1 models that
kind12 start's failure; next is a route-node offset, 0x10 or0x1c.
whaler_tick takes explicit callback facts: target says whether search resolves
prey; launch_slot is -1 for no successful launch, otherwise its selected slot.
idle is evaluated at dispatch after queue processing. No search/creation is
implemented. These inputs distinguish eligibility from a successful dispatch.
Sea legality/depth (OQ-595) and indirect search/projectile draws (OQ-594) are
refused. No PRNG, float positions, full boat life or engine state is simulated.
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


def byte(x):
    return bounded(x, 0, 255)


def word(x):
    return bounded(x, 0, 65535)


def boolean(x):
    return bounded(x, 0, 1)


def signed_word(x):
    return bounded(x, -32768, 32767)


def slot(x):
    return bounded(x, -1, 4)


def numbers(x, convert=bounded):
    return [] if x == '-' else [convert(v) for v in x.split(',')]


def triple(x, convert):
    values = numbers(x, convert)
    if len(values) != 3:
        raise Malformed('expected exactly three slot values')
    return values


def selection(x):
    if x not in ('initial', 'later'):
        raise Malformed('selection must be initial or later')
    return x


SCHEMAS = {
    'replenish': dict(elapsed=word, stamp=word,
                      occupied=lambda x: triple(x, boolean),
                      retries=lambda x: triple(x, byte),
                      success=lambda x: triple(x, boolean)),
    'whale_walk': dict(nearby=boolean, counter=lambda x: bounded(x, 0, 0x7fffffff),
                       home_x=word, home_z=word, figure_x=word, figure_z=word,
                       width=word, height=word),
    'whale_route': dict(selection=selection, failed=boolean),
    'whale_hp': dict(old=signed_word, new=signed_word, ending=boolean),
    'whaler_tick': dict(elapsed=word, switch=boolean, active=byte, progress=word,
                        selected=slot, idle=boolean, raw=word, capacity=word,
                        need=lambda x: bounded(x, 1, 65535), rate=word,
                        output=word, activity=word, target=boolean, launch_slot=slot),
    'boat_request': dict(money=lambda x: bounded(x, 0, 0x7fffffff),
                         wood=bounded, rope=bounded, count=byte, active=byte),
    'ship_attack': dict(count=lambda x: bounded(x, 1, 255)),
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


def tick(r):
    # §4: gate -> queue -> activity decay -> hunt -> conversion, in that order.
    advanced = 500 if r['elapsed'] >= 500 else 0
    running = advanced and r['switch']
    progress, selected = r['progress'], r['selected']
    launch = 0
    if running and r['active'] and progress < 25600:
        progress += 853
        launch = int(progress >= 25600)
    if r['launch_slot'] != -1 and (not launch or selected != -1):
        raise Malformed('successful launch_slot requires a launch and selected=-1')
    if launch and selected == -1:
        selected = r['launch_slot']
    activity, raw, output = r['activity'], r['raw'], r['output']
    hunt, ratio, debit, added = 0, 0, 0, 0
    if running:
        activity = activity * 127 // 128
        hunt = int(selected != -1 and r['idle'] and raw < r['capacity'] and r['target'])
        if output < r['capacity']:
            ratio = min(128, raw * 128 // r['need'])
            if ratio >= 64:
                debit = ratio * r['need'] // 128
                if debit:
                    added = r['rate'] * ratio // 128
                    raw = (raw - debit) & 65535
                    output = (output + added) & 65535
                    activity = (activity + 16 * added) & 65535
    return r.result(advanced=advanced, launch_attempted=launch, progress=progress,
                    selected=selected, hunt_dispatched=hunt, r=ratio, debit=debit,
                    added=added, raw=raw, output=output, activity=activity)


def evaluate(kind, fields):
    if kind in ('sea_legality', 'sea_depth'):
        raise Malformed('OQ-595: sea legality/depth is not settled')
    if kind in ('indirect_search', 'projectile_draws'):
        raise Malformed('OQ-594: indirect search/projectile draws are not settled')
    if kind not in SCHEMAS:
        raise Malformed('unknown record type: ' + kind)
    r = Record(fields, SCHEMAS[kind])
    if kind == 'replenish':
        # §3: add2500 once, skip occupied, increment byte before threshold.
        occupied, retries = list(r['occupied']), list(r['retries'])
        attempted = []
        advanced = 2500 if r['elapsed'] >= 2500 else 0
        if advanced:
            for i in range(3):
                if not occupied[i]:
                    retries[i] = (retries[i] + 1) & 255
                    if retries[i] >= 10:
                        attempted.append(i)
                        if r['success'][i]:
                            occupied[i], retries[i] = 1, 0
        return r.result(stamp=(r['stamp'] + advanced) & 65535, advanced=advanced,
                        occupied=occupied, retries=retries, attempted=attempted)
    if kind == 'whale_walk':
        # §7 direct draws only; all inputs to the coordinate branch are integers.
        dx, dz = 7 + r.draw() % 5, 7 + r.draw() % 5
        if r.draw() & 1:
            dx = -dx
        if r.draw() & 1:
            dz = -dz
        if r['nearby'] and r['counter'] < 4:
            x, z = r['figure_x'] + dx, r['figure_z'] + dz
            counter = r['counter'] + 1
        else:
            x, z = r['home_x'] + dx, r['home_z'] + dz
            counter = max(0, r['counter'] - 1)
        r.draw()  # Fifth world draw is consumed even though its result is discarded.
        return r.result(offsets=[dx, dz], before=[x, z],
                        goal=[min(r['width'], max(0, x)), min(r['height'], max(0, z))],
                        counter=counter)
    if kind == 'whale_route':
        # §7 both selections choose R+0x10; failed kind12 start falls to R+0x1c.
        return r.result(next=0x1c if r['failed'] else 0x10,
                        initialize=int(r['selection'] == 'initial'))
    if kind == 'whale_hp':
        # §6 restricts this schema to signed16 HP writes; no wide-int alias model.
        transition = r['old'] > 0 and r['new'] <= 0 and not r['ending']
        return r.result(transition=int(transition))
    if kind == 'whaler_tick':
        return tick(r)
    if kind == 'boat_request':
        # §8 check gold, wood, rope; enqueue separately refuses a full queue.
        reason = (1 if r['money'] < 600 else 2 if r['wood'] < 3072 else
                  3 if r['rope'] < 1792 else 4 if r['count'] >= 1 else 0)
        enqueued = int(reason == 0)
        return r.result(enqueued=enqueued, refused=int(not enqueued), reason=reason,
                        gold=600 * enqueued, wood=3072 * enqueued,
                        rope=1792 * enqueued, active=r['active'])
    # §9: raw u32 remainder by low-byte count; count1 still consumes one draw.
    return r.result(index=r.draw() % r['count'])


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


def expected(value, actual):
    convert = lambda x: bounded(x, -0x80000000, 0xffffffff)
    return numbers(value, convert) if isinstance(actual, list) else convert(value)


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
        print('usage: whale_ref.py VECTORS', file=sys.stderr)
        return 2
    try:
        return check(Path(sys.argv[1]))
    except (Malformed, OSError, UnicodeError) as error:
        print(str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
