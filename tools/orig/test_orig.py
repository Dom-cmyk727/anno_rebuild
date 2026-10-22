"""Unit tests for the T16 harness tools that need no running game (synthetic data only).

    python -I tools/orig/test_orig.py [-v]        (Pillow needed)
"""

from __future__ import annotations

import os
import struct
import sys
import unittest

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)
sys.path.insert(0, os.path.join(_HERE, "wine"))

import gamestate  # noqa: E402
import opening  # noqa: E402
import screensig  # noqa: E402
import timing  # noqa: E402
import userdat  # noqa: E402


class FakeMem:
    """gamestate's memory interface over a dict of {base address: bytes}."""

    def __init__(self, blocks: dict[int, bytes]):
        self.blocks = blocks

    def read(self, addr, n):
        for base, data in self.blocks.items():
            if base <= addr and addr + n <= base + len(data):
                return data[addr - base:addr - base + n]
        return None

    def regions(self):
        for base, data in sorted(self.blocks.items()):
            yield base, base + len(data)


def world_obj(vt: int, counter: int, tick: int, cursor: int = 7) -> bytes:
    b = bytearray(0x120)
    struct.pack_into("<I", b, 0, vt)
    struct.pack_into("<H", b, gamestate.WORLD_CURSOR, cursor)
    struct.pack_into("<Q", b, gamestate.WORLD_COUNTER, counter)
    struct.pack_into("<I", b, gamestate.WORLD_AT108, tick)
    struct.pack_into("<I", b, gamestate.WORLD_TICK, tick)
    return bytes(b)


def player_obj(vt: int, world: int, name: bytes, kind: bytes, money: int) -> bytes:
    b = bytearray(gamestate.PLAYER_MONEY + 4)
    struct.pack_into("<I", b, 0, vt)
    struct.pack_into("<I", b, gamestate.PLAYER_WORLD, world)
    b[gamestate.PLAYER_NAME:gamestate.PLAYER_NAME + len(name)] = name
    b[gamestate.PLAYER_KIND:gamestate.PLAYER_KIND + len(kind)] = kind
    struct.pack_into("<i", b, gamestate.PLAYER_MONEY, money)
    return bytes(b)


class GameState(unittest.TestCase):
    WELT = 0x580000

    def test_world_check_is_unmasked(self):
        good = {"counter": 1371, "tick": 685}
        self.assertTrue(gamestate.is_world(good))
        # A stray 64-bit value with 33 low zero bits: a masked compare would accept tick 0.
        stray = {"counter": 0x43F3333300000000, "tick": 0}
        self.assertFalse(gamestate.is_world(stray))
        self.assertFalse(gamestate.is_world({"counter": 0, "tick": 0}))

    def test_find_world_skips_stray_and_finds_live(self):
        vt = self.WELT + gamestate.WELT_WORLD_VT
        mem = FakeMem({0x1000: world_obj(vt, 0x43F3333300000000, 0) + world_obj(vt, 5000, 2500)})
        self.assertEqual(gamestate.find_world(mem, self.WELT), 0x1000 + 0x120)

    def test_msvc_rand_head(self):
        # MSVC rand() from seed 1: the documented head of the table (tick-loop.md F6).
        self.assertEqual(gamestate.msvc_rand(6), [41, 18467, 6334, 26500, 19169, 15724])

    def test_players_filter_by_world_and_text(self):
        vt = self.WELT + gamestate.WELT_PLAYER_VT
        world = 0x9000
        a = player_obj(vt, world, b"ANNOnymous", b"HUMAN", 500000)
        freed = player_obj(vt, 0xDEAD, b"Ghost", b"CG01", 1)           # other world: rejected
        junk = player_obj(vt, world, b"\x01\x02", b"HUMAN", 3)           # name not text: rejected
        mem = FakeMem({0x2000: a, 0x3000: freed, 0x4000: junk})
        got = gamestate.players(mem, self.WELT, world)
        self.assertEqual([(p["name"], p["kind"], p["money"]) for p in got], [("ANNOnymous", "HUMAN", 500000)])

    def test_city_capacity_plausibility(self):
        vt = self.WELT + gamestate.WELT_CITY_VT
        b = bytearray(gamestate.CITY_CAPACITY + 4)
        struct.pack_into("<I", b, 0, vt)
        struct.pack_into("<I", b, gamestate.CITY_CAPACITY, 50 * 256)
        struct.pack_into("<I", b, gamestate.CITY_STORE + 12 * 41, 48 * 256)
        dead = bytearray(b)
        struct.pack_into("<I", dead, gamestate.CITY_CAPACITY, 0)
        mem = FakeMem({0x5000: bytes(b), 0x6000: bytes(dead)})
        got = gamestate.cities(mem, self.WELT)
        self.assertEqual(len(got), 1)
        self.assertEqual((got[0]["capacity_t"], got[0]["tools_t"]), (50.0, 48.0))


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
        self.assertEqual(userdat.read(out)["volume"], [userdat.VOLUME_MIN, userdat.VOLUME_MIN])
        diff = [i for i in range(len(data)) if data[i] != out[i]]
        self.assertEqual(len(diff), 8)
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

    def picture(self, bars: int, w: int = 200, h: int = 150):
        img = self.Image.new("RGB", (w, h), (90, 90, 90))
        d = self.Draw.Draw(img)
        for i in range(bars):
            d.rectangle([20 + 35 * i, 20, 35 + 35 * i, 130], fill=(230, 230, 230))
        return img

    def test_same_picture_distance_zero_and_scale_tolerant(self):
        a = self.picture(3)
        self.assertEqual(screensig.distance(screensig.dhash(a), screensig.dhash(a)), 0)
        big = a.resize((400, 300))
        self.assertLessEqual(screensig.distance(screensig.dhash(a), screensig.dhash(big)), 12)

    def test_recognize_and_black(self):
        a, b = self.picture(2), self.picture(5)
        table = {"screens": {"two": {"regions": screensig.learn(a)}, "five": {"regions": screensig.learn(b)}}}
        self.assertEqual(screensig.recognize(a, table)["screen"], "two")
        self.assertEqual(screensig.recognize(b, table)["screen"], "five")
        black = self.Image.new("RGB", (200, 150), (0, 0, 0))
        self.assertEqual(screensig.recognize(black, table)["screen"], "black")

    def test_calibrate_halves_nearest_distance(self):
        a, b = self.picture(2), self.picture(5)
        table = {"screens": {"two": {"regions": screensig.learn(a)}, "five": {"regions": screensig.learn(b)}}}
        rep = screensig.calibrate(table, {"two": a, "five": b})
        d = screensig.distance(screensig.dhash(a), screensig.dhash(b))
        want = max(0, min(screensig.DEFAULT_MAX_DIST, d // 2 - 1))
        self.assertEqual(table["screens"]["two"]["regions"][0]["max_dist"], want)
        self.assertEqual(rep["two[0]"]["nearest"], "five")


class TimingModel(unittest.TestCase):
    def series(self, gaps_ms, speed, start=1000):
        """A synthetic poll series: frames at the given gaps, counter advanced like the game
        (whole ms with the rest carried). Frames are seen 0.25 ms late, as a poller would, so no
        timestamp sits exactly on a millisecond boundary."""
        c, out, acc, prev = start, [(0.0, start)], 0.0, 0
        for g in gaps_ms:
            acc += g
            t = (acc + 0.25) / 1000.0
            now = int(acc)
            dt = now - prev
            prev = now
            step = int(dt * speed + 0.5)
            c += min(step, timing.CAP_MS) if step >= 1 else 0
            out.append((t, c))
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

    def test_cap_counted(self):
        s = self.series([150.0] * 10, 1.0)
        r = timing.analyse(s, 1.0)
        self.assertEqual(r["capped_frames"], 10)
        self.assertAlmostEqual(r["lost_ms"], 500.0, delta=1.0)


class AgentAndOpening(unittest.TestCase):
    def test_key_names(self):
        import agent  # noqa: PLC0415 - tools/orig/wine
        self.assertEqual(agent.xkey("ESC"), "Escape")
        self.assertEqual(agent.xkey("F10"), "F10")
        self.assertEqual(agent.xkey("B"), "b")
        self.assertEqual(agent.xkey("CTRL+X"), "Control_L+x")
        self.assertEqual(agent.xkey("DOT"), "period")

    def test_spots_stay_on_the_map(self):
        cfg = opening.load()
        for at in ([0.02, 0.02], [0.74, 0.93], [0.3, 0.3]):
            for x, y in opening.spots(cfg, at):
                self.assertTrue(0.01 <= x <= opening.MAP_RIGHT - 0.01 and 0.01 <= y <= opening.MAP_BOTTOM - 0.01)

    def test_opening_fits_the_start_store(self):
        cfg = opening.load()
        tools = sum(cfg["buildings"][s["build"]]["cost"]["tools"] for s in cfg["steps"] if "build" in s)
        wood = sum(cfg["buildings"][s["build"]]["cost"]["wood"] for s in cfg["steps"] if "build" in s)
        self.assertEqual((tools, wood), (29, 42))          # measured: 50/50 -> 21/8
        self.assertLessEqual(tools, 50)
        self.assertLessEqual(wood, 50)


if __name__ == "__main__":
    unittest.main()
