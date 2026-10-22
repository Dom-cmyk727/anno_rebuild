"""Reference reader for the ANNO 1503 scenario script: the `BIGBROTHER` chunk of `.sze` scenarios
and of savegames. Spec: docs/formats/bigbrother.md. Independent of the C++ code; the oracle for the
script format (T17).

  bigbrother.py check <file.sze|.sav>...      parse every script, re-emit, compare bytes; exit 1 on
                                              any failure (the corpus check: all 50 .sze)
  bigbrother.py dump <file>                   the parse tree, one line per item
  bigbrother.py vocab <file>...               usage counts: sections, statements, conditions,
                                              actions with argument signatures, disaster types,
                                              string arguments, variables and signals
  bigbrother.py keywords <AnnoScene.dll> <file>...
                                              the interpreter's uppercase names (strings -n 4 rule)
                                              with address, block and corpus use

Design. The lexer turns EVERY byte into exactly one token (whitespace, newlines and `$` comments
are tokens too); the text is Latin-1, which maps bytes to characters 1:1. The parser builds the
tree over the significant tokens and must account for all of them, so anything outside the grammar
read from the 50 shipped scripts is an error, not a fallback. Emission walks the tree's sections
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
[SCRIPT_KERNEL]: no THEN for the condition at offset 234
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
OBJECT_KINDS = ("OBJECT_FIGUR", "OBJECT_STADT", "OBJECT_ANLAGE")
SUBJECTS = ("PLAYER", "OBJECT", "AREA")
COMPARE = ("==", "<", ">")          # the comparisons the scripts use (P); != <= >= never occur
BOOL = ("TRUE", "FALSE")

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
    semicolon: bool = True     # two statements in the corpus lack their ';'
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
        self.t = [x for x in toks if x.kind not in TRIVIA]
        self.i = 0
        self.where = where

    def peek(self, k=0) -> Tok | None:
        j = self.i + k
        return self.t[j] if j < len(self.t) else None

    def done(self) -> bool:
        return self.i >= len(self.t)

    def take(self, kind=None, text=None) -> Tok:
        t = self.peek()
        if t is None or (kind and t.kind != kind) or (text and t.text != text):
            want = text or kind
            got = "end of section" if t is None else repr(t.text)
            raise ScriptError(f"{self.where}: expected {want}, got {got} at offset {t.off if t else '-'}")
        self.i += 1
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
    raise ScriptError(f"{c.where}: expected a value at offset {t.off if t else '-'}")


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
    items = []
    for ln in lines(body):
        s = shape(ln)
        if name == "[OBJECT_DEFINE]":
            # OBJECT_FIGUR "N" id | OBJECT_STADT "N" id | OBJECT_ANLAGE "N" id  or  "N" x,y,type (P)
            if ln[0].kind != "IDENT" or ln[0].text not in OBJECT_KINDS or \
                    s not in ("IDENT STRING NUMBER", "IDENT STRING NUMBER , NUMBER , NUMBER"):
                raise ScriptError(f"{name}: unexpected line {' '.join(t.text for t in ln)!r}")
            items.append({"kind": ln[0].text, "name": ln[1].text[1:-1],
                          "numbers": [int(t.text) for t in ln if t.kind == "NUMBER"], "off": ln[0].off})
        elif name == "[AREA_DEFINE]":
            if s != "STRING NUMBER , NUMBER , NUMBER , NUMBER":
                raise ScriptError(f"{name}: unexpected line {' '.join(t.text for t in ln)!r}")
            items.append({"name": ln[0].text[1:-1], "rect": [int(t.text) for t in ln if t.kind == "NUMBER"],
                          "off": ln[0].off})
        elif name == "[PLAYERMAPPER_DEFINE]":
            if s != "STRING NUMBER":
                raise ScriptError(f"{name}: unexpected line {' '.join(t.text for t in ln)!r}")
            items.append({"name": ln[0].text[1:-1], "player": int(ln[1].text), "off": ln[0].off})
        elif name == "[SIGNALS]":
            if s != "IDENT = IDENT" or ln[2].text not in BOOL:
                raise ScriptError(f"{name}: unexpected line {' '.join(t.text for t in ln)!r}")
            items.append({"name": ln[0].text, "value": ln[2].text == "TRUE", "off": ln[0].off})
        elif name == "[CAMARAPOS]":
            if s != "NUMBER , NUMBER":
                raise ScriptError(f"{name}: unexpected line {' '.join(t.text for t in ln)!r}")
            items.append({"pos": [int(ln[0].text), int(ln[2].text)], "off": ln[0].off})
    return items


def primary(c: Cursor) -> Primary:
    t = c.take("IDENT")
    if t.text in SUBJECTS:
        p = Primary(t.text, c.take("STRING").text[1:-1], c.take("IDENT").text, off=t.off)
        if c.is_("STRING"):
            p.arg = c.take().text[1:-1]
    else:
        p = Primary(None, t.text, off=t.off)
    if c.is_("OP") and c.peek().text in COMPARE:
        p.op = c.take().text
        p.value = value(c)
    return p


def condition(c: Cursor, depth: int = 0) -> Group:
    items = []
    while True:
        if c.is_("OP", "("):
            c.take()
            items.append(condition(c, depth + 1))
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
    """A statement's ';'. Two shipped statements end without one (S01_08_01, S01_09_01)."""
    if c.is_("OP", ";"):
        c.take()
        return True
    return False


def block(c: Cursor) -> list:
    c.take("OP", "{")
    body = []
    while not c.is_("OP", "}"):
        t = c.take("IDENT")
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
            raise ScriptError(f"{c.where}: unexpected {c.peek().text!r} after {t.text} at offset {t.off}")
    c.take("OP", "}")
    return body


def parse_kernel(body: list[Tok]) -> list:
    c = Cursor(body, "[SCRIPT_KERNEL]")
    items = []
    while not c.done():
        t = c.take("IDENT")
        if t.text == "INITBLOCK":
            c.take("OP", "(")
            c.take("OP", ")")
            items.append(InitBlock(block(c), t.off))
        elif t.text == "SET":
            name = c.take("IDENT").text
            c.take("OP", "=")
            v = value(c)
            items.append(Assign(name, v, True, end(c), t.off))
        elif t.text == "IF":
            cond = condition(c)
            if not c.is_("IDENT", "THEN"):
                raise ScriptError(f"[SCRIPT_KERNEL]: no THEN for the condition at offset {t.off}")
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


def parse(data: bytes) -> Script:
    toks = lex(data.decode(ENCODING))
    heads = [i for i, t in enumerate(toks) if t.kind == "SECTION"]
    names = tuple(toks[i].text for i in heads)
    if names != SECTIONS:
        raise ScriptError(f"sections {names} differ from {SECTIONS}")
    if any(t.kind not in TRIVIA for t in toks[:heads[0]]):
        raise ScriptError("text before the first section")
    sections = []
    for k, i in enumerate(heads):
        j = heads[k + 1] if k + 1 < len(heads) else len(toks)
        first = 0 if k == 0 else i                   # leading trivia belongs to the first section
        name, body = toks[i].text, toks[i + 1:j]
        items = parse_kernel(body) if name == "[SCRIPT_KERNEL]" else parse_defines(name, body)
        sections.append(Section(name, toks[first:j], items))
    return Script(sections)


# ---- files --------------------------------------------------------------------------------------
def payload(path: str) -> bytes:
    """The BIGBROTHER payload of a scenario (ANNO*/SCENE/...) or a savegame (ANNO*/...)."""
    with open(path, "rb") as f:
        buf = f.read()
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


def cmd_dump(path: str) -> int:
    s = parse(payload(path))
    for sec in s.sections:
        print(sec.name)
        if sec.name != "[SCRIPT_KERNEL]":
            for it in sec.items:
                print("  ", {k: v for k, v in it.items() if k != "off"})
            continue

        def stmts(body, ind):
            for st in body:
                if isinstance(st, Assign):
                    print(f"{ind}{'SET ' if st.is_set else ''}{st.name} = {st.value.text}{'' if st.semicolon else '  (no ;)'}")
                elif isinstance(st, Call):
                    a = ", ".join(v.text for v in st.args)
                    print(f"{ind}CALL {st.name}({a}){'' if st.parens else '  (no ())'}{'' if st.semicolon else '  (no ;)'}")

        for it in sec.items:
            if isinstance(it, InitBlock):
                print("  INITBLOCK")
                stmts(it.body, "    ")
            elif isinstance(it, Assign):
                stmts([it], "  ")
            elif isinstance(it, If):
                print(f"  IF {describe(it.cond)} THEN")
                stmts(it.then, "    ")
                if it.els is not None:
                    print("  ELSE")
                    stmts(it.els, "    ")
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
                key = f"{x.subject} {x.query}" if x.subject else ("TIMER" if x.name.startswith("TIMER_") else "variable")
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
    if len(argv) == 2 and argv[0] == "dump":
        return cmd_dump(argv[1])
    if len(argv) >= 2 and argv[0] == "vocab":
        return cmd_vocab(argv[1:])
    if len(argv) >= 3 and argv[0] == "keywords":
        return cmd_keywords(argv[1], argv[2:])
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.stdout.reconfigure(newline="\n")
    sys.exit(main(sys.argv[1:]))
