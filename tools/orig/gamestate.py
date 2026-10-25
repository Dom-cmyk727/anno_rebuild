"""Read-only state readers for the running original, independent of the backend (T16).

Every function takes a memory object with two methods,

    mem.read(addr, n) -> bytes | None        # n bytes at a virtual address, None when unreadable
    mem.regions() -> iterable of (lo, hi)    # committed, writable regions (where heap objects live)

so the same code runs on the Windows backend (ReadProcessMemory / VirtualQueryEx) and inside the
Wine runner (/proc/<pid>/mem and /proc/<pid>/maps). Nothing here writes memory.

Offsets and their evidence (module RVAs relative to AnnoWelt.dll's load base):
  world object   vtable AnnoWelt+0xccdd8; u64 game-ms counter +0x100, tick +0x10c = counter >> 1,
                 PRNG cursor u16 +0xfc                          docs/re/tick-loop.md section 2-3
  PRNG table     AnnoWelt+0xcb8f8, 2048 u16 = MSVC rand() from seed 1   tick-loop.md section 3 (F6)
  city object    vtable AnnoWelt+0xc2f40; store +0x334 + 12*g (u32 amount, 1 t = 256),
                 capacity u32 +0x624, inhabitants i32 +0x2f0   docs/re/population.md (city store),
                                                                docs/testing/play-original.md section 2
  goods ids      tools 41, wood 42, bricks 43 (T15 stage 1: equal to the HUD at game start)
  player object  vtable AnnoWelt+0xc2238; +0x08 world pointer, +0x24 name[32], +0x44 kind[32]
                 ("HUMAN", "CG01", "VENEZIANER", ...), +0x64 GUID (the human's = the profile's
                 USGUID), money i32 +0x74                       docs/re/population.md (player
                 money; confirmed by running 2026-10-22, T16)
  view object V  game HWND -> GetWindowLong(GWL_USERDATA) -> V, [V] = AnnoFrame+0x86428 / +0x8664c;
                 f32 -x +0x1608, f32 -z +0x1610 (world tiles), u8 rotation +0x15f0, u8 zoom group
                 +0x1604, f32 scale +0x15f4                    docs/re/view.md section 3.2-3.3
Standard library only; import-safe under `python -I`.
"""

from __future__ import annotations

import struct
import time

WELT_WORLD_VT = 0xCCDD8
WELT_CITY_VT = 0xC2F40
PRNG_RVA = 0xCB8F8
PRNG_ENTRIES = 2048

WORLD_COUNTER = 0x100
WORLD_AT108 = 0x108
WORLD_TICK = 0x10C
WORLD_CURSOR = 0xFC

CITY_STORE = 0x334
CITY_STORE_STRIDE = 12
CITY_CAPACITY = 0x624
CITY_INHABITANTS = 0x2F0
STORE_UNIT = 256                      # store amounts and capacity are in 1/256 t
GOODS = {"tools": 41, "wood": 42, "bricks": 43}

WELT_PLAYER_VT = 0xC2238
PLAYER_WORLD = 0x08
PLAYER_NAME = 0x24
PLAYER_KIND = 0x44
PLAYER_TEXT = 32                      # name and kind fields are 32 bytes each (zero padded)
PLAYER_GUID = 0x64
PLAYER_MONEY = 0x74

# View object V (AnnoFrame.dll tFrameGame / tFrameGameFull base), reached through the game window's
# GWL_USERDATA (set at AnnoFrame 0x100087d0). docs/re/view.md section 3.2-3.3.
GWL_USERDATA = -21
FRAME_VIEW_VTS = (0x86428, 0x8664C)   # base vtables of tFrameGame / tFrameGameFull (RVAs)
VIEW_ROTATION = 0x15F0                # u8, 0..3
VIEW_SCALE = 0x15F4                   # f32, 1.0 / 0.5 / 0.25 for zoom groups 0 / 1 / other
VIEW_ZOOM_GROUP = 0x1604              # u8
VIEW_TRANS_X = 0x1608                 # f32, negated world tile x
VIEW_TRANS_Z = 0x1610                 # f32, negated world tile z

# Plausibility bound for a city capacity read (in store units). A freed or unrelated object that
# still carries the city vtable (review N1) shows zero or garbage here. The bound only filters
# scan hits; it is not a game limit (the largest Kontor capacity seen is far below it).
CITY_CAPACITY_SANE = 100000 * STORE_UNIT


def u16(b: bytes, off: int = 0) -> int:
    return struct.unpack_from("<H", b, off)[0]


def u32(b: bytes, off: int = 0) -> int:
    return struct.unpack_from("<I", b, off)[0]


def i32(b: bytes, off: int = 0) -> int:
    return struct.unpack_from("<i", b, off)[0]


def scan_u32(mem, value: int, limit: int | None = None) -> list[int]:
    """Every 4-aligned address in the writable regions whose u32 equals value (vtable scans).
    Unbounded by default; `limit` is for interactive value searches, whose callers report it."""
    needle = struct.pack("<I", value)
    hits: list[int] = []
    for lo, hi in mem.regions():
        data = mem.read(lo, hi - lo)
        if not data:
            continue
        i = data.find(needle)
        while i != -1:
            if (lo + i) % 4 == 0:
                hits.append(lo + i)
                if limit is not None and len(hits) >= limit:
                    return hits
            i = data.find(needle, i + 1)
    return hits


def world_fields(mem, obj: int) -> dict | None:
    b = mem.read(obj + WORLD_CURSOR, WORLD_TICK + 4 - WORLD_CURSOR)
    if not b:
        return None
    base = WORLD_CURSOR
    return {"counter": struct.unpack_from("<Q", b, WORLD_COUNTER - base)[0],
            "at108": u32(b, WORLD_AT108 - base),
            "tick": u32(b, WORLD_TICK - base),
            "cursor": u16(b, 0)}


def world_counter(mem, obj: int) -> int | None:
    """The 64-bit game-ms counter alone (the timing sampler reads only this)."""
    b = mem.read(obj + WORLD_COUNTER, 8)
    return struct.unpack("<Q", b)[0] if b and len(b) == 8 else None


def is_world(f: dict | None) -> bool:
    """Counter non-zero and tick == counter >> 1 exactly (as tools/orig/smoke.py on Windows).
    Unmasked on purpose: a stray hit whose garbage 64-bit 'counter' has 33 low zero bits would
    pass a masked compare with tick 0 (seen under Wine, 2026-10-22). Valid while the counter is
    below 2^33 ms (about 99 days of game time); the tick is stored as 32 bits (tick-loop.md)."""
    return bool(f) and f["counter"] != 0 and f["tick"] == f["counter"] >> 1


def world_candidates(mem, welt_base: int) -> list[int]:
    """Objects whose first DWORD is the world vtable and for which is_world() holds. This rejects
    stray matches of the vtable value; a freed copy with an intact header would still pass."""
    return [obj for obj in scan_u32(mem, welt_base + WELT_WORLD_VT) if is_world(world_fields(mem, obj))]


def find_world(mem, welt_base: int, settle: float = 0.0) -> int | None:
    """The live world object. With several candidates and settle > 0, the one whose counter
    advances within `settle` seconds wins (a stale copy does not advance); otherwise the first in
    address order. Returns None when there is no candidate."""
    cands = world_candidates(mem, welt_base)
    if len(cands) <= 1 or settle <= 0:
        return cands[0] if cands else None
    before = {c: world_counter(mem, c) for c in cands}
    time.sleep(settle)
    for c in cands:
        now = world_counter(mem, c)
        if now is not None and before[c] is not None and now > before[c]:
            return c
    return cands[0]


def msvc_rand(n: int, seed: int = 1) -> list[int]:
    x, out = seed, []
    for _ in range(n):
        x = (x * 214013 + 2531011) & 0xFFFFFFFF
        out.append((x >> 16) & 0x7FFF)
    return out


def prng_check(mem, welt_base: int) -> dict:
    """F6 (read-only form): the table at AnnoWelt+0xcb8f8 against MSVC rand() seed 1."""
    raw = mem.read(welt_base + PRNG_RVA, 2 * PRNG_ENTRIES)
    if not raw or len(raw) != 2 * PRNG_ENTRIES:
        return {"error": "PRNG table unreadable"}
    table = struct.unpack(f"<{PRNG_ENTRIES}H", raw)
    expect = msvc_rand(PRNG_ENTRIES)
    return {"entries": PRNG_ENTRIES, "mismatches": sum(a != b for a, b in zip(table, expect)),
            "head": list(table[:6])}


def _text(b: bytes) -> str | None:
    """A zero-terminated Latin-1 text field (player names may hold umlauts), else None."""
    s = b.split(b"\0", 1)[0]
    return s.decode("latin-1") if all(0x20 <= c < 0x7F or 0xA0 <= c for c in s) else None


def players(mem, welt_base: int, world: int | None = None) -> list[dict]:
    """Every player object whose world pointer is the live world (freed objects keep their
    vtable, review N1) and whose name and kind fields are text."""
    world = world if world is not None else find_world(mem, welt_base)
    out = []
    for obj in scan_u32(mem, welt_base + WELT_PLAYER_VT):
        b = mem.read(obj, PLAYER_MONEY + 4)
        if not b or u32(b, PLAYER_WORLD) != world:
            continue
        name = _text(b[PLAYER_NAME:PLAYER_NAME + PLAYER_TEXT])
        kind = _text(b[PLAYER_KIND:PLAYER_KIND + PLAYER_TEXT])
        if name is None or kind is None:
            continue
        out.append({"addr": hex(obj), "name": name, "kind": kind,
                    "guid": b[PLAYER_GUID:PLAYER_GUID + 16].hex(), "money": i32(b, PLAYER_MONEY)})
    return out


def city_fields(mem, obj: int) -> dict | None:
    cap = mem.read(obj + CITY_CAPACITY, 4)
    inh = mem.read(obj + CITY_INHABITANTS, 4)
    if not cap or not inh:
        return None
    rec = {"addr": hex(obj), "capacity_t": u32(cap) / STORE_UNIT, "inhabitants": i32(inh)}
    for name, g in GOODS.items():
        b = mem.read(obj + CITY_STORE + CITY_STORE_STRIDE * g, 4)
        rec[f"{name}_t"] = u32(b) / STORE_UNIT if b else None
    return rec


def cities(mem, welt_base: int) -> list[dict]:
    """Every city object that passes the plausibility check (capacity in range)."""
    out = []
    for obj in scan_u32(mem, welt_base + WELT_CITY_VT):
        cap = mem.read(obj + CITY_CAPACITY, 4)
        if not cap or not 0 < u32(cap) <= CITY_CAPACITY_SANE:
            continue
        rec = city_fields(mem, obj)
        if rec:
            out.append(rec)
    return out


def window_userdata(hwnd: int) -> int | None:
    """GetWindowLongW(hwnd, GWL_USERDATA) as an unsigned 32-bit value (Windows only, read-only).
    A WOW64 game window keeps a 32-bit pointer there; None when the call fails or is unavailable."""
    try:
        import ctypes  # noqa: PLC0415
        user32 = ctypes.WinDLL("user32", use_last_error=True)
    except (ImportError, AttributeError, OSError):
        return None
    user32.GetWindowLongW.argtypes = [ctypes.c_void_p, ctypes.c_int]
    user32.GetWindowLongW.restype = ctypes.c_long
    v = user32.GetWindowLongW(hwnd, GWL_USERDATA) & 0xFFFFFFFF
    return v or None


def camera(mem, hwnd: int, frame_base: int, userdata=window_userdata) -> dict | None:
    """The original's camera: the view object V from the game window's GWL_USERDATA, accepted only
    when [V] is one of AnnoFrame's two base vtables (view.md 3.3). Returns the world position in
    tiles (the stored translation negated, view.md 3.2 'Clamp and units'), rotation, zoom group
    and scale; None when V is missing or fails the vtable check."""
    v = userdata(hwnd)
    if not v:
        return None
    head = mem.read(v, 4)
    if not head or u32(head) not in (frame_base + rva for rva in FRAME_VIEW_VTS):
        return None
    rot = mem.read(v + VIEW_ROTATION, 1)
    grp = mem.read(v + VIEW_ZOOM_GROUP, 1)
    scale = mem.read(v + VIEW_SCALE, 4)
    tx = mem.read(v + VIEW_TRANS_X, 4)
    tz = mem.read(v + VIEW_TRANS_Z, 4)
    if not (rot and grp and scale and tx and tz):
        return None
    f = lambda b: struct.unpack("<f", b)[0]  # noqa: E731
    return {"v": v, "x": -f(tx), "z": -f(tz), "rotation": rot[0], "zoom_group": grp[0],
            "scale": f(scale)}
