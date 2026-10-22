#!/usr/bin/env python3
"""Mechanical determinism lint for src/sim (docs/sim/state.md §2.3, §14.6).

Scans the headers and sources of the simulation (src/sim/include and
src/sim/src; the tests are free to use floats, LCGs and so on) for constructs
the determinism rules of CLAUDE.md forbid in simulation code:

  unordered     std::unordered_* (iteration order is implementation defined)
  float         float / double (no floating point in state; the load-time
                conversion of production_type is allow-listed)
  rand          rand( / srand( (the sim owns a seeded table PRNG)
  time          time( (no wall clock)
  chrono        <chrono> / std::chrono
  random_device std::random_device
  long          `long` (its width differs between Windows and Linux)
  size_t        size_t as a data member of a struct or class (platform width;
                saved structs use fixed widths); local indices are fine
  vector_bool   std::vector<bool> (proxy references; allow-listed where the
                archives visit it through a local bool)
  random_engine <random> engines (mt19937, default_random_engine, ...): the sim
                owns one seeded table PRNG, no second generator
  raw_memory    memcpy / memmove / reinterpret_cast (no struct images; the one
                legitimate byte view in archive.hpp is allow-listed)
  env           getenv (the sim reads no environment)
  thread        std::thread / std::jthread / std::async (the parallel island
                executor, when it exists, allow-lists itself with a reason)
  hash          std::hash (implementation defined values)
  pointer_key   a std::set / std::map keyed by a pointer (address order)
  macro         #define (no layout-hiding or token-hiding macros in the sim)
  static        non-const static / thread_local variables
  global        non-const namespace-scope variables
  constexpr_count  an integral constexpr variable outside config.hpp /
                production.hpp that is not one of the named sentinels or
                versions (every original count is configuration data)

A hit passes only if tools/lint_sim_allow.txt has a line for it:
    <path>:<regex>  # <reason>
(path relative to the repository root with forward slashes; the regex is
searched in the source line). An entry without a reason is an error, and so
is an entry that matches nothing (the list must not rot).

A scan directory that does not exist or holds no source file is an error (the
lint must not pass by scanning nothing).

Usage: lint_sim.py [--root REPO]      lint the tree, exit 1 on any finding
       lint_sim.py --self-test        check the rules and the tree walk
"""
import argparse
import re
import sys
import tempfile
from pathlib import Path

SCAN_DIRS = ["src/sim/include", "src/sim/src"]
SUFFIXES = (".hpp", ".cpp", ".h", ".inl", ".ipp")
ALLOW_FILE = "tools/lint_sim_allow.txt"

# constexpr integral variables that are named sentinels or versions, not counts.
SENTINELS = {
    "kNoSlot",
    "kNoColour",
    "kAppendIndex",
    "kStateHashVersion",
    "kPrngTimelineVersion",
    "kOriginalRngLength",
    "kTileBytes",  # bytes of one tile in the stream (layout width, island.hpp)
    "kSaveVersion",  # version of the save frame (world.hpp)
    "kCommandWireVersion",  # version of the command wire layout (command.hpp)
    "kReplayLogVersion",  # version of the replay-log frame (replay.hpp)
}
# Headers where the original's constants live as documented defaults.
CONSTEXPR_FILES = {"config.hpp", "production.hpp"}

TOKEN_RULES = [
    ("unordered", re.compile(r"\bunordered_\w+")),
    ("float", re.compile(r"\bfloat\b")),
    ("float", re.compile(r"\bdouble\b")),
    ("rand", re.compile(r"\bs?rand\s*\(")),
    ("time", re.compile(r"\btime\s*\(")),
    ("chrono", re.compile(r"\bchrono\b")),
    ("random_device", re.compile(r"\brandom_device\b")),
    ("long", re.compile(r"\blong\b")),
    ("vector_bool", re.compile(r"\bvector\s*<\s*bool\s*>")),
    (
        "random_engine",
        re.compile(
            r"\b(?:mt19937(?:_64)?|default_random_engine|minstd_rand0?|ranlux\w*|knuth_b|"
            r"\w*_random_engine|linear_congruential_engine|mersenne_twister_engine|"
            r"subtract_with_carry_engine)\b"
        ),
    ),
    ("raw_memory", re.compile(r"\b(?:memcpy|memmove|reinterpret_cast)\b")),
    ("env", re.compile(r"\bgetenv\b")),
    ("thread", re.compile(r"\bstd::(?:thread|jthread|async)\b")),
    ("hash", re.compile(r"\bstd::hash\b")),
    ("pointer_key", re.compile(r"\b(?:multi)?(?:set|map)\s*<\s*[^,<>]*\*")),
]
# Matched on the source with comments and literals blanked but the
# preprocessor lines kept (strip_source(keep_preprocessor=True)).
MACRO_RULE = ("macro", re.compile(r"^\s*#\s*define\b"))

INTEGRAL = (
    r"(?:std::)?(?:u?int(?:8|16|32|64)_t|size_t|ptrdiff_t|int|unsigned(?:\s+int)?|short)"
    r"|Slot|ListId|GoodsId"
)
CONSTEXPR_VAR = re.compile(
    r"\bconstexpr\s+(?:static\s+)?(?:" + INTEGRAL + r")\s+(\w+)\s*(?:=|\{)"
)
STATIC_VAR = re.compile(r"\b(?:static|thread_local)\b([^(=;{]*)([(=;{])")
CONST_WORD = re.compile(r"\b(?:const|constexpr|constinit)\b")
NOT_A_VARIABLE = re.compile(
    r"^(?:using|typedef|template|namespace|struct|class|enum|union|static_assert|extern|"
    r"friend|public|private|protected|concept|requires)\b"
)


def strip_source(text, keep_preprocessor=False):
    """Comments, string and character literals and preprocessor lines blanked
    out (newlines and offsets kept), so rules never match inside them. With
    `keep_preprocessor` the preprocessor lines stay (for the macro rule)."""
    out = []
    i, n = 0, len(text)
    at_line_start = True
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join(ch if ch == "\n" else " " for ch in text[i:j]))
            i = j
        elif c in "\"'":
            # A ' may be a digit separator (1'000): only a literal start when
            # the previous character is not part of a number or name.
            if c == "'" and i > 0 and (text[i - 1].isalnum() or text[i - 1] == "_"):
                out.append(c)
                i += 1
                continue
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(c + " " * (j - i - 2) + c if j - i >= 2 else c)
            i = j
        elif c == "#" and at_line_start:
            j = i
            while j < n:
                k = text.find("\n", j)
                k = n if k < 0 else k
                cont = text[j:k].rstrip().endswith("\\")
                j = k
                if not cont:
                    break
                j += 1
            out.append(text[i:j] if keep_preprocessor else " " * (j - i))
            i = j
        else:
            out.append(c)
            at_line_start = c == "\n" or (at_line_start and c in " \t")
            i += 1
            continue
        at_line_start = False
    return "".join(out)


def line_of(text, pos):
    return text.count("\n", 0, pos) + 1


def scope_findings(clean):
    """(line, kind, statement) for declarations directly in a namespace or a
    struct/class: kind "global" is a non-const namespace-scope variable,
    kind "size_t" a size_t data member."""
    found = []
    stack = []  # "ns", "struct" or "other" per open brace
    stmt_start = 0
    stmt = []

    def members_level():
        return all(k in ("ns", "struct") for k in stack)

    def at_ns():
        return all(k == "ns" for k in stack)

    def check(text, pos):
        pos += len(text) - len(text.lstrip())  # the statement's first character
        s = " ".join(text.split())
        if not s or NOT_A_VARIABLE.match(s):
            return
        head = s.split("=")[0]
        if "(" in head or ")" in head or "operator" in s:
            return
        if stack and stack[-1] == "struct" and re.search(r"\bsize_t\b", s):
            found.append((line_of(clean, pos), "size_t", s))
            return
        if not at_ns() or CONST_WORD.search(s):
            return
        if re.search(r"\w\s*(?:\[[^\]]*\])?\s*(?:=.*)?$", s) and re.search(r"[\w>&*]\s+\w+", head):
            found.append((line_of(clean, pos), "global", s))

    def kind_of(head):
        h = " ".join(head.split())
        plain = re.sub(r"template\s*<[^{};]*?>\s*", "", h)
        if at_ns() and re.search(r"\bnamespace\b", h):
            return "ns"
        if not re.search(r"\benum\b", plain) and re.search(r"\b(?:struct|class|union)\s+\w+", plain):
            return "struct"
        return "other"

    for i, ch in enumerate(clean):
        if ch == "{":
            head = "".join(stmt)
            if members_level():
                kind = kind_of(head)
                if kind == "other":
                    check(head, stmt_start)  # `Type name{...};` (a braced initialiser)
                stack.append(kind)
            else:
                stack.append("other")
            stmt, stmt_start = [], i + 1
        elif ch == "}":
            if stack:
                stack.pop()
            stmt, stmt_start = [], i + 1
        elif ch == ";":
            if members_level():
                check("".join(stmt), stmt_start)
            stmt, stmt_start = [], i + 1
        elif members_level():
            if not stmt:
                stmt_start = i
            stmt.append(ch)
    return found


def lint_text(path, text):
    """[(line, rule, source line)] for one file's text (before the allow-list)."""
    clean = strip_source(text)
    lines = text.splitlines()
    hits = []

    def add(line, rule):
        src = lines[line - 1].strip() if 0 < line <= len(lines) else ""
        hits.append((line, rule, src))

    for ln, cl in enumerate(clean.splitlines(), 1):
        for rule, rx in TOKEN_RULES:
            if rx.search(cl):
                add(ln, rule)
    for ln, cl in enumerate(strip_source(text, keep_preprocessor=True).splitlines(), 1):
        if MACRO_RULE[1].search(cl):
            add(ln, MACRO_RULE[0])
    for m in STATIC_VAR.finditer(clean):
        if m.group(2) != "(" and not CONST_WORD.search(m.group(1)):
            add(line_of(clean, m.start()), "static")
    name = Path(path).name
    if name not in CONSTEXPR_FILES:
        for m in CONSTEXPR_VAR.finditer(clean):
            if m.group(1) not in SENTINELS:
                add(line_of(clean, m.start()), "constexpr_count")
    for ln, rule, _ in scope_findings(clean):
        add(ln, rule)
    return sorted(set(hits))


def parse_allow(text):
    """([(path, regex, reason, lineno)], [errors])"""
    entries, errors = [], []
    for ln, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        body, sep, reason = line.partition("  #")
        if not sep or not reason.strip():
            errors.append(f"{ALLOW_FILE}:{ln}: allow-list entry without a reason ('  # reason')")
            continue
        path, colon, pattern = body.strip().partition(":")
        if not colon or not pattern.strip():
            errors.append(f"{ALLOW_FILE}:{ln}: expected '<path>:<regex>  # <reason>'")
            continue
        try:
            entries.append((path.strip(), re.compile(pattern.strip()), reason.strip(), ln))
        except re.error as e:
            errors.append(f"{ALLOW_FILE}:{ln}: bad regex: {e}")
    return entries, errors


def apply_allow(findings, entries):
    """findings: [(path, line, rule, src)]. Returns (remaining, unused entries)."""
    used = set()
    remaining = []
    for path, line, rule, src in findings:
        ok = False
        for idx, (p, rx, _, _) in enumerate(entries):
            if p == path and rx.search(src):
                used.add(idx)
                ok = True
        if not ok:
            remaining.append((path, line, rule, src))
    unused = [e for i, e in enumerate(entries) if i not in used]
    return remaining, unused


def lint_tree(root):
    root = Path(root)
    findings = []
    scan_errors = []
    for d in SCAN_DIRS:
        base = root / d
        if not base.is_dir():
            scan_errors.append(f"{d}: scan directory is missing (nothing would be checked)")
            continue
        scanned = 0
        for f in sorted(base.rglob("*")):
            if not f.is_file() or f.suffix not in SUFFIXES:
                continue
            scanned += 1
            rel = f.relative_to(root).as_posix()
            text = f.read_text(encoding="utf-8")
            for line, rule, src in lint_text(rel, text):
                findings.append((rel, line, rule, src))
        if scanned == 0:
            scan_errors.append(f"{d}: no source file found (nothing would be checked)")
    allow_path = root / ALLOW_FILE
    allow_text = allow_path.read_text(encoding="utf-8") if allow_path.exists() else ""
    entries, errors = parse_allow(allow_text)
    remaining, unused = apply_allow(findings, entries)
    msgs = scan_errors + list(errors)
    for path, line, rule, src in remaining:
        msgs.append(f"{path}:{line}: [{rule}] {src}")
    for p, rx, _, ln in unused:
        msgs.append(f"{ALLOW_FILE}:{ln}: unused entry '{p}:{rx.pattern}' (matches nothing: remove it)")
    return msgs


# ---------------------------------------------------------------- self-test

# (name, file name, text, expected set of rules)
CASES = [
    ("clean code", "a.hpp", "namespace x {\nstruct S { std::uint32_t a = 0; };\nint f(int a);\n}\n", set()),
    ("unordered", "a.cpp", "void f() { std::unordered_map<int,int> m; }\n", {"unordered"}),
    ("float", "a.cpp", "void f() { float x = 1; }\n", {"float"}),
    ("double", "a.cpp", "void f() { double x = 1; }\n", {"float"}),
    ("rand", "a.cpp", "void f() { int r = rand(); }\n", {"rand"}),
    ("srand", "a.cpp", "void f() { srand(1); }\n", {"rand"}),
    ("not rand", "a.cpp", "void f() { msvc_rand_sequence(1); my_rand(2); }\n", set()),
    ("time", "a.cpp", "void f() { auto t = time(nullptr); }\n", {"time"}),
    ("not time", "a.cpp", "void f() { game_time(1); s.time = 2; }\n", set()),
    ("chrono", "a.cpp", "#include <chrono>\nvoid f() { std::chrono::seconds s; }\n", {"chrono"}),
    ("random_device", "a.cpp", "void f() { std::random_device d; }\n", {"random_device"}),
    ("long", "a.cpp", "void f() { long x = 1; }\n", {"long"}),
    ("size_t member", "a.hpp", "struct S { std::size_t n = 0; };\n", {"size_t"}),
    ("size_t optional member", "a.hpp", "struct S { std::optional<size_t> n; };\n", {"size_t"}),
    ("size_t braced member", "a.hpp", "class S { std::size_t n{0}; };\n", {"size_t"}),
    (
        "size_t local, parameter and return are fine",
        "a.cpp",
        "std::size_t f(std::size_t n) { std::size_t i = n; return i; }\n"
        "struct S { std::size_t size() const; static std::size_t g(std::size_t a); };\n",
        set(),
    ),
    (
        "size_t in a template header of a struct is fine",
        "a.hpp",
        "template <std::size_t N>\nstruct S { int a; };\n",
        set(),
    ),
    ("vector<bool>", "a.cpp", "struct S { std::vector<bool> v; };\n", {"vector_bool"}),
    (
        "comments and strings are ignored",
        "a.cpp",
        "// float double long rand( time( size_t\n/* unordered_map\n float */\n"
        "void f() { const char* s = \"float long rand(\"; char c = 'x'; }\n",
        set(),
    ),
    (
        "digit separator is not a char literal",
        "a.cpp",
        "void f() { int a = 1'000; float b = 2; }\n",
        {"float"},
    ),
    ("static mutable", "a.cpp", "void f() { static int counter = 0; }\n", {"static"}),
    ("static mutable member", "a.hpp", "struct S { static int n; };\n", {"static"}),
    (
        "static const is fine",
        "a.cpp",
        "void f() { static const int a = 1; static constexpr bool b = true; }\n",
        set(),
    ),
    ("static function is fine", "a.hpp", "struct S { static int make(int x); };\n", set()),
    ("static_cast is not static", "a.cpp", "void f() { auto x = static_cast<int>(1); }\n", set()),
    ("thread_local", "a.cpp", "void f() { thread_local int n = 0; }\n", {"static"}),
    ("global mutable", "a.cpp", "namespace x {\nint counter = 0;\n}\n", {"global"}),
    ("global mutable in anon namespace", "a.cpp", "namespace {\nstd::vector<int> table;\n}\n", {"global"}),
    ("global braced init", "a.cpp", "namespace x {\nstd::vector<int> table{1, 2};\n}\n", {"global"}),
    (
        "global const is fine",
        "a.cpp",
        "namespace x {\nconst int a = 1;\ninline constexpr bool b = true;\nusing T = int;\n"
        "struct S;\nenum class E : unsigned char { A };\nint f();\n}\n",
        set(),
    ),
    (
        "function body locals are not globals",
        "a.cpp",
        "namespace x {\nint f() {\n int a = 0;\n return a;\n}\n}\n",
        set(),
    ),
    (
        "struct members and methods are not globals",
        "a.hpp",
        "namespace x {\nstruct S {\n int a = 0;\n int get() const { return a; }\n};\n"
        "template <class T> struct W { T v; };\n}\n",
        set(),
    ),
    ("constexpr count", "a.hpp", "inline constexpr std::uint32_t kLimit = 8;\n", {"constexpr_count"}),
    ("constexpr count in config.hpp", "config.hpp", "inline constexpr std::uint32_t kLimit = 8;\n", set()),
    ("constexpr sentinel", "a.hpp", "inline constexpr std::uint32_t kStateHashVersion = 100;\n", set()),
    (
        "constexpr bool and function",
        "a.hpp",
        "struct S { static constexpr bool kReading = true; constexpr std::uint32_t f() const; };\n",
        set(),
    ),
    (
        "constexpr local count",
        "a.cpp",
        "void f() { constexpr std::size_t n = 4; }\n",
        {"constexpr_count"},
    ),
    ("mt19937", "a.cpp", "void f() { std::mt19937 g(1); }\n", {"random_engine"}),
    ("mt19937_64", "a.cpp", "void f() { std::mt19937_64 g(1); }\n", {"random_engine"}),
    ("default_random_engine", "a.cpp", "void f() { std::default_random_engine g; }\n", {"random_engine"}),
    ("minstd_rand", "a.cpp", "void f() { std::minstd_rand g; }\n", {"random_engine"}),
    ("ranlux", "a.cpp", "void f() { std::ranlux48 g; }\n", {"random_engine"}),
    ("not an engine", "a.cpp", "void f() { WorldRng rng; rng.next(); random_table(); }\n", set()),
    ("memcpy", "a.cpp", "void f() { std::memcpy(a, b, 4); }\n", {"raw_memory"}),
    ("memmove", "a.cpp", "void f() { memmove(a, b, 4); }\n", {"raw_memory"}),
    ("reinterpret_cast", "a.cpp", "auto p = reinterpret_cast<const char*>(q);\n", {"raw_memory"}),
    ("bit_cast and static_cast are fine", "a.cpp", "void f() { auto p = std::bit_cast<float_bits>(q); }\n", set()),
    ("getenv", "a.cpp", "void f() { auto p = std::getenv(\"X\"); }\n", {"env"}),
    ("std::thread", "a.cpp", "void f() { std::thread t; }\n", {"thread"}),
    ("std::jthread", "a.cpp", "void f() { std::jthread t; }\n", {"thread"}),
    ("std::async", "a.cpp", "void f() { auto r = std::async(g); }\n", {"thread"}),
    ("not a thread", "a.cpp", "void f() { int threads = 4; run_on_threads(threads); }\n", set()),
    ("std::hash", "a.cpp", "void f() { std::hash<int> h; }\n", {"hash"}),
    ("not std::hash", "a.cpp", "void f() { hash_state(x); h.hash = 1; StateHasher s; }\n", set()),
    ("pointer-keyed set", "a.cpp", "void f() { std::set<Foo*> s; }\n", {"pointer_key"}),
    ("pointer-keyed map", "a.cpp", "std::map<const char*, int> m;\n", {"pointer_key"}),
    ("pointer-keyed multimap", "a.cpp", "void f() { std::multimap<Foo *, int> m; }\n", {"pointer_key"}),
    ("pointer as map value is fine", "a.cpp", "void f() { std::map<int, Foo*> m; }\n", set()),
    ("pointer in nested template is fine", "a.cpp", "void f() { std::set<std::vector<int>> s; }\n", set()),
    ("#define", "a.hpp", "#define LIMIT 8\n", {"macro"}),
    ("#define hiding a forbidden token", "a.hpp", "#define X float\n", {"macro"}),
    ("#define with spaces", "a.hpp", "  #  define X 1\n", {"macro"}),
    (
        "#include #pragma #if are fine",
        "a.hpp",
        "#pragma once\n#include <vector>\n#if defined(__GNUC__)\n#endif\n",
        set(),
    ),
    ("#define in comments is ignored", "a.hpp", "// #define X 1\n/*\n#define Y 2\n*/\n", set()),
]


def self_test():
    failures = 0
    for name, fname, text, expected in CASES:
        got = {rule for _, rule, _ in lint_text(fname, text)}
        if got != expected:
            failures += 1
            print(f"FAIL {name}: expected {sorted(expected)}, got {sorted(got)}")
    # allow-list mechanics
    entries, errors = parse_allow(
        "# comment\n\na.cpp:float x  # load-time conversion\nb.cpp:foo\nc.cpp:(  # bad regex\n"
    )
    if len(entries) != 1 or len(errors) != 2:
        failures += 1
        print(f"FAIL allow-list parsing: entries={len(entries)} errors={errors}")
    findings = [("a.cpp", 3, "float", "float x = 1;"), ("a.cpp", 4, "float", "float y = 1;")]
    remaining, unused = apply_allow(findings, entries)
    if remaining != [("a.cpp", 4, "float", "float y = 1;")] or unused:
        failures += 1
        print(f"FAIL allow-list application: remaining={remaining} unused={unused}")
    remaining, unused = apply_allow([], entries)
    if len(unused) != 1:
        failures += 1
        print("FAIL allow-list: an entry that matches nothing must be reported")
    remaining, _ = apply_allow([("other.cpp", 1, "float", "float x = 1;")], entries)
    if len(remaining) != 1:
        failures += 1
        print("FAIL allow-list: entries are per file")
    failures += tree_self_test()
    print(f"lint_sim self-test: {len(CASES)} cases, {failures} failures")
    return 1 if failures else 0


def tree_self_test():
    """lint_tree() and main() on throw-away trees: the lint must fail when it
    would scan nothing, report a violation, and report an unused allow entry."""
    failures = 0

    def check(ok, what):
        nonlocal failures
        if not ok:
            failures += 1
            print(f"FAIL tree: {what}")

    def make(root, files):
        for rel, text in files.items():
            p = Path(root) / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(text, encoding="utf-8")

    clean_files = {
        "src/sim/include/a.hpp": "struct S { int a = 0; };\n",
        "src/sim/src/b.cpp": "int f() { return 1; }\n",
    }
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        # an empty root scans nothing: an error, and main() exits 1
        (tmp / "empty").mkdir()
        msgs = lint_tree(tmp / "empty")
        check(any("missing" in m for m in msgs), "an empty root must be reported as missing dirs")
        check(main(["--root", str(tmp / "empty")]) == 1, "main() on an empty root must exit 1")
        # the directories exist but hold no source file
        make(tmp / "hollow", {"src/sim/include/readme.txt": "x", "src/sim/src/note.md": "x"})
        msgs = lint_tree(tmp / "hollow")
        check(sum("no source file" in m for m in msgs) == 2, "zero files must be reported")
        # a clean tree passes, and main() exits 0
        make(tmp / "clean", clean_files)
        check(lint_tree(tmp / "clean") == [], "a clean tree must pass")
        check(main(["--root", str(tmp / "clean")]) == 0, "main() on a clean tree must exit 0")
        # a violation is reported with its rule, and main() exits 1
        bad = dict(clean_files)
        bad["src/sim/include/c.hpp"] = "struct T { float x = 0; };\n"
        bad["src/sim/src/d.inl"] = "inline int g() { return rand(); }\n"
        make(tmp / "bad", bad)
        msgs = lint_tree(tmp / "bad")
        check(any("[float]" in m and "c.hpp" in m for m in msgs), "a float violation is reported")
        check(any("[rand]" in m and "d.inl" in m for m in msgs), ".inl files are scanned")
        check(main(["--root", str(tmp / "bad")]) == 1, "main() on a violating tree must exit 1")
        # an unused allow entry is reported, a used one silences its hit
        stale = dict(clean_files)
        stale[ALLOW_FILE] = "src/sim/include/a.hpp:nothing_here  # matches nothing\n"
        make(tmp / "stale", stale)
        msgs = lint_tree(tmp / "stale")
        check(any("unused entry" in m for m in msgs), "an unused allow entry is reported")
        check(main(["--root", str(tmp / "stale")]) == 1, "main() with a stale entry must exit 1")
        used = dict(bad)
        used[ALLOW_FILE] = (
            "src/sim/include/c.hpp:float x  # test\nsrc/sim/src/d.inl:rand\\(\\)  # test\n"
        )
        make(tmp / "used", used)
        check(lint_tree(tmp / "used") == [], "allow entries with a reason silence their hits")
        # an entry without a reason is an error
        noreason = dict(bad)
        noreason[ALLOW_FILE] = "src/sim/include/c.hpp:float x\n"
        make(tmp / "noreason", noreason)
        check(any("reason" in m for m in lint_tree(tmp / "noreason")), "an entry needs a reason")
    check(SCAN_DIRS == ["src/sim/include", "src/sim/src"], "the scan directories are the sim sources")
    return failures


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--root", default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    msgs = lint_tree(args.root)
    for m in msgs:
        print(m)
    if msgs:
        print(f"lint_sim: {len(msgs)} finding(s)")
        return 1
    print("lint_sim: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
