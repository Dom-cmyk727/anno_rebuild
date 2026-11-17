from common import *
from harness import *
s=stage(2,2*3600);env=environment({'ANNO_TEST_MB_PER_THREAD':None,'ASAN_OPTIONS':None,'UBSAN_OPTIONS':None})
m=run(s,'stage2-release-configure',['cmake','--preset','release',*DEPS],env)
if m.get('exit')==0:
 m=run(s,'stage2-release-build',['cmake','--build','--preset','release','--parallel','8'],env)
 if m.get('exit')==0:
  inv=get_inventory('build/release',env);save(OUT/'stage2-release-inventory.json',inv);binaries={}
  for test in inv['tests']:
   cmd=test.get('command',[])
   if any(a.startswith('--test-case') for a in cmd):binaries.setdefault(cmd[0],test)
  contract=next(t for t in inv['tests'] if t['name']=='picture_matrix_contract');records=[]
  for i,(binary,test) in enumerate(binaries.items()):
   props={p['name']:p['value'] for p in test.get('properties',[])};cwd=props.get('WORKING_DIRECTORY',str(Path(binary).parent))
   for mode in ('default','1','2','3'):
    name=f'whole_{i:02d}_{Path(binary).name}_{mode}';destination=OUT/'stage2-children'/name
    records.append({'name':name,'command':['python3','-I',str(OUT/'whole_binary.py'),'--binary',binary,'--directory',cwd,'--output',str(destination),'--mode',mode],'timeout':7200,'cwd':cwd})
  build,cmd=prepare_harness('stage2-registry',records,contract);m=run(s,'stage2-registry-configure',cmd,env)
  if m.get('exit')==0:gate(s,'stage2-whole-01',build,['--select','^whole_','--asset-policy','required','--allow-skip','gpu','--allow-skip','other','--jobs','1','--timeout','7200'],env)
s['status']='awaiting_child_outcome_audit';save(OUT/'stage2.json',s)
