"""Derive the cut transitions N -> N-1 for every entry whose WARE[0] is BAUM (registry-derived, no id list).
Prints id, PRODTYPE, id-1, PRODTYPE(id-1), HEAD[0x10] of id-1 (stage count S), cuttable (PRODTYPE(id-1)==ROHSTWACHS)."""
import sys,struct
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import figuren as F
G=__import__('os').environ['ANNO1503_GAME_DIR'].rstrip('/')+'/'
buf=open(G+'Anlagen.dat','rb').read()
top=F.records(buf,16,16+struct.unpack_from('<I',buf,12)[0]); anl=next(p for n,p in top if n=='ANLAGEN')
ents={}
def walk(a):
    for n,p in F.records(a,0,len(a)):
        if n=='ENTRY': e=F.entry(p); ents[e['id']]=dict(e['records'])
        elif n in('ADDON','CLASSIC'): walk(p)
walk(anl)
s=lambda r,k: r[k].split(b'\0')[0].decode('latin1')
rows=[]
for i in sorted(ents):
    r=ents[i]
    if s(r,'WARE')!='BAUM': continue
    p=ents.get(i-1)
    pt=s(p,'PRODTYPE') if p else None
    S=p['HEAD'][0x10] if p else None
    rows.append((i,s(r,'PRODTYPE'),i-1,pt,S,pt=='ROHSTWACHS'))
print("id prod prev prevprod S(prev) cuttable")
for x in rows: print(*x)
print("count",len(rows),"cuttable",sum(1 for x in rows if x[5]),"regrowing(S>1)",sum(1 for x in rows if x[5] and x[4]>1))
print("not cuttable:",[x[0] for x in rows if not x[5]]," stump(S==1):",[x[0] for x in rows if x[5] and x[4]<=1])
