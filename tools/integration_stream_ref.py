#!/usr/bin/env python3
"""Independent populated SIMINT1 field-order/hash oracle; no C++ output is read.

The record combines native header, two raw permission rows, a sparse actor
registry (ship37 -> crew2 -> donkey7, animal8), and a nontrivial script runtime.
Each scope's expected bytes and hierarchical digest are independently encoded.
It complements the independent whole-world scheduler goldens in sim_stream.py;
this structural fixture is not an original scenario or a complete playable world.
"""
import argparse
import importlib.util
from pathlib import Path
import struct
import unittest
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parent.parent
M=(1<<64)-1

def pack(fmt,*v):return struct.pack('<'+fmt,*v)
def u8(v):return pack('B',v)
def u16(v):return pack('H',v)
def u32(v):return pack('I',v)
def u64(v):return pack('Q',v)
def text(v):return u64(len(v))+v

def fnv(data):
    h=0xcbf29ce484222325
    for b in data:h=((h^b)*0x100000001b3)&M
    return h

SOURCE=b'[OBJECT_DEFINE]\n[AREA_DEFINE]\n[PLAYERMAPPER_DEFINE]\n"ME" 0\n[SCRIPT_KERNEL]\nSET X = TRUE;\nSET Y = FALSE;\nSET Z = TRUE;\nINITBLOCK(){ MONEY_ADD("ME",7); }\n[SIGNALS]\n[CAMARAPOS]\n0,0\n'

def script():
    return (text(SOURCE)+pack('5I',1048576,65536,128,100000,4096)+b'\1'+text(b'authored/joint')
            +u32(2)+bytes([3,0])+pack('4I',0,17,0,23)+text(bytes([0,1,0,1]))+u64(1)+u32(0))

def figure(serial,kind,parent=M,crew=None,animal=False,ship=False):
    # TransportState defaults, including invalid BuildingId target.
    transport=b'\0'+u32(0)+u64(0)+pack('2I',0,0)+u64(M)+u32(0)
    home=1 if animal else M
    job=0x2e if animal else 0
    x,y=(28416,28416) if animal else (900,980)
    hp=12000 if ship else (400 if kind==718 else 350)
    b=(u64(serial)+pack('2I',kind,0)+u64(home)+pack('2I',1 if animal else 0,job)
       +pack('2i',x,y)+pack('4I',69 if animal else 0,256 if animal else 0,hp,0)
       +u16(19)+transport+b'\0'+u8(4)+u64(parent)
       +(b'\0' if crew is None else b'\1'+u32(crew)))
    holds=[(41,12801),(42,512)] if ship else ([(26,7)] if parent!=M else [])
    b+=u64(len(holds))+b''.join(u32(g)+u64(a) for g,a in holds)+u32(0x8000 if ship else 0)
    if ship:
        # Idle nav, known health and fine position matching Q8; residues deliberately nonzero.
        b+=b'\1'+b'\0\0'+bytes([0,0])+pack('2I',100,0)+u64(0)+u64(0)+u16(11)
        b+=pack('2q',900*(500<<24),980*(500<<24))+pack('2I',5,7)+b'\1'+u32(12000)
    else:b+=b'\0'
    if animal:
        b+=b'\1'+bytes([2])+pack('2I',0,4321)+pack('2i',113,111)+u64(0)+pack('2I',0,0)
    else:b+=b'\0'
    return b+b'\0'+u64(0)+b'\0' # no resident job, generation0/no route

def records():
    spec=importlib.util.spec_from_file_location('published128',ROOT/'fixtures/synthetic/sim/integrate1/legacy/published128/tools/sim_stream.py')
    old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old)
    figures={37:figure(1,1503,ship=True),2:figure(2,717,1,0),7:figure(3,718,2),8:figure(4,106,animal=True)}
    prefix=pack('qI',8,9)+u64(40)
    tail=u16(19)+u64(0)
    raw=prefix+b''.join(b'\1'+figures[i] if i in figures else b'\0' for i in range(40))+tail
    hierarchical=prefix+b''.join(u64(fnv(b'\1'+figures[i] if i in figures else b'\0')) for i in range(40))+tail
    # Independent configured price-registry stream: no types, three named goods,
    # existing25 class facts, then empty figure/group/stage tables. Present price
    # vectors follow id_sec, each count-prefixed. No C++ digest is read.
    module=importlib.util.spec_from_file_location('current_stream',ROOT/'tools/sim_stream.py')
    current=importlib.util.module_from_spec(module);module.loader.exec_module(current)
    goods=u64(0)+u64(3)
    for name,climates,taxes in ((b'UNUSED',None,None),
            (b'ALPHA',[45,45,45,50,50,50,45,17],[0,5,10,15]),
            (b'BETA',[7]*8,[2]*4)):
        goods+=text(name)+pack('3I',0,0,0)
        goods+=b'\0' if climates is None else b'\1'+u64(len(climates))+pack('8I',*climates)+u64(len(taxes))+pack('4I',*taxes)
    goods+=u64(25)+b''.join(pack('3BI',*row) for row in current.CLASSES)+u64(0)*3
    names=(b'POLAR',b'TUNDRA',b'NORD',b'PRAERIE',b'DSCHUNGEL',b'STEPPE',b'WUESTE',b'CUSTOM')
    economy=u64(8)+b''.join(text(n) for n in names)+u32(4)+u32(2000)
    return {'header':(u32(137)+b'ECONBK1\0'+u32(5),None),
            'player':(old.player_bytes(old.new_player(0))+u64(2)+pack('4I',70001,0,90001,255),None),
            'figures':(raw,fnv(hierarchical)), 'script':(script(),None),
            'accounts':(u16(65000)+pack('5Q',0x0102030405060708,105,210,4294967313,28),None),
            'economy':(economy,None),'goods_registry':(goods,None)}

def fixture():
    return '# Independent ECONBK1 structural scopes: name hex hierarchical_fnv64\n'+''.join(
        f'{name} {data.hex()} {(fnv(data) if digest is None else digest):016x}\n'
        for name,(data,digest) in records().items())

class Contracts(unittest.TestCase):
    def test_identity_literal(self):self.assertEqual(records()['header'][0].hex(),'8900000045434f4e424b310005000000')
    def test_sparse_registry_has_four_actors(self):
        self.assertEqual(len(records()['figures'][0]),20+40+209+142+138+159+10)
        self.assertNotEqual(records()['figures'][1],fnv(records()['figures'][0]))
    def test_populated_accounts_literal_and_widened_cost(self):
        self.assertEqual(records()['accounts'][0].hex(),
            'e8fd08070605040302016900000000000000d20000000000000011000000010000001c00000000000000')
        self.assertEqual(len(records()['accounts'][0]),42)
        self.assertEqual(records()['economy'][0][-8:].hex(),'04000000d0070000')
    def test_public_command_rejects_changed_or_missing_fixture(self):
        with tempfile.TemporaryDirectory() as td:
            path=Path(td)/'fields.txt';path.write_text(fixture())
            command=[sys.executable,'-I',str(Path(__file__).resolve()),'--fixture',str(path)]
            result=subprocess.run(command,capture_output=True,text=True)
            self.assertEqual(result.returncode,0);self.assertIn('7 populated combined scopes agree',result.stdout)
            path.write_text(fixture().replace('89000000','88000000',1))
            result=subprocess.run(command,capture_output=True,text=True)
            self.assertEqual(result.returncode,1);self.assertIn('combined oracle differs',result.stdout)
            path.unlink();result=subprocess.run(command,capture_output=True,text=True)
            self.assertEqual(result.returncode,1);self.assertIn('fixture unavailable',result.stdout)

    def test_populated_wrong_readings_reject_even_with_consistent_digest(self):
        for name in ('accounts','economy','goods_registry'):
            with self.subTest(record=name),tempfile.TemporaryDirectory() as td:
                original=records()[name][0];wrong=bytearray(original)
                if name=='accounts':
                    wrong[2:10],wrong[10:18]=wrong[10:18],wrong[2:10]
                elif name=='economy':wrong[-8]=3 # wrong tax count, no marker change
                else:wrong[original.index(pack('I',45))]=44 # wrong climate price
                row=f'{name} {original.hex()} {fnv(original):016x}'
                changed=f'{name} {wrong.hex()} {fnv(wrong):016x}'
                path=Path(td)/'fields.txt';path.write_text(fixture().replace(row,changed))
                result=subprocess.run([sys.executable,'-I',str(Path(__file__).resolve()),
                    '--fixture',str(path)],capture_output=True,text=True)
                self.assertEqual(result.returncode,1);self.assertIn('combined oracle differs',result.stdout)

    def test_wrong_seams_distinguishable(self):
        a=figure(4,106,animal=True)
        self.assertNotEqual(a,figure(4,106))
        self.assertNotEqual(figure(2,717,1,0),figure(2,717,2,0))
        self.assertNotEqual(script(),script().replace(b'authored/joint',b'authored/other'))
        self.assertNotEqual(fnv(pack('4I',70001,0,90001,255)),fnv(pack('4I',70001,0,90001,1)))

def main():
    p=argparse.ArgumentParser();p.add_argument('--write',action='store_true');p.add_argument('--self-test',action='store_true');p.add_argument('--fixture',type=Path);a=p.parse_args()
    if a.self_test:return 0 if unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(Contracts)).wasSuccessful() else 1
    path=a.fixture or ROOT/'fixtures/synthetic/sim/iron2/current137/populated_fields.txt'
    if a.write:path.write_text(fixture());return 0
    try: actual=path.read_text()
    except OSError as error: print('fixture unavailable: '+str(error));return 1
    if actual!=fixture():print('combined oracle differs');return 1
    print('7 populated combined scopes agree');return 0
if __name__=='__main__':raise SystemExit(main())
