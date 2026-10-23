"""Reference reader for the ANNO 1503 scenario script: the `BIGBROTHER` chunk of `.sze` scenarios
and of savegames. Spec: docs/formats/bigbrother.md. Independent of the C++ code; the oracle for the
script format (T17).

  bigbrother.py check <file.sze|.sav>...      parse every script, re-emit, compare bytes; exit 1 on
                                              any failure (the corpus check: all 50 .sze)
  bigbrother.py dump [--full] <file>...       the parse tree, one line per item; --full adds
                                              every token (the same bytes as anno-dump bigbrother)
  bigbrother.py vocab <file>...               usage counts: sections, statements, conditions,
                                              actions with argument signatures, disaster types,
                                              string arguments, variables and signals
  bigbrother.py keywords <AnnoScene.dll> <file>...
                                              the interpreter's uppercase names (strings -n 4 rule)
                                              with address, block and corpus use
  bigbrother.py quirks <file>...              the conditions mixing && and || without parentheses
                                              as the interpreter groups them, and the block
                                              statements without ';' with what it reads into them

Design. The lexer turns EVERY byte into exactly one token (whitespace, newlines and `$` comments
are tokens too); the text is Latin-1, which maps bytes to characters 1:1. The parser builds the
tree over the significant tokens and must account for all of them, so anything outside the grammar
read from the 50 shipped scripts is an error, not a fallback; only a section of an unknown name is
kept unread (STARTUP, OQ-122). Emission walks the tree's sections
and concatenates their tokens, so the round trip is byte-identical by construction and is checked.

Doctests (synthetic text of our own, no content of the original): `python -I -m doctest tools/bigbrother.py`;
with the corpus cases (ANNO1503_GAME_DIR): `python -I tools/test_bigbrother.py`.

>>> s = parse(SAMPLE)
>>> s.emit() == SAMPLE
True
>>> [sec.name for sec in s.sections] == list(SECTIONS)
True
>>> kernel = s.section("[SCRIPT_KERNEL]").items
>>> [type(x).__name__ for x in kernel]
['InitBlock', 'Assign', 'If', 'If']
>>> describe(kernel[2].cond)
'(GO && (AREA "A1" TOUCHED_BY_SHIP_FROM "HUMAN" || PLAYER "HUMAN" INHABITANTCOUNT "SIEDLER" > 9))'
>>> kernel[3].els[0].name, kernel[3].els[0].semicolon        # a call without ';'
('WON', False)
>>> s.section("[SIGNALS]").items[1]["name"], s.section("[OBJECT_DEFINE]").items[1]["numbers"]
('GO', [10, 20, 1001])
>>> try:
...     parse(SAMPLE.replace(b"THEN", b"THAN"))
... except ScriptError as e:
...     print(e)
[SCRIPT_KERNEL]: expected THEN for the IF at offset 234, got 'THAN' at offset 329
"""
from __future__ import annotations

import collections
import dataclasses
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import chunks  # noqa: E402  (tools/chunks.py, the chunk-container reference)

ENCODING = "latin-1"     # bytes <-> characters 1:1

TOKEN = re.compile(r"""
    (?P<NEWLINE>\r\n|\n)
  | (?P<WS>[ \t]+)
  | (?P<COMMENT>\$[^\r\n]*)
  | (?P<SECTION>\[[A-Z_]+\])
  | (?P<STRING>"[^"\r\n]*")
  | (?P<NUMBER>-?[0-9]+)
  | (?P<IDENT>[A-Za-z_][A-Za-z0-9_]*)
  | (?P<OP>&&|\|\||==|!=|<=|>=|<|>|=|!|\(|\)|\{|\}|,|;)
""", re.X)
TRIVIA = ("WS", "NEWLINE", "COMMENT")

# The sections in the order every shipped script has them (P, 50 of 50). The reader also knows
# STARTUP (S: AnnoScene.dll 0x1003e628 "STARTUP\n"), which no shipped script has.
SECTIONS = ("[OBJECT_DEFINE]", "[AREA_DEFINE]", "[PLAYERMAPPER_DEFINE]", "[SCRIPT_KERNEL]",
            "[SIGNALS]", "[CAMARAPOS]")
OBJECT_KINDS = {"OBJECT_FIGUR": "IDENT STRING NUMBER",          # the line shape per kind (P)
                "OBJECT_STADT": "IDENT STRING NUMBER",
                "OBJECT_ANLAGE": "IDENT STRING NUMBER , NUMBER , NUMBER"}
SUBJECTS = ("PLAYER", "OBJECT", "AREA")
COMPARE = ("==", "<", ">")          # the comparisons the scripts use (P); != <= >= never occur
BOOL = ("TRUE", "FALSE")
KEYWORDS = ("INITBLOCK", "SET", "IF", "THEN", "ELSE")
RESERVED = KEYWORDS + BOOL          # never a variable, signal, call or query name (P)
# How deep a condition may nest: along any path of its grouping (`grouped`), the connectives plus
# the parentheses. The parser and the walks here recurse once per level, as the interpreter's
# condition parser does (AnnoScene 0x10022bf0 recurses on each connective and group), so a deeper
# (modded) script is refused instead of hitting the recursion limit. The shipped scripts need 21
# (P). The same default as the C++ reader's Options::max_condition_depth; parse() takes another.
MAX_CONDITION_DEPTH = 256

# A synthetic script in the shipped shape (LF headers and define lines, CRLF script lines), for the
# doctests: no text of the original.
SAMPLE = (b'[OBJECT_DEFINE]\nOBJECT_FIGUR "BOAT" 7\nOBJECT_ANLAGE "MILL" 10,20,1001\n'
          b'[AREA_DEFINE]\n"A1" 1, 2, 30, 40\n[PLAYERMAPPER_DEFINE]\n"HUMAN" 0\n"DUMMYPLAYER" 31\n'
          b'[SCRIPT_KERNEL]\n$ comment\r\nINITBLOCK()\r\n{\r\n\tSHOWTEXT("A.HTM");\r\n}\r\n'
          b'SET GO = TRUE;\r\n'
          b'IF GO && (AREA"A1"TOUCHED_BY_SHIP_FROM"HUMAN" || PLAYER "HUMAN" INHABITANTCOUNT "SIEDLER" > 9) THEN\r\n'
          b'{\r\n GO = FALSE;\r\n QUESTLOG_DRAW;\r\n}\r\n'
          b'IF OBJECT "BOAT" KILLED THEN\r\n{\r\n LOST( "HUMAN" );\r\n}\r\nELSE\r\n{\r\n WON( "HUMAN" )\r\n}\r\n\n'
          b'[SIGNALS]\nSCHUETTIINITSIG = TRUE\r\nGO = TRUE\r\n\n[CAMARAPOS]\n0,0\r\n')


class ScriptError(Exception):
    pass


@dataclasses.dataclass
class Tok:
    kind: str
    text: str
    off: int


def lex(text: str) -> list[Tok]:
    out, pos = [], 0
    while pos < len(text):
        m = TOKEN.match(text, pos)
        if not m:
            raise ScriptError(f"unexpected {text[pos]!r} at offset {pos}")
        out.append(Tok(m.lastgroup, m.group(), pos))
        pos = m.end()
    return out


# ---- tree ---------------------------------------------------------------------------------------
@dataclasses.dataclass
class Value:
    kind: str          # "bool" | "number" | "string"
    text: str

    @property
    def py(self):
        return {"bool": lambda t: t == "TRUE", "number": int, "string": lambda t: t[1:-1]}[self.kind](self.text)


@dataclasses.dataclass
class Call:
    name: str
    args: list
    parens: bool = True        # QUESTLOG_DRAW; once without ()
    semicolon: bool = True     # four statements in the corpus lack their ';' (see end())
    off: int = 0


@dataclasses.dataclass
class Assign:
    name: str
    value: Value
    is_set: bool = False       # SET NAME = v; at the top level, NAME = v; inside a block
    semicolon: bool = True
    off: int = 0


@dataclasses.dataclass
class Primary:
    subject: str | None        # PLAYER | OBJECT | AREA | None (a variable or timer)
    name: str                  # the subject's string, or the variable name
    query: str | None = None   # e.g. INHABITANTCOUNT, TOUCHED_BY_SHIP_FROM, DEAD
    arg: str | None = None     # e.g. "SIEDLER", "HUMAN", "1217@HUMAN"
    op: str | None = None
    value: Value | None = None
    off: int = 0


@dataclasses.dataclass
class Group:
    items: list                # Primary | Group, alternating with "&&" / "||" connectives


@dataclasses.dataclass
class If:
    cond: Group
    then: list
    els: list | None = None
    off: int = 0


@dataclasses.dataclass
class InitBlock:
    body: list
    off: int = 0


@dataclasses.dataclass
class Section:
    name: str
    tokens: list               # every token of the section, header included
    items: list


@dataclasses.dataclass
class Script:
    sections: list

    def emit(self) -> bytes:
        return "".join(t.text for s in self.sections for t in s.tokens).encode(ENCODING)

    def section(self, name: str) -> Section:
        return next(s for s in self.sections if s.name == name)


# ---- parser -------------------------------------------------------------------------------------
class Cursor:
    def __init__(self, toks: list[Tok], where: str):
        self.t, self.nl, gap = [], [], False   # nl[i]: a line break lies before significant token i
        for x in toks:
            if x.kind == "NEWLINE":
                gap = True
            elif x.kind not in TRIVIA:
                self.t.append(x)
                self.nl.append(gap)
                gap = False
        self.i = 0
        self.where = where

    def at_line_end(self) -> bool:
        """Nothing else follows on the current line."""
        return self.done() or self.nl[self.i]

    def fail(self, what: str):
        t = self.peek()
        at = "at the end of the section" if t is None else f"{t.text!r} at offset {t.off}"
        raise ScriptError(f"{self.where}: {what}, got {at}")

    def peek(self, k=0) -> Tok | None:
        j = self.i + k
        return self.t[j] if j < len(self.t) else None

    def done(self) -> bool:
        return self.i >= len(self.t)

    def take(self, kind=None, text=None) -> Tok:
        t = self.peek()
        if t is None or (kind and t.kind != kind) or (text and t.text != text):
            self.fail(f"expected {text or kind}")
        self.i += 1
        return t

    def name(self) -> Tok:
        """An identifier that is not a keyword or a truth value."""
        t = self.take("IDENT")
        if t.text in RESERVED:
            self.i -= 1
            self.fail("expected a name")
        return t

    def is_(self, kind=None, text=None, k=0) -> bool:
        t = self.peek(k)
        return t is not None and (kind is None or t.kind == kind) and (text is None or t.text == text)


def value(c: Cursor) -> Value:
    t = c.peek()
    if t and t.kind == "IDENT" and t.text in BOOL:
        return Value("bool", c.take().text)
    if t and t.kind == "NUMBER":
        return Value("number", c.take().text)
    if t and t.kind == "STRING":
        return Value("string", c.take().text)
    c.fail("expected a value")


def lines(toks: list[Tok]) -> list[list[Tok]]:
    """Significant tokens per line (for the line-oriented sections)."""
    out, cur = [], []
    for t in toks:
        if t.kind == "NEWLINE":
            if cur:
                out.append(cur)
            cur = []
        elif t.kind not in ("WS", "COMMENT"):
            cur.append(t)
    if cur:
        out.append(cur)
    return out


def shape(line: list[Tok]) -> str:
    return " ".join(t.text if t.kind == "OP" else t.kind for t in line)


def parse_defines(name: str, body: list[Tok]) -> list[dict]:
    comment = next((t for t in body if t.kind == "COMMENT"), None)
    if comment:                                      # all comments are in the kernel (P)
        raise ScriptError(f"{name}: a comment at offset {comment.off}")
    items = []
    for ln in lines(body):
        s = shape(ln)
        bad = ScriptError(f"{name}: unexpected line {' '.join(t.text for t in ln)!r} at offset {ln[0].off}")
        if name == "[OBJECT_DEFINE]":
            # OBJECT_FIGUR "N" id | OBJECT_STADT "N" id | OBJECT_ANLAGE "N" x,y,type (P: 133, 80, 37)
            if ln[0].kind != "IDENT" or s != OBJECT_KINDS.get(ln[0].text):
                raise bad
            items.append({"kind": ln[0].text, "name": ln[1].text[1:-1],
                          "numbers": [int(t.text) for t in ln if t.kind == "NUMBER"], "off": ln[0].off})
        elif name == "[AREA_DEFINE]":
            if s != "STRING NUMBER , NUMBER , NUMBER , NUMBER":
                raise bad
            items.append({"name": ln[0].text[1:-1], "rect": [int(t.text) for t in ln if t.kind == "NUMBER"],
                          "off": ln[0].off})
        elif name == "[PLAYERMAPPER_DEFINE]":
            if s != "STRING NUMBER":
                raise bad
            items.append({"name": ln[0].text[1:-1], "player": int(ln[1].text), "off": ln[0].off})
        elif name == "[SIGNALS]":
            if s != "IDENT = IDENT" or ln[0].text in RESERVED or ln[2].text not in BOOL:
                raise bad
            items.append({"name": ln[0].text, "value": ln[2].text == "TRUE", "off": ln[0].off})
        elif name == "[CAMARAPOS]":
            if s != "NUMBER , NUMBER" or items:      # one line (P: 50 of 50)
                raise bad
            items.append({"pos": [int(ln[0].text), int(ln[2].text)], "off": ln[0].off})
        else:
            raise ScriptError(f"no reader for {name}")
    if name == "[CAMARAPOS]" and not items:
        raise ScriptError(f"{name}: no position")
    names = [it["name"] for it in items if "name" in it]
    dup = next((n for n in names if names.count(n) > 1), None)
    if dup is not None:                              # every name once (P: 50 of 50)
        raise ScriptError(f"{name}: {dup!r} defined twice")
    return items


def primary(c: Cursor) -> Primary:
    t = c.name()
    if t.text in SUBJECTS:
        p = Primary(t.text, c.take("STRING").text[1:-1], c.name().text, off=t.off)
        if c.is_("STRING"):
            p.arg = c.take().text[1:-1]
    else:
        p = Primary(None, t.text, off=t.off)
    if c.is_("OP") and c.peek().text in COMPARE:
        p.op = c.take().text
        p.value = value(c)
    return p


def written_depth(g: Group) -> int:
    """The depth MAX_CONDITION_DEPTH bounds: along any path of the condition as written, the
    connectives `grouped` nests it under plus the parentheses. Term i of k connectives lies under
    i + 1 of them, the last one under k."""
    terms = g.items[0::2]
    k = len(terms) - 1
    return max((i + 1 if i < k else k) + (1 + written_depth(x) if isinstance(x, Group) else 0)
               for i, x in enumerate(terms))


def condition(c: Cursor, base: int = 0, max_depth: int = MAX_CONDITION_DEPTH) -> Group:
    """`base`: a lower bound of this condition's depth (written_depth). Checking it before each
    term bounds the recursion while parsing; parse_kernel checks the exact depth."""
    items = []
    while True:
        level = base + len(items) // 2
        if level + c.is_("OP", "(") > max_depth:
            c.fail(f"condition nested deeper than {max_depth} levels")
        if c.is_("OP", "("):
            c.take()
            items.append(condition(c, level + 1, max_depth))
            c.take("OP", ")")
        else:
            items.append(primary(c))
        if c.is_("OP") and c.peek().text in ("&&", "||"):
            items.append(c.take().text)
            continue
        return Group(items)


def call_args(c: Cursor) -> list[Value]:
    args = []
    c.take("OP", "(")
    if not c.is_("OP", ")"):
        while True:
            args.append(value(c))
            if c.is_("OP", ","):
                c.take()
                continue
            break
    c.take("OP", ")")
    return args


def end(c: Cursor) -> bool:
    """A statement's ';'. Four shipped statements end without one (S01_02_01, S01_08_01, S01_09_01
    twice); each of them ends its line, so a missing ';' is accepted only at the end of a line."""
    if c.is_("OP", ";"):
        c.take()
        return True
    if not c.at_line_end():
        c.fail("expected ';'")
    return False


def block(c: Cursor) -> list:
    c.take("OP", "{")
    body = []
    while not c.is_("OP", "}"):
        t = c.name()
        if c.is_("OP", "="):
            c.take()
            v = value(c)
            body.append(Assign(t.text, v, False, end(c), t.off))
        elif c.is_("OP", "("):
            args = call_args(c)
            body.append(Call(t.text, args, True, end(c), t.off))
        elif c.is_("OP", ";"):                       # QUESTLOG_DRAW; (single42)
            c.take()
            body.append(Call(t.text, [], False, True, t.off))
        else:
            c.fail(f"expected '=', '(' or ';' after {t.text}")
    c.take("OP", "}")
    return body


def parse_kernel(body: list[Tok], max_depth: int = MAX_CONDITION_DEPTH) -> list:
    c = Cursor(body, "[SCRIPT_KERNEL]")
    items = []
    while not c.done():
        t = c.take("IDENT")
        if t.text == "INITBLOCK":
            c.take("OP", "(")
            c.take("OP", ")")
            items.append(InitBlock(block(c), t.off))
        elif t.text == "SET":
            name = c.name().text
            c.take("OP", "=")
            v = value(c)
            items.append(Assign(name, v, True, end(c), t.off))
        elif t.text == "IF":
            cond = condition(c, 0, max_depth)
            if written_depth(cond) > max_depth:
                raise ScriptError(f"[SCRIPT_KERNEL]: the condition of the IF at offset {t.off} "
                                  f"nests deeper than {max_depth} levels")
            if not c.is_("IDENT", "THEN"):
                c.fail(f"expected THEN for the IF at offset {t.off}")
            c.take()
            then = block(c)
            els = None
            if c.is_("IDENT", "ELSE"):
                c.take()
                els = block(c)
            items.append(If(cond, then, els, t.off))
        else:
            raise ScriptError(f"[SCRIPT_KERNEL]: unexpected {t.text!r} at offset {t.off}")
    return items


def parse(data: bytes, max_condition_depth: int = MAX_CONDITION_DEPTH) -> Script:
    """Offsets in errors count from the start of the payload (the chunk's data)."""
    toks = lex(data.decode(ENCODING))
    heads = [i for i, t in enumerate(toks) if t.kind == "SECTION"]
    names = tuple(toks[i].text for i in heads)
    known = tuple(n for n in names if n in SECTIONS)
    if known != SECTIONS:               # the six, each once and in order; other names are kept
        raise ScriptError(f"sections {known} differ from {SECTIONS}")
    if heads[0] != 0:
        raise ScriptError("text before the first section")
    for i in heads:            # a header is a line of its own (P: 300 of 300)
        if (i and toks[i - 1].kind != "NEWLINE") or i + 1 == len(toks) or toks[i + 1].kind != "NEWLINE":
            raise ScriptError(f"{toks[i].text} at offset {toks[i].off} is not a line of its own")
    sections = []
    for k, i in enumerate(heads):
        j = heads[k + 1] if k + 1 < len(heads) else len(toks)
        name, body = toks[i].text, toks[i + 1:j]
        if name not in SECTIONS:        # e.g. STARTUP (OQ-122): kept as read, not interpreted
            items = []
        elif name == "[SCRIPT_KERNEL]":
            items = parse_kernel(body, max_condition_depth)
        else:
            items = parse_defines(name, body)
        sections.append(Section(name, toks[i:j], items))
    return Script(sections)


# ---- files --------------------------------------------------------------------------------------
def payload(path: str) -> bytes:
    """The BIGBROTHER payload of a scenario (ANNO*/SCENE/...) or a savegame (ANNO*/...)."""
    with open(path, "rb") as f:
        return payload_of(f.read())


def payload_of(buf: bytes) -> bytes:
    """As payload, from the file's bytes."""
    tree, _ = chunks.parse_file(buf)
    if not tree or tree[0][0] not in ("ANNO", "ANNOADDON1") or tree[0][3] is None:
        raise ScriptError("no ANNO or ANNOADDON1 container")
    kids = tree[0][3]
    for n, off, sz, sub in kids:
        if n == "SCENE" and sub is not None:
            kids = sub
            break
    found = [buf[off + 16:off + 16 + sz] for n, off, sz, _ in kids if n == "BIGBROTHER"]
    if len(found) != 1:
        raise ScriptError(f"{len(found)} BIGBROTHER chunks")
    return found[0]


def walk(items):
    """Every statement and primary, depth first."""
    for it in items:
        yield it
        if isinstance(it, InitBlock):
            yield from walk(it.body)
        elif isinstance(it, If):
            yield from walk_cond(it.cond)
            yield from walk(it.then)
            if it.els:
                yield from walk(it.els)


def walk_cond(g: Group):
    for x in g.items:
        if isinstance(x, Group):
            yield from walk_cond(x)
        elif isinstance(x, Primary):
            yield x


def cmd_check(files) -> int:
    bad = 0
    for f in files:
        try:
            data = payload(f)
            s = parse(data)
            same = s.emit() == data
            info = ""
            if same:
                k = s.section("[SCRIPT_KERNEL]").items
                stmts = [x for x in walk(k) if isinstance(x, (Assign, Call))]
                nosemi = sum(1 for x in stmts if not x.semicolon)
                info = (f"items={len(k)} statements={len(stmts)} no_semicolon={nosemi} "
                        f"signals={len(s.section('[SIGNALS]').items)}")
            print(f"{'OK ' if same else 'DIFF'} {len(data):6d} {info} {f}")
            bad += not same
        except (ScriptError, OSError) as e:
            print(f"FAIL {f}: {e}")
            bad += 1
    print(f"{len(files) - bad}/{len(files)} parsed and re-emitted byte-identically")
    return 1 if bad else 0


def is_timer(name: str) -> bool:
    """TIMER_0 … TIMER_4 are the interpreter's timers (S); TIMER_FLAG (Single_08) is a variable."""
    return re.fullmatch(r"TIMER_[0-9]+", name) is not None


def describe(x) -> str:
    if isinstance(x, Primary):
        s = f"{x.subject} \"{x.name}\" {x.query}" if x.subject else x.name
        if x.arg is not None:
            s += f' "{x.arg}"'
        if x.op:
            s += f" {x.op} {x.value.text}"
        return s
    if isinstance(x, Group):
        return "(" + " ".join(describe(i) if not isinstance(i, str) else i for i in x.items) + ")"
    return str(x)


# ---- the interpreter's reading (AnnoScene.dll; docs/re/annoscene.md §12, T19) -------------------
def grouped(g):
    """The condition as the interpreter groups it: the first term, the first connective, and the
    whole rest grouped the same way, whatever the connectives (the condition parser 0x10022bf0
    recurses on everything after the first `&&`/`||`; the two have equal rank). A group of one term
    is that term. Returns a Primary or a tuple (connective, left, right).

    >>> c = parse(SAMPLE).section("[SCRIPT_KERNEL]").items[2].cond
    >>> describe_grouped(grouped(c))
    '(GO && (AREA "A1" TOUCHED_BY_SHIP_FROM "HUMAN" || PLAYER "HUMAN" INHABITANTCOUNT "SIEDLER" > 9))'
    """
    if isinstance(g, Primary):
        return g
    items = [grouped(x) if isinstance(x, Group) else x for x in g.items]
    node = items[-1]
    for i in range(len(items) - 3, -1, -2):        # items: term, op, term, op, ..., term
        node = (items[i + 1], items[i], node)
    return node


def describe_grouped(x) -> str:
    if isinstance(x, tuple):
        op, left, right = x
        return f"({describe_grouped(left)} {op} {describe_grouped(right)})"
    return describe(x)


def evaluate(x, truth) -> bool:
    """The value of a grouped condition as the interpreter computes it (0x10010290): the left
    operand first; `&&` skips the right operand when the left is false; `||` always evaluates
    both. `truth(primary)` gives a primary's value."""
    if not isinstance(x, tuple):
        return bool(truth(x))
    op, left, right = x
    a = evaluate(left, truth)
    if op == "&&" and not a:
        return False
    b = evaluate(right, truth)
    return (a and b) if op == "&&" else (a or b)


def branch(cond: Group, truth) -> str | None:
    """Which block of an IF runs: "then", "else" or None. The top node runs THEN when true and
    ELSE when false, except a top `&&` whose left operand is false: it returns at once and runs
    neither (0x10010290, the early return at 0x100102a8-0x100102b4)."""
    x = grouped(cond)
    if isinstance(x, tuple) and x[0] == "&&" and not evaluate(x[1], truth):
        return None
    return "then" if evaluate(x, truth) else "else"


def is_mixed(g: Group) -> bool:
    """A group whose own connectives are both `&&` and `||`, or that holds such a group."""
    return len({x for x in g.items if isinstance(x, str)}) > 1 or any(
        is_mixed(x) for x in g.items if isinstance(x, Group))


def read_on(script: Script) -> list[tuple]:
    """The block statements without ';' as the interpreter reads them (0x1001c9e0). A block
    statement ends at the next ';' of the kernel text, wherever it is (strpbrk, 0x1000b530): in a
    later statement, in a comment, or past the block's '}'. Returns one (statement, end, lost)
    per such statement: `end` is the payload offset of that ';', `lost` the statements of the same
    block that start before it, which are read into the statement and dropped (an action's reader
    ignores the text after its arguments). What lies past the block's '}' is not lost, because the
    kernel reader resumes after the '}'. Not modelled (no shipped case): when that ';' stands in a
    comment or a string with text after it, the interpreter's next statement starts right after
    the ';'. `end` None: no ';' follows in the kernel text, and the
    interpreter refuses the kernel ("FunktionsBlock nicht oder falsch definiert", 0x1003f7c4). A
    top-level SET without ';' is not listed: it loses nothing (the reader takes the next word)."""
    text = script.emit().decode(ENCODING)
    kernel = script.section("[SCRIPT_KERNEL]")
    start = kernel.tokens[0].off + len(kernel.tokens[0].text)
    stop = text.find("[", start)               # the kernel text ends at the next '[' (0x1000b3f0)
    stop = len(text) if stop < 0 else stop
    blocks = []
    for it in kernel.items:
        if isinstance(it, InitBlock):
            blocks.append(it.body)
        elif isinstance(it, If):
            blocks += [it.then] + ([it.els] if it.els is not None else [])
    out = []
    for body in blocks:
        gone: set[int] = set()
        for st in body:
            if st.semicolon or id(st) in gone:
                continue
            semi = text.find(";", st.off, stop)
            if semi < 0:
                out.append((st, None, []))
                continue
            lost = [s for s in body if st.off < s.off < semi]
            gone.update(id(s) for s in lost)
            out.append((st, semi, lost))
    return out


def stmt_text(st) -> str:
    if isinstance(st, Assign):
        return f"{'SET ' if st.is_set else ''}{st.name} = {st.value.text}"
    return f"{st.name}({', '.join(v.text for v in st.args)})" if st.parens else st.name


def esc(line: str) -> str:
    """The dump is ASCII on every platform: printable ASCII except a backslash as is, every other
    character (a Latin-1 byte, a tab, a line break) as backslash, x, two hex digits."""
    return "".join(c if " " <= c <= "~" and c != "\\" else f"\\x{ord(c):02x}" for c in line)


def dump_lines(s: Script, full: bool = False) -> list[str]:
    """The `dump` text of one script, before `esc` (docs/formats/bigbrother.md §8)."""
    toks = [t for sec in s.sections for t in sec.tokens]
    crlf = sum(t.text == "\r\n" for t in toks)
    lf = sum(t.text == "\n" for t in toks)
    out = [f"SCRIPT bytes={sum(len(t.text) for t in toks)} tokens={len(toks)} "
           f"sections={len(s.sections)} crlf={crlf} lf={lf}"]
    swallowed = {id(st): (end, lost) for st, end, lost in read_on(s)}
    lost_ids = {id(x) for _end, lost in swallowed.values() for x in lost}

    def stmt(st, ind):
        if isinstance(st, Assign):
            line = f"{ind}{'SET ' if st.is_set else ''}{st.name} = {st.value.text}"
        else:
            line = f"{ind}CALL {st.name}({', '.join(v.text for v in st.args)})"
            line += "" if st.parens else "  (no ())"
        line += "" if st.semicolon else "  (no ;)"
        if id(st) in swallowed:
            end, lost = swallowed[id(st)]
            line += ("  (the interpreter refuses the kernel: no ';' follows)" if end is None else
                     f"  (reads on to the ';' at offset {end}; loses {len(lost)} statement(s))")
        if id(st) in lost_ids:
            line += "  (LOST: read into the statement before)"
        out.append(line)

    for sec in s.sections:
        if sec.name not in SECTIONS:
            out.append(f"{sec.name}  (not read: {len(sec.tokens) - 1} tokens kept)")
            continue
        out.append(sec.name)
        for it in sec.items:
            if sec.name == "[OBJECT_DEFINE]":
                out.append(f'  {it["kind"]} "{it["name"]}" {",".join(str(n) for n in it["numbers"])}')
            elif sec.name == "[AREA_DEFINE]":
                out.append(f'  "{it["name"]}" {",".join(str(n) for n in it["rect"])}')
            elif sec.name == "[PLAYERMAPPER_DEFINE]":
                out.append(f'  "{it["name"]}" {it["player"]}')
            elif sec.name == "[SIGNALS]":
                out.append(f'  {it["name"]} = {"TRUE" if it["value"] else "FALSE"}')
            elif sec.name == "[CAMARAPOS]":
                out.append(f'  {it["pos"][0]},{it["pos"][1]}')
            elif isinstance(it, InitBlock):
                out.append("  INITBLOCK")
                for st in it.body:
                    stmt(st, "    ")
            elif isinstance(it, Assign):
                stmt(it, "  ")
            elif isinstance(it, If):
                out.append(f"  IF {describe(it.cond)} THEN")
                if is_mixed(it.cond):
                    out.append(f"     grouped by the interpreter: {describe_grouped(grouped(it.cond))}")
                for st in it.then:
                    stmt(st, "    ")
                if it.els is not None:
                    out.append("  ELSE")
                    for st in it.els:
                        stmt(st, "    ")
    if full:
        out += [f"TOKEN {t.off} {t.kind} {t.text}" for t in toks]
    return out


def cmd_dump(files, full: bool) -> int:
    """`dump [--full] <file>...`: the same bytes as `anno-dump bigbrother [--full] <file>...`."""
    bad = 0
    for f in files:
        try:
            with open(f, "rb") as fh:
                buf = fh.read()
        except OSError:
            print(f"{f}: cannot read", file=sys.stderr)
            bad += 1
            continue
        print(f"# {f}")
        try:
            s = parse(payload_of(buf))
        except ScriptError as e:
            print(f"NOPARSE: {e}", file=sys.stderr)
            print("NOPARSE")
            bad += 1
            continue
        for line in dump_lines(s, full):
            print(esc(line))
    return 1 if bad else 0


def cmd_quirks(files) -> int:
    """The conditions that mix `&&` and `||` without parentheses, as written and as grouped, and
    the block statements without ';' with what the interpreter reads into them."""
    mixed = lost = 0
    for f in files:
        s = parse(payload(f))
        base = os.path.basename(f)
        for it in s.section("[SCRIPT_KERNEL]").items:
            if isinstance(it, If) and is_mixed(it.cond):
                mixed += 1
                print(f"{base} @{it.off} MIXED\n  written: {describe(it.cond)}\n"
                      f"  grouped: {describe_grouped(grouped(it.cond))}")
        for st, end, gone in read_on(s):
            lost += len(gone)
            what = "no ';' follows: the kernel is refused" if end is None else (
                f"reads on to offset {end}; lost: " + ("; ".join(stmt_text(x) for x in gone) or "nothing"))
            print(f"{base} @{st.off} NO ';' {stmt_text(st)}: {what}")
    print(f"{mixed} mixed conditions, {lost} lost statements in {len(files)} files")
    return 0


def vocab(files) -> dict:
    v = {k: collections.Counter() for k in (
        "statements", "queries", "conditions", "actions", "signatures", "disasters", "string_args",
        "variables", "signals", "signal_values", "object_kinds", "compare", "connectives", "files_using")}
    examples: dict = {}
    for f in files:
        s = parse(payload(f))
        base = os.path.basename(f)
        used = set()
        for o in s.section("[OBJECT_DEFINE]").items:
            v["object_kinds"][f"{o['kind']} ({len(o['numbers'])} numbers)"] += 1
        for sg in s.section("[SIGNALS]").items:
            v["signals"][sg["name"]] += 1
            v["signal_values"][f"{sg['name'] if sg['name'] == 'SCHUETTIINITSIG' else '*'}={sg['value']}"] += 1
        kernel = s.section("[SCRIPT_KERNEL]").items
        for it in kernel:
            kind = {InitBlock: "INITBLOCK", Assign: "SET", If: "IF"}[type(it)]
            if isinstance(it, If) and it.els is not None:
                kind = "IF ... ELSE"
            v["statements"][kind] += 1
            if isinstance(it, If):
                for x in it.cond.items:
                    if isinstance(x, str):
                        v["connectives"][x] += 1
        for x in walk(kernel):
            if isinstance(x, Assign):
                v["variables"][x.name] += 1
                v["statements"]["assignment in a block" if not x.is_set else "SET (counted above)"] += 0 if x.is_set else 1
            elif isinstance(x, Call):
                v["actions"][x.name] += 1
                sig = ", ".join(a.kind for a in x.args)
                v["signatures"][f"{x.name}({sig})"] += 1
                used.add(x.name)
                examples.setdefault(("action", x.name), base)
                if x.name == "DISASTER_AT" and x.args:
                    v["disasters"][x.args[-1].py] += 1
                for a in x.args:
                    if a.kind == "string":
                        v["string_args"][a.py] += 1
            elif isinstance(x, Primary):
                key = f"{x.subject} {x.query}" if x.subject else ("TIMER" if is_timer(x.name) else "variable")
                if x.op:
                    v["compare"][x.op] += 1
                v["conditions"][key] += 1
                if x.subject:
                    v["queries"][x.query] += 1
                    used.add(x.query)
                    if x.arg is not None:
                        v["string_args"][x.arg] += 1
                examples.setdefault(("condition", key), base)
        for u in used:
            v["files_using"][u] += 1
    v["examples"] = {f"{a}:{b}": f for (a, b), f in examples.items()}
    return v


def cmd_vocab(files) -> int:
    v = vocab(files)
    for k, c in v.items():
        if k == "examples":
            continue
        print(f"== {k} ({len(c)})")
        for name, n in c.most_common():
            print(f"  {n:5d}  {name}")
    print("== first file per construct")
    for k, f in sorted(v["examples"].items()):
        print(f"  {k:50s} {f}")
    return 0


# ---- the interpreter's names ----------------------------------------------------------------------
def dll_names(path: str) -> list[tuple[str, int, str]]:
    """`strings -n 4` over the whole file, lines matching ^[A-Z][A-Z0-9_]+$ (the 284 of the task),
    with the virtual address and section of their first occurrence."""
    with open(path, "rb") as f:
        data = f.read()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    opt = pe + 24
    base = struct.unpack_from("<I", data, opt + 28)[0]
    tab = opt + struct.unpack_from("<H", data, pe + 20)[0]
    secs = []
    for i in range(nsec):
        o = tab + 40 * i
        nm = data[o:o + 8].rstrip(b"\0").decode()
        _vs, va, rs, rp = struct.unpack_from("<IIII", data, o + 8)
        secs.append((nm, va, rs, rp))
    seen, out = set(), []
    for m in re.finditer(rb"[\x20-\x7e\t]{4,}", data):
        for part in re.finditer(rb"[^\r\n]+", m.group()):
            w = part.group()
            if not re.fullmatch(rb"[A-Z][A-Z0-9_]+", w) or w in seen:
                continue
            seen.add(w)
            off = m.start() + part.start()
            sec = next(((nm, base + va + off - rp) for nm, va, rs, rp in secs if rp <= off < rp + rs), ("header", off))
            out.append((w.decode(), sec[1], sec[0]))
    return out


def cmd_keywords(dll: str, files) -> int:
    v = vocab(files)
    words = collections.Counter()
    for f in files:
        s = parse(payload(f))
        for sec in s.sections:
            for t in sec.tokens:
                if t.kind == "IDENT":
                    words[t.text] += 1
                elif t.kind == "SECTION":
                    words[t.text[1:-1]] += 1
                elif t.kind == "STRING":
                    for w in re.findall(r"[A-Z][A-Z0-9_]+", t.text):
                        words[w] += 1
    names = dll_names(dll)
    print(f"# {len(names)} names (strings -n 4, ^[A-Z][A-Z0-9_]+$) in {os.path.basename(dll)}")
    print("# name\taddress\tsection\tcorpus_uses\tfiles_using")
    for w, a, sec in names:
        print(f"{w}\t{a:#x}\t{sec}\t{words.get(w, 0)}\t{v['files_using'].get(w, 0)}")
    return 0


def main(argv) -> int:
    if len(argv) >= 2 and argv[0] == "check":
        return cmd_check(argv[1:])
    if len(argv) >= 2 and argv[0] == "dump":
        full = argv[1] == "--full"
        if len(argv) > 1 + full:
            return cmd_dump(argv[1 + full:], full)
    if len(argv) >= 2 and argv[0] == "vocab":
        return cmd_vocab(argv[1:])
    if len(argv) >= 3 and argv[0] == "keywords":
        return cmd_keywords(argv[1], argv[2:])
    if len(argv) >= 2 and argv[0] == "quirks":
        return cmd_quirks(argv[1:])
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.stdout.reconfigure(newline="\n")
    sys.exit(main(sys.argv[1:]))
