from common import *
s=stage(6,3600);env=environment({'ANNO_TEST_MB_PER_THREAD':None,'ASAN_OPTIONS':None,'UBSAN_OPTIONS':None});tsv=OUT/'atlas_report.tsv'
m=run(s,'stage6-atlas',['build/release/src/tools/bench_frame/bench_frame',GAME,'-','--atlas-report',str(tsv)],env)
if tsv.exists():
 rows=tsv.read_text(errors='replace').splitlines();headers=None;section=None;values=[];sections={};errors=[];comments=[]
 wanted={'scene':['static_bytes','water_bytes','water_shadow_bytes','enhanced_extra_bytes','figures_used_bytes','pack_bytes','rect_bytes'],'group':['bytes','player_colourable_bytes']}
 for line in rows:
  if not line:continue
  if line.startswith('#'):
   comments.append(line)
   if line.startswith(('# cannot read','# FigurenHeader.dat:','# Anlagen.dat:')):errors.append({'catalog_diagnostic':line})
   continue
  cells=line.split('\t')
  if cells[0] in ('scene','group','frames_checked','adapter'):
   headers=cells;section=cells[0];sections[section]={'header':headers,'data_rows':0,'byte_columns':wanted.get(section,[]),'byte_rows':[]};continue
  if headers is None:errors.append('data row without header');continue
  if len(cells)!=len(headers):
   errors.append({'section':section,'malformed_row':line,'fields':len(cells),'expected_fields':len(headers)});continue
  r=dict(zip(headers,cells));sections[section]['data_rows']+=1
  if section=='scene' and r.get('error'):
   errors.append({'scene':r['scene'],'error':r['error'],'byte_hash_included':False});continue
  cols=wanted.get(section,[])
  if cols:
   selected=[r[c] for c in cols]
   if any(not v.isdigit() for v in selected):errors.append({'section':section,'non_integer_bytes':selected})
   row='\t'.join(selected)+'\n';values.append(row);sections[section]['byte_rows'].append(row)
 for required in ('scene','group','frames_checked'):
  if required not in sections or sections[required]['data_rows']==0:errors.append({'missing_or_empty_section':required})
 for v in sections.values():
  data=''.join(v.pop('byte_rows')).encode();v['byte_columns_sha256']=hashlib.sha256(data).hexdigest() if v['byte_columns'] else None
 save(OUT/'stage6-atlas-summary.json',{'exit':m.get('exit'),'tsv':str(tsv.relative_to(ROOT)),'tsv_sha256':sha(tsv),'physical_lines':len(rows),'data_rows':sum(v['data_rows'] for v in sections.values()),'sections':sections,'byte_columns_sha256':hashlib.sha256(''.join(values).encode()).hexdigest(),'encoding':'UTF-8; selected decimal cell strings in original row order, tabs between cells and LF after each row; no headers, comments, section labels, paths, fill ratios or non-byte fields. Loaded scene rows first, then figure-catalog rows; refused scene rows excluded and individually reported as errors, never synthesized as zero.','errors':errors,'comments':comments,'windows_comparison':'NOT_RUN; orchestrator owns cross-platform comparison'})
finish(s,'completed' if m.get('exit')==0 and not m.get('time_box_stop') and tsv.exists() and not errors else 'completed_with_findings')
