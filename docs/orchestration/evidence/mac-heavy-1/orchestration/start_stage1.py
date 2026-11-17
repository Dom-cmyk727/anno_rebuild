from common import *
s=stage(1,6*3600);env=environment({'ANNO_TEST_MB_PER_THREAD':'2048','UBSAN_OPTIONS':'print_stacktrace=1:halt_on_error=1','ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1'})
probe=OUT/'asan-capability.cpp';probe.write_text('int main() { return 0; }\n');binary=OUT/'asan-capability'
m=run(s,'stage1-probe-build',['/usr/bin/clang++','-O1','-g','-fsanitize=address,undefined,float-cast-overflow',str(probe),'-o',str(binary)],env,60)
if m.get('exit')==0:
 cap=run(s,'stage1-lsan-capability',[str(binary)],env,30);text=(ROOT/cap['log']).read_text(errors='replace')
 unsupported='detect_leaks is not supported' in text
 if unsupported:env['ASAN_OPTIONS']='detect_leaks=0:halt_on_error=1'
 save(OUT/'sanitizer-environment.json',{'probe_exit':cap.get('exit'),'leak_sanitizer_active':cap.get('exit')==0,'unsupported_leak_sanitizer':unsupported,'effective_environment':env_record(env),'probe_log':cap['log']})
 m=run(s,'stage1-configure',['cmake','--preset','asan',*DEPS],env)
 if m.get('exit')==0:
  m=run(s,'stage1-build',['cmake','--build','--preset','asan','--parallel','8'],env)
  if m.get('exit')==0:
   gate(s,'stage1-full-01','build/asan',['--select','.','--asset-policy','required','--allow-skip','gpu','--allow-skip','other','--jobs','8','--timeout','10800'],env)
s['status']='awaiting_refusal_and_sanitizer_audit';save(OUT/'stage1.json',s)
