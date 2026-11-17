# Private command adapter: preserve all child output, never change child predicates.
import argparse,datetime,hashlib,json,os,re,subprocess,sys,time
from pathlib import Path
from zoneinfo import ZoneInfo
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools/test'));from ctest_verdict import classify,PRIORITY
p=argparse.ArgumentParser();p.add_argument('--binary',required=True);p.add_argument('--directory',required=True);p.add_argument('--output',required=True);p.add_argument('--mode',required=True);args=p.parse_args();out=Path(args.output);out.mkdir(parents=True,exist_ok=True)
flags=[] if args.mode=='default' else ['--order-by=rand','--rand-seed='+args.mode,'--test-suite-exclude=heavy']
env={k:v for k,v in sorted(os.environ.items()) if k.startswith('ANNO') or k in ('ASAN_OPTIONS','UBSAN_OPTIONS')};header='# environment '+json.dumps(env,sort_keys=True)+'\n'
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
query_started=time.monotonic();query=[args.binary,*flags,'--list-test-cases'];listing=out/'case-order.log';started=datetime.datetime.now(ZoneInfo('Europe/Berlin')).isoformat()
with listing.open('x') as f:
 f.write(header+'# command '+json.dumps(query)+'\n');f.flush();qr=subprocess.run(query,cwd=args.directory,stdout=f,stderr=subprocess.STDOUT)
order=[];inside=False;declared=None
for line in listing.read_text(errors='replace').splitlines():
 if '[doctest] listing all test case names' in line:inside=True;continue
 if '[doctest] unskipped test cases passing the current filters:' in line:
  declared=int(line.rsplit(':',1)[1]);break
 if inside and line and not set(line)<=set('=') and not line.startswith('[doctest]'):order.append(line)
meta={'binary':args.binary,'binary_sha256':digest(Path(args.binary)),'mode':args.mode,'command':[args.binary,*flags],'environment':env,'working_directory':args.directory,'started':started,'query_exit':qr.returncode,'query_wall_seconds':time.monotonic()-query_started,'query_declared_cases':declared,'ordered_cases':order,'order_log':str(listing),'order_log_sha256':digest(listing)}
if qr.returncode or declared!=len(order) or not order:
 meta['issue']='Case listing failed, mismatched or empty';(out/'result.json').write_text(json.dumps(meta,indent=2)+'\n');print(json.dumps(meta));raise SystemExit(2)
body=out/'whole.log';native_started=time.monotonic()
with body.open('x') as f:
 f.write(header+'# command '+json.dumps(meta['command'])+'\n');f.flush();result=subprocess.run(meta['command'],cwd=args.directory,stdout=f,stderr=subprocess.STDOUT)
native_finished=time.monotonic();summary=None;assertions=None;current=None;prerequisites={};failures={}
case_re=re.compile(r'\[doctest\] test cases:\s*(\d+)\s*\|\s*(\d+) passed\s*\|\s*(\d+) failed\s*\|\s*(\d+) skipped')
assert_re=re.compile(r'\[doctest\] assertions:\s*(\d+)\s*\|\s*(\d+) passed\s*\|\s*(\d+) failed')
with body.open(errors='replace') as f:
 for raw in f:
  line=raw.rstrip('\n');s=line.lstrip()
  if s.startswith('TEST CASE:'):current=s.split('TEST CASE:',1)[1].strip()
  m=case_re.search(line)
  if m:summary={'framework_cases':int(m[1]),'framework_passed':int(m[2]),'framework_failed':int(m[3]),'filter_excluded':int(m[4])}
  m=assert_re.search(line)
  if m:assertions={'total':int(m[1]),'passed':int(m[2]),'failed':int(m[3])}
  if 'MESSAGE:' in line:
   message=line.split('MESSAGE:',1)[1].strip();kind=classify(message)
   if kind:prerequisites.setdefault(current or '<unknown case>',[]).append({'class':kind,'message':message})
  if re.search(r': (?:FATAL )?ERROR:',line):failures.setdefault(current or '<unknown case>',line)
meta.update(native_exit=result.returncode,wall_seconds=native_finished-native_started,finished=datetime.datetime.now(ZoneInfo('Europe/Berlin')).isoformat(),summary=summary,assertions=assertions,prerequisite_cases=[{'name':n,'observations':v} for n,v in prerequisites.items()],first_failures=[{'name':n,'first_line':v,'preceding_cases':order[:order.index(n)] if n in order else None} for n,v in failures.items()],body_log=str(body),body_log_sha256=digest(body))
code=result.returncode if result.returncode>=0 else 128-result.returncode
if summary is None or summary['framework_cases']==0:meta['issue']='Missing or empty doctest completion';code=code or 2
elif summary['framework_failed']>0:code=code or 1
meta['adapter_exit']=code;(out/'result.json').write_text(json.dumps(meta,indent=2)+'\n')
# This CTest entry is the whole-process operation. Nested prerequisite cases are not
# concealed: complete raw output and classified nested rows remain above and in result.json.
print('WHOLE_BINARY_RESULT '+json.dumps({'binary':args.binary,'mode':args.mode,'native_exit':result.returncode,'adapter_exit':code,'summary':summary,'assertions':assertions,'prerequisite_case_count':len(prerequisites),'raw_log':str(body),'raw_log_sha256':meta['body_log_sha256']}))
for failure in meta['first_failures']:print('CHILD_FAILURE '+json.dumps(failure))
sys.exit(code)
