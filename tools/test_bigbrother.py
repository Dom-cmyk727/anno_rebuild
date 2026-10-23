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

    def test_a_condition_deeper_than_the_limit_is_refused(self):
        n = bb.MAX_CONDITION_DEPTH

        def cond(text: str, **kw):
            return bb.parse(bb.SAMPLE.replace(SAMPLE_COND, text.encode()), **kw)

        chain = lambda k, op="&&": " ".join([f"A{i} {op}" for i in range(k)] + [f"A{k}"])
        nest = lambda k: "(" * k + "A" + ")" * k
        cond(chain(n))
        cond(nest(n))
        cond("A && (" + nest(n - 2) + ")")
        cond("(" + nest(n - 2) + ") && A")              # a left operand lies under its connective
        for text in (chain(n + 1), nest(n + 1), "A && (" + nest(n - 1) + ")",
                     "(" + nest(n - 1) + ") && A", nest(100000), chain(3000) + " || Z"):
            with self.subTest(text=text[:20]), self.assertRaises(bb.ScriptError):
                cond(text)
        cond(nest(400), max_condition_depth=400)

    def test_an_unknown_section_is_kept_not_refused(self):
        # The interpreter also knows STARTUP (OQ-122); a section of another name is kept as read.
        for data in (bb.SAMPLE + b"[STARTUP]\nSPEED = 2\r\n",
                     b"[STARTUP]\n\n" + bb.SAMPLE,
                     bb.SAMPLE.replace(b"[SCRIPT_KERNEL]\n", b"[STARTUP]\n[SCRIPT_KERNEL]\n")):
            with self.subTest(data=data[:40]):
                s = bb.parse(data)
                self.assertEqual(s.emit(), data)
                self.assertEqual([x.name for x in s.sections if x.name not in bb.SECTIONS], ["[STARTUP]"])
                self.assertEqual(s.section("[STARTUP]").items, [])
                self.assertEqual(len(s.section("[SCRIPT_KERNEL]").items), 4)

    def test_the_dump(self):
        lines = bb.dump_lines(bb.parse(bb.SAMPLE))
        self.assertEqual(lines[0], f"SCRIPT bytes={len(bb.SAMPLE)} tokens=175 sections=6 crlf=22 lf=13")
        self.assertEqual(lines[1:4], ["[OBJECT_DEFINE]", '  OBJECT_FIGUR "BOAT" 7', '  OBJECT_ANLAGE "MILL" 10,20,1001'])
        self.assertIn('  "A1" 1,2,30,40', lines)
        self.assertIn('  "DUMMYPLAYER" 31', lines)
        self.assertIn("  SCHUETTIINITSIG = TRUE", lines)
        self.assertEqual(lines[-1], "  0,0")
        self.assertIn("    CALL QUESTLOG_DRAW()  (no ())", lines)
        self.assertIn("    CALL WON(\"HUMAN\")  (no ;)  (the interpreter refuses the kernel: no ';' follows)", lines)
        self.assertIn('  IF (GO && (AREA "A1" TOUCHED_BY_SHIP_FROM "HUMAN" || '
                      'PLAYER "HUMAN" INHABITANTCOUNT "SIEDLER" > 9)) THEN', lines)
        self.assertFalse(any("grouped by" in x for x in lines))        # not mixed: parenthesised
        mixed = bb.dump_lines(bb.parse(bb.SAMPLE.replace(b"GO && (", b"GO && ").replace(b"> 9)", b"> 9")))
        self.assertIn('     grouped by the interpreter: (GO && (AREA "A1" TOUCHED_BY_SHIP_FROM "HUMAN" || '
                      'PLAYER "HUMAN" INHABITANTCOUNT "SIEDLER" > 9))', mixed)
        full = bb.dump_lines(bb.parse(bb.SAMPLE), full=True)
        tokens = [x for x in full if x.startswith("TOKEN ")]
        self.assertEqual(len(tokens), 175)
        self.assertEqual(tokens[:2], ["TOKEN 0 SECTION [OBJECT_DEFINE]", "TOKEN 15 NEWLINE \n"])
        # The printed text is ASCII on every platform: other bytes as \xHH, the backslash too.
        self.assertEqual(bb.esc("A\xe4\t\r\n\\~"), "A\\xe4\\x09\\x0d\\x0a\\x5c~")
        self.assertEqual(bb.esc(tokens[1]), "TOKEN 15 NEWLINE \\x0a")
        # Numbers in define lines print as their values.
        moved = bb.dump_lines(bb.parse(bb.SAMPLE.replace(b'"HUMAN" 0\n', b'"HUMAN" -00\n')))
        self.assertIn('  "HUMAN" 0', moved)

    def test_the_synthetic_fixtures_are_the_generators_output(self):
        import bigbrother_fixtures
        for name, data in bigbrother_fixtures.files().items():
            with self.subTest(name=name), open(os.path.join(bigbrother_fixtures.OUT, name), "rb") as f:
                self.assertEqual(f.read(), data)
        for name, data in bigbrother_fixtures.files().items():
            with self.subTest(name=name):
                if name.startswith("bad_"):
                    with self.assertRaises(bb.ScriptError):
                        bb.parse(bb.payload_of(data))
                else:
                    self.assertEqual(bb.parse(bb.payload_of(data)).emit(), bb.payload_of(data))

    def test_timers_are_numbered(self):
        self.assertTrue(bb.is_timer("TIMER_0"))
        self.assertTrue(bb.is_timer("TIMER_12"))
        self.assertFalse(bb.is_timer("TIMER_FLAG"))


SAMPLE_COND = b'GO && (AREA"A1"TOUCHED_BY_SHIP_FROM"HUMAN" || PLAYER "HUMAN" INHABITANTCOUNT "SIEDLER" > 9)'
SAMPLE_BLOCK = b'{\r\n GO = FALSE;\r\n QUESTLOG_DRAW;\r\n}'


def with_cond(text: bytes) -> bb.Group:
    assert SAMPLE_COND in bb.SAMPLE
    return bb.parse(bb.SAMPLE.replace(SAMPLE_COND, text)).section("[SCRIPT_KERNEL]").items[2].cond


def c_precedence(g):
    """The C reading of a group (&& binds tighter than ||), as a value function, for comparison."""
    terms = [c_precedence(x) if isinstance(x, bb.Group) else x for x in g.items]
    ors, cur = [], [terms[0]]
    for op, t in zip(terms[1::2], terms[2::2]):
        if op == "||":
            ors.append(cur)
            cur = []
        cur.append(t)
    ors.append(cur)

    def val(truth):
        return any(all(t(truth) if callable(t) else truth(t) for t in a) for a in ors)
    return val


class Interpreter(unittest.TestCase):
    """The interpreter's reading (AnnoScene.dll, docs/re/annoscene.md §12): grouping, evaluation,
    and the block statements without ';'."""

    def test_connectives_have_equal_rank_and_group_to_the_right(self):
        for text, want in (
                (b"A && B || C", "(A && (B || C))"),
                (b"A || B && C", "(A || (B && C))"),
                (b"A && B && C || D", "(A && (B && (C || D)))"),
                (b"A || B || C", "(A || (B || C))"),
                (b"(A || B) && C", "((A || B) && C)"),
                (b"A && (B || C) && D", "(A && ((B || C) && D))"),
                (b"(A)", "A"),
                (b"A", "A")):
            with self.subTest(text=text):
                self.assertEqual(bb.describe_grouped(bb.grouped(with_cond(text))), want)

    def test_a_guard_before_alternatives_covers_all_of_them(self):
        g = with_cond(b"A && B || C")
        x = bb.grouped(g)
        for a, b, c in ((False, False, True), (True, False, True), (True, False, False), (False, True, True)):
            truth = {"A": a, "B": b, "C": c}
            with self.subTest(truth=truth):
                self.assertEqual(bb.evaluate(x, lambda p, t=truth: t[p.name]), a and (b or c))
        v = {"A": False, "B": False, "C": True}
        self.assertTrue(c_precedence(g)(lambda p: v[p.name]))   # the C reading would fire here
        self.assertFalse(bb.evaluate(x, lambda p: v[p.name]))   # the interpreter does not

    def test_and_skips_its_right_operand_or_evaluates_both(self):
        for text, values, asked in ((b"A && B", {"A": False, "B": True}, ["A"]),
                                    (b"A && B", {"A": True, "B": True}, ["A", "B"]),
                                    (b"A || B", {"A": True, "B": True}, ["A", "B"]),
                                    (b"A || B", {"A": False, "B": True}, ["A", "B"])):
            seen = []

            def truth(p, values=values, seen=seen):
                seen.append(p.name)
                return values[p.name]
            with self.subTest(text=text, values=values):
                bb.evaluate(bb.grouped(with_cond(text)), truth)
                self.assertEqual(seen, asked)

    def test_a_top_and_with_a_false_left_operand_runs_neither_block(self):
        g = with_cond(b"A && B")
        for a, b, want in ((False, False, None), (False, True, None), (True, False, "else"), (True, True, "then")):
            with self.subTest(a=a, b=b):
                self.assertEqual(bb.branch(g, lambda p, v={"A": a, "B": b}: v[p.name]), want)
        for text, values, want in ((b"A || B", {"A": False, "B": False}, "else"),
                                   (b"A", {"A": False}, "else"),
                                   (b"(A && B) || C", {"A": False, "B": False, "C": False}, "else")):
            with self.subTest(text=text):
                self.assertEqual(bb.branch(with_cond(text), lambda p, v=values: v[p.name]), want)

    def test_a_block_statement_without_semicolon_reads_on_to_the_next_semicolon(self):
        def run(block: bytes):
            assert SAMPLE_BLOCK in bb.SAMPLE
            s = bb.parse(bb.SAMPLE.replace(SAMPLE_BLOCK, block).replace(b'WON( "HUMAN" )', b'WON( "HUMAN" );'))
            return [(st.name, end is not None, [x.name for x in lost]) for st, end, lost in bb.read_on(s)]
        # the next statement of the block is read into it and lost
        self.assertEqual(run(b'{\r\n GO = FALSE\r\n QUESTLOG_DRAW;\r\n}'), [("GO", True, ["QUESTLOG_DRAW"])])
        self.assertEqual(run(b'{\r\n MONEY_ADD("HUMAN", 5)\r\n LOST("HUMAN");\r\n WON("HUMAN");\r\n}'),
                         [("MONEY_ADD", True, ["LOST"])])
        # the last statement: the ';' lies past the '}', nothing of the block is lost
        self.assertEqual(run(b'{\r\n GO = FALSE;\r\n QUESTLOG_DRAW()\r\n}'), [("QUESTLOG_DRAW", True, [])])
        # a ';' in a comment ends it before the next statement
        self.assertEqual(run(b'{\r\n GO = FALSE\r\n $ old;\r\n QUESTLOG_DRAW;\r\n}'), [("GO", True, [])])
        # two in a row: the second is swallowed by the first
        self.assertEqual(run(b'{\r\n GO = FALSE\r\n QUESTLOG_DRAW()\r\n LOST("HUMAN");\r\n}'),
                         [("GO", True, ["QUESTLOG_DRAW", "LOST"])])

    def test_no_semicolon_after_it_makes_the_interpreter_refuse_the_kernel(self):
        # the sample's last statement, WON( "HUMAN" ) in the ELSE block, has none after it
        ((st, end, lost),) = bb.read_on(bb.parse(bb.SAMPLE))
        self.assertEqual((st.name, end, lost), ("WON", None, []))

    def test_a_top_level_set_without_semicolon_is_not_listed(self):
        s = bb.parse(bb.SAMPLE.replace(b"SET GO = TRUE;\r\n", b"SET GO = TRUE\r\n")
                     .replace(b'WON( "HUMAN" )', b'WON( "HUMAN" );'))
        self.assertEqual(bb.read_on(s), [])

    def test_mixed(self):
        self.assertTrue(bb.is_mixed(with_cond(b"A && B || C")))
        self.assertTrue(bb.is_mixed(with_cond(b"A && (B && C || D)")))
        self.assertFalse(bb.is_mixed(with_cond(b"A && (B || C)")))
        self.assertFalse(bb.is_mixed(with_cond(b"A || B || C")))


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

    def test_the_interpreters_reading_of_the_shipped_scripts(self):
        mixed = collections.Counter()
        no_semi = []
        for name, s in self.scripts():
            for it in s.section("[SCRIPT_KERNEL]").items:
                if isinstance(it, bb.If) and bb.is_mixed(it.cond):
                    mixed[name] += 1
                    x = bb.grouped(it.cond)
                    prims = list(bb.walk_cond(it.cond))
                    atoms = sorted({bb.describe(p) for p in prims})
                    differs = False                 # against the C reading, over all truth values
                    for bits in range(1 << len(atoms)):
                        v = {a: bool(bits >> i & 1) for i, a in enumerate(atoms)}
                        def truth(p, v=v):
                            return v[bb.describe(p)]
                        differs |= bb.evaluate(x, truth) != c_precedence(it.cond)(truth)
                    with self.subTest(f=name, at=it.off):
                        self.assertTrue(differs)
                        self.assertEqual(it.cond.items[1], "&&")     # every one: guard(s) && alternatives
            no_semi += [(name, st.name, [x.name for x in lost]) for st, end, lost in bb.read_on(s)
                        if end is not None]
        self.assertEqual((sum(mixed.values()), len(mixed)), (28, 10))
        self.assertEqual(sorted(no_semi), [("S01_02_01.sze", "SET_PROFILE", []),
                                           ("S01_09_01.sze", "SET_DIPLOMACY", []),
                                           ("S01_09_01.sze", "SET_FOR_BUY", ["MINIMAP_ALERT"])])

    def test_the_interpreter_has_284_uppercase_names(self):
        self.assertEqual(len(bb.dll_names(os.path.join(self.game, "AnnoScene.dll"))), 284)


if __name__ == "__main__":
    unittest.main()
