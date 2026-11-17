from common import *
import re,importlib.util
s=json.loads((OUT/'stage4.json').read_text());plan=json.loads((OUT/'stage4-preflight.json').read_text());sp=importlib.util.spec_from_file_location('cv',ROOT/'tools/test/ctest_verdict.py');cv=importlib.util.module_from_spec(sp);sp.loader.exec_module(cv);rows=[]
for m in s['commands']:
 matches=[(preset,r) for preset in ('release','asan') for r in plan['cases'] if m['name']==f'stage4-{preset}-'+r['id'] or m['name'].startswith(f'stage4-{preset}-'+r['id']+'-memory')]
 if not matches:continue
 preset,r=matches[0];row={'operation':m['name'],'preset':preset,'preflight':r,'command_metadata':m}
 raw=ROOT/m['raw_log'] if 'raw_log' in m else None
 if not raw or not raw.exists():row['native_result']='NOT_RUN';rows.append(row);continue
 audit=OUT/(m['name']+'-outcomes.json')
 if not audit.exists():
  with (OUT/(m['name']+'-audit.json')).open('x') as f:subprocess.run(['python3',str(OUT/'audit_gate.py'),str(raw),str(audit)],stdout=f,check=True)
 a=json.loads(audit.read_text());native=next((v for v in a['cases'] if v['name']==r['test']),None);row['native_outcome']=native;row['receipts']=a['receipts'];row['memory_refusal_candidates']=a['memory_refusal_candidates'];row['bad_alloc_retries']=a['bad_alloc_retries'];row['sanitizer_reports']=a['sanitizer_reports'];row['native_result']='NOT_RUN' if native is None else ('prerequisite_skip' if native['prerequisites'] else native['status'])
 console=(ROOT/m['log']).read_text(errors='replace');rss=re.findall(r'^\s*(\d+)\s+maximum resident set size\s*$',console,re.M);row['peak_rss_as_reported']=int(rss[-1]) if rss else None;row['rss_scope']='Darwin /usr/bin/time -l maximum resident set size wrapping the selected gate; max child, not concurrent aggregate; field retained in native reported units.'
 body=[];num=native['number'] if native else None
 for line in raw.read_text(errors='replace').splitlines():
  q=cv.RESULT.match(line)
  if q and q[2]==r['test']:
   t=re.search(r'([0-9.]+) sec\s*$',line);row['canonical_case_wall_seconds']=float(t[1]) if t else None
  q=cv.PREFIX.match(line)
  if q and q[1]==num:body.append(q[2])
 row['measurement_lines']=[l for l in body if any(key in l for key in ('SCALE ','PROGRESSION ','K3PROBE','transport snapshot:','transport run:','transport frontier:','  type ','p50 ','ORE','iron corpus','site survey','wrote 32 runs'))]
 row['framework_summary']=[l for l in body if '[doctest] test cases:' in l or '[doctest] assertions:' in l];rows.append(row)
save(OUT/'stage4-summary.json',{'scope':'Individual opt-ins; gate wall, native case wall and maximum-resident-set-size field distinguished. Preflight says which results are asserted and which are printed. No PASS is inferred for a prerequisite return.','rows':rows})
print(json.dumps({'operations':len(rows),'results':[(r['operation'],r['native_result'],r.get('peak_rss_as_reported')) for r in rows],'alerts':sum(len(r.get('memory_refusal_candidates',[]))+len(r.get('sanitizer_reports',[])) for r in rows)},indent=2))
