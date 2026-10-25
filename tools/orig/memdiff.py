"""Read-only memory diffing of the running original (T15 play layer, stage B: the camera and the
hovered tile, docs/testing/play-original.md section 3 item 1).

Snapshots every committed writable region (<= 64 MiB each) of the game process with
ReadProcessMemory and keeps them in files under cache/orig/<session>/mem/; `find` then narrows a
candidate set across snapshots by a rule per step: which snapshot pairs must show a change (and of
what size) and which must show none. The workflow from the shell, between tools/orig/control.py
calls (the game paused with P so the simulation does not move):

  python -I tools/orig/memdiff.py --game C:\\anno1503-original --session s snap s0
  python -I tools/orig/control.py --game C:\\anno1503-original --session s move 0.50 0.50
  python -I tools/orig/memdiff.py --game C:\\anno1503-original --session s snap s1
  ...
  python -I tools/orig/memdiff.py --session s find --width 2 \\
      --changed s0:s1 --changed s1:s2 --same s2:s3 --maxdelta 3

`find` prints the addresses (and the value sequence over the named snapshots) whose `--width`-byte
little-endian value changed between every `--changed` pair by at most `--maxdelta` (0 = any change)
and stayed equal between every `--same` pair. Standard library plus numpy (present on the host's
Python; `-I` keeps user site-packages out, the system site stays). Nothing here writes memory, and
the snapshot files are derived from the original's process: they live under cache/ (git-ignored).
"""
from __future__ import annotations

import argparse
import ctypes
import json
import os
import struct
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smoke  # noqa: E402

kernel32 = smoke.kernel32
MAX_REGION = 64 << 20


def regions(handle) -> list[tuple[int, int]]:
    out = []
    mbi = smoke.MEMORY_BASIC_INFORMATION()
    addr = 0
    while addr < 0xFFFF0000:
        if not kernel32.VirtualQueryEx(handle, ctypes.c_void_p(addr), ctypes.byref(mbi),
                                       ctypes.sizeof(mbi)):
            break
        base, size = int(mbi.BaseAddress or addr), int(mbi.RegionSize)
        if mbi.State == 0x1000 and mbi.Protect in (0x04, 0x40) and size <= MAX_REGION:
            out.append((base, size))
        addr = base + size
    return out


def snap(pid: int, out_dir: str, name: str) -> dict:
    handle = smoke.open_read(pid)
    if not handle:
        raise SystemExit("cannot open the process for reading")
    try:
        t0 = time.time()
        regs = regions(handle)
        index = []
        total = 0
        with open(os.path.join(out_dir, f"{name}.bin"), "wb") as f:
            for base, size in regs:
                data = smoke.read_memory(handle, base, size)
                if data is None:
                    continue
                index.append({"base": base, "size": size, "off": total})
                f.write(data)
                total += size
        meta = {"name": name, "pid": pid, "time": time.time(), "regions": index, "bytes": total,
                "seconds": round(time.time() - t0, 3)}
        with open(os.path.join(out_dir, f"{name}.json"), "w", encoding="utf-8") as f:
            json.dump(meta, f)
        return meta
    finally:
        kernel32.CloseHandle(handle)


class Snap:
    def __init__(self, out_dir: str, name: str):
        with open(os.path.join(out_dir, f"{name}.json"), encoding="utf-8") as f:
            self.meta = json.load(f)
        self.data = np.fromfile(os.path.join(out_dir, f"{name}.bin"), dtype=np.uint8)
        self.by_base = {r["base"]: r for r in self.meta["regions"]}

    def region(self, base: int, size: int):
        r = self.by_base.get(base)
        if r is None or r["size"] != size:
            return None
        return self.data[r["off"]:r["off"] + size]


def values(s: Snap, base: int, size: int, width: int):
    v = s.region(base, size)
    if v is None:
        return None
    dt = {1: np.uint8, 2: np.uint16, 4: np.uint32}[width]
    n = size // width * width
    return v[:n].view(dt)


def find(out_dir: str, names: list[str], changed: list[tuple[str, str]], same: list[tuple[str, str]],
         width: int, maxdelta: int, limit: int, delta: list[tuple[str, str, int]] = ()) -> list[dict]:
    snaps = {n: Snap(out_dir, n) for n in names}
    first = snaps[names[0]]
    hits: list[dict] = []
    for r in first.meta["regions"]:
        base, size = r["base"], r["size"]
        mask = None
        ok = True
        for a, b in changed:
            va, vb = values(snaps[a], base, size, width), values(snaps[b], base, size, width)
            if va is None or vb is None:
                ok = False
                break
            d = va.astype(np.int64) - vb.astype(np.int64)
            m = d != 0
            if maxdelta:
                m &= np.abs(d) <= maxdelta
            mask = m if mask is None else (mask & m)
        if not ok:
            continue
        for a, b in same:
            va, vb = values(snaps[a], base, size, width), values(snaps[b], base, size, width)
            if va is None or vb is None:
                ok = False
                break
            m = va == vb
            mask = m if mask is None else (mask & m)
        for a, b, d in delta:
            va, vb = values(snaps[a], base, size, width), values(snaps[b], base, size, width)
            if va is None or vb is None:
                ok = False
                break
            m = (vb.astype(np.int64) - va.astype(np.int64)) == d
            mask = m if mask is None else (mask & m)
        if not ok or mask is None:
            continue
        idx = np.nonzero(mask)[0]
        for i in idx[:max(0, limit - len(hits))]:
            addr = base + int(i) * width
            seq = [int(values(snaps[n], base, size, width)[i]) for n in names]
            hits.append({"addr": addr, "values": seq})
        if len(hits) >= limit:
            break
    return hits


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", default=r"C:\anno1503-original")
    ap.add_argument("--session", required=True)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p_snap = sub.add_parser("snap")
    p_snap.add_argument("name")
    p_find = sub.add_parser("find")
    p_find.add_argument("--width", type=int, default=2, choices=[1, 2, 4])
    p_find.add_argument("--changed", action="append", default=[], help="a:b snapshot pair")
    p_find.add_argument("--same", action="append", default=[], help="a:b snapshot pair")
    p_find.add_argument("--maxdelta", type=int, default=0)
    p_find.add_argument("--delta", action="append", default=[], help="a:b:d exact change b - a")
    p_find.add_argument("--limit", type=int, default=200)
    p_read = sub.add_parser("read", help="read live values at addresses (hex, comma-separated)")
    p_read.add_argument("addrs")
    p_read.add_argument("--width", type=int, default=2, choices=[1, 2, 4])
    p_read.add_argument("--count", type=int, default=1, help="consecutive values per address")
    args = ap.parse_args(argv)

    out_dir = os.path.join("cache", "orig", args.session, "mem")
    os.makedirs(out_dir, exist_ok=True)
    if args.cmd == "snap":
        pids = [p for p in smoke.pids_under(os.path.abspath(args.game))
                if smoke.module_base(p, "AnnoWelt.dll")]
        if not pids:
            print("no game process with AnnoWelt.dll")
            return 1
        meta = snap(pids[0], out_dir, args.name)
        print({"name": meta["name"], "regions": len(meta["regions"]), "MiB": meta["bytes"] >> 20,
               "seconds": meta["seconds"]})
        return 0
    if args.cmd == "find":
        pairs = lambda xs: [tuple(x.split(":")) for x in xs]  # noqa: E731
        changed, same = pairs(args.changed), pairs(args.same)
        delta = [(x.split(":")[0], x.split(":")[1], int(x.split(":")[2])) for x in args.delta]
        names: list[str] = []
        for a, b in changed + same + [(a, b) for a, b, _ in delta]:
            for n in (a, b):
                if n not in names:
                    names.append(n)
        hits = find(out_dir, names, changed, same, args.width, args.maxdelta, args.limit, delta)
        print(json.dumps({"snapshots": names, "hits": len(hits)}))
        for h in hits:
            print(f"{h['addr']:08x}  {h['values']}")
        return 0
    if args.cmd == "read":
        pids = [p for p in smoke.pids_under(os.path.abspath(args.game))
                if smoke.module_base(p, "AnnoWelt.dll")]
        handle = smoke.open_read(pids[0])
        fmt = {1: "<B", 2: "<H", 4: "<I"}[args.width]
        for a in args.addrs.split(","):
            addr = int(a, 16)
            b = smoke.read_memory(handle, addr, args.width * args.count)
            vals = None if b is None else [struct.unpack_from(fmt, b, i * args.width)[0]
                                           for i in range(args.count)]
            print(f"{addr:08x} {vals}")
        kernel32.CloseHandle(handle)
        return 0
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
