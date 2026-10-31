"""Tool-only ship-start manifests. The runtime fixed-map loader does not read these.

A `start` record in `anno1503-fixed-map 1` remains a Kontor footprint. This
file is a separate JSON proposal. It does not place cities, reserve islands,
or inherit the base scenario's AI, players, or units as the five human seats.

Coordinates are integer milli-tiles: 1 tile = 1000 milli-tiles. A point on
the centre of tile (t, u) is (1000*t + 500, 1000*u + 500). Facing is an
integer 0..3 stored with the spawn. It is not a warehouse door.

Canonical bytes are UTF-8 JSON with object keys sorted, arrays in the order
given, and separators ',' and ':'. Islands must already be ordered by id and
spawns by seat. The content hash is SHA-256 of those bytes with the
`content_hash` field removed. Floats, NaN, and Infinity are rejected.

  python3 -I tools/map2_manifest.py check <file>
"""
import hashlib
import json
import os
import sys

MANIFEST_NAME = 'map2-ship-manifest'
MANIFEST_VERSION = 1
PROFILE_ID = 'map2-ship-start-v1'
MILLI = 1000
GOODS = ('wood', 'food', 'tools', 'bricks', 'spices')
HOLD_TONS_MAX = 50
HOLD_COUNT_MAX = 4
FORBIDDEN = ('cities', 'reservations', 'homes', 'partners', 'starts', 'owner_runs')

# Proposed comparison baseline from the human party of 03_1024_med_256.
# Evidence: docs/research/endless-start-conditions.md. Not an economy approval.
BASELINE_LOADOUT = {
    'status': 'proposed-baseline',
    'source': '03_1024_med_256 human party',
    'gold': 30000,
    'ship_figure': 1502,
    'scout_figure': 717,
    'donkey_figure': 718,
    'holds': (
        {'goods': 'wood', 'tons': 50},
        {'goods': 'food', 'tons': 10},
        {'goods': 'tools', 'tons': 50},
        {'goods': 'tools', 'tons': 30},
    ),
}


class ManifestError(Exception):
    pass


def _reject_constants(value):
    raise ManifestError(f'nonfinite number {value}')


def loads(text):
    try:
        return json.loads(text, parse_constant=_reject_constants)
    except ManifestError:
        raise
    except json.JSONDecodeError as err:
        raise ManifestError(f'malformed json: {err.msg}') from err


def _int(value, label):
    if isinstance(value, bool) or not isinstance(value, int):
        raise ManifestError(f'{label} is not an integer')
    return value


def _str(value, label):
    if not isinstance(value, str) or not value:
        raise ManifestError(f'{label} is not a string')
    return value


def canonical_bytes(doc):
    """doc has no content_hash. Arrays keep their order; object keys are sorted."""
    body = json.dumps(doc, sort_keys=True, separators=(',', ':'), ensure_ascii=True)
    return body.encode('ascii')


def content_hash(doc):
    return hashlib.sha256(canonical_bytes(doc)).hexdigest()


def _loadout(raw):
    if not isinstance(raw, dict):
        raise ManifestError('loadout is not an object')
    status = _str(raw.get('status'), 'loadout status')
    source = _str(raw.get('source'), 'loadout source')
    gold = _int(raw.get('gold'), 'gold')
    if gold < 0:
        raise ManifestError('gold is negative')
    ship = _int(raw.get('ship_figure'), 'ship_figure')
    scout = _int(raw.get('scout_figure'), 'scout_figure')
    donkey = _int(raw.get('donkey_figure'), 'donkey_figure')
    holds = raw.get('holds')
    if not isinstance(holds, list) or not holds or len(holds) > HOLD_COUNT_MAX:
        raise ManifestError('holds must be 1..4 entries')
    parsed = []
    for index, hold in enumerate(holds):
        if not isinstance(hold, dict):
            raise ManifestError(f'hold {index} is not an object')
        goods = _str(hold.get('goods'), f'hold {index} goods')
        if goods not in GOODS:
            raise ManifestError(f'hold {index} goods {goods} are unknown')
        tons = _int(hold.get('tons'), f'hold {index} tons')
        if tons < 0 or tons > HOLD_TONS_MAX:
            raise ManifestError(f'hold {index} tons {tons} outside 0..{HOLD_TONS_MAX}')
        parsed.append({'goods': goods, 'tons': tons})
    return {
        'status': status,
        'source': source,
        'gold': gold,
        'ship_figure': ship,
        'scout_figure': scout,
        'donkey_figure': donkey,
        'holds': tuple(parsed),
    }


def parse(text):
    raw = loads(text)
    if not isinstance(raw, dict):
        raise ManifestError('manifest is not an object')
    for key in FORBIDDEN:
        if key in raw:
            raise ManifestError(f'{key} is not allowed; this manifest has no cities or reservations')
    if raw.get('manifest') != MANIFEST_NAME:
        raise ManifestError('manifest name is unsupported')
    if _int(raw.get('version'), 'version') != MANIFEST_VERSION:
        raise ManifestError('manifest version is unsupported')
    if raw.get('profile') != PROFILE_ID:
        raise ManifestError('profile is unsupported')
    name = _str(raw.get('name'), 'name')
    edge = raw.get('edge')
    if not isinstance(edge, list) or len(edge) != 2:
        raise ManifestError('edge must be [width, height]')
    width, height = _int(edge[0], 'edge width'), _int(edge[1], 'edge height')
    if width < 1 or height < 1 or width > 16384 or height > 16384:
        raise ManifestError('edge is out of range')
    base = _str(raw.get('base'), 'base')
    seed = _int(raw.get('seed'), 'seed')
    if seed < 0 or seed > 0xFFFFFFFF:
        raise ManifestError('seed is out of range')
    generator = raw.get('generator')
    if not isinstance(generator, dict):
        raise ManifestError('generator is not an object')
    tool = _str(generator.get('tool'), 'generator tool')
    config = _str(generator.get('config'), 'generator config')
    islands = raw.get('islands')
    spawns = raw.get('spawns')
    if not isinstance(islands, list) or not islands:
        raise ManifestError('islands are missing')
    if not isinstance(spawns, list) or len(spawns) != 5:
        raise ManifestError('spawns must be five seats')
    parsed_islands = []
    seen_ids = set()
    last_id = -1
    for index, island in enumerate(islands):
        if not isinstance(island, dict):
            raise ManifestError(f'island {index} is not an object')
        ident = _int(island.get('id'), f'island {index} id')
        if ident in seen_ids:
            raise ManifestError(f'duplicate island id {ident}')
        if ident < last_id:
            raise ManifestError('islands are not ordered by id')
        last_id = ident
        seen_ids.add(ident)
        x = _int(island.get('x'), f'island {ident} x')
        y = _int(island.get('y'), f'island {ident} y')
        key = _str(island.get('key'), f'island {ident} key')
        parsed_islands.append({'id': ident, 'x': x, 'y': y, 'key': key})
    parsed_spawns = []
    seen_seats = set()
    seen_points = set()
    last_seat = -1
    for index, spawn in enumerate(spawns):
        if not isinstance(spawn, dict):
            raise ManifestError(f'spawn {index} is not an object')
        seat = _int(spawn.get('seat'), f'spawn {index} seat')
        if seat < 0 or seat > 4:
            raise ManifestError(f'spawn seat {seat} is outside 0..4')
        if seat in seen_seats:
            raise ManifestError(f'duplicate spawn seat {seat}')
        if seat < last_seat:
            raise ManifestError('spawns are not ordered by seat')
        last_seat = seat
        seen_seats.add(seat)
        x = _int(spawn.get('x'), f'spawn {seat} x')
        y = _int(spawn.get('y'), f'spawn {seat} y')
        facing = _int(spawn.get('facing'), f'spawn {seat} facing')
        if facing < 0 or facing > 3:
            raise ManifestError(f'spawn {seat} facing is outside 0..3')
        if x < 0 or y < 0 or x >= width * MILLI or y >= height * MILLI:
            raise ManifestError(f'spawn {seat} is outside the world')
        if (x, y) in seen_points:
            raise ManifestError(f'duplicate spawn position {x},{y}')
        seen_points.add((x, y))
        parsed_spawns.append({'seat': seat, 'x': x, 'y': y, 'facing': facing})
    if seen_seats != set(range(5)):
        raise ManifestError('spawn seats must be 0..4')
    loadout = _loadout(raw.get('loadout'))
    stated = raw.get('content_hash')
    doc = {
        'manifest': MANIFEST_NAME,
        'version': MANIFEST_VERSION,
        'profile': PROFILE_ID,
        'name': name,
        'edge': [width, height],
        'base': base,
        'seed': seed,
        'generator': {'tool': tool, 'config': config},
        'islands': parsed_islands,
        'spawns': parsed_spawns,
        'loadout': {
            'status': loadout['status'],
            'source': loadout['source'],
            'gold': loadout['gold'],
            'ship_figure': loadout['ship_figure'],
            'scout_figure': loadout['scout_figure'],
            'donkey_figure': loadout['donkey_figure'],
            'holds': [dict(item) for item in loadout['holds']],
        },
        'excluded': [
            'base-scenario players',
            'base-scenario units',
            'player cities',
            'island reservations',
        ],
    }
    digest = content_hash(doc)
    if stated is not None and stated != digest:
        raise ManifestError('content_hash does not match the canonical manifest')
    doc['content_hash'] = digest
    doc['_loadout'] = loadout
    return doc


def baseline_ok(doc):
    got = doc['_loadout']
    return got == {
        'status': BASELINE_LOADOUT['status'],
        'source': BASELINE_LOADOUT['source'],
        'gold': BASELINE_LOADOUT['gold'],
        'ship_figure': BASELINE_LOADOUT['ship_figure'],
        'scout_figure': BASELINE_LOADOUT['scout_figure'],
        'donkey_figure': BASELINE_LOADOUT['donkey_figure'],
        'holds': BASELINE_LOADOUT['holds'],
    }


def emit(doc):
    public = {key: value for key, value in doc.items() if not key.startswith('_')}
    public['content_hash'] = content_hash({key: value for key, value in public.items() if key != 'content_hash'})
    return json.dumps(public, sort_keys=True, separators=(',', ':'), ensure_ascii=True).encode('ascii') + b'\n'


def main(argv):
    if len(argv) != 3 or argv[1] != 'check':
        print('usage: map2_manifest.py check <file>', file=sys.stderr)
        return 2
    try:
        doc = parse(PathText(argv[2]))
    except ManifestError as err:
        print(f'error: {err}')
        return 1
    print(f"ok {doc['name']} {doc['content_hash']}")
    return 0


def PathText(path):
    with open(path, 'rb') as handle:
        data = handle.read()
    if any(byte > 127 for byte in data):
        raise ManifestError('manifest is not ASCII')
    return data.decode('ascii')


if __name__ == '__main__':
    try:
        os.nice(10)
    except OSError:
        pass
    sys.exit(main(sys.argv))
