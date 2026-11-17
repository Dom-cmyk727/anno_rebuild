from common import *
import sys
number=int(sys.argv[1]);assert number in (1,2,3)
state=OUT/'stage3.json'
if number==1:
 if state.exists():raise RuntimeError('Stage 3 already started; preserve its evidence')
 s=stage(3,3*3600)
else:s=json.loads(state.read_text())
name=f'stage3-full-{number:02d}'
if any(m['name']==name for m in s['commands']):raise RuntimeError('Round already recorded; do not rerun the full gate')
env=environment({'ANNO_TEST_MB_PER_THREAD':None,'ASAN_OPTIONS':None,'UBSAN_OPTIONS':None})
cost=ROOT/'build/release/Testing/Temporary/CTestCostData.txt'
if cost.exists():shutil.copyfile(cost,OUT/f'stage3-cost-before-{number:02d}.txt')
m=gate(s,name,'build/release',['--select','.','--asset-policy','required','--allow-skip','gpu','--allow-skip','other'],env)
s['status']='awaiting_round_audit';s['last_round']=number;save(state,s)
