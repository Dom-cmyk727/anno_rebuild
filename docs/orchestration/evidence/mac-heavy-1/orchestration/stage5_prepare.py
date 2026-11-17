from common import *
from harness import *
s=stage(5,3*3600);env=environment({'ANNO_TEST_MB_PER_THREAD':None,'ASAN_OPTIONS':None,'UBSAN_OPTIONS':None})
scenes=sorted(Path(GAME,'Scenes').rglob('*.sze'));save(OUT/'stage5-scenes.json',[str(p.relative_to(Path(GAME)/'Scenes')) for p in scenes])
fixture_paths=sorted(set(scenes+[p for p in Path(GAME,'Scenes/Islands').rglob('*') if p.is_file()]+[Path(GAME,'Anlagen.dat'),Path(GAME,'FigurenHeader.dat'),Path(GAME,'AnnoWelt.dll'),Path(GAME,'Data/BGruppen.dat'),Path(GAME,'Texte.dat')]))
fixture_rows=[{'path':str(p.relative_to(GAME)),'bytes':p.stat().st_size,'sha256':sha(p)} for p in fixture_paths]
save(OUT/'stage5-game-input-manifest.json',{'recorded':stamp(),'scope':'Observed at Stage 5 boundary, not a retroactive Stage 0 snapshot. Game tree was treated read-only throughout.','files':fixture_rows})
for preset in ('release','asan'):
 e=env if preset=='release' else environment({'ANNO_TEST_MB_PER_THREAD':'2048','ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'print_stacktrace=1:halt_on_error=1'})
 run(s,f'stage5-{preset}-build-cli',['cmake','--build','build/'+preset,'--target','anno-sim-replay','--parallel','8'],e)
s['status']='awaiting_population_execution';save(OUT/'stage5.json',s)
