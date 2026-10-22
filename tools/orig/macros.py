"""Menu macros on top of the backend interface (T16 section 4 item 4: screen recognition and a
new-game macro). Backend-agnostic: the same code drives the Wine runner and the Windows copy.

Every macro follows play-original.md section 2.3: read the state (which screen), act, verify the
expected change with a timeout, and return a result or raise MacroError with a named reason.
Coordinates come from tools/orig/menus.json, screens from tools/orig/screens_sig.json.
"""

from __future__ import annotations

import json
import os
import time

_HERE = os.path.dirname(os.path.abspath(__file__))
MENUS = os.path.join(_HERE, "menus.json")


class MacroError(RuntimeError):
    def __init__(self, reason: str, detail: str = ""):
        super().__init__(f"{reason}: {detail}" if detail else reason)
        self.reason = reason


def menus() -> dict:
    with open(MENUS, encoding="utf-8") as f:
        return json.load(f)


def wait_screen(b, want, timeout: float = 15.0, interval: float = 0.5) -> dict:
    """Recognise until the screen is one of `want`; MacroError('screen_timeout') otherwise."""
    want = {want} if isinstance(want, str) else set(want)
    end = time.monotonic() + timeout
    r = {"screen": None}
    while True:
        r = b.recognize()
        if r["screen"] in want:
            return r
        if time.monotonic() >= end:
            raise MacroError("screen_timeout", f"wanted {sorted(want)}, last {r['screen']} {r.get('scores')}")
        time.sleep(interval)


def press(b, screen: str, button: str, expect, timeout: float = 15.0) -> dict:
    """Click a menu button, but only on the screen it belongs to, then wait for `expect`."""
    m = menus()["screens"][screen]
    xy = m["rows"][button] if "rows" in m and button in m["rows"] else m[button]
    cur = b.recognize()
    if cur["screen"] != screen:
        raise MacroError("wrong_screen", f"expected {screen} before pressing {button}, saw {cur['screen']}")
    b.click(xy[0], xy[1])
    if expect is None:
        return {"clicked": button}
    return wait_screen(b, expect, timeout)


def skip_intros(b, timeout: float = 120.0) -> dict:
    """Skip the intro videos one at a time until the main menu shows (owner: two intros, ESC or a
    click each). Never presses a key on a black frame or on the main menu itself."""
    cfg = menus()["intros"]
    end = time.monotonic() + timeout
    skips, seen = 0, []
    while time.monotonic() < end:
        r = b.recognize()
        seen.append(r["screen"])
        if r["screen"] == "main_menu":
            return {"skips": skips, "expected": cfg["expected_skips"], "seen": seen[-12:]}
        if r["screen"] not in ("black",):
            b.key(cfg["skip_key"])
            skips += 1
            time.sleep(1.5)
        time.sleep(1.0)
    raise MacroError("no_main_menu", f"after {skips} skips, screens {seen[-12:]}")


def wait_in_game(b, timeout: float = 180.0) -> dict:
    """In a game = the world object is found (gamestate.is_world) and its clock advances."""
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        w = b.state("world").get("world")
        if w:
            time.sleep(1.0)
            w2 = b.state("world").get("world")
            if w2 and w2["counter"] > w["counter"]:
                return {"world": w2}
        time.sleep(2.0)
    raise MacroError("no_game", f"no advancing world object within {timeout} s")


def new_game(b, game: str = "citizen", timeout: float = 180.0) -> dict:
    """From anywhere before the main menu: intros, Singleplayer, New game, pick `game`, OK, and
    wait until the world clock runs. Returns the steps with their recognition results."""
    res = {"game": game}
    first = b.recognize()["screen"]
    if first != "main_menu":
        res["intros"] = skip_intros(b)
    press(b, "main_menu", "singleplayer", "singleplayer")
    press(b, "singleplayer", "new_game", "choose_game")
    press(b, "choose_game", game, None)
    time.sleep(0.8)
    press(b, "choose_game", "ok", None)
    res["in_game"] = wait_in_game(b, timeout)
    res["screen"] = b.recognize()
    return res
