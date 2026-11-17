from common import *
def q(s):
 s=str(s)
 if ']==]' in s:raise ValueError('unsafe CMake bracket string')
 return '[==['+s+']==]'
def get_inventory(build,env):
 p=subprocess.run(['ctest','--test-dir',str(ROOT/build),'--show-only=json-v1'],cwd=ROOT,env=env,capture_output=True,text=True,check=True);return json.loads(p.stdout)
def prepare_harness(name,records,contract):
 directory=OUT/name;directory.mkdir(exist_ok=True);hook=directory/'register.cmake'
 lines=['# Private execution registry: binds existing commands; no production test/source edit.','if(CMAKE_SOURCE_DIR STREQUAL '+q(ROOT)+')','enable_testing()']
 for record in [contract,*records]:
  lines+=['add_test(NAME '+q(record['name'])+' COMMAND '+' '.join(q(a) for a in record['command'])+')']
  props=record.get('properties',[]);vals={p['name']:p['value'] for p in props};cwd=record.get('cwd',vals.get('WORKING_DIRECTORY',str(ROOT)))
  timeout=record.get('timeout',vals.get('TIMEOUT',7200))
  lines+=['set_tests_properties('+q(record['name'])+' PROPERTIES WORKING_DIRECTORY '+q(cwd)+' TIMEOUT '+str(timeout)+')']
 lines+=['endif()'];hook.write_text('\n'.join(lines)+'\n');save(directory/'commands.json',{'actual_source':str(ROOT),'records':records,'contract':contract,'config_only':True,'tested_executables':'Previously built real Release/ASAN outputs; no targets built in this registry.'})
 build=directory/'build'
 cmd=['cmake','-S',str(ROOT),'-B',str(build),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DANNO_BUILD_TESTS=OFF','-DANNO_BUILD_VIEWER=OFF','-DANNO_BUILD_GPU=OFF','-DCMAKE_PROJECT_INCLUDE='+str(hook)]
 return str(build.relative_to(ROOT)),cmd
