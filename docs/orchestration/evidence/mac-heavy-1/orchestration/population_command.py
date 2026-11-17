# Private execution wrapper: native stdout is data, not a rewritten oracle.
import argparse,datetime,hashlib,json,os,subprocess,sys,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--binary',required=True);p.add_argument('--scene',required=True);p.add_argument('--mode',choices=['plain','houses'],required=True);p.add_argument('--output',required=True);a=p.parse_args();out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
cmd=[a.binary,'population',a.scene,'--seconds','0,60,300,600']+(['--house-updates'] if a.mode=='houses' else [])
env={k:v for k,v in sorted(os.environ.items()) if k.startswith('ANNO') or k in ('ASAN_OPTIONS','UBSAN_OPTIONS')};start=time.monotonic()
with (out/'stdout.txt').open('xb') as stdout,(out/'stderr.txt').open('xb') as stderr:
 r=subprocess.run(cmd,stdout=stdout,stderr=stderr)
wall=time.monotonic()-start;stdout=(out/'stdout.txt').read_bytes();stderr=(out/'stderr.txt').read_bytes();text=stdout.decode(errors='replace');err=stderr.decode(errors='replace')
refused=text.startswith(('refused:','create refused:'));problems=[l for l in text.splitlines() if 'check_invariants=' in l and not l.endswith('check_invariants=clean')]
meta={'command':cmd,'environment':env,'exit':r.returncode,'wall_seconds':wall,'load_refused':refused,'invariant_problems':problems,'stdout_sha256':hashlib.sha256(stdout).hexdigest(),'stderr_sha256':hashlib.sha256(stderr).hexdigest(),'stdout_bytes':len(stdout),'stderr_bytes':len(stderr),'binary_sha256':hashlib.sha256(Path(a.binary).read_bytes()).hexdigest()}
(out/'result.json').write_text(json.dumps(meta,indent=2)+'\n')
with (out/'command.log').open('x') as f:f.write('# environment '+json.dumps(env,sort_keys=True)+'\n# command '+json.dumps(cmd)+'\n'+text+'\n# stderr\n'+err+'\n# result '+json.dumps(meta)+'\n')
print('POPULATION_RESULT '+json.dumps(meta));print(text,end='');print(err,end='',file=sys.stderr)
sys.exit(r.returncode if r.returncode>=0 else 128-r.returncode)
