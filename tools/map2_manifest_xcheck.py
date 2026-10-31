"""Cross-check data for the C++ manifest reader (src/formats/src/ship_manifest.cpp).

The Python reference (tools/map2_manifest.py) is the compatibility oracle. This
tool owns a list of small synthetic manifests, writes each as a file under
fixtures/synthetic/map2_manifest/cases/, runs the reference over it and records
the verdict in fixtures/synthetic/map2_manifest/cases.txt:

    <name> | ref=ok:<sha256> or ref=reject | engine=ok:<sha256> or engine=reject | baseline=yes/no

`ref` is what the Python reference says. `engine` is what the C++ reader must say.
They differ only for the cases in ENGINE_STRICTER, where the C++ reader is
deliberately stricter (src/formats/include/anno/formats/ship_manifest.hpp).
`baseline` is whether the loadout is the one the first loader supports; it is
`no` for a rejected manifest.

  python3 -I tools/map2_manifest_xcheck.py generate   # rewrite cases/ and cases.txt
  python3 -I tools/map2_manifest_xcheck.py verify     # reference still agrees with cases.txt

The C++ test (formats_tests, "ship manifest: cross-check") reads the same files.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import map2_manifest as ref  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'fixtures', 'synthetic',
                    'map2_manifest')
CASES = os.path.join(ROOT, 'cases')
LIST = os.path.join(ROOT, 'cases.txt')

# Cases the reference accepts and the C++ reader rejects, with the reason.
ENGINE_STRICTER = {
    'island_id_minus_one': 'the reference starts its ordering check at -1',
    'integer_beyond_int64': 'the C++ reader holds int64; Python integers are unbounded',
    'lone_high_surrogate': 'Python keeps a lone surrogate in the string',
    'lone_low_surrogate': 'Python keeps a lone surrogate in the string',
}


def base_doc():
    doc = {
        'manifest': ref.MANIFEST_NAME,
        'version': 2,
        'profile': ref.PROFILE_ID,
        'name': 'synthetic',
        'edge': [128, 128],
        'base': 'Scenes/base.sze',
        'seed': 7,
        'generator': {'tool': 'xcheck', 'config': 'synthetic'},
        'islands': [
            {'id': 0, 'key': 'NORD/A.SCP', 'x': 8, 'y': 8},
            {'id': 100, 'key': 'POLAR/B.SCP', 'x': 64, 'y': 8},
        ],
        'spawns': [
            {'seat': s, 'x': 1000 * (10 + 20 * s) + 500, 'y': 1000 * 100 + 500, 'facing': s % 4}
            for s in range(5)
        ],
        'loadout': {
            'status': 'proposed-baseline',
            'source': '03_1024_med_256 human party',
            'gold': 30000,
            'ship_figure': 1502,
            'scout_figure': 717,
            'donkey_figure': 718,
            'holds': [{'goods': 'wood', 'tons': 50}, {'goods': 'food', 'tons': 10},
                      {'goods': 'tools', 'tons': 50}, {'goods': 'tools', 'tons': 30}],
        },
        'excluded': list(ref.EXCLUDED),
        'policy': ref.default_policy(),
    }
    return doc


def hashed(doc):
    out = dict(doc)
    out.pop('content_hash', None)
    out['content_hash'] = ref.content_hash(out)
    return out


def dump(doc):
    return json.dumps(doc, sort_keys=True, separators=(',', ':'), ensure_ascii=True)


def cases():
    """name -> bytes"""
    out = {}

    def add(name, value):
        out[name] = value if isinstance(value, bytes) else value.encode('ascii')

    def mutated(fn, with_hash=True):
        doc = base_doc()
        fn(doc)
        return dump(hashed(doc) if with_hash else doc)

    add('valid', dump(hashed(base_doc())))
    add('valid_no_hash', dump(base_doc()))
    add('valid_null_hash', dump(dict(base_doc(), content_hash=None)))
    doc = hashed(base_doc())
    keys = list(doc)[::-1]
    add('reordered_keys_and_whitespace',
        '{\n  ' + ',\n  '.join(json.dumps(k) + ' : ' + json.dumps(doc[k]) for k in keys) + '\n}\n')
    # Escapes: the file spells the name with \u escapes; the hash is over the text.
    named = base_doc()
    named['name'] = 'café \U0001F600 q"\\/\n\x7f'
    named_hash = ref.content_hash(named)
    raw = dump(dict(named, content_hash=named_hash))  # ensure_ascii escapes it
    add('escaped_unicode_name', raw)
    add('escaped_slash_spelling',
        dump(hashed(base_doc())).replace('"Scenes/base.sze"', '"Scenes\\/base.sze"'))
    add('negative_zero_seed', dump(base_doc()).replace('"seed":7', '"seed":-0'))
    add('alternative_loadout', mutated(lambda d: d['loadout'].update(gold=25000)))
    add('alternative_holds', mutated(lambda d: d['loadout'].update(holds=[{'goods': 'wood', 'tons': 1}])))
    add('uppercase_hash', dump(dict(base_doc(), content_hash=ref.content_hash(base_doc()).upper())))
    add('wrong_hash', dump(dict(base_doc(), content_hash='0' * 64)))
    add('hash_not_a_string', dump(dict(base_doc(), content_hash=5)))
    add('modified_content_stale_hash',
        dump(hashed(base_doc())).replace('"seed":7', '"seed":8'))

    s = dump(hashed(base_doc()))
    add('duplicate_top_key', s[:-1] + ',"name":"again"}')
    add('duplicate_nested_key', s.replace('"tool":"xcheck"', '"tool":"xcheck","tool":"x"'))
    add('duplicate_key_in_hold', s.replace('{"goods":"wood","tons":50}',
                                           '{"goods":"wood","tons":50,"tons":50}'))
    add('duplicate_key_in_spawn', s.replace('"seat":0', '"seat":0,"seat":0'))
    add('bool_as_integer_seed', dump(base_doc()).replace('"seed":7', '"seed":true'))
    add('bool_as_integer_gold', dump(base_doc()).replace('"gold":30000', '"gold":false'))
    add('bool_as_integer_edge', dump(base_doc()).replace('"edge":[128,128]', '"edge":[true,128]'))
    add('float_seed', dump(base_doc()).replace('"seed":7', '"seed":7.0'))
    add('exponent_seed', dump(base_doc()).replace('"seed":7', '"seed":7e0'))
    add('leading_zero_seed', dump(base_doc()).replace('"seed":7', '"seed":07'))
    add('plus_sign_seed', dump(base_doc()).replace('"seed":7', '"seed":+7'))
    add('nan_seed', dump(base_doc()).replace('"seed":7', '"seed":NaN'))
    add('infinity_seed', dump(base_doc()).replace('"seed":7', '"seed":Infinity'))
    add('negative_seed', mutated(lambda d: d.update(seed=-1)))
    add('seed_too_large', mutated(lambda d: d.update(seed=2 ** 32)))
    add('seed_max', mutated(lambda d: d.update(seed=2 ** 32 - 1)))
    add('string_seed', mutated(lambda d: d.update(seed='7')))
    add('unknown_top_field', mutated(lambda d: d.update(note='x')))
    add('unknown_nested_field_in_hold',
        mutated(lambda d: d['loadout']['holds'][0].update(note='x')))
    add('unknown_nested_field_in_policy', mutated(lambda d: d['policy'].update(note=1)))
    add('forbidden_cities', mutated(lambda d: d.update(cities=[])))
    add('missing_seed', mutated(lambda d: d.pop('seed')))
    add('missing_policy', mutated(lambda d: d.pop('policy')))
    add('missing_hold_tons', mutated(lambda d: d['loadout']['holds'][0].pop('tons')))
    add('version_3', mutated(lambda d: d.update(version=3)))
    add('version_as_float', dump(base_doc()).replace('"version":2', '"version":2.0'))
    add('profile_v1', mutated(lambda d: d.update(profile=ref.V1_PROFILE_ID)))
    add('wrong_manifest_name', mutated(lambda d: d.update(manifest='other')))
    add('four_seats', mutated(lambda d: d['spawns'].pop()))
    add('six_seats', mutated(lambda d: d['spawns'].append(dict(d['spawns'][0], seat=5))))
    add('seats_unordered', mutated(lambda d: d['spawns'].reverse()))
    add('duplicate_seat', mutated(lambda d: d['spawns'][4].update(seat=3)))
    add('seat_out_of_range', mutated(lambda d: d['spawns'][4].update(seat=5)))
    add('facing_4', mutated(lambda d: d['spawns'][0].update(facing=4)))
    add('facing_negative', mutated(lambda d: d['spawns'][0].update(facing=-1)))
    add('spawn_outside_world', mutated(lambda d: d['spawns'][0].update(x=128000)))
    add('spawn_last_milli_inside', mutated(lambda d: d['spawns'][0].update(x=127999)))
    add('spawn_negative', mutated(lambda d: d['spawns'][0].update(y=-1)))
    add('duplicate_spawn_position',
        mutated(lambda d: d['spawns'][1].update(x=d['spawns'][0]['x'], y=d['spawns'][0]['y'])))
    add('edge_too_large', mutated(lambda d: d.update(edge=[16385, 128])))
    add('edge_max', mutated(lambda d: d.update(edge=[16384, 16384])))
    add('edge_wrong_length', mutated(lambda d: d.update(edge=[128])))
    add('islands_empty', mutated(lambda d: d.update(islands=[])))
    add('islands_duplicate_id', mutated(lambda d: d['islands'][1].update(id=0)))
    add('islands_unordered', mutated(lambda d: d['islands'].reverse()))
    add('island_id_minus_one', mutated(lambda d: d['islands'][0].update(id=-1)))
    add('island_empty_key', mutated(lambda d: d['islands'][0].update(key='')))
    add('excluded_reordered', mutated(lambda d: d['excluded'].reverse()))
    add('excluded_short', mutated(lambda d: d['excluded'].pop()))
    add('policy_metric', mutated(lambda d: d['policy'].update(metric='euclid')))
    add('policy_clearance_65', mutated(lambda d: d['policy'].update(clearance=65)))
    add('policy_clearance_64', mutated(lambda d: d['policy'].update(clearance=64)))
    add('policy_min_city_zero', mutated(lambda d: d['policy'].update(min_city=0)))
    add('holds_five', mutated(lambda d: d['loadout']['holds'].append({'goods': 'wood', 'tons': 1})))
    add('holds_empty', mutated(lambda d: d['loadout'].update(holds=[])))
    add('hold_tons_51', mutated(lambda d: d['loadout']['holds'][0].update(tons=51)))
    add('hold_tons_negative', mutated(lambda d: d['loadout']['holds'][0].update(tons=-1)))
    add('hold_unknown_goods', mutated(lambda d: d['loadout']['holds'][0].update(goods='gold')))
    add('gold_negative', mutated(lambda d: d['loadout'].update(gold=-1)))
    add('empty_name', mutated(lambda d: d.update(name='')))
    add('integer_beyond_int64',
        dump(base_doc()).replace('"gold":30000', '"gold":99999999999999999999'))
    add('lone_high_surrogate', dump(base_doc()).replace('"name":"synthetic"', '"name":"\\ud800"'))
    add('lone_low_surrogate', dump(base_doc()).replace('"name":"synthetic"', '"name":"\\udc00"'))
    add('trailing_comma', s[:-1] + ',}')
    add('trailing_garbage', s + ' x')
    add('empty_file', '')
    add('array_top_level', '[]')
    add('truncated', s[: len(s) // 2])
    add('bom', b'\xef\xbb\xbf' + s.encode('ascii'))
    add('non_ascii_byte', s.replace('synthetic', 'synthétic').encode('utf-8'))
    add('single_quotes', s.replace('"', "'"))
    return out


def reference_verdict(data):
    """('ok', hash, baseline) or ('reject', None, False), exactly as the CLI path does."""
    path = os.path.join(CASES, '_tmp.json')
    with open(path, 'wb') as handle:
        handle.write(data)
    try:
        doc = ref.parse(ref.PathText(path))
    except ref.ManifestError:
        return 'reject', None, False
    finally:
        os.remove(path)
    return 'ok', doc['content_hash'], ref.baseline_ok(doc)


def line_for(name, data):
    verdict, digest, baseline = reference_verdict(data)
    ref_col = f'ok:{digest}' if verdict == 'ok' else 'reject'
    if name in ENGINE_STRICTER:
        assert verdict == 'ok', name
        eng_col, baseline_col = 'reject', 'no'
    else:
        eng_col, baseline_col = ref_col, 'yes' if baseline else 'no'
    return f'{name} | ref={ref_col} | engine={eng_col} | baseline={baseline_col}'


def generate():
    os.makedirs(CASES, exist_ok=True)
    for old in os.listdir(CASES):
        os.remove(os.path.join(CASES, old))
    lines = []
    for name, data in sorted(cases().items()):
        with open(os.path.join(CASES, name + '.json'), 'wb') as handle:
            handle.write(data)
        lines.append(line_for(name, data))
    with open(LIST, 'w', newline='\n') as handle:
        handle.write('# name | reference verdict | what the C++ reader must say | loadout is baseline\n')
        handle.write('# Written by tools/map2_manifest_xcheck.py generate; do not edit.\n')
        handle.write('\n'.join(lines) + '\n')
    print(f'wrote {len(lines)} cases')
    return 0


def verify():
    bad = 0
    recorded = {}
    with open(LIST) as handle:
        for line in handle:
            if line.startswith('#') or not line.strip():
                continue
            recorded[line.split(' | ')[0]] = line.rstrip('\n')
    files = sorted(f[:-5] for f in os.listdir(CASES) if f.endswith('.json'))
    if files != sorted(recorded):
        print('case files and cases.txt differ')
        bad += 1
    for name in files:
        with open(os.path.join(CASES, name + '.json'), 'rb') as handle:
            data = handle.read()
        if name not in recorded:
            continue
        want = line_for(name, data)
        if want != recorded[name]:
            print(f'MISMATCH {name}\n  recorded: {recorded[name]}\n  reference: {want}')
            bad += 1
    expected = cases()
    for name in files:
        with open(os.path.join(CASES, name + '.json'), 'rb') as handle:
            if handle.read() != expected.get(name):
                print(f'{name}: file differs from the generator')
                bad += 1
    print(f'verified {len(files)} cases, {bad} problems')
    return 1 if bad else 0


if __name__ == '__main__':
    if len(sys.argv) != 2 or sys.argv[1] not in ('generate', 'verify'):
        print(__doc__)
        sys.exit(2)
    sys.exit(generate() if sys.argv[1] == 'generate' else verify())
