from common import *
from collections import defaultdict
s=json.loads((OUT/'stage3.json').read_text());expected={t['name'] for t in json.loads((OUT/'stage2-release-inventory.json').read_text())['tests']};rounds=[];by_name=defaultdict(list)
for n in (1,2,3):
 p=OUT/f'stage3-full-{n:02d}-outcomes.json';cmd=next((m for m in s['commands'] if m['name']==f'stage3-full-{n:02d}'),None)
 if not p.exists():rounds.append({'round':n,'status':'NOT_RUN_OR_NOT_AUDITED','command_metadata':cmd});continue
 a=json.loads(p.read_text());seen={r['name'] for r in a['cases']};incomplete=[]
 for r in a['cases']:
  if r['status'] is None:incomplete.append(r['name']);continue
  classes=sorted({q['class'] for q in r.get('prerequisites',[])})
  row={'round':n,'terminal':r['status'],'prerequisite_classes':classes,'first_evidence':r.get('first_evidence'),'tail':r.get('tail')};by_name[r['name']].append(row)
 rounds.append({'round':n,'status':'terminal_audited','command_metadata':cmd,'receipts':a['receipts'],'not_started':sorted(expected-seen),'started_incomplete':incomplete,'memory_refusal_candidates':a['memory_refusal_candidates'],'bad_alloc_retries':a['bad_alloc_retries'],'sanitizer_reports':a['sanitizer_reports'],'failures':a['failures'],'raw_sha256':a['raw_sha256']})
differing=[]
for name,rows in sorted(by_name.items()):
 signatures={(r['terminal'],tuple(r['prerequisite_classes'])) for r in rows}
 if len(signatures)>1:differing.append({'name':name,'observations':rows})
save(OUT/'stage3-comparison.json',{'source':{'head':PIN,'tree':TREE},'scope':'Compare terminal status and prerequisite class by full case name. Missing or interrupted cases are coverage gaps, not silently labelled flakes. Preserve differing failure excerpts. Raw terminal COUNTS/VERDICT remain authoritative.','rounds':rounds,'differing_results':differing,'terminal_name_union':len(by_name)})
print(json.dumps({'rounds':[{'round':r['round'],'status':r['status'],'not_started':len(r.get('not_started',[])),'incomplete':len(r.get('started_incomplete',[]))} for r in rounds],'differing_result_names':[r['name'] for r in differing]},indent=2))
