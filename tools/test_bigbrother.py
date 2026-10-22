#!/usr/bin/env python3
"""Tests of the scenario-script reference (tools/bigbrother.py).

  python -I tools/test_bigbrother.py

The doctests and the synthetic cases always run. With ANNO1503_GAME_DIR set, the corpus cases parse
the BIGBROTHER chunk of every shipped .sze (skipped with a message otherwise).
"""
import doctest
import glob
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bigbrother as bb  # noqa: E402


def load_tests(loader, tests, ignore):
    tests.addTests(doctest.DocTestSuite(bb))
    return tests


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
        swapped = bb.SAMPLE.replace(b"[AREA_DEFINE]", b"[TMP]").replace(
            b"[PLAYERMAPPER_DEFINE]", b"[AREA_DEFINE]").replace(b"[TMP]", b"[PLAYERMAPPER_DEFINE]")
        for bad in (swapped, bb.SAMPLE.replace(b"[CAMARAPOS]\n0,0\r\n", b"")):
            with self.assertRaises(bb.ScriptError):
                bb.parse(bad)

    def test_text_outside_the_grammar_is_an_error_not_a_fallback(self):
        for old, new in ((b"SET GO = TRUE;", b"SET GO = = TRUE;"),       # two operators
                         (b"LOST( \"HUMAN\" );", b"LOST( \"HUMAN\" ;"),   # unclosed call
                         (b"> 9)", b"> 9"),                               # unclosed group
                         (b"[SIGNALS]\n", b"[SIGNALS]\nGO TRUE\r\n"),     # signal without '='
                         (b"QUESTLOG_DRAW;", b"QUESTLOG_DRAW; @")):       # a byte no token takes
            with self.subTest(new=new), self.assertRaises(bb.ScriptError):
                bb.parse(bb.SAMPLE.replace(old, new))

    def test_text_before_the_first_section_is_an_error(self):
        with self.assertRaises(bb.ScriptError):
            bb.parse(b"X\n" + bb.SAMPLE)


class Corpus(unittest.TestCase):
    def setUp(self):
        game = os.environ.get("ANNO1503_GAME_DIR")
        if not game:
            self.skipTest("needs ANNO1503_GAME_DIR")
        if not os.path.isfile(os.path.join(game, "Anlagen.dat")):
            self.fail(f"ANNO1503_GAME_DIR={game} has no Anlagen.dat")
        self.game = game
        self.files = sorted(glob.glob(os.path.join(game, "Scenes", "**", "*.sze"), recursive=True))

    def test_all_shipped_scripts_parse_and_re_emit_byte_identically(self):
        self.assertEqual(len(self.files), 50)
        for f in self.files:
            with self.subTest(f=os.path.basename(f)):
                data = bb.payload(f)
                self.assertEqual(bb.parse(data).emit(), data)

    def test_signals_are_the_set_variables_plus_the_init_signal(self):
        for f in self.files:
            with self.subTest(f=os.path.basename(f)):
                s = bb.parse(bb.payload(f))
                sets = [(x.name, x.value.py) for x in s.section("[SCRIPT_KERNEL]").items
                        if isinstance(x, bb.Assign) and x.is_set]
                signals = [(g["name"], g["value"]) for g in s.section("[SIGNALS]").items]
                self.assertEqual(signals, [("SCHUETTIINITSIG", True)] + sets)

    def test_the_interpreter_has_284_uppercase_names(self):
        self.assertEqual(len(bb.dll_names(os.path.join(self.game, "AnnoScene.dll"))), 284)


if __name__ == "__main__":
    unittest.main()
