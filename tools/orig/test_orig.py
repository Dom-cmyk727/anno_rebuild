"""Unit tests for the T16 harness tools that need no running game (synthetic data only).

    python -I tools/orig/test_orig.py [-v]        (Pillow needed; runs on Windows and in the distro)
"""

from __future__ import annotations

import io
import json
import os
import queue
import re
import struct
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock

_HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(_HERE))
sys.path.insert(0, _HERE)
sys.path.insert(0, os.path.join(_HERE, "wine"))

import agent  # noqa: E402
import backend  # noqa: E402
import gamestate  # noqa: E402
import live  # noqa: E402
import opening  # noqa: E402
import screensig  # noqa: E402
import timing  # noqa: E402
import userdat  # noqa: E402

STRAY_COUNTER = 0x43F3600E00000000   # the garbage "counter" a stray hit showed under Wine (2026-10-22)


class FakeMem:
    """gamestate's memory interface over a dict of {base address: bytes}."""

    def __init__(self, blocks: dict[int, bytes]):
        self.blocks = blocks

    def read(self, addr, n):
        for base, data in self.blocks.items():
            if base <= addr and addr + n <= base + len(data):
                return bytes(data[addr - base:addr - base + n])
        return None

    def regions(self):
        for base, data in sorted(self.blocks.items()):
            yield base, base + len(data)


def world_obj(vt: int, counter: int, tick: int, cursor: int = 7) -> bytearray:
    b = bytearray(0x120)
    struct.pack_into("<I", b, 0, vt)
    struct.pack_into("<H", b, gamestate.WORLD_CURSOR, cursor)
    struct.pack_into("<Q", b, gamestate.WORLD_COUNTER, counter)
    struct.pack_into("<I", b, gamestate.WORLD_AT108, tick)
    struct.pack_into("<I", b, gamestate.WORLD_TICK, tick)
    return b


def player_obj(vt: int, world: int, name: bytes, kind: bytes, money: int) -> bytes:
    b = bytearray(gamestate.PLAYER_MONEY + 4)
    struct.pack_into("<I", b, 0, vt)
    struct.pack_into("<I", b, gamestate.PLAYER_WORLD, world)
    b[gamestate.PLAYER_NAME:gamestate.PLAYER_NAME + len(name)] = name
    b[gamestate.PLAYER_KIND:gamestate.PLAYER_KIND + len(kind)] = kind
    struct.pack_into("<i", b, gamestate.PLAYER_MONEY, money)
    return bytes(b)


class Offsets(unittest.TestCase):
    """The tools' offsets against the values written in the docs they cite (docs/re/tick-loop.md,
    population.md, production.md, players.md; docs/testing/original-harness.md, original-wine.md,
    play-original.md; docs/formats/user-dat.md), parsed from the docs' text: a change on either
    side without the other fails here."""

    @staticmethod
    def doc(rel: str, pattern: str) -> tuple:
        with open(os.path.join(REPO, "docs", rel), encoding="utf-8") as f:
            m = re.search(pattern, f.read())
        if not m:
            raise AssertionError(f"docs/{rel}: no match for {pattern!r} (doc reworded? update the pattern)")
        return tuple(int(g, 16) if g.startswith("0x") else int(g) for g in m.groups())

    def test_documented_values(self):
        hexa = "(0x[0-9a-fA-F]+)"
        world_va, world_rva = self.doc("testing/original-harness.md",
                                       rf"vtable `{hexa}` in `AnnoWelt\.dll` \(offset `{hexa}` from the module base\)")
        image = world_va - world_rva                    # AnnoWelt's addresses in the RE notes are VAs
        self.assertEqual(self.doc("re/tick-loop.md", rf"World object vtable `{hexa}`"), (world_va,))
        self.assertEqual(gamestate.WELT_WORLD_VT, world_rva)
        (prng,) = self.doc("re/tick-loop.md", rf"`T` is a u16 table at `{hexa}`")
        self.assertEqual(gamestate.PRNG_RVA, prng - image)
        self.assertEqual(self.doc("testing/original-wine.md", rf"PRNG table `AnnoWelt\+{hexa}`"), (gamestate.PRNG_RVA,))
        (city_vt,) = self.doc("re/production.md", rf"City \(warehouse\) stock — city vtable `{hexa}`")
        self.assertEqual(gamestate.WELT_CITY_VT, city_vt - image)
        (player_vt,) = self.doc("re/population.md", rf"The player vtable is `{hexa}`")
        self.assertEqual(gamestate.WELT_PLAYER_VT, player_vt - image)

        self.assertEqual(self.doc("re/tick-loop.md", rf"The cursor is the u16 at world `\+{hexa}`"),
                         (gamestate.WORLD_CURSOR,))
        self.assertEqual(self.doc("re/tick-loop.md", rf"64-bit millisecond counter at `\+{hexa}/\+0x[0-9a-f]+` and "
                                                     rf"sets the \*\*world tick\*\* `\+{hexa} = counter >> 1`"),
                         (gamestate.WORLD_COUNTER, gamestate.WORLD_TICK))

        self.assertEqual(self.doc("testing/play-original.md", rf"store per good `\+{hexa} \+ (\d+)g`, capacity "
                                                              rf"`\+{hexa}`, inhabitants `\+{hexa}`"),
                         (gamestate.CITY_STORE, gamestate.CITY_STORE_STRIDE, gamestate.CITY_CAPACITY,
                          gamestate.CITY_INHABITANTS))
        self.assertEqual(self.doc("re/production.md", r"unsigned fixed point with (\d+) = 1 ton"), (gamestate.STORE_UNIT,))
        (first,) = self.doc("re/production.md", r"\| (\d+)–\d+ \| `WERKZEUG HOLZ ZIEGEL")
        self.assertEqual(gamestate.GOODS, {"tools": first, "wood": first + 1, "bricks": first + 2})

        self.assertEqual(self.doc("re/population.md", rf"In every one, `\+{hexa}` points to the live world object"),
                         (gamestate.PLAYER_WORLD,))
        self.assertEqual(self.doc("re/population.md", rf"`\+{hexa}`: name, (\d+) bytes"),
                         (gamestate.PLAYER_NAME, gamestate.PLAYER_TEXT))
        self.assertEqual(self.doc("re/population.md", rf"`\+{hexa}`: kind, (\d+) bytes"),
                         (gamestate.PLAYER_KIND, gamestate.PLAYER_TEXT))
        self.assertEqual(self.doc("re/population.md", rf"`\+{hexa}`: a 16-byte GUID"), (gamestate.PLAYER_GUID,))
        self.assertEqual(self.doc("re/population.md", rf"\*\*Player money\*\* = `i32` at player `\+{hexa}`"),
                         (gamestate.PLAYER_MONEY,))

        self.assertEqual(self.doc("re/population.md", rf"stat 1 `\+{hexa}` and stat 2 `\+{hexa}`"),
                         (opening.CITY_STAT_CIVIL, opening.CITY_STAT_MILITARY))
        a, b, rem, div = self.doc("re/players.md", rf"booking `S = \[\+{hexa}\] \+ \[\+{hexa}\] \+ u16\[\+{hexa}\]`"
                                                   r" in a 32-bit register, charge `S/(\d+)`")
        self.assertEqual(sorted(opening.PLAYER_UPKEEP), sorted((a, b)))
        self.assertEqual((opening.PLAYER_UPKEEP_REM, opening.BOOKING_DIVISOR), (rem, div))
        self.assertEqual(self.doc("formats/user-dat.md", r"`(-\d+)` = slider at the left end"), (userdat.VOLUME_MIN,))


class GameState(unittest.TestCase):
    WELT = 0x580000

    def test_world_check_is_unmasked(self):
        self.assertTrue(gamestate.is_world({"counter": 1371, "tick": 685}))
        # The observed stray value has 33 low zero bits: a masked compare (before or after the
        # shift) would accept it with tick 0.
        self.assertEqual(STRAY_COUNTER & ((1 << 33) - 1), 0)
        self.assertFalse(gamestate.is_world({"counter": STRAY_COUNTER, "tick": 0}))
        self.assertFalse(gamestate.is_world({"counter": 0, "tick": 0}))

    def test_find_world_skips_stray_and_finds_live(self):
        vt = self.WELT + gamestate.WELT_WORLD_VT
        mem = FakeMem({0x1000: world_obj(vt, STRAY_COUNTER, 0) + world_obj(vt, 5000, 2500)})
        self.assertEqual(gamestate.find_world(mem, self.WELT), 0x1000 + 0x120)

    def test_find_world_prefers_the_advancing_copy(self):
        vt = self.WELT + gamestate.WELT_WORLD_VT
        stale, live_obj = world_obj(vt, 4000, 2000), world_obj(vt, 6000, 3000)
        blocks = {0x1000: stale + live_obj}

        class Ticking(FakeMem):
            def read(self, addr, n):
                if addr == 0x1000 + 0x120 + gamestate.WORLD_COUNTER and n == 8:   # the live copy runs
                    c = struct.unpack_from("<Q", blocks[0x1000], 0x120 + gamestate.WORLD_COUNTER)[0] + 2
                    struct.pack_into("<Q", blocks[0x1000], 0x120 + gamestate.WORLD_COUNTER, c)
                    struct.pack_into("<I", blocks[0x1000], 0x120 + gamestate.WORLD_TICK, c >> 1)
                return super().read(addr, n)

        mem = Ticking(blocks)
        self.assertEqual(gamestate.find_world(mem, self.WELT), 0x1000)                 # first by address
        self.assertEqual(gamestate.find_world(mem, self.WELT, settle=0.001), 0x1120)   # the one that runs

    def test_scan_is_aligned(self):
        v = 0x11223344
        data = bytearray(16)
        struct.pack_into("<I", data, 2, v)       # unaligned copy
        struct.pack_into("<I", data, 8, v)       # aligned copy
        self.assertEqual(gamestate.scan_u32(FakeMem({0x4000: data}), v), [0x4008])

    def test_msvc_rand_head(self):
        # MSVC rand() from seed 1: the documented head of the table (tick-loop.md F6).
        self.assertEqual(gamestate.msvc_rand(6), [41, 18467, 6334, 26500, 19169, 15724])

    def test_players_filter_by_world_and_text(self):
        vt = self.WELT + gamestate.WELT_PLAYER_VT
        world = 0x9000
        a = player_obj(vt, world, b"ANNOnymous", b"HUMAN", 500000)
        umlaut = player_obj(vt, world, "Jörg".encode("latin-1"), b"CG01", 10000)
        freed = player_obj(vt, 0xDEAD, b"Ghost", b"CG01", 1)            # other world: rejected
        junk = player_obj(vt, world, b"\x01\x02", b"HUMAN", 3)           # name not text: rejected
        mem = FakeMem({0x2000: a, 0x2800: umlaut, 0x3000: freed, 0x4000: junk})
        got = gamestate.players(mem, self.WELT, world)
        self.assertEqual([(p["name"], p["kind"], p["money"]) for p in got],
                         [("ANNOnymous", "HUMAN", 500000), ("Jörg", "CG01", 10000)])

    def test_city_capacity_plausibility(self):
        vt = self.WELT + gamestate.WELT_CITY_VT

        def city(cap):
            b = bytearray(gamestate.CITY_CAPACITY + 4)
            struct.pack_into("<I", b, 0, vt)
            struct.pack_into("<I", b, gamestate.CITY_CAPACITY, cap)
            struct.pack_into("<I", b, gamestate.CITY_STORE + 12 * 41, 48 * 256)
            return bytes(b)

        mem = FakeMem({0x5000: city(50 * 256), 0x6000: city(0), 0x7000: city(gamestate.CITY_CAPACITY_SANE + 1)})
        got = gamestate.cities(mem, self.WELT)
        self.assertEqual([(c["addr"], c["capacity_t"], c["tools_t"]) for c in got], [("0x5000", 50.0, 48.0)])


class Camera(unittest.TestCase):
    FRAME = 0x2300000
    V = 0x3000000

    def view(self, vt_rva=0x86428, x=-670.5, z=-500.25, rot=1, grp=2, scale=0.25):
        b = bytearray(0x1700)
        struct.pack_into("<I", b, 0, self.FRAME + vt_rva)
        struct.pack_into("<f", b, gamestate.VIEW_TRANS_X, x)
        struct.pack_into("<f", b, gamestate.VIEW_TRANS_Z, z)
        b[gamestate.VIEW_ROTATION] = rot
        b[gamestate.VIEW_ZOOM_GROUP] = grp
        struct.pack_into("<f", b, gamestate.VIEW_SCALE, scale)
        return FakeMem({self.V: bytes(b)})

    def test_reads_negated_translation_for_both_vtables(self):
        for rva in gamestate.FRAME_VIEW_VTS:
            cam = gamestate.camera(self.view(rva), 1, self.FRAME, userdata=lambda h: self.V)
            self.assertEqual(cam, {"v": self.V, "x": 670.5, "z": 500.25, "rotation": 1,
                                   "zoom_group": 2, "scale": 0.25})

    def test_refuses_wrong_vtable_and_missing_anchor(self):
        self.assertIsNone(gamestate.camera(self.view(0x86430), 1, self.FRAME, userdata=lambda h: self.V))
        self.assertIsNone(gamestate.camera(self.view(), 1, self.FRAME, userdata=lambda h: None))
        self.assertIsNone(gamestate.camera(self.view(), 1, self.FRAME, userdata=lambda h: 0x7000000))


@unittest.skipUnless(sys.platform == "win32", "needs the Windows backend")
class CameraLive(unittest.TestCase):
    """Against a running original on this PC (C:\anno1503-original); skips without one."""

    def test_camera_inside_world(self):
        be = backend.WindowsBackend()
        try:
            be.start()
        except backend.BackendError as ex:
            self.skipTest(f"no running game: {ex}")
        try:
            st = be.state("world")
            cam = be.state("camera")["camera"]
            world = st.get("world")
            if cam is None or world is None:
                self.skipTest("game running but no scenario/camera yet")
            wobj = int(world["addr"], 16)
            wb = be.read(wobj + 0xEC, 8)
            width, height = struct.unpack("<ii", wb)
            self.assertTrue(0 <= cam["x"] <= width and 0 <= cam["z"] <= height, (cam, width, height))
            self.assertIn(cam["zoom_group"], (0, 1, 2))
        finally:
            be.stop()


def chunk(name: str, payload: bytes) -> bytes:
    return name.encode().ljust(12, b"\0") + struct.pack("<I", len(payload)) + payload


def synthetic_profile(volume=(0, 0)) -> bytes:
    songs = b"".join(struct.pack("<HH", i, 1) for i in range(2, 20))
    profil = (chunk("USGUID", bytes(range(16))) + chunk("SONGS", songs)
              + chunk("VOLUME", struct.pack("<ii", *volume)) + chunk("SPEECH", struct.pack("<I", 0xFFFFFFFF))
              + chunk("HELPPLAYER", struct.pack("<I", 1)))
    return chunk("USDAT", chunk("PROFIL", profil))


class UserDat(unittest.TestCase):
    def test_layout_and_size(self):
        data = synthetic_profile()
        self.assertEqual(len(data), 216)          # the size the game writes
        r = userdat.read(data)
        self.assertEqual(r["volume"], [0, 0])
        self.assertEqual(r["speech_mask"], 0xFFFFFFFF)
        self.assertEqual(len(r["songs"]), 18)

    def test_set_volume_changes_only_volume(self):
        data = synthetic_profile()
        out = userdat.set_volume(data)
        self.assertEqual(userdat.read(out)["volume"], [-250, -250])
        self.assertEqual(len([i for i in range(len(data)) if data[i] != out[i]]), 8)
        self.assertEqual(len(out), len(data))

    def test_missing_volume_raises(self):
        with self.assertRaises(ValueError):
            userdat.set_volume(chunk("USDAT", chunk("PROFIL", chunk("SONGS", b""))))

    def test_overrun_raises(self):
        bad = b"USDAT".ljust(12, b"\0") + struct.pack("<I", 999)
        with self.assertRaises(ValueError):
            list(userdat.chunks(bad))


class ScreenSig(unittest.TestCase):
    def setUp(self):
        from PIL import Image, ImageDraw
        self.Image, self.Draw = Image, ImageDraw

    def picture(self, bars, w: int = 200, h: int = 150, shade: int = 230):
        img = self.Image.new("RGB", (w, h), (90, 90, 90))
        d = self.Draw.Draw(img)
        for x in bars:
            d.rectangle([x, 20, x + 15, 130], fill=(shade, shade, shade))
        return img

    def test_same_picture_distance_zero_and_scale_tolerant(self):
        a = self.picture([20, 55, 90])
        self.assertEqual(screensig.distance(screensig.dhash(a), screensig.dhash(a)), 0)
        self.assertLessEqual(screensig.distance(screensig.dhash(a), screensig.dhash(a.resize((400, 300)))), 12)

    def test_uniform_picture_hashes_to_zero(self):
        # dHash bit = left strictly brighter than right: a flat image has no set bit.
        self.assertEqual(int(screensig.dhash(self.Image.new("RGB", (64, 48), (120, 120, 120))), 16), 0)

    def test_recognize_and_black(self):
        a, b = self.picture([20, 55]), self.picture([20, 55, 90, 125, 160])
        table = {"screens": {"two": {"regions": screensig.learn(a)}, "five": {"regions": screensig.learn(b)}}}
        self.assertEqual(screensig.recognize(a, table)["screen"], "two")
        self.assertEqual(screensig.recognize(b, table)["screen"], "five")
        black = self.Image.new("RGB", (200, 150), (0, 0, 0))
        self.assertEqual(screensig.recognize(black, table)["screen"], "black")

    def test_recognize_threshold_is_inclusive_and_best_wins(self):
        a, b = self.picture([20, 55]), self.picture([20, 55, 90])
        d = screensig.distance(screensig.dhash(a), screensig.dhash(b))
        self.assertGreater(d, 0)
        table = {"screens": {"x": {"regions": screensig.learn(a, max_dist=d)}}}
        self.assertTrue(screensig.recognize(b, table)["scores"]["x"]["match"])          # d <= max_dist
        table = {"screens": {"x": {"regions": screensig.learn(a, max_dist=d - 1)}}}
        self.assertFalse(screensig.recognize(b, table)["scores"]["x"]["match"])
        table = {"screens": {"far": {"regions": screensig.learn(a, max_dist=192)}},
                 }
        table["screens"]["near"] = {"regions": screensig.learn(b, max_dist=192)}
        self.assertEqual(screensig.recognize(b, table)["screen"], "near")              # smaller mean wins

    def test_calibrate_halves_the_nearest_distance(self):
        a, b, c = self.picture([20, 55]), self.picture([20, 55, 90]), self.picture([120, 160], shade=40)
        table = {"screens": {n: {"regions": screensig.learn(img)} for n, img in (("a", a), ("b", b), ("c", c))}}
        dab = screensig.distance(screensig.dhash(a), screensig.dhash(b))
        dac = screensig.distance(screensig.dhash(a), screensig.dhash(c))
        self.assertLess(dab, 2 * screensig.DEFAULT_MAX_DIST)        # the formula, not the cap, decides
        self.assertLess(dab, dac)
        rep = screensig.calibrate(table, {"a": a, "b": b, "c": c})
        self.assertEqual(rep["a[0]"]["nearest"], "b")               # nearest = the closest other screen
        self.assertEqual(table["screens"]["a"]["regions"][0]["max_dist"], dab // 2 - 1)
        self.assertEqual(rep["a[0]"]["ambiguous"], dab < 4)

    def test_calibrate_flags_ambiguous_screens(self):
        a = self.picture([20, 55])
        table = {"screens": {"a": {"regions": screensig.learn(a)}, "a2": {"regions": screensig.learn(a)}}}
        rep = screensig.calibrate(table, {"a": a, "a2": a})
        self.assertTrue(rep["a[0]"]["ambiguous"])
        self.assertEqual(table["screens"]["a"]["regions"][0]["max_dist"], 0)


class TimingModel(unittest.TestCase):
    def series(self, gaps_ms, speed, start=1000):
        """A synthetic poll series: frames at the given gaps, counter advanced like the game
        (whole ms with the rest carried). Frames are seen 0.25 ms late, as a poller would, so no
        timestamp sits exactly on a millisecond boundary."""
        c, out, acc, prev = start, [(0.0, start)], 0.0, 0
        for g in gaps_ms:
            acc += g
            now = int(acc)
            dt, prev = now - prev, now
            step = int(dt * speed + 0.5)
            c += min(step, timing.CAP_MS) if step >= 1 else 0
            out.append(((acc + 0.25) / 1000.0, c))
        return out

    def test_prediction_matches_game_rule(self):
        for speed in (1.0, 2.0, 4.0, 0.5):
            s = self.series([2.4, 2.6, 3.0, 2.0, 2.5] * 200, speed)
            r = timing.analyse(s, speed)
            self.assertEqual(r["predicted_ms"], s[-1][1] - s[0][1], speed)
            self.assertEqual(r["capped_frames"], 0)

    def test_half_speed_rounds_odd_frames_up(self):
        s = self.series([3.0] * 100, 0.5)
        self.assertEqual(timing.analyse(s, 0.5)["predicted_ms"], 200)     # 3 ms -> 2 ms, not 1.5

    def test_cap_in_prediction_and_cost(self):
        r = timing.analyse(self.series([150.0] * 10, 1.0), 1.0)
        self.assertEqual((r["predicted_ms"], r["capped_frames"]), (1000, 10))
        self.assertAlmostEqual(r["lost_ms"], 500.0, delta=1.0)
        r2 = timing.analyse(self.series([60.0] * 10, 2.0), 2.0)            # 120 ms steps capped at 100
        self.assertEqual(r2["capped_frames"], 10)
        self.assertAlmostEqual(r2["lost_ms"], 200.0, delta=1.0)

    def test_half_speed_odd_frames(self):
        # 100 frames of 3 ms: steps 2 each -> sum 200, sum dt 300, n_odd = 100 = every frame
        r = timing.half_speed_odd_frames({"game_ms": 200, "raw_s": 0.300, "frames": 100})
        self.assertEqual((r["n_odd"], r["consistent"]), (100, True))
        # truncation without +0.5 would give 100 ms: n_odd = -100, impossible
        self.assertFalse(timing.half_speed_odd_frames({"game_ms": 100, "raw_s": 0.300, "frames": 100})["consistent"])

    def test_verdict(self):
        f5 = {"rate_ms_per_host_s": 1000.0, "lost_ms": 0.0, "host_s": 65.0, "capped_frames": 0}
        f8 = {"rate_ms_per_host_s": 611.8, "half_speed": {"consistent": True}}
        pause = {"read_failures": 0, "game_ms": 0, "resumed": True}
        v = timing.verdict({"F5": f5, "F8": f8, "pause": pause})
        self.assertEqual((v["clock_f1"], v["speeds"]["F5"], v["speeds"]["F8"], v["pause_f4"]), ("pass", True, True, True))
        self.assertEqual(timing.F1_REL, 1 / 60)
        self.assertEqual(timing.verdict({"F5": dict(f5, rate_ms_per_host_s=1017.0)})["clock_f1"], "fail")
        self.assertEqual(timing.verdict({"F5": dict(f5, rate_ms_per_host_s=1016.0)})["clock_f1"], "pass")
        self.assertEqual(timing.verdict({"F5": dict(f5, capped_frames=1)})["clock_f1"], "fail")
        self.assertFalse(timing.verdict({"F8": dict(f8, rate_ms_per_host_s=499.0)})["speeds"]["F8"])
        self.assertFalse(timing.verdict({"pause": dict(pause, read_failures=1)})["pause_f4"])
        self.assertFalse(timing.verdict({"pause": dict(pause, resumed=False)})["pause_f4"])


def playbook_costs() -> dict[int, dict]:
    """docs/research/playbook.md section 14: id -> gold, tools, wood, running (the larger value)."""
    with open(os.path.join(REPO, "docs", "research", "playbook.md"), encoding="utf-8") as f:
        text = f.read()
    section = re.search(r"^## 14\..*?(?=^## |\Z)", text, re.M | re.S)
    out = {}
    for line in (section.group(0) if section else "").splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) == 8 and cells[1].isdigit():
            num = lambda s: 0 if s in ("–", "-", "") else int(s.replace(",", ""))  # noqa: E731
            run = max(num(x.strip()) for x in cells[7].split("/"))
            out[int(cells[1])] = {"gold": num(cells[2]), "tools": num(cells[3]), "wood": num(cells[4]), "running": run}
    return out


class Opening(unittest.TestCase):
    def setUp(self):
        self.cfg = opening.load()

    def test_costs_equal_the_playbook_table(self):
        table = playbook_costs()
        for key, b in self.cfg["buildings"].items():
            self.assertIn(b["id"], table, key)
            row = table[b["id"]]
            self.assertEqual((b["cost"]["gold"], b["cost"]["tools"], b["cost"]["wood"], b["upkeep"]),
                             (row["gold"], row["tools"], row["wood"], row["running"]), key)

    def test_opening_fits_the_start_store(self):
        steps = [s for s in self.cfg["steps"] if "build" in s]
        tools = sum(self.cfg["buildings"][s["build"]]["cost"]["tools"] for s in steps)
        wood = sum(self.cfg["buildings"][s["build"]]["cost"]["wood"] for s in steps)
        self.assertEqual((tools, wood), (29, 42))          # measured: 50/50 -> 21/8

    def test_map_box(self):
        sig = screensig.load()
        self.assertEqual(opening.MAP_RIGHT, sig["screens"]["in_game"]["regions"][0]["box"][0])
        self.assertEqual(opening.MAP_BOTTOM, 0.94)
        for at in ([0.02, 0.02], [0.74, 0.93], [0.3, 0.3]):
            for x, y in opening.spots(self.cfg, at):
                self.assertTrue(0.01 <= x <= 0.735 and 0.01 <= y <= 0.93)

    def snap(self, tools, wood, money, bound=12):
        return {"tools": tools, "wood": wood, "money": money, "upkeep_bound": bound}

    def test_placement_ok_identifies_the_building(self):
        cost = self.cfg["buildings"]
        before = self.snap(50.0, 50.0, 100000)
        self.assertTrue(opening.placement_ok(cost["forester"]["cost"], before, self.snap(48.0, 50.0, 99850))[0])
        self.assertTrue(opening.placement_ok(cost["forester"]["cost"], before, self.snap(48.0, 50.0, 99840))[0])
        # a sheep farm placed instead (2 tools, 4 wood, 220 gold): the money drop is too large
        self.assertFalse(opening.placement_ok(cost["forester"]["cost"], before, self.snap(48.0, 46.0, 99780))[0])
        # a weaver (3 tools) instead of the market (3 tools, 250): money and wood give it away
        self.assertFalse(opening.placement_ok(cost["market"]["cost"], before, self.snap(47.0, 46.0, 99700))[0])
        # nothing placed, only upkeep
        self.assertFalse(opening.placement_ok(cost["forester"]["cost"], before, self.snap(50.0, 50.0, 99990))[0])
        # wood fell more than the cost
        self.assertFalse(opening.placement_ok(cost["sheep"]["cost"], before, self.snap(48.0, 45.0, 99780))[0])
        # a cart delivered wood meanwhile: still this building
        self.assertTrue(opening.placement_ok(cost["sheep"]["cost"], before, self.snap(48.0, 49.0, 99780))[0])
        # no upkeep bound: cannot prove it
        self.assertFalse(opening.placement_ok(cost["forester"]["cost"], self.snap(50.0, 50.0, 100000, None),
                                              self.snap(48.0, 50.0, 99850, None))[0])

    def test_road_tiles(self):
        self.assertEqual(opening.road_tiles(15, 3, 5), (3, 3))
        self.assertEqual(opening.road_tiles(10, 5, 5), (1, 2))      # could be one tile plus upkeep
        self.assertIsNone(opening.road_tiles(15, None, 5))

    def test_find_own(self):
        class B:
            def __init__(self, cities, players):
                self.st = {"cities": cities, "players": players}

            def state(self, what):
                return self.st

        own = {"addr": "0x100", "inhabitants": 0, "tools_t": 50.0, "wood_t": 50.0}
        ai = {"addr": "0x200", "inhabitants": 64, "tools_t": 50.0, "wood_t": 35.0}
        empty = {"addr": "0x300", "inhabitants": 0, "tools_t": 0.0, "wood_t": 0.0}
        human = {"addr": "0x500", "kind": "HUMAN"}
        cg = {"addr": "0x600", "kind": "CG01"}
        city, player, _ = opening.find_own(B([ai, own, empty], [cg, human]))
        self.assertEqual((city, player), (0x100, 0x500))
        with self.assertRaises(RuntimeError):
            opening.find_own(B([own, dict(ai, inhabitants=0)], [human]))   # two candidates


class Agent(unittest.TestCase):
    def test_key_names(self):
        self.assertEqual([agent.xkey(k) for k in ("ESC", "F10", "B", "CTRL+X", "DOT")],
                         ["Escape", "F10", "b", "Control_L+x", "period"])

    def test_run_id_and_display_validation(self):
        for bad in ("..", "../x", "a/b", ".hidden", "", "x" * 101):
            with self.assertRaises(ValueError, msg=bad):
                agent.Instance(":91", bad if bad else "-")
        with self.assertRaises(ValueError):
            agent.Instance("91")
        inst = agent.Instance(":91", "run-ok_1.2")
        self.assertTrue(inst.run.endswith("/run-ok_1.2"))

    def test_env_is_silent(self):
        noisy = {"PULSE_SERVER": "unix:/mnt/wslg/PulseServer", "PULSE_COOKIE": "x", "WSL_INTEROP": "/run/x",
                 "WAYLAND_DISPLAY": "wayland-0", "XDG_RUNTIME_DIR": "/mnt/wslg/runtime-dir", "PATH": "/bin"}
        with mock.patch.dict(os.environ, noisy, clear=True):
            e = agent.Instance(":91", "r1").env()
            self.assertFalse([k for k in e if k.startswith("PULSE_")])
            self.assertNotIn("WSL_INTEROP", e)
            self.assertNotIn("WAYLAND_DISPLAY", e)
            self.assertEqual(e["XDG_RUNTIME_DIR"], f"{agent.RUNS}/r1/xdg")
            self.assertIn(agent.AUDIO_OFF, e["WINEDLLOVERRIDES"])
            self.assertEqual((e["DISPLAY"], e["WINEPREFIX"]), (":91", f"{agent.RUNS}/r1/prefix"))
            loud = agent.Instance(":91", "r2", audio=True).env()
            self.assertEqual(loud["PULSE_SERVER"], "unix:/mnt/wslg/PulseServer")

    def test_log_keeps_colliding_fields(self):
        inst = agent.Instance(":91", "r3")
        with tempfile.TemporaryDirectory() as d:
            inst.out = d
            inst.log("reg_add", kind="REG_DWORD", t=5)
            inst.log_f.close()
            with open(os.path.join(d, "actions.jsonl"), encoding="utf-8") as f:
                rec = json.loads(f.read())
        self.assertEqual((rec["kind"], rec["arg_kind"], rec["arg_t"]), ("reg_add", "REG_DWORD", 5))

    def fake_run(self, d: str, run_id: str = "fake-run", keep: bool = False):
        """An Instance whose run folder is a temp dir, marked prepared (no mounts, no processes)."""
        inst = agent.Instance(":91", run_id, keep=keep)
        inst.run = os.path.join(d, run_id)
        inst.out, inst.game, inst.prefix = (os.path.join(inst.run, x) for x in ("out", "game", "prefix"))
        os.makedirs(inst.out)
        inst.created = True
        return inst

    @staticmethod
    def kinds(path: str) -> list[str]:
        with open(path, encoding="utf-8") as f:
            return [json.loads(x)["kind"] for x in f]

    def test_log_never_creates_the_folder(self):
        with tempfile.TemporaryDirectory() as d:
            inst = agent.Instance(":91", "r5")
            inst.out = os.path.join(d, "run", "out")
            inst.log("early")                                           # before prepare: dropped
            self.assertFalse(os.path.exists(os.path.join(d, "run")))

    def test_log_after_stop_does_not_recreate_the_run(self):
        with tempfile.TemporaryDirectory() as d:
            inst = self.fake_run(d)
            inst.log("before")
            res = inst.stop()
            self.assertTrue(res.get("removed"), res)
            inst.log("watchdog_error", error="late")                    # a late thread's line
            self.assertFalse(os.path.exists(inst.run))

    def test_log_after_close_is_dropped(self):
        with tempfile.TemporaryDirectory() as d:
            inst = self.fake_run(d, keep=True)
            inst.log("before")
            inst.log_close()
            inst.log("after")
            self.assertEqual(self.kinds(os.path.join(inst.out, "actions.jsonl")), ["before"])

    def test_stop_joins_the_watchdog_before_closing_the_log(self):
        with tempfile.TemporaryDirectory() as d:
            inst = self.fake_run(d, keep=True)

            def slow_watchdog():                                        # its last pass ends after stop()
                inst.guard_stop.wait()
                time.sleep(0.3)
                inst.log("late")
            inst.watchdog = threading.Thread(target=slow_watchdog, daemon=True)
            inst.watchdog.start()
            res = inst.stop()
            self.assertIs(res["watchdog_stopped"], True)
            self.assertEqual(self.kinds(os.path.join(inst.out, "actions.jsonl")), ["late", "stop"])

    def test_concurrent_log_lines_do_not_interleave(self):

        class SlowFile:                         # writes one character at a time, yielding between
            def __init__(self, sink):
                self.sink = sink

            def write(self, s):
                for ch in s:
                    self.sink.append(ch)
                    time.sleep(0)

            def flush(self):
                pass

            def close(self):
                pass

        sink: list[str] = []
        opens: list[str] = []

        def slow_open(path, *a, **kw):          # a slow open widens the lazy-open race
            opens.append(path)
            time.sleep(0.05)
            return SlowFile(sink)

        with tempfile.TemporaryDirectory() as d:
            inst = agent.Instance(":91", "r6")
            inst.out = d
            go = threading.Event()

            def worker(i):
                go.wait()
                for j in range(20):
                    inst.log("w", who=i, j=j, pad="x" * 40)
            with mock.patch.object(agent, "open", slow_open, create=True):
                ts = [threading.Thread(target=worker, args=(i,)) for i in range(4)]
                for t in ts:
                    t.start()
                go.set()
                for t in ts:
                    t.join()
        lines = "".join(sink).splitlines()
        self.assertEqual(len(opens), 1)
        self.assertEqual(len(lines), 80)
        recs = [json.loads(x) for x in lines]                           # every line is whole
        self.assertEqual(sorted((r["who"], r["j"]) for r in recs), [(i, j) for i in range(4) for j in range(20)])

    @staticmethod
    def symlink_or_skip(test, target, link):
        try:
            os.symlink(target, link, target_is_directory=True)
        except (OSError, NotImplementedError) as ex:      # Windows without the symlink privilege
            test.skipTest(f"no symlinks here: {ex}")

    def test_within(self):
        with tempfile.TemporaryDirectory() as d:
            base, outside = os.path.join(d, "out"), os.path.join(d, "outside")
            os.makedirs(os.path.join(base, "shots"))
            os.makedirs(outside)
            ok = os.path.join(base, "shots", "a.png")
            self.assertEqual(agent.within(ok, base), os.path.realpath(ok))
            for bad in (base, os.path.join(base, "..", "outside", "a.png"),
                        os.path.join(base, "shots", "..", "a.png"),          # '..' even when it stays inside
                        os.path.join(outside, "a.png"), base + "side", "", os.path.abspath(os.sep)):
                with self.assertRaises(ValueError, msg=bad):
                    agent.within(bad, base)
            self.symlink_or_skip(self, outside, os.path.join(base, "link"))
            with self.assertRaises(ValueError):
                agent.within(os.path.join(base, "link", "a.png"), base)

    def test_out_win_must_be_inside_cache_orig(self):
        self.assertEqual(os.path.realpath(agent.OUT_WIN_BASE), os.path.realpath(os.path.join(REPO, "cache", "orig")))
        # what backend.py passes (before its /mnt/c translation, which the distro side undoes)
        out_dir = os.path.join(REPO, "cache", "orig", "wine-20261022", "d91-20261022-120000")
        self.assertEqual(agent.within(out_dir, agent.OUT_WIN_BASE), os.path.realpath(out_dir))
        with tempfile.TemporaryDirectory() as d:
            base = os.path.join(d, "cache", "orig")
            with mock.patch.object(agent, "OUT_WIN_BASE", base):
                good = os.path.join(base, "s", "d91")
                self.assertEqual(agent.Instance(":91", "r7", out_win=good).out_win, os.path.realpath(good))
                for bad in (base, os.path.join(d, "elsewhere"), os.path.join(base, "..", "x"), "/root"):
                    with self.assertRaises(ValueError, msg=bad):
                        agent.Instance(":91", "r7", out_win=bad)

    def test_screenshot_dest_and_names(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as d:
            base = os.path.join(d, "cache", "orig")
            outside = os.path.join(d, "outside")
            os.makedirs(outside)
            with mock.patch.object(agent, "OUT_WIN_BASE", base):
                inst = self.fake_run(d)
                inst.out_win = agent.within(os.path.join(base, "s", "d91"), base)
            inst.grab = lambda: Image.new("RGB", agent.DESKTOP)
            r = inst.screenshot("a", dest="extra/a.png")                       # relative: under out
            self.assertEqual(r["dest"], os.path.realpath(os.path.join(inst.out, "extra", "a.png")))
            win = os.path.join(inst.out_win, "shots", "b.png")
            self.assertTrue(os.path.isfile(inst.screenshot("b", dest=win)["dest"]))
            for bad in (os.path.join(outside, "c.png"), "../c.png", os.path.join(inst.out, "..", "c.png"),
                        os.path.join(d, "c.png")):
                with self.assertRaises(ValueError, msg=bad):
                    inst.screenshot("c", dest=bad)
            self.assertEqual(os.listdir(outside), [])
            self.assertFalse(os.path.exists(os.path.join(d, "c.png")))
            for bad in ("../c", "a/b", "", ".x"):
                with self.assertRaises(ValueError, msg=bad):
                    inst.screenshot(bad)
            self.assertFalse(os.path.exists(os.path.join(inst.out, "c.png")))
            self.symlink_or_skip(self, outside, os.path.join(inst.out, "link"))
            with self.assertRaises(ValueError):
                inst.screenshot("c", dest="link/c.png")
            self.assertEqual(os.listdir(outside), [])

    def test_file_names_are_one_component(self):
        inst = agent.Instance(":91", "r8")
        inst.key = mock.Mock()
        inst.frame_image = mock.Mock()
        for call in (lambda: inst.learn("../x"), lambda: inst.poll_start("a/b"),
                     lambda: inst.sample_start(name="..\\x"), lambda: inst.quicksave(keep_as="../m0001")):
            with self.assertRaises(ValueError):
                call()
        inst.key.assert_not_called()                       # refused before F10 is pressed
        inst.frame_image.assert_not_called()

    def test_stop_without_prepare_touches_nothing(self):
        inst = agent.Instance(":91", "never-prepared")
        res = inst.stop()
        self.assertIn("skipped", res)
        self.assertIs(inst.stop(), res)                                 # idempotent

    def test_reg_add_and_launch_need_prepare(self):
        inst = agent.Instance(":91", "r4")
        with self.assertRaises(RuntimeError):
            inst.reg_add("HKCU\\X", "Y", "1")
        with self.assertRaises(RuntimeError):
            inst.launch()


class FakeInstance:
    run, display = "/root/anno/runs/fake", ":91"

    def __init__(self):
        self.stops = 0

    def stop(self):
        self.stops += 1
        return {"stopped": self.stops}

    def status(self):
        return {"alive": True}


class BrokenOut:
    def write(self, s):
        raise BrokenPipeError

    def flush(self):
        raise BrokenPipeError


class Serve(unittest.TestCase):
    def run_serve(self, lines, out=None):
        inst = FakeInstance()
        out = out or io.StringIO()
        agent.serve(inst, stdin=io.StringIO("".join(x + "\n" for x in lines)), stdout=out)
        return inst, out

    def test_eof_stops_once(self):
        inst, out = self.run_serve(['{"id": 1, "cmd": "status"}'])
        self.assertEqual(inst.stops, 1)
        replies = [json.loads(x) for x in out.getvalue().splitlines()]
        self.assertEqual([r["id"] for r in replies], [0, 1])

    def test_non_object_and_unknown_lines_answer_errors_then_stop_once(self):
        inst, out = self.run_serve(["[1, 2]", "not json", '{"id": 2, "cmd": "rm_rf"}', '{"id": 3, "args": 7, "cmd": "status"}'])
        replies = [json.loads(x) for x in out.getvalue().splitlines()][1:]
        self.assertEqual([r["ok"] for r in replies], [False, False, False, False])
        self.assertEqual(inst.stops, 1)

    def test_broken_pipe_still_stops(self):
        inst, _ = self.run_serve(['{"id": 1, "cmd": "status"}'], out=BrokenOut())
        self.assertEqual(inst.stops, 1)

    def test_stop_command_then_finally(self):
        inst, _ = self.run_serve(['{"id": 1, "cmd": "stop"}', '{"id": 2, "cmd": "status"}'])
        self.assertGreaterEqual(inst.stops, 1)                          # Instance.stop is idempotent


class BackendCall(unittest.TestCase):
    def fake(self, replies):
        b = object.__new__(backend.WineBackend)
        b._id, b.timeout, b.noise = 0, 1.0, []
        b._lines = queue.Queue()
        for r in replies:
            b._lines.put(r)

        class In:
            def __init__(self):
                self.sent = []

            def write(self, data):
                self.sent.append(json.loads(data))

            def flush(self):
                pass

        b.proc = mock.Mock(stdin=In())
        return b

    def test_id_matching_noise_and_args(self):
        b = self.fake([b'{"id": 0, "ok": true, "result": "stale"}\n', "wsl: kein Verteiler\n".encode("utf-16-le"),
                       b'{"id": 1, "ok": true, "result": 42}\n'])
        self.assertEqual(b.call("find_game", {"wait": 150, "timeout": 3}, timeout=1.0), 42)
        self.assertEqual(b.proc.stdin.sent[0]["args"], {"wait": 150, "timeout": 3})
        self.assertEqual(len(b.noise), 1)

    def test_error_reply_raises(self):
        b = self.fake([b'{"id": 1, "ok": false, "error": "boom"}\n'])
        with self.assertRaises(backend.BackendError):
            b.call("status")


class BackendInterface(unittest.TestCase):
    def test_key_hold_default_is_the_same_everywhere(self):
        import inspect
        defaults = {cls.__name__: inspect.signature(cls.key).parameters["hold"].default
                    for cls in (backend.WineBackend, backend.WindowsBackend, agent.Instance)}
        self.assertEqual(defaults, dict.fromkeys(defaults, 0.0))
        self.assertIn("key(name, hold=0)", backend.__doc__)


class LiveRunOne(unittest.TestCase):
    class B:
        def __init__(self):
            self.calls = []

        def status(self):
            return {"alive": True}

        def call(self, cmd, args=None, timeout=None):
            self.calls.append((cmd, args, timeout))
            return "agent:" + cmd

    def test_items(self):
        b = self.B()
        self.assertFalse(live.run_one(b, [1, 2])["ok"])
        self.assertFalse(live.run_one(b, {"macro": "_private"})["ok"])
        self.assertTrue(live.run_one(b, {"cmd": "status"})["ok"])
        self.assertEqual(live.run_one(b, {"cmd": "scan", "args": {"value": 5}, "timeout": 9})["result"], "agent:scan")
        self.assertEqual(b.calls[-1], ("scan", {"value": 5}, 9))
        for quit_cmd in ("quit", "stop"):
            with self.assertRaises(live.SessionOver):
                live.run_one(b, {"cmd": quit_cmd})
        with self.assertRaises(live.SessionOver):
            live.run_one(b, {"batch": [{"cmd": "status"}, {"cmd": "quit"}]})


if __name__ == "__main__":
    unittest.main()
