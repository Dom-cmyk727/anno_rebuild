#!/usr/bin/env python3
"""Generator of the ship-founding coast-rule fixtures (task MAP-2 U2).

Expected values come from EMULATING the original instructions (AnnoGame.dll type slot
+0x68 at 0x10008f90, which calls the coast routine 0x10008ea0) with Unicorn over synthetic
memory; nothing is computed by the C++ under test or by a re-implementation. The
output (fixtures/synthetic/founding/coast_cases.txt) holds only synthetic height grids and
verdicts, never original bytes or disassembly. Ordinary unit tests read that file and need
neither this script, Unicorn nor the game files.

Needs: python3 with `unicorn` and `pefile`, and ANNO1503_GAME_DIR (read-only use of
AnnoGame.dll). Usage: founding_fixtures.py [--check] [--out FILE]
  --check  regenerate in memory and fail (exit 1) when the committed file differs
Exit: 0 ok, 1 differs (--check), 2 emulation incomplete/error or missing inputs.
"""
import hashlib, os, random, struct, sys

GENERATOR_VERSION = 1
SEED = 7
RANDOM_CASES = 400
SLOT68 = 0x10008F90
COAST_ROUTINE = 0x10008EA0
FLAGS = 0x2087
CALL_FLAGS = 0x8000000
MAX_INSTR = 100000


def fnv64(data):
    h = 0xCBF29CE484222325
    for b in data:
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def _ver(name):
    from importlib.metadata import version
    return version(name)


def emulator(game_dll):
    import pefile
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
    from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_EAX
    pe = pefile.PE(game_dll, fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    size = (pe.OPTIONAL_HEADER.SizeOfImage + 0xFFF) & ~0xFFF
    image = pe.get_memory_mapped_image()[:size]
    TYPE, HP, RET = 0x30010000, 0x30020000, 0x40000000

    def run(w, h, rot, grid, stride):
        mu = Uc(UC_ARCH_X86, UC_MODE_32)
        mu.mem_map(base, size)
        mu.mem_write(base, image)
        mu.mem_map(0x30000000, 0x100000)
        mu.mem_map(0x40000000, 0x100000)
        tobj = bytearray(0x40)
        tobj[6], tobj[7] = w, h
        struct.pack_into('<H', tobj, 0x12, FLAGS)
        mu.mem_write(TYPE, bytes(tobj))
        mu.mem_write(HP, bytes(grid))
        sp = 0x30080000
        for a in reversed([HP, stride, rot, CALL_FLAGS]):
            sp -= 4
            mu.mem_write(sp, struct.pack('<I', a & 0xFFFFFFFF))
        sp -= 4
        mu.mem_write(sp, struct.pack('<I', RET))
        mu.reg_write(UC_X86_REG_ESP, sp)
        mu.reg_write(UC_X86_REG_ECX, TYPE)
        mu.emu_start(SLOT68, RET, count=MAX_INSTR)
        if mu.reg_read(UC_X86_REG_EIP) != RET:
            raise RuntimeError('emulation did not reach the return sentinel')
        eax = mu.reg_read(UC_X86_REG_EAX)
        return eax if eax < 0x80000000 else eax - 0x100000000

    return run


def named_cases():
    """(name, w, h, rot, fw, fh, grid-builder) hand-made cases; grids are the vertex window."""
    out = []

    def grid(fw, fh, f):
        return [f(x, y) & 0xFF for y in range(fh + 1) for x in range(fw + 1)]

    # 4x4 type, every rotation: shore on the rotation's edge, water (0x10) opposite.
    for rot in range(4):
        fw = fh = 4
        shore = {0: lambda x, y: y == 0, 1: lambda x, y: x == 0,
                 2: lambda x, y: y == fh, 3: lambda x, y: x == fw}[rot]
        opp = {0: lambda x, y: y == fh, 1: lambda x, y: x == fw,
               2: lambda x, y: y == 0, 3: lambda x, y: x == 0}[rot]
        out.append((f'square-4x4-rot{rot}-valid', 4, 4, rot, fw, fh,
                    grid(fw, fh, lambda x, y: 0x30 if shore(x, y) else (0x10 if opp(x, y) else 0x20))))
        out.append((f'square-4x4-rot{rot}-wrong-edge', 4, 4, rot, fw, fh,
                    grid(fw, fh, lambda x, y: 0x30 if opp(x, y) else (0x10 if shore(x, y) else 0x20))))
    # Non-square 4x3 type, rotation 1: footprint 3 wide x 4 tall, window 4x5, shore column x=0
    # with n = 5 vertices (unrotated width + 1). (a) valid; (b) fifth shore vertex 0x10 (<= 0x3f,
    # so the window maximum stays 0x3x and only the edge test isolates the length).
    fw, fh = 3, 4
    base = lambda x, y: 0x30 if x == 0 else (0x10 if x == fw else 0x20)
    out.append(('nonsquare-4x3-rot1-valid', 4, 3, 1, fw, fh, grid(fw, fh, base)))
    out.append(('nonsquare-4x3-rot1-fifth-shore-vertex-wrong', 4, 3, 1, fw, fh,
                grid(fw, fh, lambda x, y: 0x10 if (x == 0 and y == 4) else base(x, y))))
    # Non-square 4x3 at rotations 2 and 3 (shore: bottom row / right column, n = 5 vertices).
    fw, fh = 4, 3
    base2 = lambda x, y: 0x30 if y == fh else (0x10 if y == 0 else 0x20)
    out.append(('nonsquare-4x3-rot2-valid', 4, 3, 2, fw, fh, grid(fw, fh, base2)))
    out.append(('nonsquare-4x3-rot2-last-shore-vertex-wrong', 4, 3, 2, fw, fh,
                grid(fw, fh, lambda x, y: 0x10 if (x == 4 and y == fh) else base2(x, y))))
    fw, fh = 3, 4
    base3 = lambda x, y: 0x30 if x == fw else (0x10 if x == 0 else 0x20)
    out.append(('nonsquare-4x3-rot3-valid', 4, 3, 3, fw, fh, grid(fw, fh, base3)))
    out.append(('nonsquare-4x3-rot3-fifth-shore-vertex-wrong', 4, 3, 3, fw, fh,
                grid(fw, fh, lambda x, y: 0x10 if (x == fw and y == 4) else base3(x, y))))
    # A second non-square type (5x3), every rotation: valid, and the first shore vertex wrong.
    for rot in range(4):
        fw, fh = (5, 3) if rot % 2 == 0 else (3, 5)
        sh = {0: lambda x, y: y == 0, 1: lambda x, y: x == 0, 2: lambda x, y: y == fh, 3: lambda x, y: x == fw}[rot]
        op = {0: lambda x, y: y == fh, 1: lambda x, y: x == fw, 2: lambda x, y: y == 0, 3: lambda x, y: x == 0}[rot]
        v = lambda x, y, sh=sh, op=op: 0x30 if sh(x, y) else (0x10 if op(x, y) else 0x20)
        first = {0: (0, 0), 1: (0, 0), 2: (0, fh), 3: (fw, 0)}[rot]
        out.append((f'nonsquare-5x3-rot{rot}-valid', 5, 3, rot, fw, fh, grid(fw, fh, v)))
        out.append((f'nonsquare-5x3-rot{rot}-first-shore-vertex-wrong', 5, 3, rot, fw, fh,
                    grid(fw, fh, lambda x, y, v=v, first=first: 0x10 if (x, y) == first else v(x, y))))
    # Same type at rotation 0 (footprint 4x3, window 5x4, shore row y=0, n = 5).
    fw, fh = 4, 3
    base0 = lambda x, y: 0x30 if y == 0 else (0x10 if y == fh else 0x20)
    out.append(('nonsquare-4x3-rot0-valid', 4, 3, 0, fw, fh, grid(fw, fh, base0)))
    out.append(('nonsquare-4x3-rot0-last-shore-vertex-wrong', 4, 3, 0, fw, fh,
                grid(fw, fh, lambda x, y: 0x10 if (x == 4 and y == 0) else base0(x, y))))
    # One-wide / one-deep shortcut (n <= 2 or m <= 2): accepted without the edge test.
    for (w, h, rot) in ((1, 4, 0), (4, 1, 0), (1, 3, 1), (3, 1, 3)):
        fw, fh = (w, h) if rot % 2 == 0 else (h, w)
        out.append((f'shortcut-{w}x{h}-rot{rot}-flat-0x40', w, h, rot, fw, fh, grid(fw, fh, lambda x, y: 0x40)))
        out.append((f'shortcut-{w}x{h}-rot{rot}-mixed', w, h, rot, fw, fh,
                    grid(fw, fh, lambda x, y: 0x3F if (x + y) % 2 else 0x10)))
    # Opposite edge exactly at the floor(n/2) threshold: n = 5 -> 2 low vertices needed.
    fw = fh = 4
    for low in (1, 2, 3):
        out.append((f'square-4x4-rot0-opposite-low-{low}', 4, 4, 0, fw, fh,
                    grid(fw, fh, lambda x, y, low=low: 0x30 if y == 0 else (0x10 if (y == fh and x < low) else 0x20))))
    # Maximum 0x40 in the window rejects even with a perfect shore.
    out.append(('square-4x4-rot0-max-0x40', 4, 4, 0, 4, 4,
                grid(4, 4, lambda x, y: 0x40 if (x, y) == (2, 2) else (0x30 if y == 0 else 0x10))))
    return out


def random_cases(rng):
    alpha = [0x10, 0x1F, 0x20, 0x2F, 0x30, 0x30, 0x30, 0x3F, 0x40]
    out = []
    for i in range(RANDOM_CASES):
        w, h, rot = rng.choice([1, 2, 3, 4, 5]), rng.choice([1, 2, 3, 4]), rng.randrange(4)
        fw, fh = (w, h) if rot % 2 == 0 else (h, w)
        g = [rng.choice(alpha) for _ in range((fw + 1) * (fh + 1))]
        if rng.random() < 0.6:
            n = w + 1
            edge = {0: [(k, 0) for k in range(n)], 2: [(k, fh) for k in range(n)],
                    1: [(0, k) for k in range(n)], 3: [(fw, k) for k in range(n)]}[rot]
            for (x, y) in edge:
                g[y * (fw + 1) + x] = 0x30
        out.append((f'random-{i:03d}', w, h, rot, fw, fh, g))
    return out


def render(game_dll, run):
    sha = hashlib.sha256(open(game_dll, 'rb').read()).hexdigest()
    rng = random.Random(SEED)
    cases = named_cases() + random_cases(rng)
    lines = []
    accepted = rejected = 0
    for (name, w, h, rot, fw, fh, g) in cases:
        want = run(w, h, rot, g, fw + 1)
        if want == -1:
            rejected += 1
        else:
            accepted += 1
        lines.append(f'{name} {w} {h} {rot} {FLAGS:#06x} {fw + 1} {fh + 1} {want} {bytes(g).hex()}')
    body = '\n'.join(lines) + '\n'
    head = [
        '# Ship-founding coast-rule fixtures (task MAP-2 U2). SYNTHETIC grids; expected values are the',
        '# results of EMULATING the original instructions, not of the code under test.',
        f'# module: AnnoGame.dll sha256 {sha}',
        f'# emulated: type slot +0x68 at {SLOT68:#x} (calls the coast routine {COAST_ROUTINE:#x}), thiscall, call flags {CALL_FLAGS:#x}',
        f'# generator: tools/founding_fixtures.py version {GENERATOR_VERSION}, seed {SEED}, {RANDOM_CASES} random cases + named cases',
        f'# cases: {len(cases)} (accepted {accepted}, rejected {rejected})',
        f'# body sha256: {hashlib.sha256(body.encode()).hexdigest()}',
        f'# body fnv64: {fnv64(body.encode()):016x} (over the non-comment lines, each with its newline; checked by the unit test)',
        f'# tools: unicorn {_ver("unicorn")}, pefile {_ver("pefile")}',
        '# columns: name type_w type_h rotation flag_word window_w window_h expected hex_window',
        '# type_w/type_h are the type\'s UNROTATED size; the window is (fw+1) x (fh+1) vertices row-major, fw x fh the rotated footprint.',
        '# expected: -1 = rejected, otherwise the result byte (record byte 8).',
    ]
    return '\n'.join(head) + '\n' + body


def main(argv):
    out = 'fixtures/synthetic/founding/coast_cases.txt'
    if '--out' in argv:
        out = argv[argv.index('--out') + 1]
    game_dir = os.environ.get('ANNO1503_GAME_DIR')
    if not game_dir:
        print('ANNO1503_GAME_DIR is not set', file=sys.stderr)
        return 2
    dll = os.path.join(game_dir, 'AnnoGame.dll')
    if not os.path.isfile(dll):
        print(f'{dll} not found', file=sys.stderr)
        return 2
    try:
        text = render(dll, emulator(dll))
    except Exception as e:  # emulation incomplete or unavailable
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
