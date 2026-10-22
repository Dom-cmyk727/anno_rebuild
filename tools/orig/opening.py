"""The playbook's Citizen opening, step 1, as a verified macro (T16 acceptance; playbook section 12).

Data: tools/orig/opening.json (menu positions, costs read from the original's building panels and
equal to Anlagen.dat, the steps with a preferred screen spot each). For every building the macro
opens the construction menu, picks the category and the cell, and clicks the preferred spot, then
candidates around it. A placement counts only when memory shows it (play-original.md section 2.3):

  - the city's tools fell by exactly the building's tools cost,
  - the money fell by at least the building's gold and at most gold + one upkeep booking,
  - the wood fell by the building's wood, less at most one cart load delivered meanwhile.

Upkeep during the ~1.3 s window: the city books its running costs every 2000 ticks (4 s) as
(stat 1 + stat 2) / 15, the stats being the per-minute running costs at city +0x604 / +0x608
(docs/re/population.md section 8), and the player books S / 15 with S = [+0x88] + [+0x8c] +
u16[+0xa2] (docs/re/players.md); a window shorter than one period holds at most one of each.
There is no income in the opening (no residents, no trade orders). With these bounds the
(tools, gold) pair tells every building of the opening apart, so a wrong menu cell cannot pass.

A road is verified by the gold spent (5 per tile, at least 2 tiles after subtracting the bound).
Not verified yet: that buildings are connected by road (needs the building lists,
play-original.md section 2.2 item 3). Backend-agnostic (state('all') and read()).
"""

from __future__ import annotations

import json
import math
import os
import time

import gamestate

_HERE = os.path.dirname(os.path.abspath(__file__))
PLAN = os.path.join(_HERE, "opening.json")
MAP_RIGHT = 0.745          # the map ends where the HUD column starts (screens_sig.json in_game box x0)
MAP_BOTTOM = 0.94          # the info bar starts at about 0.95 of the frame (gold readout crop)
CITY_STAT_CIVIL = 0x604    # city stat 1: civil running costs per game minute (population.md section 8)
CITY_STAT_MILITARY = 0x608 # city stat 2: military running costs per game minute
PLAYER_UPKEEP = (0x88, 0x8C)   # player fields summed into S (players.md, per-player tick)
PLAYER_UPKEEP_REM = 0xA2       # u16 remainder carried between bookings
BOOKING_DIVISOR = 15           # bookings per game minute (every 2000 ticks)
CART_TONS = 5                  # market cart 220 carries 5 t (production.md section 5.4, likely)


def load(path: str | None = None) -> dict:
    with open(path or PLAN, encoding="utf-8") as f:
        return json.load(f)


class Probe:
    """The player's city store, money and upkeep bound, read directly (cheap, no scans)."""

    def __init__(self, b, city_addr: int, player_addr: int):
        self.b, self.city, self.player = b, city_addr, player_addr

    def _u32(self, addr: int) -> int | None:
        raw = self.b.read(addr, 4)
        return gamestate.u32(raw) if raw else None

    def upkeep_bound(self) -> int | None:
        """Most gold one city booking plus one player booking can take (None if unreadable)."""
        s1, s2 = self._u32(self.city + CITY_STAT_CIVIL), self._u32(self.city + CITY_STAT_MILITARY)
        p = [self._u32(self.player + off) for off in PLAYER_UPKEEP]
        rem = self.b.read(self.player + PLAYER_UPKEEP_REM, 2)
        if None in (s1, s2, *p) or not rem:
            return None
        s = sum(p) + gamestate.u16(rem)
        return math.ceil((s1 + s2) / BOOKING_DIVISOR) + math.ceil(s / BOOKING_DIVISOR) + 1

    def snap(self) -> dict:
        out = {}
        for name, g in gamestate.GOODS.items():
            raw = self.b.read(self.city + gamestate.CITY_STORE + gamestate.CITY_STORE_STRIDE * g, 4)
            out[name] = gamestate.u32(raw) / gamestate.STORE_UNIT if raw else None
        raw = self.b.read(self.player + gamestate.PLAYER_MONEY, 4)
        out["money"] = gamestate.i32(raw) if raw else None
        out["upkeep_bound"] = self.upkeep_bound()
        return out


def placement_ok(cost: dict, before: dict, after: dict) -> tuple[bool, str]:
    """Did exactly this building get placed between the two snapshots?"""
    if None in (before.get("tools"), after.get("tools"), before.get("money"), after.get("money")):
        return False, "unreadable store or money"
    d_tools = round(after["tools"] - before["tools"], 3)
    if d_tools != -cost["tools"]:
        return False, f"tools changed by {d_tools}, cost {cost['tools']}"
    drop = before["money"] - after["money"]
    if drop < cost["gold"]:
        return False, f"money fell by {drop}, less than the gold cost {cost['gold']}"
    bound = max(x for x in (before.get("upkeep_bound"), after.get("upkeep_bound"), 0) if x is not None)
    if before.get("upkeep_bound") is None and after.get("upkeep_bound") is None:
        return False, "no upkeep bound (city/player fields unreadable)"
    if drop > cost["gold"] + bound:
        return False, f"money fell by {drop}, more than gold {cost['gold']} + upkeep bound {bound}"
    if before.get("wood") is not None and after.get("wood") is not None:
        d_wood = round(after["wood"] - before["wood"], 3)
        if not -cost["wood"] <= d_wood <= -cost["wood"] + CART_TONS:
            return False, f"wood changed by {d_wood}, cost {cost['wood']}"
    return True, "ok"


def road_tiles(spent: int, bound: int | None, gold_per_tile: int) -> tuple[int, int] | None:
    """The range of road tiles the gold spent can stand for, given the upkeep bound."""
    if bound is None:
        return None
    return max(0, math.ceil((spent - bound) / gold_per_tile)), spent // gold_per_tile


def find_own(b) -> tuple[int, int, dict]:
    """The human player (kind HUMAN) and the player's city at game start (no inhabitants, holding
    the start goods; the computer players' cities start populated)."""
    st = b.state("all")
    humans = [p for p in st.get("players", []) if p["kind"] == "HUMAN"]
    own = [c for c in st["cities"] if c["inhabitants"] == 0 and c["tools_t"] and c["wood_t"]]
    if len(humans) != 1 or len(own) != 1:
        raise RuntimeError(f"cannot identify the player ({len(humans)} humans) or the city ({len(own)} candidates)")
    return int(own[0]["addr"], 16), int(humans[0]["addr"], 16), {"player": humans[0], "city": own[0]}


def open_cell(b, cfg: dict, key: str) -> None:
    m, bc = cfg["menu"], cfg["buildings"][key]
    b.key(m["leave_key"])
    time.sleep(0.4)
    b.key(m["open_key"])
    time.sleep(0.8)
    b.click(m["category_x"], m["categories"][bc["category"]])
    time.sleep(0.6)
    col, row = bc["cell"]
    b.click(m["grid_x"][col], m["grid_y"][row])
    time.sleep(0.6)


def spots(cfg: dict, at) -> list[tuple[float, float]]:
    out = []
    for dx, dy in cfg["candidates"]["offsets"]:
        x, y = round(at[0] + dx, 4), round(at[1] + dy, 4)
        if 0.01 <= x <= MAP_RIGHT - 0.01 and 0.01 <= y <= MAP_BOTTOM - 0.01:
            out.append((x, y))
    return out


def place(b, cfg: dict, probe: Probe, key: str, at) -> dict:
    cost = cfg["buildings"][key]["cost"]
    open_cell(b, cfg, key)
    tries = []
    for x, y in spots(cfg, at):
        before = probe.snap()
        b.move(x, y)
        time.sleep(0.3)
        b.click(x, y)
        time.sleep(1.0)
        after = probe.snap()
        ok, why = placement_ok(cost, before, after)
        tries.append({"at": [x, y], "ok": ok, "why": why,
                      "d_tools": None if None in (before["tools"], after["tools"]) else round(after["tools"] - before["tools"], 3),
                      "d_wood": None if None in (before["wood"], after["wood"]) else round(after["wood"] - before["wood"], 3),
                      "d_money": None if None in (before["money"], after["money"]) else after["money"] - before["money"],
                      "upkeep_bound": after["upkeep_bound"]})
        if ok:
            break
        if tries[-1]["d_tools"] not in (0, 0.0, None):
            break                              # something was placed but it was not this building
    b.key(cfg["menu"]["leave_key"])
    time.sleep(0.4)
    good = [t for t in tries if t["ok"]]
    return {"build": key, "ok": bool(good), "at": good[0]["at"] if good else None, "tries": len(tries),
            "detail": good[0] if good else tries[-3:],
            "reason": None if good else tries[-1]["why"] if tries else "no candidate on the map"}


def road(b, cfg: dict, probe: Probe, path) -> dict:
    gold = cfg["buildings"]["road"]["cost"]["gold"]
    open_cell(b, cfg, "road")
    before = probe.snap()
    (x0, y0), (x1, y1) = path
    b.drag(x0, y0, x1, y1)
    time.sleep(1.0)
    after = probe.snap()
    b.key(cfg["menu"]["leave_key"])
    spent = before["money"] - after["money"]
    bound = max((x for x in (before["upkeep_bound"], after["upkeep_bound"]) if x is not None), default=None)
    tiles = road_tiles(spent, bound, gold)
    ok = tiles is not None and tiles[0] >= 2 and after["tools"] == before["tools"]
    return {"road": path, "ok": ok, "gold_spent": spent, "upkeep_bound": bound, "tiles": tiles,
            "reason": None if ok else "fewer than 2 tiles paid for beyond the upkeep bound"}


def run(b, log=print, plan: dict | None = None) -> dict:
    cfg = plan or load()
    city, player, who = find_own(b)
    probe = Probe(b, city, player)
    start = probe.snap()
    log("opening_start", city=hex(city), player=hex(player), store=start)
    steps = []
    for st in cfg["steps"]:
        r = place(b, cfg, probe, st["build"], st["at"]) if "build" in st else road(b, cfg, probe, st["road"])
        steps.append(r)
        log("opening_step", **r)
    end = probe.snap()
    placed = [s for s in steps if s.get("build") and s["ok"]]
    res = {"ok": all(s["ok"] for s in steps), "placed": len(placed),
           "failed": [s.get("build") or "road" for s in steps if not s["ok"]],
           "store_start": start, "store_end": end, "who": who, "steps": steps,
           "not_verified": "road connectivity of the buildings (needs the city's building lists)"}
    return res
