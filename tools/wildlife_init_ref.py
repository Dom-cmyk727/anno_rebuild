#!/usr/bin/env python3
"""Ordered wildlife-load decision oracle; docs/sim/wildlife-initialization.md.

JSON records, one per non-comment line: name, input, expected. Exit 0 agrees,
1 disagrees, 2 malformed/missing/empty/exhausted. Draws and resolved intervening
load effects are explicit inputs, not an emulation of the world generator.
All record allocations succeed. Restore is the proposed rebuild contract only.
"""
import json
from pathlib import Path
import sys


class Invalid(ValueError):
    pass


def object_keys(value, keys):
    if not isinstance(value, dict) or set(value) != set(keys):
        raise Invalid('object keys must be ' + ','.join(sorted(keys)))


def number(value, maximum):
    if type(value) is not int or not 0 <= value <= maximum:
        raise Invalid('unsigned integer out of range')
    return value


def array(value):
    if not isinstance(value, list):
        raise Invalid('expected array')
    return value


def label(value):
    if not isinstance(value, str) or not value or not value.isascii():
        raise Invalid('expected nonempty ASCII label')
    return value


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise Invalid('duplicate key ' + key)
        result[key] = value
    return result


def evaluate(data):
    object_keys(data, ('operation', 'mode', 'islands', 'draws'))
    operation = data['operation']
    if operation not in ('load', 'restore'):
        raise Invalid('unknown operation')
    mode = number(data['mode'], 1)  # Query result of world +0x10c(-1), not other world bits.
    draws = [number(v, 0xffffffff) for v in array(data['draws'])]
    used = 0
    records, headers, traversal, intervening = [], [], [], []
    slots = set()

    def take():
        nonlocal used
        if used == len(draws):
            raise Invalid('draw stream exhausted')
        value = draws[used]
        used += 1
        return value

    for island in array(data['islands']):
        object_keys(island, ('slot', 'raw_stamp', 'headers'))
        slot = number(island['slot'], 0xffffffff)
        if slot in slots:
            raise Invalid('reused island slot is outside fresh-load profile')
        slots.add(slot)
        base = number(island['raw_stamp'], 65535) // 500
        chunks = {}
        for hi, header in enumerate(array(island['headers'])):
            if not isinstance(header, dict) or 'kind' not in header:
                raise Invalid('header needs kind')
            if header['kind'] == 'draw':
                object_keys(header, ('kind', 'site', 'count'))
                site = label(header['site'])
                count = number(header['count'], 0xffffffff)
                if operation == 'restore':
                    raise Invalid('restore cannot run load callbacks')
                # Bound work by the supplied stream, not an invented game limit.
                if count > len(draws) - used:
                    raise Invalid('draw stream exhausted')
                start = used
                values = [take() for _ in range(count)]
                intervening.append(dict(island=slot, header=hi, site=site,
                                        first_draw=start, values=values))
                continue
            if header['kind'] != 'hirsch':
                raise Invalid('unknown header kind; unresolved effects must not be skipped')
            object_keys(header, ('kind', 'records'))
            source = array(header['records'])
            headers.append(dict(island=slot, header=hi, count=len(source)))
            for ri, record in enumerate(source):
                keys = ('x', 'y', 'flags', 'raw5', 'goods')
                object_keys(record, keys + (('timestamp',) if operation == 'restore' else ()))
                x, y = number(record['x'], 65535), number(record['y'], 65535)
                flags, raw5 = number(record['flags'], 255), number(record['raw5'], 255)
                goods = number(record['goods'], 65535)
                draw_index = None
                if operation == 'restore':
                    timestamp = number(record['timestamp'], 255)
                else:
                    jitter = 0
                    if not flags & 1:
                        draw_index = used
                        jitter = take() & 31
                    timestamp = (base + jitter) & 255
                    if mode:
                        goods = 255
                index = len(records)
                records.append(dict(island=slot, header=hi, record=ri, x=x, y=y,
                                    flags=flags, raw5=raw5, goods=goods, timestamp=timestamp,
                                    draw_index=draw_index, consumed=int(draw_index is not None)))
                chunks.setdefault((x // 8, y // 8), []).append(index)
        # Canonical report order ONLY: no claim about the original cross-chunk sweep.
        for (cx, cy), indices in sorted(chunks.items(), key=lambda item: (item[0][1], item[0][0])):
            traversal.append(dict(island=slot, chunk=[cx, cy], records=list(reversed(indices))))
    return dict(records=records, headers=headers, traversal=traversal,
                intervening=intervening, consumed=used)



def expected_shape(value):
    object_keys(value, ('records', 'headers', 'traversal', 'intervening', 'consumed'))
    number(value['consumed'], 0xffffffff)
    for r in array(value['records']):
        object_keys(r, ('island', 'header', 'record', 'x', 'y', 'flags', 'raw5', 'goods',
                        'timestamp', 'draw_index', 'consumed'))
        for key in ('island', 'header', 'record'):
            number(r[key], 0xffffffff)
        for key in ('x', 'y', 'goods'):
            number(r[key], 65535)
        for key in ('flags', 'raw5', 'timestamp'):
            number(r[key], 255)
        number(r['consumed'], 1)
        if r['draw_index'] is not None:
            number(r['draw_index'], 0xffffffff)
    for h in array(value['headers']):
        object_keys(h, ('island', 'header', 'count'))
        for v in h.values():
            number(v, 0xffffffff)
    for c in array(value['traversal']):
        object_keys(c, ('island', 'chunk', 'records'))
        number(c['island'], 0xffffffff)
        if len(array(c['chunk'])) != 2:
            raise Invalid('chunk needs two coordinates')
        for v in c['chunk'] + array(c['records']):
            number(v, 0xffffffff)
    for d in array(value['intervening']):
        object_keys(d, ('island', 'header', 'site', 'first_draw', 'values'))
        label(d['site'])
        for key in ('island', 'header', 'first_draw'):
            number(d[key], 0xffffffff)
        for v in array(d['values']):
            number(v, 0xffffffff)


def main(argv):
    if len(argv) != 1:
        print('usage: wildlife_init_ref.py VECTORS', file=sys.stderr)
        return 2
    try:
        rows = []
        for line in Path(argv[0]).read_text(encoding='utf-8').splitlines():
            if not line.strip() or line.lstrip().startswith('#'):
                continue
            row = json.loads(line, object_pairs_hook=unique_object)
            object_keys(row, ('name', 'input', 'expected'))
            label(row['name'])
            actual = evaluate(row['input'])
            expected_shape(row['expected'])
            # Validate all expected output types too: bool must not compare equal to int.
            if json.dumps(actual, sort_keys=True) != json.dumps(row['expected'], sort_keys=True):
                rows.append((row['name'], actual, row['expected']))
            else:
                rows.append((row['name'], None, None))
        if not rows:
            raise Invalid('zero vectors')
    except (ValueError, OSError, UnicodeError, RecursionError) as error:
        print('INVALID: ' + str(error), file=sys.stderr)
        return 2
    disagreements = 0
    for name, actual, expected in rows:
        if actual is not None:
            disagreements += 1
            print(name + ': expected ' + json.dumps(expected, sort_keys=True)
                  + ', got ' + json.dumps(actual, sort_keys=True))
    print(f'{len(rows)} vectors, {disagreements} disagreements')
    return int(disagreements != 0)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
