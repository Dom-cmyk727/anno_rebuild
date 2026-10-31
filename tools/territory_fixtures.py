#!/usr/bin/env python3
"""Generator of the ship-founding territory-claim fixtures (task MAP-2 U3).

Expected owner grids come from EMULATING the original instructions (AnnoWelt.dll island
slot +0xe8 at 0x10085840) with Unicorn over a synthetic island: radius > 0 runs up to the
tile painting's end (0x10085ac4), radius 0 (the footprint-rectangle call) up to 0x100858a9,
i.e. before the virtual notification call. Object discovery, events and the reveal that
follow in the original are NOT part of these fixtures. Nothing is computed by the code under
test. The output (fixtures/synthetic/founding/territory_cases.txt) holds synthetic grids and
hashes only, never original bytes or disassembly; ordinary tests need neither this script,
Unicorn nor the game files.

Needs python3 with `unicorn` and `pefile`, and ANNO1503_GAME_DIR (read-only use of AnnoWelt.dll).
Usage: territory_fixtures.py [--check] [--out FILE]
Exit: 0 ok, 1 differs (--check), 2 emulation incomplete/error or missing inputs.
"""
import hashlib, os, random, struct, sys

GENERATOR_VERSION = 1
SEED = 23
RANDOM_CASES = 160
ENTRY = 0x10085840
STOP_DISC = 0x10085AC4
STOP_RECT = 0x100858A9
NEUTRAL = 15
MAX_INSTR = 2000000


def emulator(dll):
    import pefile
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
    from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_ESP
    pe = pefile.PE(dll, fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    img = pe.get_memory_mapped_image()
    size = (pe.OPTIONAL_HEADER.SizeOfImage + 0xFFF) & ~0xFFF
    ISL, TILES, RECT, RET = 0x30010000, 0x30100000, 0x30020000, 0x40000000

    def paint(grid, x0, y0, W, H, rx, ry, rw, rh, city, radius):
        """grid: list of rows of owners (0..15). Returns the grid after one call."""
        mu = Uc(UC_ARCH_X86, UC_MODE_32)
        mu.mem_map(base, size)
        mu.mem_write(base, img[:size])
        mu.mem_map(0x30000000, 0x400000)
        mu.mem_map(0x40000000, 0x10000)
        isl = bytearray(0x200)
        struct.pack_into('<HHHH', isl, 4, x0, y0, W, H)
        struct.pack_into('<I', isl, 0xC, TILES)
        mu.mem_write(ISL, bytes(isl))
        t = bytearray(W * H * 8)
        for y in range(H):
            for x in range(W):
                struct.pack_into('<I', t, (y * W + x) * 8 + 4, 0x1E000200 & 0xE1FFFFFF | (grid[y][x] << 25))
        mu.mem_write(TILES, bytes(t))
        mu.mem_write(RECT, struct.pack('<HHBB', rx, ry, rw, rh))
        sp = 0x30200000 - 0x1000
        for a in (0, 0, radius, city, RECT):  # clip = null, force = 0, radius, city, rect
            sp -= 4
            mu.mem_write(sp, struct.pack('<I', a))
        sp -= 4
        mu.mem_write(sp, struct.pack('<I', RET))
        mu.reg_write(UC_X86_REG_ESP, sp)
        mu.reg_write(UC_X86_REG_ECX, ISL)
        stop = STOP_RECT if radius == 0 else STOP_DISC
        mu.emu_start(ENTRY, stop, count=MAX_INSTR)
        if mu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('emulation did not reach the stop address')
        out = mu.mem_read(TILES, W * H * 8)
        return [[(struct.unpack_from('<I', out, (y * W + x) * 8 + 4)[0] >> 25) & 0xF for x in range(W)]
                for y in range(H)]

    return paint


def fnv64_bytes(data):
    h = 0xCBF29CE484222325
    for b in data:
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def _ver(name):
    from importlib.metadata import version
    return version(name)


def fnv64(rows):
    h = 0xCBF29CE484222325
    for r in rows:
        for v in r:
            h = ((h ^ v) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def make_cases(rng):
    cases = []

    def add(name, isl, rect, city, radius, pre=(), pair=False):
        cases.append(dict(name=name, isl=isl, rect=rect, city=city, radius=radius, pre=list(pre), pair=pair))

    # Hand-made: radius 0, small radii, the 24 of the warehouse, odd/even/non-square, edges.
    add('r0-4x4', (0, 0, 20, 14), (5, 4, 4, 4), 3, 0)
    add('r0-1x1', (2, 3, 9, 9), (6, 7, 1, 1), 1, 0)
    add('r0-nonsquare-5x2-origin', (7, 11, 18, 10), (9, 12, 5, 2), 0, 0)
    for R in (1, 2, 3, 4, 5, 6, 7, 8):
        add(f'disc-R{R}-4x4', (0, 0, 40, 40), (18, 18, 4, 4), 2, R)
    for (rw, rh) in ((1, 1), (2, 2), (3, 3), (4, 4), (5, 3), (3, 5), (2, 7), (6, 1)):
        add(f'disc-R6-{rw}x{rh}', (3, 5, 40, 36), (16, 14, rw, rh), 5, 6)
    add('disc-R24-4x4', (0, 0, 64, 64), (30, 30, 4, 4), 3, 24)
    add('disc-R24-4x4-pair', (0, 0, 64, 64), (30, 30, 4, 4), 3, 24, pair=True)
    add('disc-R24-nonzero-origin', (10, 20, 70, 50), (40, 38, 4, 4), 7, 24)
    add('disc-R24-west-edge', (0, 0, 64, 64), (3, 30, 4, 4), 3, 24)
    add('disc-R24-north-edge', (0, 0, 64, 64), (30, 2, 4, 4), 3, 24)
    add('disc-R24-south-east-corner', (0, 0, 40, 40), (34, 34, 4, 4), 3, 24)
    add('disc-R24-smaller-than-disc', (0, 0, 20, 12), (8, 4, 4, 4), 3, 24)
    add('disc-R9-rival-and-own-blocks', (0, 0, 48, 48), (20, 20, 4, 4), 4, 9,
        pre=[(x, y, 9) for x in range(17, 21) for y in range(22, 26)] + [(x, y, 4) for x in range(24, 28) for y in range(16, 20)])
    add('pair-R5-rival-under-footprint', (0, 0, 30, 30), (12, 12, 4, 4), 2, 5,
        pre=[(13, 13, 8), (14, 14, 8)], pair=True)
    add('pair-R0-after-own', (0, 0, 20, 20), (5, 5, 3, 3), 2, 0, pre=[(5, 5, 2), (6, 6, 3)], pair=True)
    for i in range(RANDOM_CASES):
        R = rng.choice([0, 0, 1, 2, 3, 4, 5, 6, 9, 12, 17, 24, 30])
        rw, rh = rng.randint(1, 7), rng.randint(1, 7)
        W, H = rng.choice([14, 20, 33, 48, 64]), rng.choice([10, 20, 31, 48, 64])
        x0, y0 = rng.randint(0, 9), rng.randint(0, 9)
        rx, ry = rng.randint(x0, x0 + W - rw), rng.randint(y0, y0 + H - rh)
        city = rng.randint(0, 14)
        pre = [(rng.randint(x0, x0 + W - 1), rng.randint(y0, y0 + H - 1), rng.randint(0, 14))
               for _ in range(rng.choice([0, 0, 30, 120]))]
        add(f'random-{i:03d}', (x0, y0, W, H), (rx, ry, rw, rh), city, R, pre, pair=(R > 0 and rng.random() < 0.4))
    return cases


def run_case(paint, c):
    x0, y0, W, H = c['isl']
    rx, ry, rw, rh = c['rect']
    grid = [[NEUTRAL] * W for _ in range(H)]
    for (x, y, o) in c['pre']:
        grid[y - y0][x - x0] = o
    before = [r[:] for r in grid]
    out = paint(grid, x0, y0, W, H, rx, ry, rw, rh, c['city'], c['radius'])
    if c['pair']:  # the founding order: disc, then the footprint rectangle (radius 0)
        out = paint(out, x0, y0, W, H, rx, ry, rw, rh, c['city'], 0)
    changed = sum(1 for y in range(H) for x in range(W) if out[y][x] != before[y][x])
    return before, out, changed


def render(dll, paint):
    sha = hashlib.sha256(open(dll, 'rb').read()).hexdigest()
    rng = random.Random(SEED)
    cases = make_cases(rng)
    lines = []
    grids = []
    for c in cases:
        before, out, changed = run_case(paint, c)
        x0, y0, W, H = c['isl']
        pre = ';'.join(f'{x}:{y}:{o}' for (x, y, o) in c['pre']) or '-'
        lines.append(f"case {c['name']} isl={x0},{y0},{W},{H} rect={','.join(map(str, c['rect']))} "
                     f"city={c['city']} radius={c['radius']} pair={int(c['pair'])} pre={pre} "
                     f"claimed={changed} fnv={fnv64(out):016x}")
        if W * H <= 20 * 14 or c['name'] in ('disc-R5-4x4',) or c['name'].startswith('disc-R24-smaller'):
            grids.append(f"grid {c['name']} " + '/'.join(''.join('%x' % v for v in r) for r in out))
    body = '\n'.join(lines + grids) + '\n'
    head = [
        '# Ship-founding territory-claim fixtures (task MAP-2 U3). SYNTHETIC islands; expected owner grids',
        '# are the results of EMULATING the original instructions, not of the code under test.',
        f'# module: AnnoWelt.dll sha256 {sha}',
        f'# emulated: island slot +0xe8 at {ENTRY:#x}: radius > 0 up to {STOP_DISC:#x} (disc painting), radius 0 up to {STOP_RECT:#x}',
        '#   (footprint rectangle); clip null, force 0; neutral owner nibble 15. NOT modelled: object discovery, events, reveal.',
        f'# generator: tools/territory_fixtures.py version {GENERATOR_VERSION}, seed {SEED}, {RANDOM_CASES} random cases + named cases',
        f'# cases: {len(lines)}, full grids: {len(grids)}',
        f'# body sha256: {hashlib.sha256(body.encode()).hexdigest()}',
        f'# body fnv64: {fnv64_bytes(body.encode()):016x} (over the non-comment lines, each with its newline; checked by the unit test)',
        f'# tools: unicorn {_ver("unicorn")}, pefile {_ver("pefile")}',
        '# case: isl=x0,y0,W,H rect=x,y,w,h (footprint, tiles) city radius pair (1 = disc call then radius-0 call)',
        '#   pre=x:y:owner;... (world tile coordinates, owner 0..14; - = none) claimed=tiles whose owner changed',
        '#   fnv=FNV-1a 64 over the final owner grid, row-major, one byte per tile.',
        '# grid: final owner grid, rows joined by /, one hex digit per tile.',
    ]
    return '\n'.join(head) + '\n' + body


def main(argv):
    out = 'fixtures/synthetic/founding/territory_cases.txt'
    if '--out' in argv:
        out = argv[argv.index('--out') + 1]
    game_dir = os.environ.get('ANNO1503_GAME_DIR')
    if not game_dir:
        print('ANNO1503_GAME_DIR is not set', file=sys.stderr)
        return 2
    dll = os.path.join(game_dir, 'AnnoWelt.dll')
    if not os.path.isfile(dll):
        print(f'{dll} not found', file=sys.stderr)
        return 2
    try:
        text = render(dll, emulator(dll))
    except Exception as e:
        print(f'emulation failed: {type(e).__name__}: {e}', file=sys.stderr)
        return 2
    if '--check' in argv:
        same = os.path.isfile(out) and open(out).read() == text
        print('fixture file is up to date' if same else 'fixture file DIFFERS from regeneration')
        return 0 if same else 1
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, 'w') as f:
        f.write(text)
    print(f'wrote {out}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
