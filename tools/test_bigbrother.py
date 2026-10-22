#!/usr/bin/env python3
"""Tests of the scenario-script reference (tools/bigbrother.py).

  python -I tools/test_bigbrother.py

The doctests and the synthetic cases always run. With ANNO1503_GAME_DIR set, the corpus cases parse
the BIGBROTHER chunk of every shipped .sze (skipped with a message otherwise).
"""
import collections
import doctest
import glob
import os
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bigbrother as bb  # noqa: E402


def load_tests(loader, tests, ignore):
    tests.addTests(doctest.DocTestSuite(bb))
    return tests


def chunk(name: bytes, data: bytes) -> bytes:
    return name.ljust(12, b"\0") + struct.pack("<I", len(data)) + data


class Tree(unittest.TestCase):
    def setUp(self):
        self.s = bb.parse(bb.SAMPLE)
        self.k = self.s.section("[SCRIPT_KERNEL]").items

    def test_defines(self):
        objs = self.s.section("[OBJECT_DEFINE]").items
        self.assertEqual([(o["kind"], o["name"], o["numbers"]) for o in objs],
                         [("OBJECT_FIGUR", "BOAT", [7]), ("OBJECT_ANLAGE", "MILL", [10, 20, 1001])])
        self.assertEqual([(a["name"], a["rect"]) for a in self.s.section("[AREA_DEFINE]").items],
                         [("A1", [1, 2, 30, 40])])
        self.assertEqual([(p["name"], p["player"]) for p in self.s.section("[PLAYERMAPPER_DEFINE]").items],
                         [("HUMAN", 0), ("DUMMYPLAYER", 31)])
        self.assertEqual([c["pos"] for c in self.s.section("[CAMARAPOS]").items], [[0, 0]])
        moved = bb.parse(bb.SAMPLE.replace(b"[CAMARAPOS]\n0,0", b"[CAMARAPOS]\n3,4"))
        self.assertEqual(moved.section("[CAMARAPOS]").items[0]["pos"], [3, 4])
        self.assertEqual([(g["name"], g["value"]) for g in self.s.section("[SIGNALS]").items],
                         [("SCHUETTIINITSIG", True), ("GO", True)])

    def test_statements(self):
        init, set_, if1, if2 = self.k
        (show,) = init.body
        self.assertEqual((show.name, [(a.kind, a.py) for a in show.args], show.parens, show.semicolon),
                         ("SHOWTEXT", [("string", "A.HTM")], True, True))
        self.assertEqual((set_.name, set_.value.kind, set_.value.py, set_.is_set, set_.semicolon),
                         ("GO", "bool", True, True, True))
        assign, draw = if1.then
        self.assertEqual((assign.name, assign.value.py, assign.is_set), ("GO", False, False))
        self.assertEqual((draw.name, draw.args, draw.parens, draw.semicolon), ("QUESTLOG_DRAW", [], False, True))
        self.assertIsNone(if1.els)
        (lost,), (won,) = if2.then, if2.els
        self.assertEqual((lost.name, [a.py for a in lost.args], lost.semicolon), ("LOST", ["HUMAN"], True))
        self.assertEqual((won.name, [a.py for a in won.args], won.semicolon), ("WON", ["HUMAN"], False))

    def test_conditions(self):
        go, op, group = self.k[2].cond.items
        self.assertEqual((go.subject, go.name, go.query, go.op), (None, "GO", None, None))
        self.assertEqual(op, "&&")
        area, op2, player = group.items
        self.assertEqual((area.subject, area.name, area.query, area.arg), ("AREA", "A1", "TOUCHED_BY_SHIP_FROM", "HUMAN"))
        self.assertEqual(op2, "||")
        self.assertEqual((player.subject, player.query, player.arg, player.op, player.value.kind, player.value.py),
                         ("PLAYER", "INHABITANTCOUNT", "SIEDLER", ">", "number", 9))
        (killed,) = self.k[3].cond.items
        self.assertEqual((killed.subject, killed.name, killed.query, killed.arg), ("OBJECT", "BOAT", "KILLED", None))

    def test_a_missing_semicolon_at_the_end_of_a_line_is_kept(self):
        s = bb.parse(bb.SAMPLE.replace(b"SET GO = TRUE;\r\n", b"SET GO = TRUE\r\n"))
        self.assertFalse(s.section("[SCRIPT_KERNEL]").items[1].semicolon)


class Synthetic(unittest.TestCase):
    def test_every_byte_is_one_token(self):
        toks = bb.lex(bb.SAMPLE.decode(bb.ENCODING))
        self.assertEqual("".join(t.text for t in toks).encode(bb.ENCODING), bb.SAMPLE)
        self.assertEqual([t.off for t in toks],
                         [sum(len(u.text) for u in toks[:i]) for i in range(len(toks))])

    def test_latin1_bytes_round_trip(self):
        data = bb.SAMPLE.replace(b'"A.HTM"', b'"\xe4\xf6\xfc.HTM"')
        self.assertEqual(bb.parse(data).emit(), data)

    def test_sections_out_of_order_or_missing_are_errors(self):
        head, sig = bb.SAMPLE.split(b"[SIGNALS]")
        sig, cam = (b"[SIGNALS]" + sig).split(b"[CAMARAPOS]")
        cam = b"[CAMARAPOS]" + cam
        bb.parse(head + sig + cam)                               # the pieces are right
        a0, rest = bb.SAMPLE.split(b"[AREA_DEFINE]")
        area, rest = (b"[AREA_DEFINE]" + rest).split(b"[PLAYERMAPPER_DEFINE]")
        pm, rest = (b"[PLAYERMAPPER_DEFINE]" + rest).split(b"[SCRIPT_KERNEL]")
        for bad in (head + cam + sig,                            # whole sections swapped, bodies valid
                    a0 + pm + area + b"[SCRIPT_KERNEL]" + rest,
                    head + sig,                                  # one missing
                    head + sig + cam + cam):                     # one twice
            with self.assertRaises(bb.ScriptError):
                bb.parse(bad)

    def test_text_outside_the_grammar_is_an_error_not_a_fallback(self):
        cases = (
            (b"SET GO = TRUE;", b"SET GO = = TRUE;"),                          # two operators
            (b"LOST( \"HUMAN\" );", b"LOST( \"HUMAN\" ;"),                     # unclosed call
            (b"LOST( \"HUMAN\" );", b"LOST( \"HUMAN\", );"),                   # trailing ','
            (b"> 9)", b"> 9"),                                                 # unclosed group
            (b"> 9)", b"> +9)"),                                               # no '+' sign
            (b"[SIGNALS]\n", b"[SIGNALS]\nGO TRUE\r\n"),                       # signal without '='
            (b"GO = TRUE\r\n\n", b"GO = MAYBE\r\n\n"),                         # signal value
            (b"GO = TRUE\r\n\n", b"GO = TRUE\r\nGO = TRUE\r\n\n"),             # signal twice
            (b"QUESTLOG_DRAW;", b"QUESTLOG_DRAW; @"),                          # a byte no token takes
            (b"QUESTLOG_DRAW;", b"QUESTLOG_DRAW;;;"),                          # empty statements
            (b"LOST( \"HUMAN\" );", b"LOST( \"HUMAN\" );;"),
            (b"SET GO = TRUE;\r\n", b"FOO\r\nSET GO = TRUE;\r\n"),            # a stray name
            (b" QUESTLOG_DRAW;", b" ELSE;"),                                   # keyword as a call
            (b" QUESTLOG_DRAW;", b" IF GO THEN { QUESTLOG_DRAW; }"),           # nested IF
            (b" QUESTLOG_DRAW;\r\n}", b" QUESTLOG_DRAW;\r\n FOO\r\n}"),        # bare name in a block
            (b"WON( \"HUMAN\" )\r\n}", b"WON( \"HUMAN\" );\r\n FOO\r\n"),     # ... at the section end
            (b" GO = FALSE;", b" GO = FALSE FALSE;"),                          # value then a name
            (b" GO = FALSE;", b" GO = FALSE QUESTLOG_DRAW;"),                  # missing ';' mid-line
            (b" GO = FALSE;", b" GO = MAYBE;"),                                # a name as a value
            (b"SET GO = TRUE;", b"SET IF = TRUE;"),                            # keyword as a variable
            (b"SET GO = TRUE;\r\n", b"SET GO = TRUE;\r\nFOO;\r\n"),           # unknown statement
            (b'SHOWTEXT("A.HTM")', b'SHOWTEXT("A.\r\nHTM")'),                  # string over two lines
            (b'IF OBJECT "BOAT" KILLED THEN', b"IF THEN THEN"),                # keyword as a condition
            (b'IF OBJECT "BOAT" KILLED THEN', b"IF TRUE == FALSE THEN"),       # value as a condition
            (b'"BOAT" KILLED THEN', b'"BOAT" THEN THEN'),                      # keyword as a query
            (b'"SIEDLER" > 9', b'"SIEDLER" "ADEL" > 9'),                       # two query strings
            (b"> 9)", b"!= 9)"), (b"> 9)", b"<= 9)"), (b"> 9)", b">= 9)"),    # unused comparisons
            (b"> 9)", b"= 9)"),
            (b"GO && (", b"GO , ("),                                           # ',' as a connective
            (b"[OBJECT_DEFINE]\nOBJECT_FIGUR", b"[OBJECT_DEFINE]OBJECT_FIGUR"),  # header glued
            (b"31\n[SCRIPT_KERNEL]", b"31 [SCRIPT_KERNEL]"),                   # header mid-line
            (b'OBJECT_FIGUR "BOAT" 7', b'OBJECT_FIGUR "BOAT" 7,8,9'),          # numbers per kind
            (b'OBJECT_ANLAGE "MILL" 10,20,1001', b'OBJECT_ANLAGE "MILL" 10'),
            (b'OBJECT_FIGUR "BOAT" 7', b'OBJECT_SCHIFF "BOAT" 7'),             # unknown kind
            (b'"A1" 1, 2, 30, 40\n', b'"A1" 1, 2, 30, 40 $ c\n'),              # comment in a define
            (b'"A1" 1, 2, 30, 40\n', b'"A1" 1, 2, 30, 40\n"A1" 5, 6, 7, 8\n'),  # area twice
            (b'"A1" 1, 2, 30, 40\n', b'"A1" 1, 2\n'),                          # area numbers
            (b'"HUMAN" 0\n', b'"HUMAN" 0, 1\n'),                               # one slot per name
            (b"[CAMARAPOS]\n0,0\r\n", b"[CAMARAPOS]\n"),                       # no position
            (b"[CAMARAPOS]\n0,0\r\n", b"[CAMARAPOS]\n0,0\r\n1,1\r\n"),         # two positions
        )
        for old, new in cases:
            self.assertIn(old, bb.SAMPLE)
            with self.subTest(new=new), self.assertRaises(bb.ScriptError):
                bb.parse(bb.SAMPLE.replace(old, new, 1))

    def test_text_before_the_first_section_is_an_error(self):
        for lead in (b"X\n", b"\n", b" "):
            with self.subTest(lead=lead), self.assertRaises(bb.ScriptError):
                bb.parse(lead + bb.SAMPLE)

    def test_the_payload_is_the_one_bigbrother_chunk(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "x.sze")
            for n, kids in ((1, chunk(b"SCENE", chunk(b"BIGBROTHER", bb.SAMPLE))),
                            (1, chunk(b"BIGBROTHER", bb.SAMPLE)),
                            (2, chunk(b"SCENE", chunk(b"BIGBROTHER", bb.SAMPLE) * 2)),
                            (0, chunk(b"SCENE", chunk(b"OTHER", b"x" * 16)))):
                with open(path, "wb") as f:
                    f.write(chunk(b"ANNOADDON1", kids))
                with self.subTest(n=n, kids=kids[:24]):
                    if n == 1:
                        self.assertEqual(bb.payload(path), bb.SAMPLE)
                    else:
                        with self.assertRaises(bb.ScriptError):
                            bb.payload(path)

    def test_timers_are_numbered(self):
        self.assertTrue(bb.is_timer("TIMER_0"))
        self.assertTrue(bb.is_timer("TIMER_12"))
        self.assertFalse(bb.is_timer("TIMER_FLAG"))


class Corpus(unittest.TestCase):
    def setUp(self):
        game = os.environ.get("ANNO1503_GAME_DIR")
        if not game:
            self.skipTest("needs ANNO1503_GAME_DIR")
        if not os.path.isfile(os.path.join(game, "Anlagen.dat")):
            self.fail(f"ANNO1503_GAME_DIR={game} has no Anlagen.dat")
        self.game = game
        self.files = sorted(glob.glob(os.path.join(game, "Scenes", "**", "*.sze"), recursive=True))

    def scripts(self):
        return [(os.path.basename(f), bb.parse(bb.payload(f))) for f in self.files]

    def test_all_shipped_scripts_parse_and_re_emit_byte_identically(self):
        self.assertEqual(len(self.files), 50)
        for f in self.files:
            with self.subTest(f=os.path.basename(f)):
                data = bb.payload(f)
                self.assertEqual(bb.parse(data).emit(), data)

    def test_the_counts_of_the_spec(self):
        n = collections.Counter()
        variables = set()
        for _name, s in self.scripts():
            kernel = s.section("[SCRIPT_KERNEL]")
            n["comments"] += sum(t.kind == "COMMENT" for t in kernel.tokens)
            for it in kernel.items:
                n[type(it).__name__] += 1
                n["SET"] += isinstance(it, bb.Assign) and it.is_set
                n["ELSE"] += isinstance(it, bb.If) and it.els is not None
            for x in bb.walk(kernel.items):
                if isinstance(x, bb.Assign):
                    variables.add(x.name)
                    n["assign in block"] += not x.is_set
                    n["no ;"] += not x.semicolon
                elif isinstance(x, bb.Call):
                    n["call"] += 1
                    n["no ;"] += not x.semicolon
                    n["no ()"] += not x.parens
                elif isinstance(x, bb.Primary) and not x.subject:
                    n["timer" if bb.is_timer(x.name) else "variable"] += 1
        self.assertEqual(
            (n["InitBlock"], n["SET"], n["If"], n["ELSE"], n["assign in block"], n["call"], n["no ;"],
             n["no ()"], n["timer"], n["variable"], n["comments"], len(variables)),
            (35, 271, 308, 3, 382, 1656, 4, 1, 24, 429, 1273, 147))

    def test_signals_are_the_set_variables_plus_the_init_signal(self):
        for name, s in self.scripts():
            with self.subTest(f=name):
                sets = [(x.name, x.value.py) for x in s.section("[SCRIPT_KERNEL]").items
                        if isinstance(x, bb.Assign) and x.is_set]
                signals = [(g["name"], g["value"]) for g in s.section("[SIGNALS]").items]
                self.assertEqual(signals, [("SCHUETTIINITSIG", True)] + sets)

    def test_the_interpreter_has_284_uppercase_names(self):
        self.assertEqual(len(bb.dll_names(os.path.join(self.game, "AnnoScene.dll"))), 284)


if __name__ == "__main__":
    unittest.main()
