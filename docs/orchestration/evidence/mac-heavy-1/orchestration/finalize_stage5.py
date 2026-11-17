from common import *
from collections import Counter
scenes=json.loads((OUT/'stage5-scenes.json').read_text());groups={};all_reports=[];differences=[]
def classify(r):
 if r['load_refused']:return 'load_refused'
 if r['exit']==0:return 'exit_0'
 if r['exit']==1:return 'exit_1_invariant' if r['invariant_problems'] else 'exit_1_other'
 return 'exit_other'
def first_diff(a,b):
 aa=a.decode(errors='replace').splitlines();bb=b.decode(errors='replace').splitlines()
 for i in range(max(len(aa),len(bb))):
  av=aa[i] if i<len(aa) else '<EOF>';bv=bb[i] if i<len(bb) else '<EOF>'
  if av!=bv:return {'line':i+1,'release':av,'asan':bv}
 return {'issue':'bytes differ without a decoded line difference','release_bytes':len(a),'asan_bytes':len(b)}
for preset in ('release','asan'):
 selection=OUT/f'stage5-{preset}-selection.json'
 if not selection.exists():continue
 indices=json.loads(selection.read_text())['indices_zero_based'];rows=[]
 for i in indices:
  for mode in ('plain','houses'):
   name=f'population_{i:03d}_{mode}';d=OUT/'stage5-children'/preset/name;p=d/'result.json'
   if not p.exists():rows.append({'scene':scenes[i],'mode':mode,'class':'not_run_or_incomplete','operation':name,'result_present':False});continue
   r=json.loads(p.read_text());native_stdout=(d/'stdout.txt').read_text(errors='replace');refusal=next((l for l in native_stdout.splitlines() if l.startswith(('refused:','create refused:'))),None);r['wrapper_load_refused_initial']=r['load_refused'];r['load_refused']=refusal is not None;c=classify(r);r.update(scene=scenes[i],mode=mode,operation=name,result_present=True);r['class']=c
   stdout=(d/'stdout.txt').read_text(errors='replace');stderr=(d/'stderr.txt').read_text(errors='replace')
   r['refusal_line']=next((l for l in stdout.splitlines() if l.startswith(('refused:','create refused:'))),None)
   r['native_signal']=-r['exit'] if r['exit']<0 else None
   r['stderr_first_lines']=stderr.splitlines()[:10] if r['exit']!=0 else []
   r['checkpoint_lines']=[l for l in stdout.splitlines() if l.startswith('t=')]
   r['checkpoint_completeness']=len(r['checkpoint_lines'])==4 if c=='exit_0' else None;rows.append(r)
   for stream in ('stdout','stderr'):
    lines=(d/(stream+'.txt')).read_text(errors='replace').splitlines()
    for n,line in enumerate(lines):
     if not any(marker in line for marker in ('ERROR: AddressSanitizer:','runtime error:','AddressSanitizer:DEADLYSIGNAL')):continue
     frames=[]
     for more in lines[n+1:]:
      if more.lstrip().startswith('#') and more.lstrip()[1:2].isdigit():frames.append(more)
      elif frames:break
      if len(frames)==8:break
     all_reports.append({'preset':preset,'scene':scenes[i],'mode':mode,'stream':stream,'first_line':line,'first_8_frames':frames,'raw_path':str(d/(stream+'.txt'))})
 groups[preset]={'selection':json.loads(selection.read_text()),'rows':rows,'counts':{}}
 for mode in ('plain','houses'):
  sub=[r for r in rows if r['mode']==mode];groups[preset]['counts'][mode]={'selected':len(sub),'native_terminal':sum(r['result_present'] for r in sub),'raw_exit_counts':dict(Counter(str(r['exit']) for r in sub if r['result_present'])),'exclusive_class_counts':dict(Counter(r['class'] for r in sub)),'checkpoint_incomplete_exit0':[r['scene'] for r in sub if r.get('checkpoint_completeness') is False]}
if 'release' in groups:
 for mode in ('plain','houses'):
  chunks=[]
  for r in groups['release']['rows']:
   if r['mode']!=mode:continue
   chunks.append(('=== '+r['scene']+' ===\nclass='+r['class']+'\n').encode())
   if not r['result_present']:chunks.append(b'NOT_RUN_OR_INCOMPLETE\n\n');continue
   d=OUT/'stage5-children/release'/r['operation'];chunks.append(('exit='+str(r['exit'])+'\nstdout:\n').encode());chunks.append((d/'stdout.txt').read_bytes());chunks.append(b'\nstderr:\n');chunks.append((d/'stderr.txt').read_bytes());chunks.append(b'\n\n')
  (OUT/f'population-release-{mode}.txt').write_bytes(b''.join(chunks))
compared=0
if 'asan' in groups and 'release' in groups:
 release={r['operation']:r for r in groups['release']['rows']}
 for a in groups['asan']['rows']:
  r=release.get(a['operation'])
  if not r or not r['result_present'] or not a['result_present']:continue
  compared+=1;delta={}
  if r['exit']!=a['exit']:delta['exit']={'release':r['exit'],'asan':a['exit']}
  for stream in ('stdout','stderr'):
   left=(OUT/'stage5-children/release'/a['operation']/(stream+'.txt')).read_bytes();right=(OUT/'stage5-children/asan'/a['operation']/(stream+'.txt')).read_bytes()
   if left!=right:delta[stream]=first_diff(left,right)
  if delta:differences.append({'scene':a['scene'],'mode':a['mode'],'differences':delta})
save(OUT/'stage5-summary.json',{'scenes':len(scenes),'groups':groups,'byte_comparison':{'scope':'Exact native stdout, stderr and exit, per matching scene/mode; no path, timing, whitespace or coordinator normalization','compared_scene_modes':compared,'differences':differences},'sanitizer_reports':all_reports,'scope_note':'A load refusal is not an invariant run. Exit 0 establishes the existing population CLI checkpoints only, not a full state-hash comparison or an explicit world-fault assertion. Native ThreadExecutor creates up to hardware-concurrency threads during world creation; ANNO_TEST_THREADS does not control that CLI implementation.'})
print(json.dumps({'scenes':len(scenes),'counts':{k:v['counts'] for k,v in groups.items()},'compared':compared,'differences':len(differences),'sanitizer_reports':len(all_reports)},indent=2))
