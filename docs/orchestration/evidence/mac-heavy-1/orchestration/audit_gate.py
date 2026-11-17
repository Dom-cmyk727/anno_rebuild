# Private evidence reader. Authoritative verdict rows remain those of the canonical runner.
import argparse,hashlib,importlib.util,json,re,shutil,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('cv',ROOT/'tools/test/ctest_verdict.py');cv=importlib.util.module_from_spec(spec);spec.loader.exec_module(cv)
p=argparse.ArgumentParser();p.add_argument('raw');p.add_argument('output');args=p.parse_args();raw=Path(args.raw);dest=Path(args.output);tests={};receipts=[]
with raw.open(errors='replace') as f:
 for original in f:
  line=original.rstrip('\n')
  if line.startswith(('COUNTS:','VERDICT:','GATE_END ')):receipts.append(line)
  m=cv.RESULT.match(line)
  if m:tests.setdefault(m[1],{'body':[]}).update(name=m[2],status=m[4],terminal_line=line);continue
  m=cv.START.match(line)
  if m:tests.setdefault(m[1],{'body':[]})['name']=m[2];continue
  m=cv.PREFIX.match(line)
  if m:tests.setdefault(m[1],{'body':[]})['body'].append(m[2])
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
 return h.hexdigest()
def events(body,name,origin):
 found=[]
 for i,line in enumerate(body):
  if not any(k in line for k in ('ERROR: AddressSanitizer:','ERROR: LeakSanitizer:','runtime error:','AddressSanitizer:DEADLYSIGNAL')):continue
  if 'DEADLYSIGNAL' in line and found and found[-1]['first_line']==line:continue
  kind='UBSan' if 'runtime error:' in line else ('LeakSanitizer' if 'LeakSanitizer' in line else 'AddressSanitizer');frames=[]
  for later in body[i+1:]:
   if re.match(r'\s*#\d+\s',later):frames.append(later)
   elif frames:break
   if len(frames)==8:break
  found.append({'test':name,'kind':kind,'first_line':line,'first_8_frames':frames,'origin':origin})
 return found
rows=[];reports=[];refused=[];retries=[];coordinators=set()
artifacts=dest.parent/(dest.stem+'-artifacts')
for number,t in tests.items():
 name=t.get('name','<unknown '+number+'>');body=t['body'];skip=[];cases=None
 for line in body:
  m=cv.DOCTEST.search(line)
  if m:cases=int(m[1])
  if '[anno-test memory] coordinator build=' in line:coordinators.add(line)
  text=cv.message_text(line)
  if text is None:
   plain=(line[3:] if line.startswith('-- ') else line).strip()
   if re.match(r'skipp(ing|ed)\b',plain,re.I) or (cv.REQUIRED.search(plain) and cv.SKIP_WORD.search(plain)):text=plain
  if text is not None:
   kind=cv.classify(text)
   if kind:skip.append({'class':kind,'reason':text})
  if '[anno-test memory] refused' in line:refused.append({'test':name,'status':t.get('status'),'first_line':line})
  if '[anno-test memory] item ' in line and 'std::bad_alloc' in line:retries.append({'test':name,'line':line})
 reports.extend(events(body,name,str(raw)))
 row={'number':number,'name':name,'status':t.get('status'),'doctest_cases':cases,'prerequisites':skip}
 if t.get('status') not in ('Passed','Skipped'):
  patterns=(r': (?:FATAL )?ERROR:',r'^FAIL line ',r'AssertionError:',r'^FAIL:',r'CMake Error',r'ERROR: AddressSanitizer:',r'runtime error:')
  first=None
  for pattern in patterns:
   first=next((l for l in body if re.search(pattern,l)),None)
   if first:break
  row.update(first_evidence=first or t.get('terminal_line','No terminal result'),tail=body[-12:])
  for line in body:
   m=re.search(r'stderr (/[\S]+/run\.stderr\.txt)',line)
   if not m:continue
   source=Path(m[1]);real=source.resolve();temp_root=Path(tempfile.gettempdir()).resolve()
   if temp_root not in real.parents or not source.parent.name.startswith('viewer_wiring_'):continue
   holder=artifacts/number;holder.mkdir(parents=True,exist_ok=True)
   row.setdefault('secondary_artifacts',[])
   for filename in ('run.stderr.txt','run.stdout.txt'):
    f=source.parent/filename
    if not f.exists():continue
    to=holder/filename
    if not to.exists():shutil.copyfile(f,to)
    elif sha(f)!=sha(to):to=holder/(sha(f)+'-'+filename);shutil.copyfile(f,to)
    row['secondary_artifacts'].append({'path':str(to),'sha256':sha(to)})
    if filename=='run.stderr.txt':reports.extend(events(to.read_text(errors='replace').splitlines(),name,str(to)))
 rows.append(row)
unique=[];seen=set()
for event in reports:
 key=(event['test'],event['kind'],event['first_line'],tuple(event['first_8_frames']))
 if key not in seen:unique.append(event);seen.add(key)
result={'raw_log':str(raw),'raw_sha256':sha(raw),'receipts':receipts,'cases':rows,'failures':[r for r in rows if r['status'] not in ('Passed','Skipped')],'memory_refusal_candidates':refused,'bad_alloc_retries':retries,'sanitizer_reports':unique,'coordinator_observations':sorted(coordinators),'reader_sha256':sha(Path(__file__))}
dest.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'output':str(dest),'cases':len(rows),'failures':[(r['name'],r['first_evidence']) for r in result['failures']],'memory_refusal_candidates':len(refused),'bad_alloc_retry_lines':len(retries),'sanitizer_reports':len(unique),'receipts':receipts},indent=2))
