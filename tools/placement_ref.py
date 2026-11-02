#!/usr/bin/env python3
"""Independent reference for the island placement predicate of an ordinary building
(MAP-BUILD-1 package B; trace: docs/re/ship-placement.md, contract: docs/sim/ship-placement.md).

It transcribes, from the disassembly alone and with no C++ in sight, the rule of
  AnnoWelt.dll 0x1007c790        island slot +0x6c: bounds, existing objects, height byte, acceptance
  AnnoGame.dll 0x10008920        type slot +0x6c:  may the placed type stand on an existing object
  AnnoGame.dll 0x10008f90        type slot +0x68:  height analysis of the vertex window
  AnnoGame.dll 0x10008ea0        coast helper (shore edge test)
and nothing else. It is the oracle the engine must agree with (fixtures/synthetic/sim/placement_vectors.txt).
Domains the trace did not decode are reported as UNSUPPORTED, never as accept or reject.

  placement_ref.py selftest                      the rule's own checks (hand-derived cases)
  placement_ref.py check [vectors.txt]           run a vector file (default: the fixture); exit 0/1
  placement_ref.py classify [game_dir]           support classes over every Anlagen.dat type
  placement_ref.py corpus [game_dir]             the rule against every PROD2/GELAENDE item of the shipped islands
  placement_ref.py roads [game_dir]              MAP-ROAD-1: the STRASSE inventory and every shipped STRASSE placement against the road rule

Runtime vertex grid: the original keeps heights in a corner grid of stride W + 1 (docs/re/ship-founding.md
12.3); vertex (W, y) and (x, H) are 0. A vector gives that grid explicitly (rows 0..H, W+1 values each).
Only the Python standard library is used (`classify` and `corpus` also use tools/figuren.py and tools/insel.py).
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_VECTORS = os.path.join(HERE, '..', 'fixtures', 'synthetic', 'sim', 'placement_vectors.txt')

# Island-function flag bits that the chain tests (everything else the callers pass is not read):
F_FLAGS_NO_CLASS_CHECK = 0x1000000   # 0x1007cb48: skip the height analysis (the apply path sets it)
F_SKIP_EXISTING = 0x400000           # 0x1007c89a: skip the existing-object and coverage tests
F_NO_OVERLAP = 0x10000000            # 0x10008945: type slot +0x6c answers 0 unless the existing type has 0x20
ANALYSIS = 0x2000000                 # a flag value that the chain ignores; used to say "analysis requested"
THRESHOLD = 32                       # trunc(2 * 0.64f / 0.04f), 0x1007cbeb-0x1007cc03 (E, docs/re 10.2)

ACCEPT, REJECT, UNSUPPORTED = 'accept', 'reject', 'unsupported'
STRASSE = 193                        # PRODTYPE of the ordinary land roads (AnnoGame name table, sim types.cpp Named{193, "STRASSE"})
ROAD_FLOOR = 32                      # MAP-ROAD-1 adaptation R-A2: the smallest accepted road level (the 0x4000 class is accepted at once by the original)


def group(prodtype):
    """((p - 1) & ~0x1f) + 1 in 32-bit unsigned arithmetic (0x10008970, 0x1007cbdf)."""
    return (((prodtype - 1) & 0xFFFFFFFF) & 0xFFFFFFE0) + 1 & 0xFFFFFFFF


class Type:
    """The fields of an Anlagen.dat type that the placement chain reads."""

    def __init__(self, w, h, prodtype, flags, replaceable=False):
        self.w, self.h, self.prodtype, self.flags = w, h, prodtype, flags & 0xFFFF
        self.replaceable = replaceable          # bit 8 of type word +0x14 = HEAD byte 0x2e bit 0 (slot +0xd0)

    @property
    def group(self):
        return group(self.prodtype)


class Island:
    """x0, y0, W, H in tiles; vertex(vx, vy) in world tile coordinates; object(x, y) the tile's object id."""

    def __init__(self, x0, y0, W, H, vertex, obj):
        self.x0, self.y0, self.W, self.H = x0, y0, W, H
        self.vertex, self.obj = vertex, obj


def trunc4(s):
    """cdq / and edx,3 / add / sar 2 (0x10008fd3-0x10008fd9): truncating division by 4."""
    return (s + (3 if s < 0 else 0)) >> 2


class Unsupported(Exception):
    pass


def coast_ok(w, h, rot, V, mx):
    """Coast helper 0x10008ea0 (docs/re/ship-founding.md 10.4, 3000 emulated grids): True = accept.
    n = w + 1 and m = h + 1 are the UNROTATED type dimensions; the window is the rotated one."""
    n, m = w + 1, h + 1
    if n <= 2 or m <= 2:
        return True
    if mx & 0xF0 != 0x30:
        return False
    if rot == 0:
        first, second = [V(i, 0) for i in range(n)], [V(i, m - 1) for i in range(n)]
    elif rot == 2:
        first, second = [V(i, m - 1) for i in range(n)], [V(i, 0) for i in range(n)]
    elif rot == 1:
        first, second = [V(0, i) for i in range(n)], [V(m - 1, i) for i in range(n)]
    else:
        first, second = [V(m - 1, i) for i in range(n)], [V(0, i) for i in range(n)]
    if any(v != 0x30 for v in first):
        return False
    return sum(1 for v in second if v < 0x20) >= n // 2


def height_analysis(t, rot, V, flags):
    """Type slot +0x68 (0x10008f90). V(dx, dy) = vertex of the window (origin at the footprint's
    north-west corner). `flags` is the island function's combined flags (type word | caller flags),
    exactly the fourth argument. Returns the height byte, or -1 (reject); raises Unsupported."""
    if t.flags & 3 == 1:                                  # 0x10008fa1: one-vertex-quad scenery
        return trunc4(V(0, 0) + V(1, 0) + V(0, 1) + V(1, 1))
    fw, fh = (t.w, t.h) if rot & 1 == 0 else (t.h, t.w)
    if fw == 0 or fh == 0:
        raise Unsupported('zero footprint')
    allv = [V(x, y) for y in range(fh + 1) for x in range(fw + 1)]
    mx = max(allv)
    cellmin = [min(V(x, y), V(x + 1, y), V(x, y + 1), V(x + 1, y + 1)) & 0xF0
               for y in range(fh) for x in range(fw)]
    mode = flags & 3
    res = {0: max(cellmin), 2: min(cellmin), 3: mx & 0xF0}.get(mode, -1)
    if flags & 0x1000004 == 4:                            # 0x1000911-0x10009116
        cls = flags & 0x7000
        if cls == 0:
            if mx - min(cellmin) > 0:
                return -1
        elif cls == 0x1000:
            if mx - min(cellmin) > 0x10:
                return -1
        elif cls == 0x2000:
            if (t.flags & 0x7017) != 0x2007:
                raise Unsupported('coast class with another low-bit pattern (helper arguments not decoded)')
            if not coast_ok(t.w, t.h, rot, V, mx):
                return -1
        elif cls == 0x3000:
            raise Unsupported('class 0x3000: helper 0x10008a80 needs type +0x234 (HIGHMAP) and float helpers')
        # every other class value passes without a test (0x10009199 jne 0x100091c6)
    return res


def compat(p, e, flags):
    """Type slot +0x6c (0x10008920): p = placed type (this), e = existing object's type. 1 or 0."""
    if e.flags & 0x20:
        return 1
    if flags & F_NO_OVERLAP:
        return 0
    G, pp, ep = p.group, p.prodtype, e.prodtype
    if G == 0x81:
        return 1 if (group(ep) in (0xC1, 0x101) and pp == 0x81) else 0
    if G in (1, 0x21, 0x61):
        if ep == pp and e.replaceable:
            return 1
        if ((p.flags & 0x10) | (flags & 0x10)) == 0:
            return 0
        return 1 if group(ep) == 0x141 else 0
    if G == 0xC1:
        if ep == 0xC6:
            return 1 if pp == 0xC6 else 0
        return 1 if group(ep) == 0xC1 else 0
    if G == 0x101:
        return 1 if group(ep) in (0xC1, 0x101) else 0
    if G == 0x141:
        return 1 if group(ep) == 0x141 else 0
    return 0


def place(isl, reg, tid, x, y, rot, flags, rec8_in=0):
    """Island slot +0x6c (0x1007c790). Returns (verdict, reason, rec8). `rec8` is the byte the function
    leaves in record byte 8: the analysis result when it ran, else rec8_in."""
    t = reg.get(tid)
    if t is None:
        return UNSUPPORTED, 'unknown type id (the original dereferences the unchecked lookup)', rec8_in
    if rot not in (0, 1, 2, 3):
        return UNSUPPORTED, 'rotation above 3 (footprint bytes uninitialised in the original)', rec8_in
    fw, fh = (t.w, t.h) if rot & 1 == 0 else (t.h, t.w)
    if x < isl.x0 or x + fw > isl.x0 + isl.W or y < isl.y0 or y + fh > isl.y0 + isl.H:
        return REJECT, 'BOUNDS', rec8_in
    flags |= t.flags
    if not flags & F_SKIP_EXISTING:
        for dy in range(fh):
            for dx in range(fw):
                o = isl.obj(x + dx, y + dy)
                if o == 0:
                    continue
                e = reg.get(o)
                if e is None:
                    return UNSUPPORTED, 'object id %d not in the registry' % o, rec8_in
                if compat(t, e, flags) == 0:
                    return REJECT, 'OCCUPIED', rec8_in
        if flags & 0x10:
            return UNSUPPORTED, 'flag 0x10: per-chunk coverage test (0x1007c955-0x1007cb3d) not decoded', rec8_in
    rec8 = rec8_in
    if not flags & F_FLAGS_NO_CLASS_CHECK:
        try:
            r = height_analysis(t, rot, lambda dx, dy: isl.vertex(x + dx, y + dy), flags)
        except Unsupported as ex:
            return UNSUPPORTED, str(ex), rec8_in
        if r < 0:
            return REJECT, 'TERRAIN', rec8_in
        rec8 = r
    cls = flags & 0x7000
    if cls in (0x4000, 0x2000) or t.group == 0x141:
        return ACCEPT, '', rec8
    if rec8 < THRESHOLD:
        return REJECT, 'LOWLEVEL', rec8
    return ACCEPT, '', rec8


def road_class(t):
    """MAP-ROAD-1 R1: the types a BuildCmd may place as a tile object (docs/sim/road-placement.md section 2). Facts only:
    PRODTYPE STRASSE, class bits 0x4000 or 0x1000, no coverage bit 0x10, a mode other than the single-quad average (F & 3 == 1)."""
    f = t.flags
    return (t.prodtype == STRASSE and f <= 0xFFFF and f & 0x10 == 0 and f & 0x7000 in (0x4000, 0x1000)
            and f & 3 != 1 and t.w > 0 and t.h > 0)


def engine_place(isl, reg, tid, x, y, rot, flags, rec8_in=0, road_floor=True, road_preserve=True):
    """The BuildCmd contract (docs/sim/ship-placement.md, docs/sim/road-placement.md): place() for the supported terrain
    classes, UNSUPPORTED for every other flag word, so a class the trace did not cover never gets a default answer.
    A road class type is the original rule (analysis requested: the server derives the byte) plus, as separate enhanced
    choices (Rules::road_require_land_level / road_preserve_nonroad_pieces), the level floor R-A2 for the class 0x4000
    (the class 0x1000 threshold is the original's and never optional) and the ramp/pier refusal R-A3."""
    t = reg.get(tid)
    if t is not None and road_class(t):
        fw, fh = (t.w, t.h) if rot & 1 == 0 else (t.h, t.w)
        inside = isl.x0 <= x and x + fw <= isl.x0 + isl.W and isl.y0 <= y and y + fh <= isl.y0 + isl.H
        if inside and rot in (0, 1, 2, 3):                                         # the occupancy step, row-major: the first deciding tile wins
            for dy in range(fh):
                for dx in range(fw):
                    o = isl.obj(x + dx, y + dy)
                    if o == 0:
                        continue
                    e = reg.get(o)
                    if e is None:
                        return UNSUPPORTED, 'object id %d not in the registry' % o, rec8_in
                    if compat(t, e, t.flags | flags) == 0:
                        return REJECT, 'OCCUPIED', rec8_in
                    if road_preserve and e.group == 0xC1 and not road_class(e):     # R-A3: ramp, pier (a bridge piece the original refuses itself)
                        return REJECT, 'OCCUPIED', rec8_in
        v, why, b = place(isl, reg, tid, x, y, rot, flags, rec8_in)
        if v == ACCEPT and road_floor and b < ROAD_FLOOR:                          # R-A2 (class 0x1000 has the original's own threshold in place())
            return REJECT, 'LOWLEVEL', b
        return v, why, b
    if t is not None:
        name, sup = terrain_class(t.flags)
        if not sup:
            return UNSUPPORTED, 'terrain class not supported for BuildCmd: ' + name, rec8_in
    return place(isl, reg, tid, x, y, rot, flags, rec8_in)


# --- support classes (B3) -------------------------------------------------------------

def terrain_class(flags_word):
    """Classify a type's flag word for the BuildCmd terrain/height path.
    Returns (name, supported)."""
    if flags_word > 0xFFFF:                              # the original's word is a u16; wider is not decoded
        return 'flag word wider than 16 bits', False
    f = flags_word
    if (f & ~0x0080) == 0x0004:
        return 'ordinary-land', True                    # no class, bit 4, mode 0, bit 0x10 clear
    if f & 0x7017 == 0x2007:
        return 'coast', True                            # the evidenced coast rule
    if f & 3 == 1:
        return 'tile-object (single quad, average height)', False
    if f & 0x7000 == 0x4000:
        return 'road/bridge/pier-on-road (accepted at once)', False
    if f & 0x7000 == 0x3000:
        return 'mountain-slope helper 0x3000 (mines, quarries)', False
    if f & 0x7000 == 0x1000:
        return 'wall/road-on-slope 0x1000 (rule decoded, building not implemented)', False
    if f & 0x10:
        return 'overlay/scenery with bit 0x10 (coverage test)', False
    if f & 4:
        return 'bit 4 with mode 2 (ruins)', False
    return 'other scenery', False


# --- vector files -----------------------------------------------------------------------

def parse_vectors(text):
    reg, cases, cur = {}, [], None
    for ln, raw in enumerate(text.splitlines(), 1):
        line = raw.split('#', 1)[0].strip()
        if not line:
            continue
        w = line.split()
        k = w[0]
        if k == 'type':                               # type <id> <w> <h> <prodtype> <flags hex> <replaceable>
            reg[int(w[1])] = Type(int(w[2]), int(w[3]), int(w[4]), int(w[5], 16), bool(int(w[6])))
        elif k == 'case':
            cur = dict(name=w[1], line=ln, island=None, rows={}, objs={}, place=None, expect=None, original=None)
            cases.append(cur)
        elif k == 'island':                           # island <x0> <y0> <W> <H>
            cur['island'] = tuple(int(v) for v in w[1:5])
        elif k == 'v':                                # v <row> <hex bytes, W+1 vertices>
            cur['rows'][int(w[1])] = bytes.fromhex(w[2])
        elif k == 'obj':                              # obj <x> <y> <type id>
            cur['objs'][(int(w[1]), int(w[2]))] = int(w[3])
        elif k == 'place':                            # place <type> <x> <y> <rot> <flags hex> <rec8>
            cur['place'] = (int(w[1]), int(w[2]), int(w[3]), int(w[4]), int(w[5], 16), int(w[6]))
        elif k == 'expect':                           # expect accept <byte> | reject <REASON> | unsupported
            cur['expect'] = tuple(w[1:])
        elif k == 'original':                         # original accept <byte> | reject <REASON>: what the ORIGINAL answers where a documented adaptation differs
            cur['original'] = tuple(w[1:])
        elif k == 'end':
            cur = None
        else:
            raise ValueError('line %d: unknown keyword %r' % (ln, k))
    return reg, cases


def case_island(c):
    x0, y0, W, H = c['island']
    rows = c['rows']

    def vertex(vx, vy):
        r = rows.get(vy - y0)
        if r is None or not (0 <= vx - x0 < len(r)):
            return 0
        return r[vx - x0]
    return Island(x0, y0, W, H, vertex, lambda tx, ty: c['objs'].get((tx, ty), 0))


def run_case(reg, c, road_floor=True, road_preserve=True):
    tid, x, y, rot, flags, rec8 = c['place']
    return engine_place(case_island(c), reg, tid, x, y, rot, flags, rec8, road_floor, road_preserve)


def check_vectors(path):
    reg, cases = parse_vectors(open(path).read())
    bad = 0
    for c in cases:
        verdict, reason, byte = run_case(reg, c)
        e = c['expect']
        if e[0] == ACCEPT:
            ok = verdict == ACCEPT and byte == int(e[1])
        elif e[0] == REJECT:
            ok = verdict == REJECT and reason == e[1]
        else:
            ok = verdict == UNSUPPORTED
        if not ok:
            bad += 1
            print('MISMATCH %s (line %d): expected %s, got %s %s %d' % (c['name'], c['line'], ' '.join(e), verdict, reason, byte))
        # The faithful mode (both enhanced road choices off) answers the `original` line where the file gives one,
        # else `expect`; and for a road it must equal the transcription of the original island function (place()).
        fv, fr, fb = run_case(reg, c, False, False)
        o = c['original'] or e
        if o[0] == ACCEPT:
            fok = fv == ACCEPT and fb == int(o[1])
        elif o[0] == REJECT:
            fok = fv == REJECT and fr == o[1]
        else:
            fok = fv == UNSUPPORTED
        if not fok:
            bad += 1
            print('MISMATCH %s (line %d) faithful mode: expected %s, got %s %s %d' % (c['name'], c['line'], ' '.join(o), fv, fr, fb))
        tid, x, y, rot, flags, rec8 = c['place']
        if c['original'] is not None and reg[tid].prodtype == STRASSE:
            ov, orr, ob = place(case_island(c), reg, tid, x, y, rot, flags, rec8)
            ook = (ov == ACCEPT and o[0] == ACCEPT and ob == int(o[1])) or (ov == REJECT and o[0] == REJECT and orr == o[1])
            if not ook:
                bad += 1
                print('MISMATCH %s (line %d): original line %s, transcription %s %s %d' % (c['name'], c['line'], ' '.join(o), ov, orr, ob))
    print('placement vectors: %d cases, %d mismatches (%s)' % (len(cases), bad, os.path.normpath(path)))
    return bad == 0


# --- selftest ---------------------------------------------------------------------------

def flat(level, W, H):
    return lambda vx, vy: level if (0 <= vx < W and 0 <= vy < H) else 0


def selftest():
    fails = []

    def eq(name, got, want):
        if got != want:
            fails.append('%s: got %r want %r' % (name, got, want))
    house = Type(2, 2, 97, 0x84)                      # PIONIER-like: flag word 0x84
    reg = {1: house, 2: Type(1, 1, 226, 0x29), 3: Type(3, 3, 1, 0x84)}
    plain = Island(0, 0, 10, 10, flat(0x30, 10, 10), lambda x, y: 0)
    eq('flat 0x30 accepts with byte 0x30', place(plain, reg, 1, 2, 2, 0, ANALYSIS)[::2], (ACCEPT, 0x30))
    eq('flat 0x10 below threshold', place(Island(0, 0, 10, 10, flat(0x10, 10, 10), lambda x, y: 0), reg, 1, 2, 2, 0, ANALYSIS)[:2], (REJECT, 'LOWLEVEL'))
    eq('flat 0x20 is the smallest accepted level', place(Island(0, 0, 10, 10, flat(0x20, 10, 10), lambda x, y: 0), reg, 1, 2, 2, 0, ANALYSIS)[::2], (ACCEPT, 0x20))
    eq('flat 0x31 (not a multiple of 16) rejected', place(Island(0, 0, 10, 10, flat(0x31, 10, 10), lambda x, y: 0), reg, 1, 2, 2, 0, ANALYSIS)[:2], (REJECT, 'TERRAIN'))
    eq('east edge: vertex column W is 0', place(plain, reg, 1, 8, 2, 0, ANALYSIS)[:2], (REJECT, 'TERRAIN'))
    eq('outside', place(plain, reg, 1, 9, 2, 0, ANALYSIS)[:2], (REJECT, 'BOUNDS'))
    reg[4] = Type(3, 1, 1, 0x84)
    eq('wide type pokes out at rotation 0', place(plain, reg, 4, 8, 2, 0, ANALYSIS)[:2], (REJECT, 'BOUNDS'))
    eq('the same type turned fits', place(plain, reg, 4, 8, 2, 1, ANALYSIS)[::2], (ACCEPT, 0x30))
    eq('trunc4', (trunc4(0x31 * 4 + 3), trunc4(0)), (0x31, 0))
    # MAP-ROAD-CLOSE-1: the 2 x 2 table of the two enhanced road choices (floor, preserve), hand-derived:
    # a road on flat 0x10 refuses only with the floor; a road over a ramp refuses only with preserve;
    # both at once: preserve is the occupancy step and answers first; class 0x1000 keeps its own floor.
    rr = {1: Type(1, 1, 193, 0x4186), 2: Type(1, 1, 194, 0x4186), 3: Type(1, 1, 193, 0x1186)}
    low = Island(0, 0, 10, 10, flat(0x10, 10, 10), lambda x, y: 0)
    low_ramp = Island(0, 0, 10, 10, flat(0x10, 10, 10), lambda x, y: 2 if (x, y) == (3, 3) else 0)
    land_ramp = Island(0, 0, 10, 10, flat(0x30, 10, 10), lambda x, y: 2 if (x, y) == (3, 3) else 0)
    for fl in (False, True):
        for pr in (False, True):
            tag = 'floor=%d preserve=%d ' % (fl, pr)
            eq(tag + 'low road', engine_place(low, rr, 1, 3, 3, 0, ANALYSIS, 0, fl, pr)[:2], (REJECT, 'LOWLEVEL') if fl else (ACCEPT, ''))
            eq(tag + 'ramp on land', engine_place(land_ramp, rr, 1, 3, 3, 0, ANALYSIS, 0, fl, pr)[:2], (REJECT, 'OCCUPIED') if pr else (ACCEPT, ''))
            eq(tag + 'ramp on low land', engine_place(low_ramp, rr, 1, 3, 3, 0, ANALYSIS, 0, fl, pr)[:2],
               (REJECT, 'OCCUPIED') if pr else ((REJECT, 'LOWLEVEL') if fl else (ACCEPT, '')))
            eq(tag + 'class 0x1000 keeps its floor', engine_place(low, rr, 3, 3, 3, 0, ANALYSIS, 0, fl, pr)[:2], (REJECT, 'LOWLEVEL'))
    if fails:
        print('\n'.join('FAIL ' + f for f in fails))
        return False
    print('placement_ref selftest: %d checks passed' % (9 + 16))
    return True


# --- registry classification and corpus (need the original files) ----------------------------

PRODTYPE_NUMBERS = {
    'HANDWERK': 1, 'PLANTAGE': 2, 'MINE': 3, 'WEIDETIER': 4, 'JAGDHAUS': 5, 'FISCHER': 6, 'WERFT': 7,
    'STEINMETZ': 8, 'UNIVERSAL': 9, 'SCHAFFARM': 10, 'KANONENBAU': 11, 'HANDWERKDUAL': 12,
    'STEINBRUCH': 13, 'EISLOCH': 14, 'WALFAENGER': 15, 'PELZJAEGER': 16, 'HANDWERKWAFFEN': 17,
    'HAUPT': 33, 'MARKT': 34, 'KONTOR': 35, 'MARKTKOMBI': 36, 'BRUNNEN': 65, 'WIRT': 66, 'KAPELLE': 67,
    'KIRCHE': 68, 'BADEHAUS': 69, 'THEATER': 70, 'ARZT': 71, 'SCHULE': 72, 'HOCHSCHULE': 73,
    'SCHLOSS': 74, 'GALGEN': 75, 'GERICHT': 76, 'FEUERWEHR': 77, 'BIBLIOTHEK': 78, 'PAVILLION': 79,
    'PIONIER': 97, 'SIEDLER': 98, 'BUERGER': 99, 'KAUFLEUTE': 100, 'ARISTOKRATEN': 101,
    'WACHTURM': 129, 'MILITAR': 130, 'KANONENTURM': 131, 'MARKTSTAND': 161, 'STRASSE': 193,
    'RAMPE': 194, 'RAMPELOW': 195, 'RAMPEHIGH': 196, 'PIER': 197, 'BRUECKE': 198, 'PLATZ': 199,
    'GAUKLERPLATZ': 200, 'ZAUN': 201, 'ROHSTOFF': 225, 'ROHSTWACHS': 226, 'ROHSTDUERR': 227,
    'MAUER': 257, 'MAUERUP': 258, 'TOR': 259, 'DENKMAL': 289, 'TRIUMPH': 290, 'ZIERDE': 291,
    'FELSEN': 321, 'KLIPPE': 322, 'STEIN': 323, 'ROHSTSPEZ': 324, 'TIERHOEHLE': 325, 'FLUSS': 353,
    'MEER': 354, 'WASSERFALL': 355, 'RUINE': 385, 'UNUSED': 0}   # src/sim/src/types.cpp kProdtypeNames (AnnoGame 0x100138e0)


def load_registry(game_dir):
    sys.path.insert(0, HERE)
    import figuren
    buf = open(os.path.join(game_dir, 'Anlagen.dat'), 'rb').read()
    top = figuren.records(buf, 16, 16 + struct.unpack_from('<I', buf, 12)[0])
    anl = next(p for n, p in top if n == 'ANLAGEN')
    reg = {}
    for n, p in figuren.records(anl, 0, len(anl)):
        if n != 'ENTRY':
            continue
        e = figuren.entry(p)
        rec = dict(e['records'])
        head = rec.get('HEAD', b'').ljust(56, b'\0')          # the loader zero-fills 56 bytes (0x10009351)
        pt = rec.get('PRODTYPE', b'').split(b'\0')[0].decode()
        word14 = (head[0x2e] << 8) | (head[0x25] & 1)          # type word +0x14 (0x10009432-0x10009449)
        t = Type(head[0], head[1], PRODTYPE_NUMBERS[pt], struct.unpack_from('<H', head, 0x1c)[0], bool(word14 & 0x100))
        t.name = pt
        reg[e['id']] = t
    return reg


def classify(game_dir):
    reg = load_registry(game_dir)
    from collections import Counter, defaultdict
    cnt, names, words = Counter(), defaultdict(Counter), defaultdict(Counter)
    for tid, t in reg.items():
        name, sup = terrain_class(t.flags)
        cnt[(name, sup)] += 1
        names[name][t.name] += 1
        words[name][t.flags] += 1
    print('types %d' % len(reg))
    for (name, sup), n in sorted(cnt.items(), key=lambda kv: (not kv[0][1], -kv[1])):
        print('%-3d %-9s %s' % (n, 'SUPPORTED' if sup else 'refused', name))
        print('      flag words: %s' % ', '.join('0x%04x x%d' % kv for kv in sorted(words[name].items())))
        print('      PRODTYPE:   %s' % ', '.join('%s %d' % kv for kv in names[name].most_common(9)))
    return reg


def corpus(game_dir):
    sys.path.insert(0, HERE)
    import insel
    reg = load_registry(game_dir)
    from collections import Counter
    stat = Counter()
    samples = []
    files = []
    for root, _, fs in os.walk(os.path.join(game_dir, 'Scenes')):
        files += [os.path.join(root, f) for f in fs if f.lower().endswith(('.scp', '.sze'))]
    for path in sorted(files):
        try:
            isls = insel.islands(open(path, 'rb').read())
        except insel.Bad:
            continue
        for i in isls:
            try:
                hs = insel.grid(i, 'HEIGHT', 1)
            except insel.Bad:
                continue
            W, H = i['width'], i['height']

            def vertex(vx, vy, hs=hs, W=W, H=H):
                return hs[vy * W + vx] if 0 <= vx < W and 0 <= vy < H else 0
            isl = Island(0, 0, W, H, vertex, lambda x, y: 0)
            for name in ('PROD2', 'GELAENDE'):
                for bx, by, bw, bh, data in insel.blocks(i, name):
                    for k in range(0, len(data), 6):
                        idrot, state, ix, iy = struct.unpack_from('<HHBB', data, k)
                        tid, rot = idrot & 0xFFF, idrot >> 14
                        t = reg.get(tid)
                        if t is None:
                            stat[(name, 'unknown id')] += 1
                            continue
                        cname, sup = terrain_class(t.flags)
                        want = state >> 8
                        # terrain analysis only (no occupancy: neighbours of the same file are not loaded)
                        try:
                            r = height_analysis(t, rot, lambda dx, dy: vertex(bx + ix + dx, by + iy + dy), t.flags | ANALYSIS)
                        except Unsupported:
                            stat[(name, cname, 'unsupported')] += 1
                            continue
                        key = (name, cname)
                        if r < 0:
                            stat[key + ('analysis rejects the stored placement',)] += 1
                            if len(samples) < 12:
                                samples.append((os.path.basename(path), name, tid, t.name, hex(t.flags), bx + ix, by + iy, rot, want))
                        elif r == want:
                            stat[key + ('byte equals stored height',)] += 1
                        else:
                            stat[key + ('byte differs from stored height',)] += 1
    for k, v in sorted(stat.items(), key=lambda kv: (kv[0][0], str(kv[0][1:]))):
        print('%7d  %s' % (v, ' | '.join(k)))
    for s in samples:
        print('sample reject:', s)


def roads(game_dir):
    """MAP-ROAD-1 R1 evidence: which registry types are land roads (road_class) and how the shipped STRASSE placements
    (PROD2 items of every island) relate to the road rule: stored height vs the mode result, the level floor R-A2 and
    the 0x1000 slope test. Prints counts only; the numbers are quoted in docs/re/road-placement.md section 4."""
    sys.path.insert(0, HERE)
    import insel
    from collections import Counter
    reg = load_registry(game_dir)
    inv = Counter()
    for tid, t in reg.items():
        if t.name in ('STRASSE', 'RAMPE', 'RAMPELOW', 'RAMPEHIGH', 'BRUECKE', 'PIER'):
            inv[(t.name, hex(t.flags), 'road_class' if road_class(t) else 'not road_class')] += 1
    for k, n in sorted(inv.items()):
        print('%4d types  %s' % (n, ' | '.join(k)))
    stat = Counter()
    files = []
    for root, _, fs in os.walk(os.path.join(game_dir, 'Scenes')):
        files += [os.path.join(root, f) for f in fs if f.lower().endswith(('.scp', '.sze'))]
    for path in sorted(files):
        try:
            isls = insel.islands(open(path, 'rb').read())
        except insel.Bad:
            continue
        for i in isls:
            try:
                hs = insel.grid(i, 'HEIGHT', 1)
            except insel.Bad:
                continue
            W, H = i['width'], i['height']

            def vertex(vx, vy, hs=hs, W=W, H=H):
                return hs[vy * W + vx] if 0 <= vx < W and 0 <= vy < H else 0
            for bx, by, bw, bh, data in insel.blocks(i, 'PROD2'):
                for k in range(0, len(data), 6):
                    idrot, state, ix, iy = struct.unpack_from('<HHBB', data, k)
                    t = reg.get(idrot & 0xFFF)
                    if t is None or not road_class(t):
                        continue
                    x, y = bx + ix, by + iy
                    r = height_analysis(t, (idrot >> 14) & 3, lambda dx, dy: vertex(x + dx, y + dy), t.flags | ANALYSIS)
                    cls = hex(t.flags & 0x7000)
                    stat[(cls, 'items')] += 1
                    stat[(cls, 'analysis rejects (class 0x1000 slope)' if r < 0 else ('byte equals stored height' if r == state >> 8 else 'byte differs from stored height'))] += 1
                    if r >= 0:
                        stat[(cls, 'byte >= 32 (floor R-A2 accepts)' if r >= ROAD_FLOOR else 'byte < 32 (floor R-A2 refuses)')] += 1
    for k, n in sorted(stat.items()):
        print('%7d  %s' % (n, ' | '.join(k)))


def main(argv):
    cmd = argv[1] if len(argv) > 1 else ''
    if cmd == 'selftest':
        return 0 if selftest() else 1
    if cmd == 'check':
        return 0 if check_vectors(argv[2] if len(argv) > 2 else DEFAULT_VECTORS) else 1
    if cmd in ('classify', 'corpus', 'roads'):
        gd = argv[2] if len(argv) > 2 else os.environ.get('ANNO1503_GAME_DIR')
        if not gd:
            print('set ANNO1503_GAME_DIR or pass the game directory', file=sys.stderr)
            return 2
        {'classify': classify, 'corpus': corpus, 'roads': roads}[cmd](gd)
        return 0
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
