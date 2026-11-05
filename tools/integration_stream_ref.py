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

SOURCE=b'[OBJECT_DEFINE]\n[AREA_DEFINE]\n[PLAYERMAPPER_DEFINE]\n"ME" 0\n[SCRIPT_KERNEL]\nINITBLOCK(){ MONEY_ADD("ME",7); }\n[SIGNALS]\n[CAMARAPOS]\n0,0\n'

def script():
    return (text(SOURCE)+pack('5I',1048576,65536,128,100000,4096)+b'\1'+text(b'authored/joint')
            +u32(2)+bytes([3,0])+pack('4I',0,17,0,23)+text(bytes([1,0,1]))+u64(1)+u32(0))

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
    return b+b'\0' # no resident job

def records():
    spec=importlib.util.spec_from_file_location('published128',ROOT/'fixtures/synthetic/sim/integrate1/legacy/published128/tools/sim_stream.py')
    old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old)
    figures={37:figure(1,1503,ship=True),2:figure(2,717,1,0),7:figure(3,718,2),8:figure(4,106,animal=True)}
    prefix=pack('qI',8,9)+u64(40)
    tail=u16(19)+u64(0)
    raw=prefix+b''.join(b'\1'+figures[i] if i in figures else b'\0' for i in range(40))+tail
    hierarchical=prefix+b''.join(u64(fnv(b'\1'+figures[i] if i in figures else b'\0')) for i in range(40))+tail
    return {'header':(u32(131)+b'SIMINT1\0'+u32(5),None),
            'player':(old.player_bytes(old.new_player(0))+u64(2)+pack('4I',70001,0,90001,257),None),
            'figures':(raw,fnv(hierarchical)), 'script':(script(),None)}

def fixture():
    return '# Independent combined131 structural scopes: name hex hierarchical_fnv64\n'+''.join(
        f'{name} {data.hex()} {(fnv(data) if digest is None else digest):016x}\n'
        for name,(data,digest) in records().items())

class Contracts(unittest.TestCase):
    def test_identity_literal(self):self.assertEqual(records()['header'][0].hex(),'8300000053494d494e54310005000000')
    def test_sparse_registry_has_four_actors(self):
        self.assertEqual(len(records()['figures'][0]),20+40+200+133+129+150+10)
        self.assertNotEqual(records()['figures'][1],fnv(records()['figures'][0]))
    def test_wrong_seams_distinguishable(self):
        a=figure(4,106,animal=True)
        self.assertNotEqual(a,figure(4,106))
        self.assertNotEqual(figure(2,717,1,0),figure(2,717,2,0))
        self.assertNotEqual(script(),script().replace(b'authored/joint',b'authored/other'))
        self.assertNotEqual(fnv(pack('4I',70001,0,90001,257)),fnv(pack('4I',70001,0,90001,1)))

def main():
    p=argparse.ArgumentParser();p.add_argument('--write',action='store_true');p.add_argument('--self-test',action='store_true');a=p.parse_args()
    if a.self_test:return 0 if unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(Contracts)).wasSuccessful() else 1
    path=ROOT/'fixtures/synthetic/sim/integrate1/populated_fields.txt'
    if a.write:path.write_text(fixture());return 0
    if path.read_text()!=fixture():print('combined oracle differs');return 1
    print('4 populated combined scopes agree');return 0
if __name__=='__main__':raise SystemExit(main())
