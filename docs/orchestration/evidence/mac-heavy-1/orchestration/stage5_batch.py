from common import *
from harness import *
import sys
preset=sys.argv[1];subset=len(sys.argv)>2 and sys.argv[2]=='fourth';assert preset in ('release','asan');assert not subset or preset=='asan'
s=json.loads((OUT/'stage5.json').read_text());env=environment({'ANNO_TEST_MB_PER_THREAD':None,'ASAN_OPTIONS':None,'UBSAN_OPTIONS':None})
if preset=='asan':env=environment({'ANNO_TEST_MB_PER_THREAD':'2048','ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'print_stacktrace=1:halt_on_error=1'})
scenes=json.loads((OUT/'stage5-scenes.json').read_text());indices=list(range(len(scenes)));indices=indices[3::4] if subset else indices
name=f'stage5-{preset}';assert not any(m['name']==name for m in s['commands']),'No batch rerun'
inv=json.loads((OUT/'stage2-release-inventory.json').read_text());contract=next(t for t in inv['tests'] if t['name']=='picture_matrix_contract');records=[]
for i in indices:
 for mode in ('plain','houses'):
  case=f'population_{i:03d}_{mode}';destination=OUT/'stage5-children'/preset/case
  records.append({'name':case,'command':['python3','-I',str(OUT/'population_command.py'),'--binary',str(ROOT/'build'/preset/'src/tools/sim_replay/anno-sim-replay'),'--scene',str(Path(GAME)/'Scenes'/scenes[i]),'--mode',mode,'--output',str(destination)],'cwd':str(ROOT),'timeout':10800})
build,cmd=prepare_harness(name+'-registry',records,contract);save(OUT/(name+'-selection.json'),{'preset':preset,'selection':'positions 4,8,... in sorted scenes' if subset else 'all','indices_zero_based':indices,'scenes':[scenes[i] for i in indices],'native_command_count':len(records)})
m=run(s,name+'-registry-configure',cmd,env)
if m.get('exit')==0:gate(s,name,build,['--select','^population_','--asset-policy','required','--allow-skip','gpu','--allow-skip','other','--jobs','16' if preset=='release' else '8','--timeout','10800'],env)
s['status']='awaiting_population_audit';save(OUT/'stage5.json',s)
