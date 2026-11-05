"""Read-only construction ledger. No simulator outputs or original assets are written.

Run --self-test for synthetic arithmetic/reader controls; --check checks the independently
transcribed audit bill. A bill is not a minimum layout or a production sufficiency proof.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct

# These ids choose the audit bill only; they are not an engine construction allowlist.
CHAIN = [1302, 801, 809, 807, 1001, 805, 1023, 1310, 1314, 1335, 1320, 1203, 1022]
EXTRA = [501, 520, 2520, 1301, 1201, 804, 1205, 1017, 1019]

def records(data, start=0):
    while start < len(data):
        if start + 16 > len(data):
            raise ValueError('truncated chunk header')
        name = data[start:start+12].split(b'\0')[0].decode('ascii')
        size = struct.unpack_from('<I', data, start+12)[0]
        end = start + 16 + size
        if end > len(data):
            raise ValueError('truncated chunk payload')
        yield name, data[start+16:end]
        start = end

def entries(data):
    root = dict(records(data))['ANNO']
    for name, payload in records(dict(records(root))['ANLAGEN']):
        if name != 'ENTRY':
            continue
        rid = struct.unpack_from('<I', payload, 4)[0]
        yield rid, dict(records(payload, 8))

def cost(record):
    h = record['HEAD']
    if len(h) < 56:
        raise ValueError('audit requires full original HEAD')
    return dict(gold=struct.unpack_from('<H', h, 8)[0], wood=h[4], tools=h[5],
                bricks=h[3], marble=h[6])

def bill(rows, quantities):
    return {key: sum(rows[str(rid)]['cost'][key] * count for rid,count in quantities.items())
            for key in ('gold','wood','tools','bricks','marble')}

def ledger(game):
    data = (game / 'Anlagen.dat').read_bytes()
    rows = {}
    for rid, r in entries(data):
        if rid not in CHAIN + EXTRA:
            continue
        text = lambda name: r.get(name,b'').split(b'\0')[0].decode('latin1')
        names = lambda name: [x.decode('latin1') for x in r.get(name,b'').split(b'\0') if x]
        rows[str(rid)] = dict(cost=cost(r), inframin=text('INFRAMIN'),
                             prodtype=text('PRODTYPE'), inputs=names('ROHSTOFF'),
                             outputs=names('WARE'), flags=struct.unpack_from('<H',r['HEAD'],0x1c)[0])
    if set(rows) != {str(x) for x in CHAIN + EXTRA}:
        raise ValueError('audit type missing')
    quantities = {rid:1 for rid in CHAIN}
    one_each = bill(rows, quantities)
    quantities.update({501:16, 520:1})
    return dict(anlagen_sha256=hashlib.sha256(data).hexdigest(), rows=rows,
                chain_bill=one_each, capacity_lower_bound_bill=bill(rows,quantities),
                assumptions=dict(pioneer_houses=16, upgrades=1, dirt_road_gold_per_tile=5,
                                 starting_gold=30000, starting_wood=50, starting_tools=80,
                                 starting_food=10))

def self_test():
    h=bytearray(56);h[3:7]=bytes([7,11,13,17]);struct.pack_into('<H',h,8,300)
    assert cost({'HEAD':h}) == dict(gold=300,wood=11,tools=13,bricks=7,marble=17)
    rows={'1':{'cost':cost({'HEAD':h})}}
    assert bill(rows,{1:2}) == dict(gold=600,wood=22,tools=26,bricks=14,marble=34)
    try: list(records(b'bad'))
    except ValueError: pass
    else: raise AssertionError('truncated chunk accepted')
    # This oracle distinguishes wrong cost offsets and incorrect quantities.
    wrong=dict(rows['1']['cost']);wrong['wood']=13
    assert bill({'1':{'cost':wrong}},{1:2})['wood'] != 22
    print('bootstrap_budget_contract: synthetic reader, arithmetic and wrong-offset control PASS')

def main():
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path);p.add_argument('--check',type=Path);p.add_argument('--self-test',action='store_true');p.add_argument('--out',type=Path)
    args=p.parse_args()
    if args.self_test:
        self_test();return
    game=args.game_dir or (Path(os.environ['ANNO1503_GAME_DIR']) if os.environ.get('ANNO1503_GAME_DIR') else None)
    if game is None:
        print('SKIP: ANNO1503_GAME_DIR not set (required assets)');return
    got=ledger(game)
    if args.check:
        expected=json.loads(args.check.read_text())
        for key in ('anlagen_sha256','chain_bill','capacity_lower_bound_bill'):
            if got[key] != expected[key]: raise ValueError(f'{key}: expected {expected[key]}, got {got[key]}')
        for rid, cost_expected in expected['costs'].items():
            if got['rows'][rid]['cost'] != cost_expected: raise ValueError(f'type{rid}: cost mismatch')
        print('bootstrap_budget_original: independent bill matches original HEAD records')
    data=json.dumps(got,indent=2,sort_keys=True)+'\n'
    if args.out: args.out.write_text(data)
    elif not args.check: print(data,end='')
if __name__ == '__main__': main()
