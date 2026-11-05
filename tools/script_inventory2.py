#!/usr/bin/env python3
"""Publish bounded SCRIPT2 capability facts from a successful frozen canonical gate."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
ROW = re.compile(r'MESSAGE: CAPABILITY2 (\S+) compiled=([01]) startup=([01]) visits=(\d+) branches=(\d+) empty=([01]) dependency=(.*)')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def parse_gate(text, expected):
    ends = [json.loads(line[len('GATE_END '):]) for line in text.splitlines() if line.startswith('GATE_END ')]
    if len(ends) != 1 or ends[0]['status'] != 0:
        raise ValueError('require one successful canonical terminal record')
    identities = [json.loads(line[len('== tested identity '):]) for line in text.splitlines() if line.startswith('== tested identity ')]
    if len(identities) != 1:
        raise ValueError('missing/ambiguous tested identity')
    identity = identities[0]
    source = identity['source_tested']
    if source['status'] or source['untracked'] or not re.fullmatch('[0-9a-f]{40}', source['head']):
        raise ValueError('require clean frozen source')
    result = {}
    for line in text.splitlines():
        if 'MESSAGE: CAPABILITY2 ' not in line:
            continue
        match = ROW.search(line)
        if not match:
            raise ValueError('malformed capability row')
        file, compiled, startup, visits, branches, empty, refusal = match.groups()
        if file in result:
            raise ValueError('duplicate capability row')
        c, s, v, b, e = map(int, (compiled, startup, visits, branches, empty))
        if (not c and (s or v or b or e)) or (not s and v) or (e and b) or (not c and refusal == 'none'):
            raise ValueError('impossible capability relationship')
        result[file] = dict(compiled=bool(c), normal_startup=bool(s), normal_zero_ms_calls=v,
                            branch_count=b, empty_kernel=bool(e), refusal=None if refusal == 'none' else refusal)
    if not expected or set(result) != set(expected):
        raise ValueError('capability rows do not exactly cover original corpus')
    return identity, result


class Contracts(unittest.TestCase):
    identity = dict(source_tested=dict(head='a'*40,status='',untracked={}), binaries={'exe':'b'*64})
    def text(self, row='A compiled=1 startup=1 visits=0 branches=0 empty=1 dependency=none'):
        return ('== tested identity '+json.dumps(self.identity)+'\nX: MESSAGE: CAPABILITY2 '+row+'\nGATE_END {"status":0}\n')
    def test_legal(self):
        _, rows = parse_gate(self.text(), ['A'])
        self.assertTrue(rows['A']['compiled'])
        self.assertTrue(rows['A']['empty_kernel'])
    def test_failed_or_truncated_gate(self):
        for text in (self.text().replace('"status":0','"status":1'), self.text().split('GATE_END')[0]):
            with self.assertRaises(ValueError): parse_gate(text,['A'])
    def test_impossible_or_malformed(self):
        for row in ('A compiled=0 startup=1 visits=0 branches=0 empty=0 dependency=unsupported',
                    'A compiled=1 startup=0 visits=1 branches=1 empty=0 dependency=failed',
                    'A compiled=1 startup=1 visits=0 branches=1 empty=1 dependency=none',
                    'A compiled=2 startup=1 visits=0 branches=0 empty=1 dependency=none'):
            with self.assertRaises(ValueError): parse_gate(self.text(row),['A'])
    def test_duplicate_missing_empty(self):
        duplicate=self.text().replace('GATE_END','X: MESSAGE: CAPABILITY2 A compiled=1 startup=1 visits=0 branches=0 empty=1 dependency=none\nGATE_END')
        for text, names in ((duplicate,['A']),(self.text(),['A','B']),(self.text(),[])):
            with self.assertRaises(ValueError): parse_gate(text,names)
    def test_dirty_source(self):
        with self.assertRaises(ValueError): parse_gate(self.text().replace('"status": ""','"status": " M source"'),['A'])


def publish(log, game, manifest_path, output):
    sys.path.insert(0,str(ROOT/'tools'))
    import bigbrother as bb
    import script_canary2 as canary
    files = sorted((game/'Scenes').rglob('*.sze'))
    if len(files) != 50:
        raise ValueError('expected original 50-program corpus')
    expected = {p.relative_to(game).as_posix(): p for p in files}
    identity, rows = parse_gate(log.read_text(), expected)
    source = identity['source_tested']['head']
    manifest = json.loads(manifest_path.read_text())
    if manifest['verdict'] != 'PASS' or manifest['source_revision'] != source:
        raise ValueError('fresh-process proof must use same frozen source')
    canary.validate_input_identity(manifest['inputs'])
    processes = manifest['processes']
    if len(processes) != 2 or processes[0]['pid'] == processes[1]['pid']:
        raise ValueError('require distinct producer and worker')
    for process in processes:
        canary.checked_child(process['exit'], manifest_path.with_name(process['role']+'.log').read_text())
    for name, fact in manifest['artifacts'].items():
        p = manifest_path.with_name(name)
        if p.stat().st_size != fact['size'] or sha(p) != fact['sha256']:
            raise ValueError('fresh artifact identity differs: '+name)
    programs = []
    for name, path in expected.items():
        payload = bb.payload(path)
        ast = bb.parse(payload)
        items = ast.section('[SCRIPT_KERNEL]').items
        walked = list(bb.walk(items))
        row = rows[name]
        synthetic = [0,5,6] if name == 'Scenes/0011SingleEndlosAddon/endless01.sze' else []
        if synthetic and hashlib.sha256(payload).hexdigest() != manifest['inputs']['program_sha256']:
            raise ValueError('synthetic coverage source differs')
        programs.append(dict(file=name,sha256=sha(path),program_sha256=hashlib.sha256(payload).hexdigest(),
                             parsed=True,**row,kernel_items=len(items),
                             actions=dict(Counter(n.name.upper() for n in walked if isinstance(n,bb.Call))),
                             queries=dict(Counter(n.subject+':'+n.query for n in walked if isinstance(n,bb.Primary) and n.subject)),
                             original_true_branch_coverage=[],synthetic_true_branch_coverage=synthetic,
                             execution_scope=('EMPTY_KERNEL_ONLY; no original tutorial behavior claim' if row['empty_kernel'] else
                                              'INIT_AND_BOUNDED_CALLS_ONLY; no full original-world acceptance' if row['normal_startup'] else 'NOT_EXECUTED'),
                             executed_full_original_scenario=False))
    data = dict(schema='SCRIPT2-capabilities-1',source_revision=source,
                workflow_revision=identity['workflow']['head'],gate=str(log),gate_sha256=sha(log),
                binaries=identity['binaries'],parsed=50,compiled=sum(r['compiled'] for r in rows.values()),
                normal_startups=sum(r['normal_startup'] for r in rows.values()),executed_full_original_scenarios=0,
                original_world_canary='OPEN: original SCHIFF/SOLDAT/CREW import absent; no Human actors/city',
                synthetic_canary_manifest=str(manifest_path),synthetic_canary_manifest_sha256=sha(manifest_path),programs=programs)
    output.write_text(json.dumps(data,indent=2)+'\n')
    if json.loads(output.read_text()) != data:
        raise ValueError('inventory write verification failed')
    print('SCRIPT2 inventory: parsed='+str(data['parsed'])+' compiled='+str(data['compiled'])+' normal_startups='+str(data['normal_startups'])+' full_original_acceptance=0')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--self-test',action='store_true')
    p.add_argument('--log',type=Path)
    p.add_argument('--game-dir',type=Path)
    p.add_argument('--canary-manifest',type=Path)
    p.add_argument('--output',type=Path)
    a = p.parse_args()
    if a.self_test:
        unittest.main(argv=['script_inventory2'])
    elif all((a.log,a.game_dir,a.canary_manifest,a.output)):
        publish(a.log,a.game_dir,a.canary_manifest,a.output)
    else:
        p.error('require --self-test or --log/--game-dir/--canary-manifest/--output')
