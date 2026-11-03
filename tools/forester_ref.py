#!/usr/bin/env python3
"""Independent transcription of the forester chain's pure rules (draft for tools/forester_ref.py).

  forester_ref.py [vectors.txt]      runs every vector line and prints PASS/FAIL per line

Evidence for each rule: docs/re/forestry.md (addresses). Nothing here reads a game file.
Rules transcribed (D = disassembly of AnnoWelt.dll / AnnoGame.dll, GOG 2.0.0.5):
  conv     AnnoGame 0x10009627..0x10009717 (rate = ftol(256/T0 + .5), need = ftol(256/T0 * I0), double)
  step     AnnoWelt 0x1006ea90 farm tick (801: max_figures(0) = 2, so the dispatch runs every step)
  area     AnnoWelt 0x10058ae0 (step A rectangle, u16 words)
  harvest  AnnoWelt 0x10087340 (island slot +0x110)
  cand     AnnoWelt 0x10092320 mode 0 (island slot +0x10c)
  grow     AnnoWelt 0x1008b5f0
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
    made_total = 0
    out = 0
    while True:
        r2, o2, status, _ = step(raw, out, 2, max_w=MAX_W)   # workers=2: only production matters
        if status == 1:
            return raw, made_total
        made_total += o2 - out
        raw, out = r2, o2


def main(path):
    types = {}
    fails = 0
    n = 0
    for line in open(path):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        n += 1
        if line.startswith("type "):
            p = line.split()
            types[int(p[1])] = (p[2], p[3] if len(p) > 3 else "UNUSED")
            n -= 1
            continue
        left, _, right = line.partition("->")
        cmd, *args = left.split()
        exp = right.split()
        ok = False
        got = None
        if cmd == "conv":
            got = list(map(str, conv(float(args[0]), float(args[1]))))
        elif cmd == "step":
            got = list(map(str, step(int(args[0]), int(args[1]), int(args[2]))))
        elif cmd == "area":
            got = list(map(str, area(*map(int, args))))
        elif cmd == "harvest":
            r = harvest(types, int(args[0]), int(args[1], 0))
            got = [str(r[0]), "0x%02X" % r[1], str(r[2])]
            exp = [exp[0], "0x%02X" % int(exp[1], 0), exp[2]]
        elif cmd == "cand":
            goods = args[0]
            parts = " ".join(args[1:]).split("|")
            isl = tuple(map(int, parts[0].split()))
            rect = tuple(map(int, parts[1].split()))
            tiles = {}
            for tok in parts[2].split():
                x, y, i, c = map(int, tok.split(","))
                tiles[(x, y)] = (i, c)
            got = ["%d,%d" % p for p in cand(types, goods, isl, rect, tiles)]
            # the expected list is the text after the third '|'
            exp = parts[3].split()
        elif cmd == "grow":
            S, k, P, tid = int(args[0]), int(args[1]), int(args[2]), int(args[3])
            phase = int(args[4], 0)
            t, obj = grow_first_change(S, k, P, tid, phase, int(args[5]), int(args[6]), int(args[7]))
            got = [str(t), str(obj)]
        elif cmd == "drain":
            got = list(map(str, drain(int(args[0]))))
        elif cmd == "walkcode":
            f, s2 = int(args[0]), int(args[1])
            got = [str(4 if (f == 0 and s2 == 0) else f)]
        elif cmd == "goalfree":
            got = ["0x%03x" % (0x1FF - int(args[0], 0))]
        else:
            print("UNKNOWN", line)
            fails += 1
            continue
        ok = got == exp
        if not ok:
            fails += 1
        print(("PASS " if ok else "FAIL ") + line + ("" if ok else "   got=" + " ".join(got)))
    print("%d vectors, %d failures" % (n, fails))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "forester_vectors.txt"))
