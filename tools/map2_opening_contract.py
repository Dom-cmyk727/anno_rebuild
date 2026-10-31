"""Hand-specified expectations for the opening audit.

These rows are written here. They are not copied from a candidate report.
"""
import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import map2_opening as opening


def fail(name, detail):
    print(f'FAIL {name}: {detail}')
    return 1


def test_help_does_not_promote_polar():
    polar = {'climate': 'polar', 'geometric': True, 'records': True}
    got = opening.classify(polar)
    if got['geometric_v2'] is not True or got['help_first'] is not False:
        return fail('polar', got)
    if got['records_present'] is not False or got['founding_click'] is not None:
        return fail('polar levels', got)
    return 0


def test_large_area_is_not_a_founding():
    row = {'climate': 'nord', 'geometric': True, 'records': True}
    got = opening.classify(row)
    if got['founding_click'] is not None:
        return fail('founding', got)
    if got['help_first'] is not True:
        return fail('nord help', got)
    return 0


def test_missing_records_stay_unknown():
    row = {'climate': 'nord', 'geometric': True, 'records': None}
    got = opening.classify(row)
    if got['records_present'] is not None:
        return fail('missing', got)
    return 0


def test_disconnected_seat_is_not_filled():
    """A seat with no geometric island is not given one by climate or records."""
    facts = {
        0: {'climate': 'nord', 'geometric': True, 'records': True},
        1: {'climate': 'nord', 'geometric': False, 'records': True},
    }
    seats = {0: [0], 1: [], 2: [0], 3: [0], 4: [0]}
    # Seat 1 has no island. Matching cannot cover five seats.
    pairs, status = opening.distinct(seats, facts, 'geometric_v2')
    if status != 'count' or len(pairs) != 1:
        return fail('disconnected', (pairs, status))
    pairs, status = opening.distinct(seats, facts, 'help_first')
    if len(pairs) != 1:
        return fail('disconnected help', pairs)
    return 0


def test_five_recommended_islands():
    facts = {}
    seats = {}
    for seat in range(5):
        facts[seat] = {'climate': 'nord', 'geometric': True, 'records': True}
        seats[seat] = [seat]
    facts[5] = {'climate': 'polar', 'geometric': True, 'records': True}
    seats[0] = [0, 5]
    pairs, status = opening.distinct(seats, facts, 'geometric_v2')
    if status != 'count' or len(pairs) != 5:
        return fail('five geo', (pairs, status))
    pairs, status = opening.distinct(seats, facts, 'help_first')
    islands = {island for _, island in pairs}
    if 5 in islands or len(pairs) != 5:
        return fail('five help', pairs)
    pairs, status = opening.distinct(seats, facts, 'founding_click')
    if pairs is not None or status != 'unknown':
        return fail('five founding', (pairs, status))
    return 0


def test_unread_records_do_not_count_as_present():
    facts = {seat: {'climate': 'dschungel', 'geometric': True, 'records': True} for seat in range(5)}
    facts[4] = {'climate': 'dschungel', 'geometric': True, 'records': None}
    seats = {seat: [seat] for seat in range(5)}
    pairs, status = opening.distinct(seats, facts, 'records_present')
    islands = {island for _, island in pairs}
    if status != 'partial' or 4 in islands or len(pairs) != 4:
        return fail('unread', (pairs, status))
    return 0


def _chunk(name, payload):
    return name.encode('ascii').ljust(12, b'\0') + struct.pack('<I', len(payload)) + payload


def _file(extra):
    height = _chunk('HEIGHT', struct.pack('<HHBB', 0, 0, 2, 2) + bytes([20, 20, 20, 20]))
    info = _chunk('INFO', bytes([2]) + bytes(11))
    body = info + height + b''.join(extra)
    head = struct.pack('<4HI', 0, 0, 2, 2, 0)
    insel = _chunk('INSEL5', head + body)
    return _chunk('ANNO', insel)


def test_record_reader_hand_counts():
    import insel
    fertility = bytes([0, 1, 0, 10])
    raw = _file([
        _chunk('FRUCHTBAR', fertility),
        _chunk('BERGWERK2', b'\x01\x00'),
        _chunk('OCCUR', b'\x02\x00'),
        _chunk('ANBAU', b'\x03'),
        _chunk('GELAENDE', struct.pack('<HHBB', 0, 0, 2, 2) + b'\x00' * 6),
    ])
    # The reader must see one scenery item: 6 header bytes + one 6-byte item.
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / 'a.scp'
        path.write_bytes(raw)
        got = opening.template_facts(path)
    if got['info0'] != 2 or got['records_present'] is not True:
        return fail('records', got)
    if got['fertility_histogram'] != [2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1]:
        return fail('histogram', got['fertility_histogram'])
    if got['fertility_meaning'] != 'uninterpreted':
        return fail('meaning', got)
    if got['scenery_items'] != 1 or got['scenery_removal'] != 'unknown':
        return fail('scenery', got)
    missing = _file([])
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / 'b.scp'
        path.write_bytes(missing)
        got = opening.template_facts(path)
    if got['records_present'] is not False or got['fertility_histogram'] is not None:
        return fail('absent', got)
    # Confirm the synthetic file is a real island the shared reader accepts.
    insel.islands(raw)
    return 0


def test_isolated_interpreter_finds_the_script():
    """python3 -I does not put the script directory on sys.path.

    The parent test inserts that directory for its own imports. This
    subprocess does not, and it must still reach the usage check.
    """
    script = Path(__file__).resolve().parent / 'map2_opening.py'
    env = os.environ.copy()
    env.pop('PYTHONPATH', None)
    proc = subprocess.run(
        [sys.executable, '-I', '-u', str(script)],
        cwd=script.parent.parent,
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode != 2 or 'ModuleNotFoundError' in proc.stderr:
        return fail('isolated', (proc.returncode, proc.stderr))
    if 'usage: map2_opening.py' not in proc.stderr:
        return fail('isolated usage', proc.stderr)
    return 0


def main():
    tests = [
        test_isolated_interpreter_finds_the_script,
        test_help_does_not_promote_polar,
        test_large_area_is_not_a_founding,
        test_missing_records_stay_unknown,
        test_disconnected_seat_is_not_filled,
        test_five_recommended_islands,
        test_unread_records_do_not_count_as_present,
        test_record_reader_hand_counts,
    ]
    failed = 0
    for test in tests:
        result = test()
        failed += result
        if result == 0:
            print('PASS', test.__name__)
    print(f'{len(tests) - failed} pass, {failed} fail')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
