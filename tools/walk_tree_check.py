"""U-1 helper: walk code, mask and layer-2 admission/cost for tree tiles (reads Anlagen.dat and AnnoWelt.dll tables).
Rules: code from HEAD[0x2f],[0x30] (docs/sim/state.md 1448, AnnoGame 0x100091f0); mask = WALKMASK 3x3 sample;
entry test for neighbour n: (mapper[mask9] & geom) != 0 and (L[code] & key) != 0 (state.md 5.9 clause 4)."""
import sys,struct,collections
sys.path.insert(0,'/Users/dom/Dev/anno1503-worktrees/anno1503-worktrees/anno1503-MAP-2/tools')
import figuren as F, annowelt_tables as T
G='/Users/dom/Dev/anno1503-worktrees/anno1503/game/app/'
tb=T.read_walk_tables(open(G+'AnnoWelt.dll','rb').read())
buf=open(G+'Anlagen.dat','rb').read()
top=F.records(buf,16,16+struct.unpack_from('<I',buf,12)[0]); anl=next(p for n,p in top if n=='ANLAGEN')
ents={}
def walk(a):
    for n,p in F.records(a,0,len(a)):
        if n=='ENTRY': e=F.entry(p); ents[e['id']]=dict(e['records'])
        elif n in('ADDON','CLASSIC'): walk(p)
walk(anl)
def code(h):
    f,s=h[0x2f],h[0x30]
    if f==0 and s==0: return 4
    if f<8: return f
    return ((f-8)|0x80)|[0x50,0x48,0x58,0x78,0x10][s]  # shape<=4
def s(r,k): return r[k].split(b'\0')[0].decode('latin1')
L=tb['layers'][2]; M=tb['mapper']
GEOM={'up':0x2111,'down':0x8444,'left':0x4888,'right':0x1222}
KEY ={'up':0x40,'down':0x04,'left':0x01,'right':0x10}   # admission bit tested for a step in that direction (clause 2)
def mask9(r):
    w=struct.unpack_from('<3I',r['WALKMASK'],0)   # one row of 3 sub-cells per u32 row (1x1 footprint: rows 0..2)
    return w
groups=collections.defaultdict(list)
for i,r in ents.items():
    if s(r,'PRODTYPE') in('ROHSTOFF','ROHSTWACHS','ROHSTDUERR') and (s(r,'WARE')=='BAUM' or (i+1 in ents and s(ents[i+1],'WARE')=='BAUM' and s(r,'PRODTYPE')=='ROHSTWACHS')):
        h=r['HEAD']; wm=r['WALKMASK']; rows=struct.unpack_from('<%dI'%(len(wm)//4),wm,0)
        groups[(h[0x2f],h[0x30],code(h),rows[:3])].append(i)
for (f,sn,c,rows),ids in sorted(groups.items()):
    m9=(rows[0]&7)|((rows[1]&7)<<3)|((rows[2]&7)<<6)
    low=M[m9]
    print('HEAD[2f,30]=(%d,%d) code=%d rows=%s mask9=0x%03x mapper=0x%04x  ids=%s'%(f,sn,c,rows,m9,low,ids[:6]+(['...%d'%len(ids)] if len(ids)>6 else [])))
    print('   L2 adm[code]=0x%02x  cost[code][0..7]=%s'%(L[c],list(L[0x400+8*c:0x400+8*c+8])))
    for d in GEOM:
        print('   enter from %-5s step: geom&low=%s adm&key=%s'%(d,bool(low&GEOM[d]),bool(L[c]&KEY[d])))
# controls: empty land code 2 mask 0
print('control empty land code 2 mask0: adm=0x%02x low=0x%04x'%(L[2],M[0]), {d:(bool(M[0]&GEOM[d]),bool(L[2]&KEY[d])) for d in GEOM})
print('--- mature (WARE BAUM) ids and their young predecessors, by (code, mask9):')
mat=[i for i,r in ents.items() if s(r,'WARE')=='BAUM' and s(r,'PRODTYPE')=='ROHSTOFF']
by=collections.defaultdict(list)
for i in sorted(mat):
    for j in (i-1,i):
        r=ents[j]; h=r['HEAD']; rows=struct.unpack_from('<3I',r['WALKMASK'],0)
        by[(code(h),(rows[0]&7)|((rows[1]&7)<<3)|((rows[2]&7)<<6))].append(j)
for k,v in sorted(by.items()): print(k,len(v),sorted(set(v)))
bad=[i for i in mat if s(ents[i-1],'PRODTYPE')!='ROHSTWACHS']
print('mature ids whose id-1 is not ROHSTWACHS:',bad)
print('all id-1 exist:', all((i-1) in ents for i in mat))
