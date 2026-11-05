#!/usr/bin/env python3
"""Publish bounded SCRIPT2 capability facts from a successful frozen canonical gate."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import unittest
import tempfile
import io
from contextlib import redirect_stdout

ROOT = Path(__file__).resolve().parents[1]
ROW = re.compile(r'MESSAGE: CAPABILITY2 (\S+) compiled=([01]) startup=([01]) visits=(\d+) branches=(\d+) empty=([01]) dependency=(.*)')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def validate_original_scenario_file(path, inputs):
    if sha(path) != inputs['scenario_sha256']:
        raise ValueError('original scenario file differs from fresh-process proof')


def execution_scope(row, original=False):
    if not row['normal_startup']:
        return 'NOT_EXECUTED'
    if original:
        return 'ORIGINAL_PUBLIC_COMMAND_CANARY: branch0 wave, founding, unload, HANDWERK1001, native cuts and fresh replay; other branches separate'
    if row['empty_kernel']:
        return 'EMPTY_KERNEL_ONLY; no original tutorial behavior claim'
    return 'INIT_AND_BOUNDED_CALLS_ONLY; no full original-world acceptance'


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


REQUIRED_ARTIFACTS={'normal.log','wrong-command.log','wrong-program.log','expected.txt'}
CASES={'producer':'SCRIPT2 synthetic complete original program canary producer',
       'worker':'SCRIPT2 synthetic encoded canary replay worker'}
ORIGINAL_CASES={'producer':'SCRIPT2 original public voyage founding and construction',
                'worker':'SCRIPT2 original encoded canary replay worker'}


def validate_manifest(identity, manifest, original=False):
    source=identity['source_tested']['head']
    if manifest['verdict']!='PASS' or manifest['source_revision']!=source:
        raise ValueError('fresh-process proof must use same frozen source')
    executable=manifest['executable']
    digest=manifest['sha256']
    if not re.fullmatch('[0-9a-f]{64}',digest) or identity['binaries'].get(executable)!=digest:
        raise ValueError('manifest executable differs from canonical tested binary')
    cases=ORIGINAL_CASES if original else CASES
    if original and manifest.get('mode') != 'original':
        raise ValueError('require explicit original-world protocol')
    processes=manifest['processes']
    if len(processes)!=2 or {p['role'] for p in processes}!=set(cases):
        raise ValueError('require exactly producer and worker roles')
    if len({p['pid'] for p in processes})!=2 or any(p['pid']<=0 or p['exit']!=0 for p in processes):
        raise ValueError('require distinct successful child processes')
    for process in processes:
        if process['command']!=[executable,'--test-case='+cases[process['role']]]:
            raise ValueError('child command differs from tested executable/case')
    if set(manifest['artifacts'])!=REQUIRED_ARTIFACTS:
        raise ValueError('require exact nonempty replay artifact set')
    for fact in manifest['artifacts'].values():
        if fact['size']<=0 or not re.fullmatch('[0-9a-f]{64}',fact['sha256']):
            raise ValueError('invalid nonempty artifact identity')


def validate_program_row(row, items, branch_count, synthetic):
    if row['compiled']:
        if row['branch_count']!=branch_count or row['empty_kernel']!=(len(items)==0):
            raise ValueError('compiled row differs from independently parsed AST')
        if row['normal_startup'] and row['normal_zero_ms_calls']!=branch_count:
            raise ValueError('bounded-call count differs from exercised protocol')
    if any(i<0 or i>=branch_count for i in synthetic):
        raise ValueError('synthetic branch index outside program')


class Contracts(unittest.TestCase):
    identity = dict(source_tested=dict(head='a'*40,status='',untracked={}), binaries={'exe':'b'*64})
    def text(self, row='A compiled=1 startup=1 visits=0 branches=0 empty=1 dependency=none'):
        return ('== tested identity '+json.dumps(self.identity)+'\nX: MESSAGE: CAPABILITY2 '+row+'\nGATE_END {"status":0}\n')
    def manifest(self):
        return dict(verdict='PASS',source_revision='a'*40,executable='exe',sha256='b'*64,
                    processes=[dict(role=r,pid=i+1,exit=0,command=['exe','--test-case='+case])
                               for i,(r,case) in enumerate(CASES.items())],
                    artifacts={n:dict(size=1,sha256='e'*64) for n in REQUIRED_ARTIFACTS})
    def test_manifest_join_and_required_proof(self):
        validate_manifest(self.identity,self.manifest())
        for kind in range(7):
            m=self.manifest()
            if kind==0: m['artifacts']={}
            if kind==1: m['executable']='/unrelated/executable'
            if kind==2: m['sha256']='0'*64
            if kind==3: m['processes'][1]['role']='producer'
            if kind==4: m['processes'][0]['command'][0]='other'
            if kind==5: m['artifacts']['normal.log']['size']=0
            if kind==6: m['source_revision']='f'*40
            with self.assertRaises(ValueError): validate_manifest(self.identity,m)
    def test_original_manifest_cannot_borrow_synthetic_protocol(self):
        m=self.manifest()
        with self.assertRaises(ValueError): validate_manifest(self.identity,m,True)
        m['mode']='original'
        with self.assertRaises(ValueError): validate_manifest(self.identity,m,True)
        for process in m['processes']:
            process['command']=['exe','--test-case='+ORIGINAL_CASES[process['role']]]
        validate_manifest(self.identity,m,True)
        for key, value in [('verdict','FAIL'),('source_revision','f'*40),('sha256','0'*64)]:
            with self.assertRaises(ValueError): validate_manifest(self.identity,m|{key:value},True)
    def test_execution_scope_requires_startup_before_empty_kernel_label(self):
        started=dict(normal_startup=True,empty_kernel=True)
        self.assertTrue(execution_scope(started).startswith('EMPTY_KERNEL_ONLY'))
        self.assertEqual(execution_scope(started|dict(normal_startup=False)),'NOT_EXECUTED')
        self.assertEqual(execution_scope(started|dict(normal_startup=False),True),'NOT_EXECUTED')
        self.assertTrue(execution_scope(started|dict(empty_kernel=False)).startswith('INIT_AND_BOUNDED'))
        self.assertTrue(execution_scope(started,True).startswith('ORIGINAL_PUBLIC_COMMAND_CANARY'))
    def test_full_file_join_distinguishes_same_program_different_map(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'scenario.sze'
            program=b'[SCRIPT_KERNEL]\nSET X=TRUE;\n'
            path.write_bytes(b'map0'+program)
            inputs=dict(scenario_sha256=sha(path))
            validate_original_scenario_file(path,inputs)
            path.write_bytes(b'map1'+program)
            self.assertEqual(path.read_bytes()[4:],program)
            with self.assertRaisesRegex(ValueError,'scenario file differs'):
                validate_original_scenario_file(path,inputs)
    def test_program_row_semantics(self):
        row=dict(compiled=True,normal_startup=True,normal_zero_ms_calls=0,branch_count=0,empty_kernel=True)
        validate_program_row(row,[],0,[])
        for mutation in (dict(branch_count=999),dict(empty_kernel=False),dict(normal_zero_ms_calls=1)):
            with self.assertRaises(ValueError): validate_program_row(row|mutation,[],0,[])
        with self.assertRaises(ValueError): validate_program_row(row,[],0,[0])
    def test_public_publish_rejects_unrelated_or_empty_proof_before_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); game=root/'game'; scenes=game/'Scenes'; scenes.mkdir(parents=True)
            for i in range(50): (scenes/(str(i)+'.sze')).write_bytes(b'x')
            rows=''.join('X: MESSAGE: CAPABILITY2 Scenes/'+str(i)+'.sze compiled=1 startup=1 visits=0 branches=0 empty=1 dependency=none\n' for i in range(50))
            log=root/'gate.log'; log.write_text('== tested identity '+json.dumps(self.identity)+'\n'+rows+'GATE_END {"status":0}\n')
            manifest=root/'manifest.json'; output=root/'output.json'
            for kind in range(2):
                m=self.manifest()
                if kind==0: m['artifacts']={}
                else: m['executable']='/unrelated/executable'
                manifest.write_text(json.dumps(m))
                captured=io.StringIO()
                with redirect_stdout(captured),self.assertRaises(ValueError): publish(log,game,manifest,output)
                self.assertFalse(output.exists())
                self.assertNotIn('SCRIPT2 inventory:',captured.getvalue())
    def test_public_publish_rejects_wrong_original_proof_before_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); game=root/'game'; scenes=game/'Scenes'; scenes.mkdir(parents=True)
            for i in range(50): (scenes/(str(i)+'.sze')).write_bytes(b'x')
            rows=''.join('X: MESSAGE: CAPABILITY2 Scenes/'+str(i)+'.sze compiled=0 startup=0 visits=0 branches=0 empty=0 dependency=unsupported\n' for i in range(50))
            log=root/'gate.log'; log.write_text('== tested identity '+json.dumps(self.identity)+'\n'+rows+'GATE_END {"status":0}\n')
            sys.path.insert(0,str(ROOT/'tools'))
            import script_canary2 as canary
            m=self.manifest(); m['inputs']=canary.EXPECTED_INPUTS
            for name in REQUIRED_ARTIFACTS:
                path=root/name; path.write_bytes(b'nonempty')
                m['artifacts'][name]=dict(size=path.stat().st_size,sha256=sha(path))
            for role in CASES:
                (root/(role+'.log')).write_text(canary.Contracts.good)
            manifest=root/'synthetic.json'; manifest.write_text(json.dumps(m))
            original=root/'original.json'; output=root/'output.json'
            for mutation in ({'mode':'original'}, {'mode':'original','verdict':'FAIL'}, {'mode':'original','source_revision':'f'*40}):
                original.write_text(json.dumps(m|mutation))
                captured=io.StringIO()
                with redirect_stdout(captured),self.assertRaises(ValueError):
                    publish(log,game,manifest,output,original)
                self.assertFalse(output.exists())
                self.assertNotIn('SCRIPT2 inventory:',captured.getvalue())
            # All protocol/manifest checks pass, but the corpus scenario differs.
            # This must fail on its full-file identity before parsing or output.
            old=scenes/'49.sze'; old.unlink()
            scenario=scenes/'0011SingleEndlosAddon/endless01.sze'
            scenario.parent.mkdir(); scenario.write_bytes(b'different map; unchanged program')
            rows=rows.replace('Scenes/49.sze','Scenes/0011SingleEndlosAddon/endless01.sze')
            log.write_text('== tested identity '+json.dumps(self.identity)+'\n'+rows+'GATE_END {"status":0}\n')
            m['mode']='original'
            for process in m['processes']:
                process['command']=['exe','--test-case='+ORIGINAL_CASES[process['role']]]
            original.write_text(json.dumps(m))
            captured=io.StringIO()
            with redirect_stdout(captured),self.assertRaisesRegex(ValueError,'scenario file differs'):
                publish(log,game,manifest,output,original)
            self.assertFalse(output.exists())
            self.assertNotIn('SCRIPT2 inventory:',captured.getvalue())

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


def publish(log, game, manifest_path, output, original_manifest_path=None):
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
    validate_manifest(identity,manifest)
    canary.validate_input_identity(manifest['inputs'])
    processes=manifest['processes']
    for process in processes:
        canary.checked_child(process['exit'], manifest_path.with_name(process['role']+'.log').read_text())
    for name, fact in manifest['artifacts'].items():
        p = manifest_path.with_name(name)
        if p.stat().st_size != fact['size'] or sha(p) != fact['sha256']:
            raise ValueError('fresh artifact identity differs: '+name)
    original_manifest = None
    if original_manifest_path is not None:
        original_manifest=json.loads(original_manifest_path.read_text())
        validate_manifest(identity,original_manifest,True)
        canary.validate_input_identity(original_manifest['inputs'])
        scenario=expected.get('Scenes/0011SingleEndlosAddon/endless01.sze')
        if scenario is None:
            raise ValueError('original canary scenario missing from corpus')
        validate_original_scenario_file(scenario,original_manifest['inputs'])
        for process in original_manifest['processes']:
            canary.checked_child(process['exit'],original_manifest_path.with_name(process['role']+'.log').read_text())
        for name,fact in original_manifest['artifacts'].items():
            path=original_manifest_path.with_name(name)
            if path.stat().st_size!=fact['size'] or sha(path)!=fact['sha256']:
                raise ValueError('original fresh artifact identity differs: '+name)
    programs = []
    for name, path in expected.items():
        payload = bb.payload(path)
        ast = bb.parse(payload)
        items = ast.section('[SCRIPT_KERNEL]').items
        walked = list(bb.walk(items))
        row = rows[name]
        synthetic = [0,5,6] if name == 'Scenes/0011SingleEndlosAddon/endless01.sze' else []
        original = [0] if synthetic and original_manifest is not None else []
        if original and not row['normal_startup']:
            raise ValueError('original canary requires normal startup in the joined inventory')
        validate_program_row(row,items,sum(isinstance(n,bb.If) for n in items),synthetic)
        if synthetic and hashlib.sha256(payload).hexdigest() != manifest['inputs']['program_sha256']:
            raise ValueError('synthetic coverage source differs')
        programs.append(dict(file=name,sha256=sha(path),program_sha256=hashlib.sha256(payload).hexdigest(),
                             parsed=True,**row,kernel_items=len(items),
                             actions=dict(Counter(n.name.upper() for n in walked if isinstance(n,bb.Call))),
                             queries=dict(Counter(n.subject+':'+n.query for n in walked if isinstance(n,bb.Primary) and n.subject)),
                             original_true_branch_coverage=original,synthetic_true_branch_coverage=synthetic,
                             execution_scope=execution_scope(row,bool(original)),
                             executed_full_original_scenario=False))
    data = dict(schema='SCRIPT2-capabilities-2',source_revision=source,
                publisher_revision=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip(),
                publisher_script_sha256=sha(__file__),
                workflow_revision=identity['workflow']['head'],gate=str(log),gate_sha256=sha(log),
                binaries=identity['binaries'],parsed=50,compiled=sum(r['compiled'] for r in rows.values()),
                normal_startups=sum(r['normal_startup'] for r in rows.values()),executed_full_original_scenarios=0,
                original_public_command_canaries=1 if original_manifest is not None else 0,
                original_world_canary='PUBLIC_COMMAND_BRANCH0_AND_CONSTRUCTION_PROVEN; not full campaign or all-branch coverage' if original_manifest is not None else 'NOT_JOINED: no original-world fresh-process artifact supplied',
                original_canary_manifest=str(original_manifest_path) if original_manifest is not None else None,
                original_canary_manifest_sha256=sha(original_manifest_path) if original_manifest is not None else None,
                synthetic_canary_manifest=str(manifest_path),synthetic_canary_manifest_sha256=sha(manifest_path),programs=programs)
    output.write_text(json.dumps(data,indent=2)+'\n')
    if json.loads(output.read_text()) != data:
        raise ValueError('inventory write verification failed')
    print('SCRIPT2 inventory: parsed='+str(data['parsed'])+' compiled='+str(data['compiled'])+' normal_startups='+str(data['normal_startups'])+' original_public_command_canaries='+str(data['original_public_command_canaries'])+' full_original_scenarios=0')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--self-test',action='store_true')
    p.add_argument('--log',type=Path)
    p.add_argument('--game-dir',type=Path)
    p.add_argument('--canary-manifest',type=Path)
    p.add_argument('--original-manifest',type=Path)
    p.add_argument('--output',type=Path)
    a = p.parse_args()
    if a.self_test:
        unittest.main(argv=['script_inventory2'])
    elif all((a.log,a.game_dir,a.canary_manifest,a.output)):
        publish(a.log,a.game_dir,a.canary_manifest,a.output,a.original_manifest)
    else:
        p.error('require --self-test or --log/--game-dir/--canary-manifest/--output')
