from common import *
import sys,re
preset,case=sys.argv[1:];assert preset in ('release','asan');plan=json.loads((OUT/'stage4-preflight.json').read_text());row=next(r for r in plan['cases'] if r['id']==case);assert row[preset]
state=OUT/'stage4.json'
s=json.loads(state.read_text()) if state.exists() else stage(4,4*3600)
name=f'stage4-{preset}-{case}';assert not any(m['name']==name for m in s['commands']),'Never rerun a valid measure'
extra={'ANNO_TEST_MB_PER_THREAD':None,'ASAN_OPTIONS':None,'UBSAN_OPTIONS':None,row['variable']:row['value']}
if preset=='asan':extra.update(ANNO_TEST_MB_PER_THREAD='2048',ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='print_stacktrace=1:halt_on_error=1')
env=environment(extra);raw=OUT/(name+'.raw.log')
opts=['--build-dir','build/'+preset,'--select','^'+re.escape(row['test'])+'$','--asset-policy','required','--allow-skip','gpu','--allow-skip','other','--jobs','1','--timeout','14400','--log',str(raw.relative_to(ROOT))]
if any(m['name'].startswith('stage4-'+preset+'-') and m.get('required_contract_passed') for m in s['commands']):opts+=['--skip-auto-contract']
m=run(s,name,['/usr/bin/time','-l','python3','-I','tools/test/run_selected.py',*opts],env)
if raw.exists():
 body=raw.read_bytes();view=OUT/(name+'.log');view.write_bytes(('# environment '+json.dumps(env_record(env),sort_keys=True)+'\n# canonical raw bytes preserved at '+str(raw.relative_to(ROOT))+'\n').encode()+body)
 m['required_contract_passed']='--skip-auto-contract' not in opts and len([l for l in body.decode(errors='replace').splitlines() if l.startswith(('COUNTS:','VERDICT:'))])>=2 and [l for l in body.decode(errors='replace').splitlines() if l.startswith(('COUNTS:','VERDICT:'))][1]=='VERDICT: PASS'
 m.update(raw_log=str(raw.relative_to(ROOT)),raw_sha256=sha(raw),log_view=str(view.relative_to(ROOT)),log_view_sha256=sha(view),receipts=[l for l in body.decode(errors='replace').splitlines() if l.startswith(('COUNTS:','VERDICT:','GATE_END '))]);save(state,s)
s['status']='awaiting_case_audit';save(state,s)
