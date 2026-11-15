#!/usr/bin/env python3
"""SO-1a reference from docs/re/script-objects.md; no engine or world generator.

script_objects_ref.py VECTORS: exit 0 agree, 1 disagree, 2 malformed/missing/empty.
query LABEL objects=name:n,... ref=name islands=i,... city_slots=N
initial=i:c:owner:buildings,... frames=.../... => all expected output keys.
'-' denotes an empty list/table. Inputs are snapshots, not destruction actions.
ASCII comparison models AnnoScene's default branch only. Unsafe dereferences
(null island / city outside the supplied readable domain) have no Boolean model.
"""
import re
import sys
from pathlib import Path


class Malformed(ValueError):
    pass


def integer(text, low=0, high=0xffffffff):
    if not re.fullmatch(r'-?[0-9]+', text):
        raise Malformed('expected decimal integer: ' + text)
    try:
        value = int(text)
    except ValueError as error:
        raise Malformed('integer representation too long') from error
    if not low <= value <= high:
        raise Malformed('integer out of range: ' + text)
    return value


def name(text):
    if not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_@]*', text):
        raise Malformed('reference requires portable ASCII names')
    return text


def fields(text):
    result = {}
    for token in text.split():
        if token.count('=') != 1:
            raise Malformed('expected key=value')
        key, value = token.split('=')
        if not key or not value or key in result:
            raise Malformed('empty or duplicate field')
        result[key] = value
    return result


def cities(text, islands, slots):
    result = {}
    for row in ([] if text == '-' else text.split(',')):
        parts = row.split(':')
        if len(parts) != 4:
            raise Malformed('city needs island:slot:owner:building_count')
        island, city, owner, buildings = map(integer, parts)
        if island not in islands or city >= slots or (island, city) in result:
            raise Malformed('invalid or duplicate city snapshot entry')
        result[island, city] = (owner, buildings)
    return result


def evaluate(given):
    required = {'objects', 'ref', 'islands', 'city_slots', 'initial', 'frames'}
    if set(given) != required:
        raise Malformed('input keys differ: missing=' + ','.join(sorted(required - set(given)))
                        + ' unknown=' + ','.join(sorted(set(given) - required)))
    objects = []
    for row in ([] if given['objects'] == '-' else given['objects'].split(',')):
        parts = row.split(':')
        if len(parts) != 2:
            raise Malformed('object needs name:parsed_signed32_operand')
        objects.append((name(parts[0]), integer(parts[1], -0x80000000, 0x7fffffff)))
    ref = name(given['ref'])
    islands = [] if given['islands'] == '-' else [integer(v) for v in given['islands'].split(',')]
    if len(set(islands)) != len(islands):
        raise Malformed('duplicate island')
    slots = integer(given['city_slots'], 1)
    initial = cities(given['initial'], islands, slots)
    frames = [cities(v, islands, slots) for v in given['frames'].split('/')]
    definition = next((n for symbol, n in objects if symbol.lower() == ref.lower()), None)
    if definition is None:
        return dict(compiled='0', island='-', city='-', killed='-',
                    parse_reads='0', city_reads='0', writes='0')
    packed = definition & 0xffff
    island, city = packed & 0xff, (packed >> 8) & 0xff
    if island not in islands:
        raise Malformed('null island: original city query has no guarded Boolean result')
    if city >= slots:
        raise Malformed('city index outside the supplied readable city-slot domain')
    target = (island, city)
    killed = []
    for frame in frames:
        present = target in frame
        killed.append('0' if present else '1')
    # Descriptor construction and all city-query leaves are read-only with respect to the world.
    return dict(compiled='1', island=str(island), city=str(city), killed=','.join(killed),
                parse_reads='0', city_reads=str(len(frames)), writes='0')


def run(path):
    count = bad = 0
    labels = set()
    for line_number, raw in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        line = raw.split('#', 1)[0].strip()
        if not line:
            continue
        if line.count('=>') != 1:
            raise Malformed(f'line {line_number}: expected one =>')
        left, right = line.split('=>')
        words = left.split(maxsplit=2)
        if len(words) != 3 or words[0] != 'query' or words[1] in labels:
            raise Malformed(f'line {line_number}: invalid/duplicate query label')
        labels.add(words[1])
        actual, expected = evaluate(fields(words[2])), fields(right)
        if set(actual) != set(expected):
            raise Malformed(f'line {line_number}: output keys differ')
        integer(expected['compiled'], 0, 1)
        for key in ('parse_reads', 'city_reads', 'writes'):
            integer(expected[key])
        if expected['compiled'] == '0':
            if any(expected[key] != '-' for key in ('island', 'city', 'killed')):
                raise Malformed('failed compilation requires placeholder outputs')
        else:
            integer(expected['island'], 0, 255)
            integer(expected['city'], 0, 255)
            if not re.fullmatch(r'[01](?:,[01])*', expected['killed']):
                raise Malformed('expected killed must be a Boolean sequence')
        count += 1
        for key in actual:
            if actual[key] != expected[key]:
                print(f'{words[1]} {key}: expected {expected[key]}, got {actual[key]}')
                bad += 1
    if count == 0:
        raise Malformed('zero vectors')
    print(f'{count} vectors, {bad} disagreements')
    return int(bad != 0)


def main(argv):
    try:
        if len(argv) != 1:
            raise Malformed('usage: script_objects_ref.py VECTORS')
        return run(Path(argv[0]))
    except (Malformed, OSError, UnicodeError) as error:
        print('ERROR: ' + str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
