"""E-level check: runs the REAL bytes of AnnoWelt.dll island slots +0x110 (0x10087340, harvest),
+0xc8/+0xc0 (0x10087400 / 0x10087880, tile flag read/write, mode 0) and +0x10c mode 0 (0x10092320,
candidate list) on a synthetic island. The island object, the type registry and the type objects are
synthetic memory with tiny stub methods (documented below); only the AnnoWelt functions are original.
This is a test harness over original code on synthetic memory, NOT a run of the game.
Read-only on the DLL.
"""
import struct, sys, pefile
from unicorn import *
from unicorn.x86_const import *

DLL = '/Users/dom/Dev/anno1503-worktrees/anno1503/game/app/AnnoWelt.dll'
pe = pefile.PE(DLL, fast_load=True)
BASE = pe.OPTIONAL_HEADER.ImageBase
SIZE = (pe.OPTIONAL_HEADER.SizeOfImage + 0xfff) & ~0xfff
IMG = pe.get_memory_mapped_image()[:SIZE]

ISL, REC, REG, TYPES, VT_ISL, VT_REG, VT_TYPE, STUBS = (0x30000000, 0x30010000, 0x30020000, 0x31000000,
                                                        0x30040000, 0x30041000, 0x30042000, 0x30050000)
STACK = 0x30100000
# stub methods (x86-32, thiscall, callee-cleans)
CODE = {
 'reg70': bytes([0x8B,0x44,0x24,0x04, 0x8B,0x04,0x85]) + struct.pack('<I', REG) + bytes([0xC2,0x04,0x00]),   # registry slot +0x70(id) -> type object
 'prod5c': bytes([0x0F,0xB7,0x41,0x10, 0xC3]),                                                               # type slot +0x5c -> u16 [type+0x10]
 'id40':   bytes([0x0F,0xB7,0x41,0x04, 0xC3]),                                                               # type slot +0x40 -> u16 [type+4]
 'ware_ac': bytes([0x8B,0x44,0x24,0x04, 0x0F,0xB6,0x84,0x01]) + struct.pack('<I', 0x206) + bytes([0xC2,0x04,0x00]), # type slot +0xac(i) -> byte [type+0x206+i]
 'ret0c':  bytes([0xC2,0x0C,0x00]),                                                                          # island slot +0x188 (redraw notify, 3 args)
 'ret0':   bytes([0x33,0xC0,0xC3]),
}
OFF = {}
cur = STUBS
for k, v in CODE.items():
    OFF[k] = cur
    cur += (len(v) + 15) & ~15

PROD = {'ROHSTOFF': 225, 'ROHSTWACHS': 226, 'PLANTAGE': 2}
BAUM, GETREIDE, UNUSED = 70, 56, 0

def build(width, height, x0, y0, tiles, types):
    """tiles: dict (x,y) -> (id, phase, dword4); types: dict id -> (prodtype name, ware goods)"""
    mu = Uc(UC_ARCH_X86, UC_MODE_32)
    mu.mem_map(BASE, SIZE); mu.mem_write(BASE, IMG)
    for a in (ISL, REC, REG, VT_ISL, STUBS, STACK - 0x10000):
        mu.mem_map(a, 0x10000)
    mu.mem_map(TYPES, 0x100000)
    for k, v in CODE.items():
        mu.mem_write(OFF[k], v)
    # vtables
    vt = bytearray(0x400); struct.pack_into('<I', vt, 0x188, OFF['ret0c']); mu.mem_write(VT_ISL, bytes(vt))
    vr = bytearray(0x400); struct.pack_into('<I', vr, 0x70, OFF['reg70']); mu.mem_write(VT_REG, bytes(vr))
    vy = bytearray(0x400); struct.pack_into('<I', vy, 0x5c, OFF['prod5c']); struct.pack_into('<I', vy, 0x40, OFF['id40'])
    struct.pack_into('<I', vy, 0xac, OFF['ware_ac']); mu.mem_write(VT_TYPE, bytes(vy))
    # registry object = ISL+0x800 (vtable ptr), pointer table = REG
    regobj = ISL + 0x2000
    mu.mem_write(regobj, struct.pack('<I', VT_REG))
    table = bytearray(0x10000)
    for tid, (pt, ware) in types.items():
        tobj = TYPES + tid * 0x400
        d = bytearray(0x400); struct.pack_into('<I', d, 0, VT_TYPE); struct.pack_into('<H', d, 4, tid)
        struct.pack_into('<H', d, 0x10, PROD[pt]); d[0x206] = ware
        mu.mem_write(tobj, bytes(d)); struct.pack_into('<I', table, 4 * tid, tobj)
    mu.mem_write(REG, bytes(table))
    isl = bytearray(0x1000)
    struct.pack_into('<I', isl, 0, VT_ISL); struct.pack_into('<HHHH', isl, 4, x0, y0, width, height)
    struct.pack_into('<I', isl, 0xc, REC); struct.pack_into('<I', isl, 0x1c, regobj)
    mu.mem_write(ISL, bytes(isl))
    rec = bytearray(width * height * 8)
    for (x, y), (tid, ph, d4) in tiles.items():
        o = ((y - y0) * width + (x - x0)) * 8
        struct.pack_into('<HBBI', rec, o, tid, 0, ph, d4)
    mu.mem_write(REC, bytes(rec))
    return mu

def call(mu, va, ecx, args):
    sp = STACK
    for a in reversed(args):
        sp -= 4; mu.mem_write(sp, struct.pack('<I', a & 0xffffffff))
    ret = 0x30090000
    mu.mem_map(ret, 0x1000) if not hasattr(mu, '_ret') else None
    mu._ret = True
    sp -= 4; mu.mem_write(sp, struct.pack('<I', ret))
    mu.reg_write(UC_X86_REG_ESP, sp); mu.reg_write(UC_X86_REG_ECX, ecx)
    def hook(uc, access, address, size, value, user):
        print('UNMAPPED', access, hex(address), 'eip', hex(uc.reg_read(UC_X86_REG_EIP)))
        return False
    mu.hook_add(UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED, hook)
    mu.emu_start(va, ret, count=200000)
    assert mu.reg_read(UC_X86_REG_EIP) == ret, hex(mu.reg_read(UC_X86_REG_EIP))
    return mu.reg_read(UC_X86_REG_EAX)

def tile(mu, x, y, x0, y0, w):
    o = ((y - y0) * w + (x - x0)) * 8
    return struct.unpack('<HBBI', bytes(mu.mem_read(REC + o, 8)))

types = {104: ('ROHSTWACHS', UNUSED), 105: ('ROHSTOFF', BAUM), 107: ('ROHSTOFF', BAUM), 258: ('ROHSTWACHS', UNUSED),
         259: ('ROHSTOFF', BAUM), 260: ('ROHSTOFF', UNUSED), 261: ('ROHSTOFF', BAUM),
         712: ('ROHSTWACHS', UNUSED), 713: ('ROHSTOFF', GETREIDE)}
ok = True
def check(name, got, exp):
    global ok
    good = got == exp
    ok &= good
    print(('PASS ' if good else 'FAIL ') + name + ('' if good else f'  got={got} expected={exp}'))

# --- harvest (0x10087340): (x, y) stack args pushed y then x? determine by the function: arg1=[esp+0x1c-..] x? use both orders
W, H, X0, Y0 = 8, 8, 100, 200
cases = [  # (id, phase, dword4) -> expected (id', phase', ret)
    ((105, 0x45, 0), (104, 0x00, 1)),
    ((105, 0xC5, 0), (104, 0x80, 1)),
    ((259, 0x13, 0), (258, 0x00, 1)),
    ((261, 0x13, 0), (261, 0x13, 0)),
    ((0, 0x00, 0), (0, 0x00, 0)),
    ((713, 0x00, 0), (712, 0x00, 1)),
    ((104, 0x07, 0), (104, 0x07, 0)),   # a young tree: id-1 = 103 has no type object here -> unchanged
]
for (tid, ph, d4), exp in cases:
    if exp is None:
        continue
    mu = build(W, H, X0, Y0, {(103, 203): (tid, ph, d4)}, types)
    r = call(mu, 0x10087340, ISL, [103, 203])     # args (x, y): thiscall, ret 8
    t = tile(mu, 103, 203, X0, Y0, W)
    check(f'harvest id={tid} phase={ph:#04x}', (t[0], t[2], r & 0xff), exp)

# --- claim bit (mode 0): set via 0x10087880(x, y, 0, 1); read via 0x10087400(x, y, 0)
mu = build(W, H, X0, Y0, {(103, 203): (105, 0, 0x01234567)}, types)
before = call(mu, 0x10087400, ISL, [103, 203, 0]) & 0xff
call(mu, 0x10087880, ISL, [103, 203, 0, 1])
after = call(mu, 0x10087400, ISL, [103, 203, 0]) & 0xff
d4 = tile(mu, 103, 203, X0, Y0, W)[3]
check('claim bit read before/after set; dword4 bit 29 only', (before, after, d4), (0, 1, 0x01234567 | 0x20000000))
call(mu, 0x10087880, ISL, [103, 203, 0, 0])
check('claim bit cleared, other bits kept', tile(mu, 103, 203, X0, Y0, W)[3], 0x01234567)

# --- candidates (0x10092320 mode 0): args (rect*, mode, goods, out)
RECT, OUT = ISL + 0x900, ISL + 0xa00
def cands(tiles_, rect, goods=BAUM, island=(8, 8, 100, 200)):
    w, h, x0, y0 = island
    mu = build(w, h, x0, y0, tiles_, types)
    mu.mem_write(RECT, struct.pack('<HHBB', *rect))
    n = call(mu, 0x10092320, ISL, [RECT, 0, goods, OUT])
    buf = bytes(mu.mem_read(OUT, 4 * n))
    return [struct.unpack_from('<HH', buf, 4 * i) for i in range(n)]
T = {(101, 201): (105, 0, 0), (102, 201): (104, 0, 0), (103, 201): (105, 0, 0x20000000), (104, 201): (105, 0, 0),
     (101, 202): (259, 0, 0), (102, 203): (261, 0, 0), (103, 203): (713, 0, 0), (104, 204): (107, 0, 0)}
check('candidates, rect (101,201,4,4)', cands(T, (101, 201, 4, 4)), [(101, 201), (104, 201), (101, 202), (102, 203), (104, 204)])
T2 = {(107, 207): (105, 0, 0)}
check('candidates, rect entirely outside the island east/south', cands(T2, (108, 208, 4, 4)), [])
T3 = {(100, 200): (105, 0, 0), (107, 207): (105, 0, 0)}
check('candidates, rect larger than island is clipped', cands(T3, (98, 198, 20, 20)), [(100, 200), (107, 207)])
# rect.x as u16 wrap (x0 = 65535): nothing found
check('candidates, wrapped rect.x = 65535 finds nothing', cands(T3, (65535, 198, 11, 11)), [])
print('ALL PASS' if ok else 'SOME FAIL')
sys.exit(0 if ok else 1)
