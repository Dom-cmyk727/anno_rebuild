from common import *
import re
s=json.loads((OUT/'stage2.json').read_text());assert s['status']!='running';manifest=json.loads((OUT/'stage2-registry/commands.json').read_text());raw=OUT/'stage2-whole-01.raw.log'
subprocess.run(['python3',str(OUT/'audit_gate.py'),str(raw),str(OUT/'stage2-outcomes.json')],check=True)
terminals={}
for line in raw.read_text(errors='replace').splitlines():
 # Use exact terminal name from the known registry; names contain no spaces.
 for name in re.findall(r'Test\s+#\d+: (whole_\S+)\s+\.+',line):
  t=re.search(r'([0-9.]+) sec\s*$',line);terminals[name]={'line':line,'wall_seconds':float(t[1]) if t else None}
rows=[];alerts=[]
for rec in manifest['records']:
 name=rec['name'];directory=OUT/'stage2-children'/name;p=directory/'result.json'
 if not p.exists():
  rows.append({'operation':name,'status':'no native terminal result','canonical_terminal':terminals.get(name),'raw_log':str(directory/'whole.log')});continue
 r=json.loads(p.read_text());failed={f['name'] for f in r['first_failures']};prereqs=[v for v in r['prerequisite_cases'] if v['name'] not in failed];classes={k:0 for k in ('required-asset','platform','opt-in','gpu','other')}
 priority=['required-asset','opt-in','platform','gpu','other']
 for q in prereqs:
  kind=next((kind for kind in priority if any(v['class']==kind for v in q['observations'])),'other');classes[kind]+=1
 row={'operation':name,'mode':r['mode'],'binary':r['binary'],'binary_sha256':r['binary_sha256'],'command':r['command'],'native_exit':r['native_exit'],'framework':r['summary'],'executed_passes_excluding_prerequisite_cases':r['summary']['framework_passed']-len(prereqs),'prerequisite_skipped_cases':len(prereqs),'skip_classes':classes,'prerequisite_cases':prereqs,'filter_excluded':r['summary']['filter_excluded'],'canonical_terminal':terminals.get(name),'ordered_cases':r['ordered_cases'],'first_failures':r['first_failures'],'raw_log':r['body_log'],'raw_sha256':r['body_log_sha256'],'environment':r['environment']};rows.append(row)
 lines=Path(r['body_log']).read_text(errors='replace').splitlines()
 for i,line in enumerate(lines):
  if ('[anno-test memory]' in line and ('refused' in line or 'std::bad_alloc' in line)) or any(k in line for k in ('ERROR: AddressSanitizer:','runtime error:','AddressSanitizer:DEADLYSIGNAL')):
   alerts.append({'operation':name,'line_number':i+1,'line':line,'context':lines[max(0,i-4):i+12]})
save(OUT/'stage2-matrix.json',{'scope':'Whole-native-binary operations; native cases, prerequisite returns and filter exclusions classified separately. Canonical CTest duration is used uniformly; see duration-metadata-note.json.','rows':rows,'native_log_alerts':alerts})
if alerts:print('Native alerts need disposition before stage closure');raise SystemExit(3)
status='partial_time_box' if any('time_box_stop' in m for m in s['commands']) else ('completed_with_findings' if any(r.get('native_exit',0) for r in rows) else 'completed')
finish(s,status);print('Stage 2',status,'native terminal operations',sum('native_exit' in r for r in rows))
