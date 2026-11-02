"""Opening-opportunity audit for the three ship-start proposals.

This is not the version-2 pass/fail checker. It reads that checker's
geometric result and adds evidence levels the checker does not decide.
Unknown stays unknown. A large geometric component is not a sustainable
opening, and a help page is not a founding predicate.

  map2_opening.py audit <manifest> --game-dir <dir> [--json <path>]
"""
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import map2_ship as ship
import map2_shape as shape

HELP_FIRST = frozenset({'nord', 'dschungel'})
HELP_LATER = frozenset({'praerie', 'steppe', 'tundra'})
HELP_WORKSHOPS = frozenset({'polar'})
RECORD_NAMES = ('FRUCHTBAR', 'BERGWERK2', 'OCCUR', 'ANBAU')
_RECORD_CACHE = {}


def guidance(climate):
    """Help text for a first settlement. Not a click predicate."""
    if climate in HELP_FIRST:
        return 'recommended_first'
    if climate in HELP_LATER:
        return 'after_another_settlement'
    if climate in HELP_WORKSHOPS:
        return 'workshops_only'
    if climate == 'wueste':
        return 'no_help_page'
    return 'unknown'


def classify(island):
    """One island. `records` is True, False, or None when the file was unread.

    Geometric eligibility is the caller's boolean. Founding-click legality is
    never inferred from it.
    """
    geo = bool(island['geometric'])
    help_ok = geo and guidance(island['climate']) == 'recommended_first'
    records = island['records']
    if records is None:
        record_state = None
    else:
        record_state = bool(help_ok and records)
    return {
        'geometric_v2': geo,
        'help_first': help_ok,
        'records_present': record_state,
        'founding_click': None,
    }


def distinct(seat_islands, facts, level):
    """Matching at one evidence level.

    Returns (pairs or None, status). Status is 'unknown' when the level is
    not decidable, 'count' when every considered island was decided.
    Islands with a None predicate are left out of the matching and the
    status becomes 'partial' if any such island was reachable.
    """
    if level == 'founding_click':
        return None, 'unknown'
    options = {seat: [] for seat in range(5)}
    partial = False
    for seat in range(5):
        for island in seat_islands.get(seat, ()):
            state = classify(facts[island])[level]
            if state is None:
                partial = True
            elif state:
                options[seat].append(island)
    pairs = ship.matching(options)
    return pairs, ('partial' if partial else 'count')


def template_facts(path):
    """Record presence. Fertility bytes are counted, not interpreted."""
    import insel
    key = str(path)
    cached = _RECORD_CACHE.get(key)
    if cached is not None:
        return cached
    isl = insel.islands(Path(path).read_bytes())[0]
    lengths = {}
    fertility = None
    scenery_items = 0
    for name, payload in isl['records']:
        lengths[name] = lengths.get(name, 0) + len(payload)
        if name == 'FRUCHTBAR':
            fertility = payload
        elif name == 'GELAENDE' and len(payload) >= 6:
            scenery_items += (len(payload) - 6) // 6
    info = next((payload for name, payload in isl['records'] if name == 'INFO'), b'')
    histogram = None
    if fertility is not None:
        histogram = [0] * 11
        for byte in fertility:
            if byte < 11:
                histogram[byte] += 1
            else:
                histogram = None
                break
    present = all(lengths.get(name, 0) > 0 for name in RECORD_NAMES)
    facts = {
        'info0': info[0] if info else None,
        'records_present': present,
        'record_lengths': {name: lengths.get(name, 0) for name in RECORD_NAMES},
        'fertility_histogram': histogram,
        'fertility_meaning': 'uninterpreted',
        'scenery_items': scenery_items,
        'scenery_removal': 'unknown',
    }
    _RECORD_CACHE[key] = facts
    return facts


def _paths(game, keys):
    root = Path(game) / 'Scenes' / 'Islands'
    found = {}
    for path in root.rglob('*.scp'):
        key = path.relative_to(root).as_posix().upper().replace('\\', '/')
        if key in keys:
            found[key] = path
    return found


def audit(doc, report, templates, game):
    """Annotate one already-checked manifest. Does not change the checker."""
    by_id = {}
    keys = {island['key'] for island in doc['islands']}
    paths = _paths(game, keys)
    for island in doc['islands']:
        template = templates[island['key']]
        folder = island['key'].split('/', 1)[0].lower()
        climate = shape.CLIMATE_NAME[template['climate']] if template['climate'] is not None and template['climate'] < len(shape.CLIMATE_NAME) else 'unknown'
        path = paths.get(island['key'])
        records = template_facts(path) if path is not None else None
        info_matches = records is not None and records['info0'] == template['climate']
        by_id[island['id']] = {
            'id': island['id'],
            'key': island['key'],
            'folder': folder,
            'climate': climate,
            'guidance': guidance(climate),
            'info_matches_catalog': info_matches if records is not None else None,
            'source_sha256': template.get('source_sha256'),
            'geometric': False,
            'records': None if records is None else records['records_present'],
            'record_lengths': None if records is None else records['record_lengths'],
            'fertility_histogram': None if records is None else records['fertility_histogram'],
            'fertility_meaning': 'uninterpreted' if records is not None else 'missing',
            'scenery_items': None if records is None else records['scenery_items'],
            'scenery_removal': 'unknown',
            'founding_click': None,
        }
    seat_islands = {seat: [] for seat in range(5)}
    for seat, items in report['feasible'].items():
        for item in items:
            seat_islands[seat].append(item['island'])
            by_id[item['island']]['geometric'] = True
    levels = {}
    for level in ('geometric_v2', 'help_first', 'records_present', 'founding_click'):
        pairs, status = distinct(seat_islands, by_id, level)
        levels[level] = {
            'status': status,
            'distinct': None if pairs is None else len(pairs),
            'pairs': None if pairs is None else [list(pair) for pair in pairs],
        }
    return {
        'name': doc['name'],
        'content_hash': doc['content_hash'],
        'geometric_problems': list(report['problems']),
        'geometric_witness': [list(pair) for pair in report['witness']],
        'levels': levels,
        'islands': [by_id[island['id']] for island in doc['islands']],
        'alternatives': [
            {
                'seat': item['seat'],
                'island': item['island'],
                'cost': item['cost'],
                'site': list(item['site']),
                'water': list(item['water']),
                'city': item['city'],
                'climate': by_id[item['island']]['climate'],
                'guidance': by_id[item['island']]['guidance'],
                'records_present': by_id[item['island']]['records'],
                'founding_click': None,
            }
            for seat in range(5)
            for item in report['feasible'].get(seat, ())
        ],
        'engine': 'NOT_RUN_ENGINE',
        'original': 'NOT_RUN_ORIGINAL',
    }


def main(argv):
    try:
        os.nice(10)
    except OSError:
        pass
    args = list(argv[1:])
    json_path = None
    if '--json' in args:
        index = args.index('--json')
        json_path = args[index + 1]
        del args[index:index + 2]
    if len(args) != 4 or args[0] != 'audit' or args[2] != '--game-dir':
        print('usage: map2_opening.py audit <manifest> --game-dir <dir> [--json <path>]', file=sys.stderr)
        return 2
    doc, report, templates = ship.load_and_check(args[1], args[3])
    payload = audit(doc, report, templates, args[3])
    text = json.dumps(payload, sort_keys=True, indent=2)
    if json_path:
        Path(json_path).write_text(text + '\n', encoding='utf-8')
    levels = payload['levels']
    print(payload['name'], payload['content_hash'][:16])
    for name, item in levels.items():
        print(f"  {name}: {item['status']} distinct={item['distinct']}")
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
