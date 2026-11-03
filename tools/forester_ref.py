#!/usr/bin/env python3
"""Independent transcription of the forester chain's pure rules (draft for tools/forester_ref.py).

  forester_ref.py [vectors.txt]      runs every vector line and prints PASS/FAIL per line
Exit status: 0 = at least one vector ran and none failed; 1 = a vector failed; 2 = usage error, unreadable file,
or ZERO vectors executed (an empty file or a file with only `type` directives is not a pass).

Evidence for each rule: docs/re/forestry.md (addresses). Nothing here reads a game file.
Rules transcribed (D = disassembly of AnnoWelt.dll / AnnoGame.dll, GOG 2.0.0.5):
  conv     AnnoGame 0x10009627..0x10009717 (rate = ftol(256/T0 + .5), need = ftol(256/T0 * I0), double)
  step     AnnoWelt 0x1006ea90 farm tick (801: max_figures(0) = 2, so the dispatch runs every step)
  area     AnnoWelt 0x10058ae0 (step A rectangle, u16 words)
  harvest  AnnoWelt 0x10087340 (island slot +0x110)
  cand     AnnoWelt 0x10092320 mode 0 (island slot +0x10c)
  grow     AnnoWelt 0x1008b5f0
  candskip A: the candidate list of the enhanced rule forestry_skip_uncuttable (cand + the cut's own id-1 test)
  visit    A: ONE figure call of the harvest body (overlay semantics, section 3.2 of docs/sim/forestry.md); the
           walk is an abstract line (500 ticks per tile) standing in for the existing tile walker
  claims   A/D: one tile's claim bit under the faithful (bit only) and enhanced (owner-keyed) rules
  draws    D: eligible dispatch attempts (= world draws) over a list of farm steps
"""
import sys

CAP0 = 1280          # PROD3[4] << 8 for 801 (5 t)
MAX_W = 2            # PROD3[0x29] for 801
RATE = 14
NEED = 32


def ftol(x):
    return int(x)    # truncation toward zero, like __ftol


def conv(t0, i0):
    q0 = 256.0 / t0
    return ftol(q0 + 0.5), ftol(q0 * i0)


def div_trunc(a, b):
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b >= 0) else -q


def sar7(v):
    # (v + ((v >> 31) & 0x7f)) >> 7 with arithmetic shifts, as cdq / and 0x7f / add / sar 7
    return (v + (0x7f if v < 0 else 0)) >> 7


def step(raw, out, workers, rate=RATE, need=NEED, cap=CAP0, max_w=MAX_W):
    status = 0
    made = 0
    if max_w == 1 and workers > 0:
        pass
    elif cap <= out:
        status = 2
    else:
        ratio = (raw << 7) // need
        if ratio > 128:
            ratio = 128
        if ratio < 64:
            status = 1
        else:
            raw -= sar7(need * ratio)
            made = sar7(rate * ratio)
            out += made
    send = False
    if max_w > 1 or made == 0:
        if workers < max_w and div_trunc(384, rate) >= raw // need:
            send = True
    return raw, out, status, int(send)


def area(x, y, w, h, r):
    x0 = (x + (w - 1) // 2 - r) & 0xFFFF
    y0 = (y + (h - 1) // 2 - r) & 0xFFFF
    return x0, y0, 2 * r + 1 + ((w - 1) % 2), 2 * r + 1 + ((h - 1) % 2)


def harvest(types, tid, phase):
    if tid == 0:
        return tid, phase, 0
    prev = types.get(tid - 1)
    if prev is None or prev[0] != "ROHSTWACHS":
        return tid, phase, 0
    return tid - 1, phase & 0x80, 1


def cand(types, goods, isl, rect, tiles):
    ix, iy, iw, ih = isl
    rx, ry, rw, rh = rect
    x_lo, x_hi = max(rx, ix), min(rx + rw, ix + iw)
    y_lo, y_hi = max(ry, iy), min(ry + rh, iy + ih)
    out = []
    for y in range(y_lo, y_hi):
        for x in range(x_lo, x_hi):
            t = tiles.get((x, y))
            if t is None:
                continue
            tid, claim = t
            ty = types.get(tid)
            if ty is not None and ty[1] == goods and not claim:
                out.append((x, y))
    return out


def grow_first_change(S, k, P, tid, phase, v0, vstep, vend):
    half = P // 2
    t = v0
    obj = tid
    while t <= vend:
        parity = (t // half) & 1
        if parity != ((phase >> 6) & 1):
            counter = ((phase & 0x3F) + 1) & 63
            if (counter & ((1 << (k & 31)) - 1)) == 0 and counter >= (S << (k & 31)):
                return t, obj + 1
            phase = (phase & 0x80) | (parity << 6) | counter
        t += vstep
    return None, obj


def drain(raw):
    """Run production steps from out = 0 until a step makes nothing: starved (status 1) or full output
    (status 2, out >= cap). The step arithmetic is unchanged; only the loop's exit is defined here.
    Returns (raw_left, made_total)."""
    made_total = 0
    out = 0
    while True:
        r2, o2, status, _ = step(raw, out, 2, max_w=MAX_W)   # workers=2: only production matters
        if status != 0 or (r2, o2) == (raw, out):
            return raw, made_total
        made_total += o2 - out
        raw, out = r2, o2


def visit(cap, wait, ticks, trees, stale=False, per_tile=500):
    """ONE harvest-body call (docs/sim/forestry.md 3.2). Home and start at position 0; a walk costs
    per_tile ticks per position; the nearest unclaimed live tree wins (ties: lower position).
    stale=False: the local overlay (claim/cut edits visible to later transitions of the call).
    stale=True : the DEFECT the review found (edits queued until the call ends, reads see the island only).
    Returns (cargo, step, left, claimed, cuts, edits)."""
    island_alive = {p: True for p in trees}
    island_claim = {p: False for p in trees}
    ov_alive, ov_claim = {}, {}
    edits = []

    def alive(p):
        return island_alive[p] if stale else ov_alive.get(p, island_alive[p])

    def claimed(p):
        return island_claim[p] if stale else ov_claim.get(p, island_claim[p])

    cargo, pos, left, cuts = 0, 0, ticks, []
    step_name, need, tgt = "Start", 0, None
    while True:
        if step_name == "Start":
            cands = [p for p in sorted(trees) if alive(p) and not claimed(p)]
            if not cands:
                step_name, need = "ToHome", abs(pos) * per_tile
                continue
            tgt = min(cands, key=lambda p: (abs(p - pos), p))
            edits.append("claim%d" % tgt)
            ov_claim[tgt] = True
            step_name, need = "ToTree", abs(tgt - pos) * per_tile
        elif step_name == "ToTree":
            if left < need:
                need -= left
                return _visit_result(cargo, step_name, need, ov_claim, cuts, edits, stale)
            left -= need
            pos = tgt
            step_name, need = "Cut", wait
        elif step_name == "Cut":
            if left < need:
                need -= left
                return _visit_result(cargo, step_name, need, ov_claim, cuts, edits, stale)
            left -= need
            ok = alive(tgt)                         # the planned consumption's own validity test
            edits.append("unclaim%d" % tgt)
            ov_claim[tgt] = False
            edits.append("cut%d" % tgt)
            ov_alive[tgt] = False
            cuts.append(tgt)
            if ok:
                cargo += 256
            step_name = "Start" if cap > cargo else "ToHome"
            need = abs(pos) * per_tile
        elif step_name == "ToHome":
            if left < need:
                need -= left
                return _visit_result(cargo, step_name, need, ov_claim, cuts, edits, stale)
            left -= need
            return _visit_result(cargo, "Done", left, ov_claim, cuts, edits, stale)


def _visit_result(cargo, step_name, left, claim, cuts, edits, stale):
    held = ",".join(str(p) for p in sorted(p for p, v in claim.items() if v)) or "-"
    return (cargo, step_name, left, held, ",".join(map(str, cuts)) or "-", edits)


def claims(mode, ops):
    """One tile's claim under the two rule sets. ops: `W+` select-and-claim (refused when the bit is set),
    `W-` unclaim, `P` placement (clears the bit: D 0x100824a7). faithful: only the bit exists, every clear is
    unconditional (D 0x100545c0, 0x10054530). enhanced: the bit is mirrored by an owner (the worker's figure id);
    a worker clears only its own claim. Returns (bit, owner)."""
    bit, owner = 0, None
    for op in ops:
        if op == "P":
            bit, owner = 0, None
        elif op.endswith("+"):
            if bit == 0:
                bit, owner = 1, op[:-1]
        elif op.endswith("-"):
            if mode == "faithful" or owner == op[:-1]:
                bit, owner = 0, None
        else:
            raise ValueError("bad claim op " + op)
    return bit, ("-" if (mode == "faithful" or owner is None) else owner)


def draws(steps):
    """Eligible dispatch attempts = world draws (D 0x1006edf3..0x1006ee06): the steps whose send test passes.
    A refused send (0x10072d20 answers 0) is still a draw; success is not required."""
    n = 0
    for raw, out, workers in steps:
        if step(raw, out, workers)[3]:
            n += 1
    return n


def run_line(line, types):
    """Returns (got, exp) lists of strings for one vector line."""
    left, _, right = line.partition("->")
    cmd, *args = left.split()
    exp = right.split()
    if cmd == "conv":
        got = list(map(str, conv(float(args[0]), float(args[1]))))
    elif cmd == "step":
        got = list(map(str, step(int(args[0]), int(args[1]), int(args[2]))))
    elif cmd == "area":
        got = list(map(str, area(*map(int, args))))
    elif cmd == "harvest":
        r = harvest(types, int(args[0]), int(args[1], 0))
        got = [str(r[0]), "0x%02X" % r[1], str(r[2])]
        exp = [exp[0], "0x%02X" % int(exp[1], 0)] + exp[2:]
    elif cmd in ("cand", "candskip"):
        goods = args[0]
        parts = " ".join(args[1:]).split("|")
        isl = tuple(map(int, parts[0].split()))
        rect = tuple(map(int, parts[1].split()))
        tiles = {}
        for tok in parts[2].split():
            x, y, i, c = map(int, tok.split(","))
            tiles[(x, y)] = (i, c)
        lst = cand(types, goods, isl, rect, tiles)
        if cmd == "candskip":
            lst = [p for p in lst if harvest(types, tiles[p][0], 0)[2]]   # the cut's own id-1 test
        got = ["%d,%d" % p for p in lst]
        exp = parts[3].split()      # the expected list is the text after the third '|'
    elif cmd == "grow":
        S, k, P, tid = int(args[0]), int(args[1]), int(args[2]), int(args[3])
        phase = int(args[4], 0)
        t, obj = grow_first_change(S, k, P, tid, phase, int(args[5]), int(args[6]), int(args[7]))
        got = [str(t), str(obj)]
    elif cmd == "drain":
        got = list(map(str, drain(int(args[0]))))
    elif cmd in ("visit", "visitstale"):
        cap, wait, ticks = int(args[0]), int(args[1]), int(args[2])
        trees = [int(a) for a in " ".join(args[3:]).split("|")[1].split()]
        r = visit(cap, wait, ticks, trees, stale=(cmd == "visitstale"))
        got = [str(r[0]), r[1], str(r[2]), r[3], r[4]]
    elif cmd == "claims":
        bit, owner = claims(args[0], args[1:])
        got = [str(bit), owner]
    elif cmd == "draws":
        steps = [tuple(map(int, a.split(","))) for a in args]
        got = [str(draws(steps))]
    elif cmd == "walkcode":
        f, s2 = int(args[0]), int(args[1])
        got = [str(4 if (f == 0 and s2 == 0) else f)]
    elif cmd == "goalfree":
        got = ["0x%03x" % (0x1FF - int(args[0], 0))]
    else:
        raise ValueError("UNKNOWN command " + cmd)
    return got, exp


def main(path):
    try:
        text = open(path).read().splitlines()
    except OSError as e:
        print("ERROR: cannot read %s: %s" % (path, e))
        return 2
    types = {}
    fails = 0
    n = 0
    for line in text:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("type "):
            p = line.split()
            types[int(p[1])] = (p[2], p[3] if len(p) > 3 else "UNUSED")
            continue
        n += 1
        try:
            got, exp = run_line(line, types)
        except Exception as e:      # a malformed line is a failed vector, never a silent skip
            print("FAIL " + line + "   error=%s: %s" % (type(e).__name__, e))
            fails += 1
            continue
        ok = got == exp
        if not ok:
            fails += 1
        print(("PASS " if ok else "FAIL ") + line + ("" if ok else "   got=" + " ".join(got)))
    print("%d vectors, %d failures" % (n, fails))
    if n == 0:
        print("ERROR: 0 vectors executed in %s (an empty file or a file with only type directives "
              "is not a pass)" % path)
        return 2
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "forester_vectors.txt"))
