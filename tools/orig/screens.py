"""T15 screen table: the HUD and construction-menu layout of the original, as data.

Loads `tools/orig/screens.json` (window-fraction coordinates, resolution-independent) and offers a
few typed accessors for play.py / control.py, so the UI layout lives in ONE data file instead of
magic numbers scattered through the macros. Standard library only; import-safe under `python -I`.

Only coordinates measured by running are in the JSON; anything still to be captured in an
owner-present session is marked `{"capture": "owner-session"}` and is reported by `missing()` so a
driving session knows exactly what is left to record. Nothing is fabricated.

    python -I tools/orig/screens.py            # print a summary + validate; non-zero exit on error
    python -I tools/orig/screens.py --missing  # list every field still to capture with the owner

From code:

    import screens
    t = screens.load()
    nx, ny = screens.tab("construction")          # a main-tab coordinate
    steps  = screens.menu_path("new_citizen_game") # the scripted menu path to a game
    key    = screens.key("quicksave")              # "F10"
"""

from __future__ import annotations

import argparse
import json
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
_JSON = os.path.join(_HERE, "screens.json")

CAPTURE = "owner-session"   # sentinel value meaning "record this with the owner, not yet measured"


def load(path: str | None = None) -> dict:
    with open(path or _JSON, encoding="utf-8") as f:
        return json.load(f)


def _is_capture(v: object) -> bool:
    return isinstance(v, dict) and v.get("capture") == CAPTURE


def tab(name: str, table: dict | None = None) -> tuple[float, float]:
    """A main-tab coordinate (construction / military / help / options)."""
    t = table or load()
    xy = t["hud"]["main_tabs"][name]
    return float(xy[0]), float(xy[1])


def category(key: str, table: dict | None = None) -> dict:
    """A construction-menu category record by its key (public, roads, farms, ...)."""
    t = table or load()
    for c in t["construction_menu"]["categories"]:
        if c["key"] == key:
            return c
    raise KeyError(f"no construction category {key!r}")


def menu_path(name: str, table: dict | None = None) -> list:
    """A scripted menu navigation path, e.g. new_citizen_game -> list of [op, nx, ny, label]."""
    t = table or load()
    return t["menu_navigation"][name]


def key(name: str, table: dict | None = None) -> str:
    """A gameplay key by semantic name (quicksave -> 'F10', construction_menu -> 'B', ...)."""
    t = table or load()
    return t["keys"][name]


def _walk(node, path: str):
    """Yield (path, value) for every leaf; recurse dicts/lists."""
    if isinstance(node, dict):
        for k, v in node.items():
            yield from _walk(v, f"{path}.{k}" if path else k)
    elif isinstance(node, list):
        for i, v in enumerate(node):
            yield from _walk(v, f"{path}[{i}]")
    else:
        yield path, node


def missing(table: dict | None = None) -> list[str]:
    """Every JSON path still marked {\"capture\": \"owner-session\"}, so a session knows what is left."""
    t = table or load()
    out: list[str] = []

    def rec(node, path: str) -> None:
        if _is_capture(node):
            out.append(path)
            return
        if isinstance(node, dict):
            for k, v in node.items():
                rec(v, f"{path}.{k}" if path else k)
        elif isinstance(node, list):
            for i, v in enumerate(node):
                rec(v, f"{path}[{i}]")

    rec(t, "")
    return out


def validate(table: dict | None = None) -> list[str]:
    """Internal consistency checks that must hold regardless of what is still to capture.
    Returns a list of problems (empty = OK)."""
    t = table or load()
    problems: list[str] = []

    # Every filled-in coordinate pair must be window fractions in [0, 1].
    for path, val in _walk(t, ""):
        base = path.split("[")[0]
        leaf = base.rsplit(".", 1)[-1]
        # heuristics: fields that hold a single fraction
        if leaf in ("y", "nx", "ny", "nameplate_y", "category_column_x") and isinstance(val, (int, float)):
            if not (0.0 <= val <= 1.0):
                problems.append(f"{path} = {val} is not a window fraction in [0,1]")

    # main tabs: four, each a 2-tuple in range.
    for name in ("construction", "military", "help", "options"):
        try:
            x, y = tab(name, t)
        except Exception as e:  # noqa: BLE001 - report, don't crash
            problems.append(f"main tab {name!r}: {e}")
            continue
        if not (0.0 <= x <= 1.0 and 0.0 <= y <= 1.0):
            problems.append(f"main tab {name!r} coord out of range: {(x, y)}")

    # the manual's eight construction categories, index 1..8, unique keys.
    cats = t["construction_menu"]["categories"]
    if len(cats) != 8:
        problems.append(f"expected 8 construction categories, found {len(cats)}")
    if [c["index"] for c in cats] != list(range(1, len(cats) + 1)):
        problems.append("construction category indices are not 1..N in order")
    if len({c["key"] for c in cats}) != len(cats):
        problems.append("construction category keys are not unique")

    # civ levels match what the unlock tables will key by.
    if t["civ_levels"][:4] != ["pioneers", "settlers", "citizens", "merchants"]:
        problems.append("civ_levels do not start pioneers/settlers/citizens/merchants")

    # menu nav path rows are [op, nx, ny, label] with in-range coords.
    for name, steps in t["menu_navigation"].items():
        if name.startswith("_") or not isinstance(steps, list):
            continue  # skip doc keys like _src
        for i, step in enumerate(steps):
            if not (isinstance(step, list) and len(step) >= 3):
                problems.append(f"menu_navigation.{name}[{i}] malformed: {step}")
                continue
            _, nx, ny = step[0], step[1], step[2]
            if not (0.0 <= nx <= 1.0 and 0.0 <= ny <= 1.0):
                problems.append(f"menu_navigation.{name}[{i}] coord out of range: {(nx, ny)}")

    return problems


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="screen table: load, validate, list owner-capture TODOs")
    ap.add_argument("--missing", action="store_true", help="list fields still to capture with the owner")
    args = ap.parse_args(argv)

    t = load()
    problems = validate(t)
    todo = missing(t)

    if args.missing:
        print(f"{len(todo)} field(s) still to capture in an owner session:")
        for p in todo:
            print(f"  - {p}")
        return 0 if not problems else 1

    cats = ", ".join(c["key"] for c in t["construction_menu"]["categories"])
    tabs = [k for k in t["hud"]["main_tabs"] if not k.startswith("_") and k != "y"]
    paths = [k for k in t["menu_navigation"] if not k.startswith("_")]
    print(f"screens.json: base {t['base_resolution']}, {len(t['civ_levels'])} civ levels")
    print(f"  main tabs: {tabs}")
    print(f"  construction categories (8): {cats}")
    print(f"  menu paths: {paths}")
    print(f"  owner-capture TODO: {len(todo)} field(s) (run --missing to list)")
    if problems:
        print(f"VALIDATION FAILED ({len(problems)}):", file=sys.stderr)
        for p in problems:
            print(f"  - {p}", file=sys.stderr)
        return 1
    print("validation OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
