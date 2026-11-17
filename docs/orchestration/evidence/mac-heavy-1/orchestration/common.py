import datetime,hashlib,json,os,signal,subprocess,time,shutil
from pathlib import Path
from zoneinfo import ZoneInfo
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'cache/mac-heavy-1'
PIN='4dfdd9b7ce51b9ad04970da0b247294345f05ac7'
TREE='e218b8f5dae9e7e133e8f803618ba6e14d355286'
TOOLBIN='/Users/dom/Dev/anno1503-worktrees/anno1503/cache/mac-tools/venv/bin'
GAME='/Users/dom/Dev/anno1503-worktrees/anno1503/game/app'
def stamp():return datetime.datetime.now(ZoneInfo('Europe/Berlin')).isoformat()
def save(path,value):
 path=Path(path);tmp=Path(str(path)+'.tmp');tmp.write_text(json.dumps(value,indent=2)+'\n');tmp.replace(path)
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def freeze():
 h=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip();t=subprocess.check_output(['git','rev-parse','HEAD^{tree}'],cwd=ROOT,text=True).strip();status=subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True)
 if h!=PIN or t!=TREE or status:raise RuntimeError('Frozen source changed: '+h+' '+t+' '+status)
 return {'head':h,'tree':t,'status':status}
def environment(extra=None):
 env=os.environ.copy();env['PATH']=TOOLBIN+':'+env['PATH'];env['ANNO1503_GAME_DIR']=GAME;env['ANNO_TEST_THREADS']='4'
 if extra:
  for k,v in extra.items():
   if v is None:env.pop(k,None)
   else:env[k]=str(v)
 return env
def env_record(env):return {k:v for k,v in sorted(env.items()) if k.startswith('ANNO') or k in ('ASAN_OPTIONS','UBSAN_OPTIONS')}
def stage(number,box):
 freeze();s={'stage':number,'started':stamp(),'started_unix':time.time(),'deadline_unix':time.time()+box,'box_seconds':box,'source':freeze(),'commands':[],'status':'running'};save(OUT/f'stage{number}.json',s);return s
def finish(s,status='terminal'):
 s.update(status=status,finished=stamp(),wall_seconds=time.time()-s['started_unix'],source_after=freeze());save(OUT/f'stage{s["stage"]}.json',s)
def descendants(pid):
 rows=subprocess.check_output(['ps','-axo','pid=,ppid=,comm='],text=True).splitlines();procs={}
 for row in rows:
  p=row.strip().split(None,2)
  if len(p)==3:procs[int(p[0])]=(int(p[1]),p[2])
 owned={pid};again=True
 while again:
  again=False
  for child,(parent,cmd) in procs.items():
   if parent in owned and child not in owned and 'anno_budget_coord' not in cmd:owned.add(child);again=True
 return {p:procs[p][1] for p in owned if p in procs and 'anno_budget_coord' not in procs[p][1]}
def stop_owned(p):
 owned=descendants(p.pid);ctests=[pid for pid,cmd in owned.items() if Path(cmd).name=='ctest']
 for pid in ctests:
  try:os.kill(pid,signal.SIGINT)
  except ProcessLookupError:pass
 if ctests:
  try:p.wait(timeout=20);return {'ctest_sigint':ctests}
  except subprocess.TimeoutExpired:pass
 owned=descendants(p.pid)
 for pid in reversed(list(owned)):
  try:os.kill(pid,signal.SIGTERM)
  except ProcessLookupError:pass
 try:p.wait(timeout=10)
 except subprocess.TimeoutExpired:
  for pid in descendants(p.pid):
   try:os.kill(pid,signal.SIGKILL)
   except ProcessLookupError:pass
  p.wait()
 return {'terminated_owned':owned,'coordinators_excluded':True}
def run(s,name,cmd,env=None,limit=None):
 freeze();env=env or environment();path=OUT/(name+'.console.log');start=time.time();end=min(s['deadline_unix'],start+limit if limit else s['deadline_unix'])
 if start>=end:
  m={'name':name,'command':cmd,'not_run':'stage time box exhausted','environment':env_record(env)};s['commands'].append(m);save(OUT/f'stage{s["stage"]}.json',s);return m
 m={'name':name,'command':cmd,'environment':env_record(env),'started':stamp(),'log':str(path.relative_to(ROOT)),'source_before':freeze()}
 with path.open('x') as log:
  log.write('# environment '+json.dumps(m['environment'],sort_keys=True)+'\n# command '+json.dumps(cmd)+'\n# started '+m['started']+'\n');log.flush();p=subprocess.Popen(cmd,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
  m['pid']=p.pid;save(OUT/'active-command.json',m)
  while p.poll() is None:
   if time.time()>=end:m['time_box_stop']=stop_owned(p);break
   time.sleep(1)
 m.update(exit=p.returncode,finished=stamp(),wall_seconds=time.time()-start,log_sha256=sha(path),source_after=freeze());s['commands'].append(m);save(OUT/f'stage{s["stage"]}.json',s);save(OUT/'active-command.json',{'status':'terminal',**m});print(name,m.get('exit'),round(m.get('wall_seconds',0),3),flush=True);return m
def gate(s,name,build,options,env=None):
 env=env or environment();raw=OUT/(name+'.raw.log');cmd=['python3','-I','tools/test/run_selected.py','--build-dir',build,*options,'--log',str(raw.relative_to(ROOT))];m=run(s,name,cmd,env)
 if raw.exists():
  body=raw.read_bytes();view=OUT/(name+'.log');header='# environment '+json.dumps(env_record(env),sort_keys=True)+'\n# canonical raw bytes preserved at '+str(raw.relative_to(ROOT))+'\n';view.write_bytes(header.encode()+body)
  inv=Path(str(raw)+'.inventory.json')
  if inv.exists():shutil.copyfile(inv,Path(str(view)+'.inventory.json'))
  m.update(raw_log=str(raw.relative_to(ROOT)),raw_sha256=sha(raw),log_view=str(view.relative_to(ROOT)),log_view_sha256=sha(view),receipts=[l for l in body.decode(errors='replace').splitlines() if l.startswith(('COUNTS:','VERDICT:','GATE_END '))]);save(OUT/f'stage{s["stage"]}.json',s)
 return m
DEPS=['-DFETCHCONTENT_SOURCE_DIR_DOCTEST=/Users/dom/.codex/worktrees/whale-re-1c/anno1503/build/dev/_deps/doctest-src','-DFETCHCONTENT_SOURCE_DIR_SDL3=/Users/dom/.codex/worktrees/gate-run-eb11f0c6/anno1503/build/release/_deps/sdl3-src','-DFETCHCONTENT_SOURCE_DIR_WGPU_NATIVE=/Users/dom/.codex/worktrees/gate-run-eb11f0c6/anno1503/build/release/_deps/wgpu_native-src']
