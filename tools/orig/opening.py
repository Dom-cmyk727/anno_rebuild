"""The playbook's Citizen opening, step 1, as a verified macro (T16 acceptance; playbook section 12).

Data: tools/orig/opening.json (menu positions, costs read from the original's building panels,
the steps with a preferred screen spot each). For every building the macro opens the construction
menu, picks the category and the cell, and clicks the preferred spot, then candidates around it.
A placement counts only when memory shows it (play-original.md section 2.3: never assume):
  - the player's city store lost exactly the building's tools, and
  - the player's money fell by at least the building's gold.
A rejected click costs nothing, so the next candidate is tried. A road is verified by the gold
spent (5 per tile). What this does NOT verify yet: that buildings are connected by road (needs
the building lists, play-original.md section 2.2 item 3) - reported as such.
Backend-agnostic (any tools/orig/backend.py backend with state('all') and read()).
"""

from __future__ import annotations

import json
import os
import time

import gamestate

_HERE = os.path.dirname(os.path.abspath(__file__))
PLAN = os.path.join(_HERE, "opening.json")
MAP_RIGHT = 0.745          # the map ends where the HUD column starts (screens: in_game box 0.745)
MAP_BOTTOM = 0.94          # the info bar starts below


def load(path: str | None = None) -> dict:
    with open(path or PLAN, encoding="utf-8") as f:
        return json.load(f)


class Probe:
    """The player's city store and money, read directly (cheap, no scans)."""

    def __init__(self, b, city_addr: int, player_addr: int):
        self.b, self.city, self.player = b, city_addr, player_addr

    def snap(self) -> dict:
        out = {}
        for name, g in gamestate.GOODS.items():
            raw = self.b.read(self.city + gamestate.CITY_STORE + gamestate.CITY_STORE_STRIDE * g, 4)
            out[name] = gamestate.u32(raw) / gamestate.STORE_UNIT if raw else None
        raw = self.b.read(self.player + gamestate.PLAYER_MONEY, 4)
        out["money"] = gamestate.i32(raw) if raw else None
        return out


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
        d_tools = round(after["tools"] - before["tools"], 3)
        d_money = after["money"] - before["money"]
        ok = d_tools == -cost["tools"] and -d_money >= cost["gold"]
        tries.append({"at": [x, y], "d_tools": d_tools, "d_wood": round(after["wood"] - before["wood"], 3),
                      "d_money": d_money, "ok": ok})
        if ok:
            break
    b.key(cfg["menu"]["leave_key"])
    time.sleep(0.4)
    good = [t for t in tries if t["ok"]]
    return {"build": key, "ok": bool(good), "at": good[0]["at"] if good else None, "tries": len(tries),
            "detail": good[0] if good else tries[-3:],
            "reason": None if good else "no candidate accepted (blocked, outside the city area, or no material)"}


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
    return {"road": path, "ok": spent >= 2 * gold, "gold_spent": spent, "tiles_est": spent // gold,
            "reason": None if spent >= 2 * gold else "no road tiles paid for"}


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
