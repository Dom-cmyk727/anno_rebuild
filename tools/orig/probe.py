"""T15 object probe: locate a game object in memory by its vtable and read its fields (OQ-T15-BLD).

Generalises smoke.py's world-object locator: scan the committed writable heap for a DWORD equal to
`module base + vtable RVA` (the first field of a C++ object is its vtable pointer), and for each hit
read the requested fields. This is how the production/population fixtures reach a specific building,
city or player object without a debugger (the RE notes give the vtable RVA and the field offsets;
bases rebase at run time, so the value scanned for is `module base + RVA`).

Read-only. Standard-library ctypes only. Run with the game already launched (tools/orig/control.py).

  python -I tools/orig/probe.py --game C:\\anno1503-original --module AnnoWelt.dll --vtable 0xc2f40 \\
         --read 0x624:u32 --read 0x334:u32 --limit 20

`--read OFF:TYPE` (u8/u16/u32/i32/u64) may be repeated; each hit reports those fields. With no
--read, it just lists the hit addresses. --store G reads the 12-byte warehouse record at +0x334+12G
(u32 stock, u16 consumed, u16 added, u16 removed) for goods number G (city fixtures).
"""

from __future__ import annotations

import argparse
import ctypes
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smoke  # noqa: E402

kernel32 = smoke.kernel32

_TYPES = {"u8": (1, "<B"), "u16": (2, "<H"), "i16": (2, "<h"), "u32": (4, "<I"),
          "i32": (4, "<i"), "u64": (8, "<Q")}


def scan_vtable(pid: int, vtable_value: int, limit: int, align: int = 4) -> tuple[list[int], int]:
    """Addresses of committed writable heap locations whose first DWORD == vtable_value and that are
    `align`-byte aligned (a C++ object's vtable is at its start, so a real object is pointer-aligned;
    the alignment filter drops the value appearing unaligned inside some other structure). Returns
    (hits, regions_skipped): regions_skipped counts committed writable regions larger than the 64 MiB
    read cap that were not scanned. NOTE: a freed object keeps its vtable pointer, so a hit is not
    proof of a live object — prune with a field invariant (the --require option, or reaching the
    object from the world/island list)."""
    hits: list[int] = []
    skipped = 0
    handle = smoke.open_read(pid)
    if not handle:
        return hits, skipped
    try:
        needle = struct.pack("<I", vtable_value)
        addr = 0
        mbi = smoke.MEMORY_BASIC_INFORMATION()
        while addr < 0x7FFF0000 and len(hits) < limit:
            if not kernel32.VirtualQueryEx(handle, ctypes.c_void_p(addr), ctypes.byref(mbi),
                                           ctypes.sizeof(mbi)):
                break
            region = int(mbi.BaseAddress or addr)
            size = int(mbi.RegionSize)
            if mbi.State == 0x1000 and mbi.Protect in (0x04, 0x40):
                if size > (64 << 20):
                    skipped += 1
                else:
                    data = smoke.read_memory(handle, region, size)
                    if data:
                        i = data.find(needle)
                        while i != -1 and len(hits) < limit:
                            if (region + i) % align == 0:   # pointer-aligned object start only
                                hits.append(region + i)
                            i = data.find(needle, i + 4)
            addr = region + size
        return hits, skipped
    finally:
        kernel32.CloseHandle(handle)


def read_fields(pid: int, obj: int, reads: list[tuple[int, str]]) -> dict:
    handle = smoke.open_read(pid)
    out: dict = {}
    if not handle:
        return out
    try:
        for off, ty in reads:
            size, fmt = _TYPES[ty]
            b = smoke.read_memory(handle, obj + off, size)
            out[f"+{off:#x}:{ty}"] = struct.unpack(fmt, b)[0] if b else None
        return out
    finally:
        kernel32.CloseHandle(handle)


def read_store(pid: int, obj: int, goods: int) -> dict:
    """The 12-byte warehouse record at city +0x334 + 12*g (production.md:325)."""
    handle = smoke.open_read(pid)
    if not handle:
        return {}
    try:
        b = smoke.read_memory(handle, obj + 0x334 + 12 * goods, 12)
        if not b:
            return {}
        stock, consumed, added, removed = struct.unpack_from("<IHHH", b, 0)
        return {"goods": goods, "stock_8_8": stock, "stock_tons": round(stock / 256.0, 3),
                "consumed": consumed, "added": added, "removed": removed}
    finally:
        kernel32.CloseHandle(handle)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True)
    ap.add_argument("--module", default="AnnoWelt.dll")
    ap.add_argument("--vtable", required=True, help="vtable RVA, e.g. 0xc2f40 (world 0xccdd8)")
    ap.add_argument("--read", action="append", default=[], help="OFF:TYPE, repeatable")
    ap.add_argument("--store", type=int, action="append", default=[], help="goods number(s) to read")
    ap.add_argument("--limit", type=int, default=40)
    ap.add_argument("--align", type=int, default=4, help="object-start alignment filter (4 or 8)")
    ap.add_argument("--require", action="append", default=[],
                    help="prune freed/ghost objects: OFF:TYPE:MIN:MAX, keep only those whose field "
                         "is in [MIN,MAX]; repeatable")
    ap.add_argument("--out", default=None)
    args = ap.parse_args()

    def parse_int(x: str) -> int:
        return int(x, 16) if x.lower().startswith("0x") else int(x)

    game_dir = os.path.abspath(args.game)
    vtable_rva = parse_int(args.vtable)   # 0x.. = hex, else decimal (consistent with --read)
    reads: list[tuple[int, str]] = []
    for r in args.read:
        off, ty = r.split(":")
        reads.append((parse_int(off), ty))
    requires: list[tuple[int, str, int, int]] = []
    for r in args.require:
        off, ty, lo, hi = r.split(":")
        requires.append((parse_int(off), ty, parse_int(lo), parse_int(hi)))

    result: dict = {"module": args.module, "vtable_rva": hex(vtable_rva)}
    pids = smoke.pids_under(game_dir)
    for pid in pids:
        base = smoke.module_base(pid, args.module)
        if not base:
            continue
        vtable_value = base[0] + vtable_rva
        hits, skipped = scan_vtable(pid, vtable_value, args.limit, args.align)
        objs = []
        for h in hits:
            if requires:
                fv = read_fields(pid, h, [(o, t) for o, t, _, _ in requires])
                def _ok(o: int, t: str, lo: int, hi: int) -> bool:
                    v = fv.get(f"+{o:#x}:{t}")
                    return v is not None and lo <= v <= hi
                if not all(_ok(*req) for req in requires):
                    continue   # prune a freed/ghost object or a value out of the plausible range
            rec: dict = {"addr": hex(h)}
            if reads:
                rec["fields"] = read_fields(pid, h, reads)
            if args.store:
                rec["store"] = [read_store(pid, h, g) for g in args.store]
            objs.append(rec)
        result.update({"pid": pid, "module_base": hex(base[0]), "vtable_value": hex(vtable_value),
                       "hit_count": len(hits), "kept_count": len(objs),
                       "regions_over_cap_skipped": skipped, "align": args.align})
        result["objects"] = objs
        break
    print(json.dumps(result, indent=2))
    if args.out:
        os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
        with open(args.out, "w", encoding="utf-8") as f:
            json.dump(result, f, indent=2)
    return 0 if result.get("hit_count") else 1


if __name__ == "__main__":
    sys.exit(main())
