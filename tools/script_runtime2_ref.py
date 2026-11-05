#!/usr/bin/env python3
"""Independent SCRIPT2 hand-vector oracle. No engine bindings or output ingestion."""
import argparse
import json
from pathlib import Path
import unittest


def signed32(n):
    return (n + 2**31) % 2**32 - 2**31


def trunc(n, d):
    return n // d if n >= 0 else -((-n) // d)


def touch(v, scan=False):
    y0, x0, x1, y1 = v['authored_y0_x0_x1_y1']
    x, y = (trunc(n, 256) for n in v['pos_q8'])
    kind = v['bewohntype']
    lo, hi = (113, 128) if v['query'] == 'ship' else (97, 112)
    eligible = (v['sector_registered'] and not v.get('removed', False)
                and not v.get('parent_present', False)
                and v['mapped_owner'] == v['figure_owner'] and lo <= kind <= hi
                and not (v['state_word'] & 0x200e)
                and (not 108 <= kind <= 112 or v.get('crew_occupied', False)))
    if scan:
        sx, sy = trunc(x, 16), trunc(y, 16)
        eligible = (eligible and trunc(max(x0, 0), 16) <= sx
                    < trunc(min(signed32(x1 + 15), v['world_width']), 16)
                    and trunc(max(y0, 0), 16) <= sy
                    < trunc(min(signed32(y1 + 15), v['world_height']), 16))
    return bool(eligible and x0 <= x < x1 and y0 <= y < y1)


def wave(namespace, actions):
    return [dict(asset=namespace + '/' + a, code=317, audience='AllLocal',
                 original_recipient=0xaa) for a in actions]


def check(data):
    count = 0
    base = data['default_case']
    for v in data['vectors'] + data['query_scan_domain_vectors']:
        assert touch(base | v, 'world_width' in v) == v.get('expected', v.get('expected_original')), v['id']
        count += 1
    for v in data['transitions']:
        assert [touch(base | dict(pos_q8=p)) for p in v['pos_q8_sequence']] == v['expected_sequence']
        count += 1
    for a, b, result in data['money_original_vectors']:
        assert signed32(a + b) == result
        count += 1
    for a, b, result in data['money_engine_vectors']:
        assert max(-2**63, min(2**63-1, a + b)) == result
        count += 1
    for v in data['wave_vectors']:
        ident = v['id']
        if 'namespace' in v:
            events = wave(v['namespace'], v['actions'])
            if 'expected' in v:
                assert events == v['expected']
            if 'expected_assets' in v:
                assert [e['asset'] for e in events] == v['expected_assets']
            if 'reject_asset' in v:
                assert events[0]['asset'] != v['reject_asset']
            if 'reject_audience' in v:
                assert events[0]['audience'] != v['reject_audience']
        elif ident == 'transaction-rollback':
            money, events = v['money_before'], []
            if not v['fault_after']:
                money += v['money_delta']
                events = v['actions']
            assert (money, events) == (v['expected_money'], v['expected_events'])
        elif ident == 'queue-drop-oldest':
            assert v['actions'][-v['capacity']:] == v['expected_assets']
            assert len(v['actions']) - v['capacity'] == v['expected_dropped']
            assert not any(v[k] for k in ['drain_changes_state', 'drain_changes_prng', 'drain_changes_work'])
        elif ident == 'ready-reload':
            assert not v['expected_events'] and v['next_visit_actions'] == v['expected_next']
        else:
            raise AssertionError(ident)
        count += 1
    assert count > 40
    return count


class Contract(unittest.TestCase):
    def test_wrap(self):
        self.assertEqual(signed32(2147483647 + 15), -2147483634)
        self.assertEqual(trunc(-128, 256), 0)

    def test_wrong_recipient(self):
        self.assertNotEqual(wave('scenes/mod', ['A.WAV'])[0]['audience'], 'Player170')

    def test_wrong_expected_rejected(self):
        d = json.loads(Path(__file__).resolve().parents[1].joinpath(
            'fixtures/synthetic/sim/script2/handler_vectors.json').read_text())
        check(d)
        d['vectors'][0]['expected'] = False
        with self.assertRaises(AssertionError):
            check(d)


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--check', type=Path)
    p.add_argument('--self-test', action='store_true')
    a = p.parse_args()
    if a.self_test:
        unittest.main(argv=['script_runtime2_ref'], exit=True)
    elif a.check:
        print('SCRIPT2 hand vectors:', check(json.loads(a.check.read_text())))
    else:
        p.error('require --check or --self-test')
