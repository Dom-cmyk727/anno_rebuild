"""Writes the synthetic BIGBROTHER fixtures (fixtures/synthetic/bigbrother_synthetic/).

  python -I tools/bigbrother_fixtures.py [<dir>]

Every script below is our own text, written for the reader's tests: no line, name or string of
the shipped scripts. They cover the grammar of docs/formats/bigbrother.md §3.4, every token kind of
§2, both line endings, touching tokens, Latin-1 bytes, the conditions that mix `&&` and `||`, the
four kinds of statement without `;` (§3.4, "The interpreter reads on"), an unknown section (OQ-122)
and the savegame shape. The `bad_*` files break one rule each and must be refused.

The files are chunk files (`.sze`: ANNO or ANNOADDON1 > SCENE > BIGBROTHER; `.sav`: ANNOADDON1 >
BIGBROTHER), so their bytes, line endings included, are kept by git as binary files.
`tools/test_bigbrother.py` checks that the committed files equal this script's output; the C++
tests (src/formats/tests/bigbrother_test.cpp) and tools/agree.py read them.
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "fixtures", "synthetic", "bigbrother_synthetic")


def chunk(name: bytes, data: bytes) -> bytes:
    return name.ljust(12, b"\0") + struct.pack("<I", len(data)) + data


def scenario(script: bytes, root: bytes = b"ANNOADDON1") -> bytes:
    """A scenario: the script is the last child of SCENE, after a chunk of our own."""
    return chunk(root, chunk(b"SCENE", chunk(b"NOTES", b"synthetic fixture") + chunk(b"BIGBROTHER", script)))


def savegame(script: bytes) -> bytes:
    """A savegame: the script is a direct child of the root."""
    return chunk(b"ANNOADDON1", chunk(b"NOTES", b"synthetic fixture") + chunk(b"BIGBROTHER", script))


# Section headers and define lines end in LF, script lines in CRLF: the shape the game writes (§1).
HEAD = (b'[OBJECT_DEFINE]\n'
        b'OBJECT_FIGUR "SCOUT" 4\n'
        b'OBJECT_STADT "HARBOUR" 2\n'
        b'OBJECT_ANLAGE "TOWER" 12,34,5678\n'
        b'[AREA_DEFINE]\n'
        b'"COVE" 10, 20, 300, 400\n'
        b'"RIDGE" 500,60,70,800\n'
        b'[PLAYERMAPPER_DEFINE]\n'
        b'"ME" 0\n'
        b'"RIVAL\xc4" 1\n'
        b'"NOBODY" 31\n'
        b'[SCRIPT_KERNEL]\n')

# Every token kind; touching tokens; tabs; a negative number; Latin-1 bytes in a comment, a string
# and a name; a call without parentheses; an IF with ELSE; a parenthesised group; all comparisons.
GRAMMAR = HEAD + (
    b'$ A synthetic script for the reader (\xe4\xf6\xfc\xdf), not from the game.\r\n'
    b'INITBLOCK()\r\n'
    b'{\r\n'
    b'\tNOTE_SHOW( "ME", "INTRO_1", TRUE );\r\n'
    b'\tSTARTED = TRUE;\t$ an assignment in a block\r\n'
    b'}\r\n'
    b'SET STARTED = FALSE;\r\n'
    b'SET DONE = FALSE;\r\n'
    b'IF STARTED == TRUE && DONE == FALSE && PLAYER "ME" HOUSES "TOWN" > 49 THEN\r\n'
    b'{\r\n'
    b' DONE = TRUE;\r\n'
    b' PAY( "ME", -250 );\r\n'
    b' REFRESH;\r\n'
    b'}\r\n'
    b'IF AREA"COVE"REACHED_BY"RIVAL\xc4"||OBJECT "SCOUT" GONE THEN\r\n'
    b'{\r\n'
    b' ALARM(120,340);\r\n'
    b'}\r\n'
    b'ELSE\r\n'
    b'{\r\n'
    b' TEXT_SHOW( "\xdcbung.htm" );\r\n'
    b'}\r\n'
    b'IF (TIMER_2 < 30 || TIMER_2 > 90) && OBJECT "TOWER" OWNER == "ME" THEN\r\n'
    b'{\r\n'
    b' TIMER_RESET( 2, 0 );\r\n'
    b'}\r\n'
    b'\n'
    b'[SIGNALS]\n'
    b'SCHUETTIINITSIG = TRUE\r\n'
    b'STARTED = FALSE\r\n'
    b'DONE = FALSE\r\n'
    b'\n'
    b'[CAMARAPOS]\n'
    b'0,0\r\n')

# Conditions that mix && and || without parentheses (equal rank, grouped to the right), with a
# guard, in a group, inside a group; and two that do not mix.
MIXED = HEAD + (
    b'SET GUARD = TRUE;\r\n'
    b'IF GUARD && A || B THEN\r\n{\r\n GUARD = FALSE;\r\n}\r\n'
    b'IF GUARD && A && B || C || D THEN\r\n{\r\n GUARD = FALSE;\r\n}\r\n'
    b'IF A || B && C THEN\r\n{\r\n GUARD = FALSE;\r\n}\r\n'
    b'IF GUARD == FALSE && (A && B && C) || PLAYER "ME" HOUSES "ALL" > 9 THEN\r\n{\r\n GUARD = TRUE;\r\n}\r\n'
    b'IF (A || B && C) && D THEN\r\n{\r\n GUARD = FALSE;\r\n}\r\n'
    b'IF A && (B || C) THEN\r\n{\r\n GUARD = FALSE;\r\n}\r\n'
    b'IF A || B || C THEN\r\n{\r\n GUARD = FALSE;\r\n}\r\n'
    b'\n'
    b'[SIGNALS]\nSCHUETTIINITSIG = TRUE\r\nGUARD = TRUE\r\n\n[CAMARAPOS]\n0,0\r\n')

# The four kinds of statement without ';' (§3.4): a block statement that reads on to the ';' of a
# commented-out line (nothing lost); the INITBLOCK's last statement, which reads on past its '}'
# to the ';' of the next top-level SET (nothing lost); a top-level SET (nothing lost); a block
# statement that swallows the next statement of its block (that one is lost).
READ_ON = HEAD + (
    b'INITBLOCK()\r\n'
    b'{\r\n'
    b' FIRST( 1 );\r\n'
    b' LAST_ONE( "ME", 2 )\r\n'
    b'}\r\n'
    b'SET FLAG_A = TRUE;\r\n'
    b'SET FLAG_B = FALSE\r\n'
    b'IF FLAG_A THEN\r\n'
    b'{\r\n'
    b' SLOW( "ME", "PACE", 3 )\r\n'
    b' $OLD_CALL( 4 );\r\n'
    b' NEXT( 5 );\r\n'
    b'}\r\n'
    b'IF FLAG_B THEN\r\n'
    b'{\r\n'
    b' TRADE( "GOODS", "RIVAL", 6 )\r\n'
    b' BEEP( 7, 8 );\r\n'
    b' AFTER( 9 );\r\n'
    b'}\r\n'
    b'IF FLAG_A && FLAG_B THEN\r\n'
    b'{\r\n'
    b' FLAG_A = FALSE\r\n'
    b' FLAG_B = TRUE;\r\n'
    b' REDRAW;\r\n'
    b'}\r\n'
    b'\n'
    b'[SIGNALS]\nSCHUETTIINITSIG = TRUE\r\nFLAG_A = TRUE\r\nFLAG_B = FALSE\r\n\n[CAMARAPOS]\n0,0\r\n')

# The last block statement has no ';' and none follows in the kernel: the interpreter refuses the
# kernel; the reader keeps it as written.
REFUSE = HEAD + (
    b'SET OVER = FALSE;\r\n'
    b'IF OVER THEN\r\n'
    b'{\r\n'
    b' THE_END( "ME" )\r\n'
    b'}\r\n'
    b'\n'
    b'[SIGNALS]\nSCHUETTIINITSIG = TRUE\r\nOVER = FALSE\r\n\n[CAMARAPOS]\n7,9\r\n')

# The same script with LF only, and with CRLF only.
LF = GRAMMAR.replace(b"\r\n", b"\n")
CRLF = LF.replace(b"\n", b"\r\n")

# An unknown section (the interpreter also knows STARTUP, OQ-122): kept as read.
STARTUP = GRAMMAR + b'[STARTUP]\nSPEED = 2\r\n'

# A savegame's script: an empty kernel, the init signal cleared.
SAVED = (b'[OBJECT_DEFINE]\n[AREA_DEFINE]\n[PLAYERMAPPER_DEFINE]\n"ME" 0\n"NOBODY" 31\n'
         b'[SCRIPT_KERNEL]\n\n[SIGNALS]\nSCHUETTIINITSIG = FALSE\r\n\n[CAMARAPOS]\n0,0\r\n')


def files() -> dict[str, bytes]:
    return {
        "grammar.sze": scenario(GRAMMAR, b"ANNO"),
        "mixed.sze": scenario(MIXED),
        "read_on.sze": scenario(READ_ON),
        "refuse.sze": scenario(REFUSE),
        "lf.sze": scenario(LF),
        "crlf.sze": scenario(CRLF),
        "startup.sze": scenario(STARTUP),
        "saved.sav": savegame(SAVED),
        # one broken rule each: refused
        "bad_order.sze": scenario(GRAMMAR.replace(b"[AREA_DEFINE]", b"[AREA_X]")
                                  .replace(b"[OBJECT_DEFINE]", b"[AREA_DEFINE]")
                                  .replace(b"[AREA_X]", b"[OBJECT_DEFINE]")),
        "bad_byte.sze": scenario(GRAMMAR.replace(b" REFRESH;", b" REFRESH; @")),
        "bad_then.sze": scenario(GRAMMAR.replace(b"> 49 THEN", b"> 49")),
        "bad_semicolon.sze": scenario(GRAMMAR.replace(b" DONE = TRUE;", b" DONE = TRUE REFRESH;")),
        "bad_define.sze": scenario(GRAMMAR.replace(b"12,34,5678", b"12")),
        "bad_two.sze": chunk(b"ANNOADDON1", chunk(b"SCENE", chunk(b"BIGBROTHER", GRAMMAR) * 2)),
        "bad_none.sze": chunk(b"ANNOADDON1", chunk(b"SCENE", chunk(b"NOTES", b"synthetic fixture"))),
    }


def main(argv) -> int:
    out = argv[0] if argv else OUT
    os.makedirs(out, exist_ok=True)
    for name, data in files().items():
        with open(os.path.join(out, name), "wb") as f:
            f.write(data)
    print(f"{len(files())} files in {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
