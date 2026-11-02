"""Round-trip and rejection checks for ship-start manifests.

Expected error strings are written here, not copied from the parser's success path.
"""
import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import map2_manifest as mf

ROOT = Path(__file__).resolve().parent.parent
FAILURES = []


def expect(condition, message):
    print(('pass ' if condition else 'FAIL ') + message)
    if not condition:
        FAILURES.append(message)


def sample():
    return {
        'manifest': mf.MANIFEST_NAME,
        'version': mf.MANIFEST_VERSION,
        'profile': mf.PROFILE_ID,
        'name': 'synthetic-zero-cities',
        'edge': [32, 32],
        'base': 'Scenes/0010SingleEndlos/03_1024_med_256.sze',
        'seed': 1,
        'generator': {'tool': 'map2_manifest_selftest.py', 'config': 'synthetic'},
        'islands': [
            {'id': 0, 'x': 0, 'y': 0, 'key': 'NORD/N.SCP'},
        ],
        'spawns': [
            {'seat': seat, 'x': 20000 + seat * 1000, 'y': 20000, 'facing': seat % 4}
            for seat in range(5)
        ],
        'loadout': {
            'status': 'proposed-baseline',
            'source': '03_1024_med_256 human party',
            'gold': 30000,
            'ship_figure': 1502,
            'scout_figure': 717,
            'donkey_figure': 718,
            'holds': [
                {'goods': 'wood', 'tons': 50},
                {'goods': 'food', 'tons': 10},
                {'goods': 'tools', 'tons': 50},
                {'goods': 'tools', 'tons': 30},
            ],
        },
        'excluded': list(mf.EXCLUDED),
        'policy': mf.default_policy(),
    }


def test_round_trip():
    doc = sample()
    text = json.dumps(doc)
    parsed = mf.parse(text)
    again = mf.parse(mf.emit(parsed).decode('ascii'))
    expect(parsed['content_hash'] == again['content_hash'], 'a manifest round-trips to the same hash')
    expect(mf.baseline_ok(parsed), 'the sample carries the 03 human baseline')
    expect('cities' not in parsed, 'the parsed manifest has no cities field')
    blob = mf.emit(parsed)
    expect(blob.endswith(b'\n'), 'emitted bytes end with a newline')
    expect(mf.parse(blob.decode('ascii'))['content_hash'] == parsed['content_hash'],
           'parsing the emitted bytes keeps the hash')


def rejects(mutator, needle, message):
    doc = sample()
    mutator(doc)
    try:
        mf.parse(json.dumps(doc))
    except mf.ManifestError as err:
        expect(needle in str(err), message)
        return
    expect(False, message)


def test_rejections():
    rejects(lambda d: d.update(cities=[{'seat': 0}]), 'cities is not allowed', 'player cities are rejected')
    rejects(lambda d: d.update(reservations=[0]), 'reservations is not allowed', 'island reservations are rejected')
    rejects(lambda d: d.update(version=1), 'version is unsupported', 'version 1 is rejected')
    rejects(lambda d: d['spawns'].append(dict(d['spawns'][0])), 'spawns must be five', 'a sixth spawn is rejected')
    rejects(lambda d: d['spawns'].__setitem__(1, dict(d['spawns'][0])), 'duplicate spawn seat', 'a duplicate seat is rejected')
    rejects(lambda d: d['spawns'][0].update(x=1.5), 'not an integer', 'a fractional spawn coordinate is rejected')
    rejects(lambda d: d['loadout']['holds'][0].update(tons=51), 'outside 0..50', '51 tons in one hold is rejected')
    rejects(lambda d: d['loadout']['holds'][0].update(goods='gold'), 'unknown', 'unknown cargo is rejected')
    rejects(lambda d: d['loadout']['holds'].append({'goods': 'wood', 'tons': 1}), 'holds must be 1..4',
            'a fifth hold is rejected')
    rejects(lambda d: d.update(content_hash='0' * 64), 'content_hash does not match', 'a wrong hash is rejected')
    try:
        mf.loads('{"edge": NaN}')
        expect(False, 'NaN is rejected')
    except mf.ManifestError as err:
        expect('nonfinite' in str(err), 'NaN is rejected')


def test_lan5_dump():
    proc = subprocess.run(
        [sys.executable, '-I', str(ROOT / 'tools' / 'fixed_map.py'), 'dump',
         str(ROOT / 'data' / 'maps' / 'lan5.txt')],
        capture_output=True, text=True)
    expect(proc.returncode == 0, 'fixed_map dump of lan5 exits 0')
    expect('digest c6af83f95104cd4f' in proc.stdout, 'lan5 digest stays c6af83f95104cd4f')
    data = (ROOT / 'data' / 'maps' / 'lan5.txt').read_bytes()
    expect(len(data) == 1476, 'lan5.txt stays 1476 bytes')


def test_upgrade():
    old = sample()
    old['version'] = 1
    old['profile'] = mf.V1_PROFILE_ID
    del old['policy']
    text = json.dumps(old)
    try:
        mf.parse(text)
        expect(False, 'version 1 stays rejected by parse')
    except mf.ManifestError as err:
        expect('unsupported' in str(err), 'version 1 stays rejected by parse')
    doc = mf.upgrade_v1(text)
    expect(doc['version'] == 2, 'upgrade writes version 2')
    expect(doc['islands'] == [{'id': 0, 'x': 0, 'y': 0, 'key': 'NORD/N.SCP'}],
           'upgrade keeps the island placement')
    expect(doc['policy']['metric'] == 'manhattan-4', 'upgrade records the manhattan-4 policy')
    again = mf.parse(mf.emit(doc).decode('ascii'))
    expect(again['content_hash'] == doc['content_hash'], 'upgraded bytes round-trip')
    old['policy'] = mf.default_policy()
    try:
        mf.upgrade_v1(json.dumps(old))
        expect(False, 'a version 1 policy is refused')
    except mf.ManifestError as err:
        expect('policy' in str(err), 'a version 1 policy is refused')


def main():
    test_round_trip()
    test_rejections()
    test_upgrade()
    test_lan5_dump()
    print(f'{len(FAILURES)} failed')
    return 1 if FAILURES else 0


if __name__ == '__main__':
    sys.exit(main())
