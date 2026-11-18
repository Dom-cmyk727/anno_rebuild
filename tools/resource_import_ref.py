#!/usr/bin/env python3
"""Owned ordered resource prefix; docs/sim/wildlife-resource-initialization.md.

Authored key=value vectors, decimal integers, tuples separated by ';', lists by ','.
No world generator: bounded raw draws are supplied. Original place(), not engine_place().
"""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from placement_ref import Island, Type, place  # noqa: E402


def numbers(text):
    return [] if text == '-' else [int(x) for x in text.split(',')]


def fields(text):
    result = {}
    for token in text.split():
        k, v = token.split('=', 1)
        if k in result:
            raise ValueError('duplicate key')
        result[k] = v
    return result


def evaluate(p):
    required = {'w', 'h', 'heights', 'objects', 'plane', 'types', 'records', 'draws', 'slots', 'map104', 'map201', 'map404'}
    if set(p) not in (required, required | {'anchors'}):
        raise ValueError('keys')
    w, h = int(p['w']), int(p['h'])
    heights, objects, plane = (numbers(p[k]) for k in ('heights', 'objects', 'plane'))
    if w <= 0 or h <= 0 or any(len(a) != w*h for a in (heights, objects, plane)):
        raise ValueError('geometry')
    if any(v < 0 or v > 255 for a in (heights, plane) for v in a):
        raise ValueError('byte')
    reg, facts = {}, {}
    for row in p['types'].split(';'):
        tid, kind, tw, th, flags, goods, tag = map(int, row.split(':'))
        if (not 0 < tid < 0xffffffff or tid in reg or not 0 <= kind <= 0xffffffff
                or min(tw, th) <= 0 or max(tw, th) > (1 << 16)
                or not 0 <= flags <= 65535 or not 0 <= goods <= 255 or not 0 <= tag <= 65535):
            raise ValueError('facts')
        reg[tid] = Type(tw, th, kind, flags)
        facts[tid] = goods, tag
    if any(v and v not in reg for v in objects):
        raise ValueError('initial type')
    slots = numbers(p['slots'])
    if len(slots) != 16:
        raise ValueError('slots')
    maps = {key: numbers(p[key]) for key in ('map104', 'map201', 'map404')}
    if any(not 0 <= v < 0xffffffff for row in [slots, *maps.values()] for v in row):
        raise ValueError('row type id')
    if any(a and len(a) != 8 for a in maps.values()):
        raise ValueError('maps')
    draws = numbers(p['draws'])
    if any(v < 0 or v > 0xffffffff for v in draws):
        raise ValueError('draw')
    phase, item_height, status, consumed = [0]*(w*h), [0]*(w*h), [], 0
    anchors = numbers(p['anchors']) if 'anchors' in p else [0]*(w*h)
    if len(anchors) != w*h or any(v not in (0, 1) for v in anchors):
        raise ValueError('anchors')
    vertex = lambda x, y: heights[y*w+x] if 0 <= x < w and 0 <= y < h else 0
    island = Island(0, 0, w, h, vertex, lambda x, y: objects[y*w+x], lambda x, y: anchors[y*w+x])
    for row in p['records'].split(';'):
        tid, x, y, rot, state = map(int, row.split(':'))
        if (tid not in reg or rot not in range(4) or not 0 <= state <= 65535
                or not 0 <= x <= 0x7fffffff or not 0 <= y <= 0x7fffffff):
            raise ValueError('record')
        t = reg[tid]
        mode2_overlay = t.flags in (58, 18) and t.group == 0x141
        if (not mode2_overlay and t.flags not in (9, 41)) or x < 0 or y < 0:
            raise ValueError('unsupported')
        if mode2_overlay and max(t.w, t.h) > 8:
            raise ValueError('original coverage/offset domain')
        verdict, reason, height = place(island, reg, tid, x, y, rot, 0)
        if verdict != 'accept':
            if verdict == 'unsupported':
                raise ValueError(reason)
            status.append('bounds' if reason == 'BOUNDS' else 'occupied' if reason == 'OCCUPIED' else 'coverage' if reason == 'COVERAGE' else 'terrain')
            continue
        fw, fh = (t.h, t.w) if rot & 1 else (t.w, t.h)
        # Original 0x1008213b / 0x10082289: first actual write per world block.
        written_blocks = set()
        for dy in range(fh):
            for dx in range(fw):
                i = (y+dy)*w+x+dx
                if mode2_overlay and objects[i] and reg[objects[i]].flags & 16 and anchors[i]:
                    continue
                block = ((x+dx)//8, (y+dy)//8)
                anchors[i] = int(block not in written_blocks)
                written_blocks.add(block)
                objects[i], phase[i], item_height[i] = tid, state & 63, height
        status.append('accepted')
        if 225 <= t.prodtype <= 256:
            if consumed == len(draws):
                raise ValueError('draws exhausted')
            draw = draws[consumed]
            consumed += 1
            candidate = tid
            goods, tag = facts[tid]
            if t.prodtype == 225 and 56 <= goods <= 75:
                candidate -= 1
            elif t.prodtype == 226:
                if tid+1 not in facts:
                    raise ValueError('following type')
                goods = facts[tid+1][0]
            key = {104:'map104', 105:'map104', 201:'map201', 202:'map201', 404:'map404', 405:'map404'}.get(tag)
            if goods == 70 and key:
                if not maps[key]:
                    raise ValueError('missing map')
                candidate = maps[key][draw & 7]
            variant = slots.index(candidate) if candidate in slots else 0
            i = y*w+x
            plane[i] = (plane[i] & 15) | (variant << 4)
    join = lambda a: ','.join(map(str, a))
    result = dict(objects=join(objects), phase=join(phase), height=join(item_height), plane=join(plane), status=','.join(status), consumed=str(consumed))
    if 'anchors' in p:
        result['anchor'] = join(anchors)
    return result


def main():
    try:
        if len(sys.argv) != 2:
            raise ValueError('usage: resource_import_ref.py VECTORS')
        count, failed = 0, 0
        for line in Path(sys.argv[1]).read_text().splitlines():
            line = line.split('#', 1)[0].strip()
            if not line:
                continue
            left, right = line.split('=>')
            actual, expected = evaluate(fields(left)), fields(right)
            if set(actual) != set(expected):
                raise ValueError('expected keys')
            for key in ('objects', 'phase', 'height', 'plane') + (('anchor',) if 'anchor' in expected else ()):
                if len(numbers(expected[key])) != int(fields(left)['w'])*int(fields(left)['h']):
                    raise ValueError('expected geometry')
            if 'anchor' in expected and any(v not in (0, 1) for v in numbers(expected['anchor'])):
                raise ValueError('expected anchor bit')
            if int(expected['consumed']) < 0 or any(s not in {'accepted', 'bounds', 'occupied', 'terrain', 'coverage'} for s in expected['status'].split(',')):
                raise ValueError('expected disposition/count')
            count += 1
            if actual != expected:
                failed += 1
                print(f'MISMATCH {count}: {actual}, expected {expected}')
        if not count:
            raise ValueError('zero vectors')
        print(f'resource import: {count} vectors, {failed} disagreements')
        return int(bool(failed))
    except (ValueError, OSError, KeyError, OverflowError) as error:
        print(f'malformed: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
