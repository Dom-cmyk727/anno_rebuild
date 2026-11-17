#!/usr/bin/env python3
"""Keeps anno_formats/anno_sim/anno_sim_load free of app, render, SDL and GPU code (CLAUDE.md:
determinism, "Separate libraries by responsibility", the dependency direction of the former
crates; LAYER-CHECK-1).

The rule, stated here and nowhere else (the docs cite this file, they do not copy it):

  Library        May #include (its own anno/<lib>/... plus)   May link
  anno_formats   nothing below it                              nothing
  anno_sim       anno/formats/...                              anno_formats
  anno_sim_load  anno/sim/..., anno/formats/...                anno_sim

  Tests (src/<lib>/tests/) of each library get the above, plus doctest and anno/test/...;
  anno_sim's own tests also get anno/sim_load/... (the scenario corpus case reads it,
  docs/sim/state.md S14 stage 2 - CMake already links anno_sim_tests to anno_sim_load).

Forbidden everywhere in the three libraries (code and tests alike): SDL (<SDL.h>, <SDL3/...>),
WebGPU/wgpu (<webgpu.h>, <wgpu.h>, <wgpu/...>), and anno/render/, anno/app/, anno/assets/,
anno/net/, anno/ui/ - the layers CLAUDE.md says these three must stay below.

Two checks:
  include  every #include directive in the three libraries' source and header files (<...> and
           "..." forms, spaces after '#' allowed), against the table above.
  link     each of the three targets' real CMake LINK_LIBRARIES (read from a file CMake itself
           generates at configure time with $<TARGET_PROPERTY:...,LINK_LIBRARIES>, --link-list;
           never a hand-written copy of the graph), against the same table's "may link" column.

Usage: layer_check.py --root REPO --link-list FILE   check the real tree and link graph, exit 1
                                                      on any violation
       layer_check.py --self-test                    check the rule and the scanner on synthetic
                                                       trees/link lists, including the two required
                                                       failing cases (an injected SDL include, a
                                                       link list with anno_render under anno_sim)
"""
import argparse
import re
import sys
import tempfile
from pathlib import Path

# Library name -> (its own header prefix, source directories relative to repo root).
LIBS = {
    "anno_formats": ("anno/formats/", ["src/formats/include", "src/formats/src"]),
    "anno_sim": ("anno/sim/", ["src/sim/include", "src/sim/src"]),
    "anno_sim_load": ("anno/sim_load/", ["src/sim_load/include", "src/sim_load/src"]),
}
# Stack order, bottom first: what each library (by index) may reach below it.
STACK = ["anno_formats", "anno_sim", "anno_sim_load"]
TEST_DIRS = {
    "anno_formats": "src/formats/tests",
    "anno_sim": "src/sim/tests",
    "anno_sim_load": "src/sim_load/tests",
}
# anno_sim's own tests reach one level above sim (sim_load's scenario loading), nowhere else.
TEST_EXTRA_PREFIXES = {
    "anno_sim": ["anno/sim_load/"],
}
TEST_COMMON_PREFIXES = ["anno/test/"]

# Header prefixes (outside anno/, or an anno/ area not in LIBS at all) that are always refused,
# wherever they appear: the upper layers CLAUDE.md says formats/sim/sim_load must stay below, and
# the two third-party libraries that pull in a window/GPU dependency.
FORBIDDEN_PATTERNS = [
    ("SDL", re.compile(r"^(SDL3?/|SDL\.h$|SDL3?\.h$)")),
    ("webgpu/wgpu", re.compile(r"^(webgpu\.h$|wgpu\.h$|wgpu/)")),
    ("anno/render", re.compile(r"^anno/render/")),
    ("anno/app", re.compile(r"^anno/app/")),
    ("anno/assets", re.compile(r"^anno/assets/")),
    ("anno/net", re.compile(r"^anno/net/")),
    ("anno/ui", re.compile(r"^anno/ui/")),
]

SUFFIXES = (".hpp", ".cpp", ".h", ".inl", ".ipp")
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[>"]')


def allowed_prefixes(lib, is_test):
    """The anno/ header prefixes `lib`'s own files (or, with is_test, its tests) may use."""
    index = STACK.index(lib)
    prefixes = [LIBS[name][0] for name in STACK[: index + 1]]
    if is_test:
        prefixes = prefixes + TEST_COMMON_PREFIXES + TEST_EXTRA_PREFIXES.get(lib, [])
    return prefixes


def classify_include(header, lib, is_test):
    """None if `header` is allowed for `lib` (or its tests); else the broken rule's name."""
    for name, pattern in FORBIDDEN_PATTERNS:
        if pattern.search(header):
            return name
    if not header.startswith("anno/"):
        return None  # the standard library, or a third-party header this rule does not name.
    allowed = allowed_prefixes(lib, is_test)
    if any(header.startswith(p) for p in allowed):
        return None
    return "outside the allowed layers"


def scan_includes(root):
    """[(file, line, header, rule)] for every forbidden #include under the three libraries."""
    root = Path(root)
    violations = []
    for lib, (_, dirs) in LIBS.items():
        kinds = [(d, False) for d in dirs] + [(TEST_DIRS[lib], True)]
        for rel_dir, is_test in kinds:
            d = root / rel_dir
            if not d.is_dir():
                continue
            for path in sorted(d.rglob("*")):
                if not path.is_file() or path.suffix not in SUFFIXES:
                    continue
                text = path.read_text(encoding="utf-8", errors="replace")
                for lineno, line in enumerate(text.splitlines(), start=1):
                    m = INCLUDE_RE.match(line)
                    if not m:
                        continue
                    header = m.group(1)
                    rule = classify_include(header, lib, is_test)
                    if rule is not None:
                        rel = path.relative_to(root).as_posix()
                        violations.append((rel, lineno, header, rule))
    return violations


def parse_link_list(text):
    """{library: [linked target, ...]} from a file CMake's file(GENERATE) wrote: one
    "<library>:<;-separated LINK_LIBRARIES>" line per library (the generator-expression value is
    empty for a library with no link_libraries call)."""
    out = {}
    for line in text.splitlines():
        line = line.strip()
        if not line or ":" not in line:
            continue
        lib, _, rest = line.partition(":")
        out[lib] = [t for t in rest.split(";") if t]
    return out


def check_links(links):
    """[(library, bad target)] for every link LIBS's table does not allow."""
    violations = []
    for lib, linked in links.items():
        if lib not in LIBS:
            continue
        index = STACK.index(lib)
        allowed = set(STACK[:index])  # strictly below, not itself
        for target in linked:
            if target not in allowed:
                violations.append((lib, target))
    return violations


def format_include_violation(v):
    rel, lineno, header, rule = v
    return f"{rel}:{lineno}: #include <{header}> breaks the rule [{rule}]"


def format_link_violation(v):
    lib, target = v
    return f"{lib} links {target}, which the rule does not allow"


def run(root, link_list_path):
    messages = [format_include_violation(v) for v in scan_includes(root)]
    if link_list_path is not None:
        link_text = Path(link_list_path).read_text(encoding="utf-8")
        messages += [format_link_violation(v) for v in check_links(parse_link_list(link_text))]
    return messages


def self_test():
    failures = 0

    def check(ok, what):
        nonlocal failures
        if not ok:
            failures += 1
            print(f"FAIL: {what}")

    # classify_include: the rule table itself.
    check(classify_include("vector", "anno_formats", False) is None, "std headers are allowed")
    check(
        classify_include("anno/formats/figuren.hpp", "anno_formats", False) is None,
        "a library may include its own headers",
    )
    check(
        classify_include("anno/formats/figuren.hpp", "anno_sim", False) is None,
        "sim may include formats (below it)",
    )
    check(
        classify_include("anno/sim/world.hpp", "anno_formats", False) is not None,
        "formats may not include sim (above it)",
    )
    check(
        classify_include("anno/sim_load/scenario.hpp", "anno_sim", False) is not None,
        "sim may not include sim_load (above it) outside its own tests",
    )
    check(
        classify_include("anno/sim_load/scenario.hpp", "anno_sim", True) is None,
        "sim's own tests may include sim_load",
    )
    check(
        TEST_EXTRA_PREFIXES.get("anno_sim_load", []) == [],
        "sim_load's tests get no extra exception (only sim's own tests do, into sim_load)",
    )
    check(
        classify_include("anno/test/game_dir.hpp", "anno_sim", False) is not None,
        "non-test sim code may not include test_support",
    )
    check(
        classify_include("anno/test/game_dir.hpp", "anno_sim", True) is None,
        "sim's tests may include test_support",
    )
    for header in ["SDL3/SDL.h", "SDL.h", "webgpu.h", "wgpu.h", "wgpu/wgpu.h"]:
        check(
            classify_include(header, "anno_formats", False) is not None,
            f"{header} is refused everywhere",
        )
    for area in ["render", "app", "assets", "net", "ui"]:
        check(
            classify_include(f"anno/{area}/x.hpp", "anno_sim_load", False) is not None,
            f"anno/{area}/ is refused everywhere",
        )

    # check_links: the rule's link table.
    check(check_links({"anno_formats": []}) == [], "formats linking nothing is clean")
    check(
        check_links({"anno_sim": ["anno_formats"]}) == [],
        "sim linking formats is clean",
    )
    check(
        check_links({"anno_sim_load": ["anno_sim"]}) == [],
        "sim_load linking sim is clean",
    )
    check(
        check_links({"anno_sim": ["anno_formats", "anno_sim_load"]})
        == [("anno_sim", "anno_sim_load")],
        "sim may not link sim_load (above it)",
    )
    check(
        ("anno_sim", "anno_render") in check_links({"anno_sim": ["anno_render"]}),
        "a synthetic link list with anno_render under anno_sim must fail (Done when)",
    )

    failures += tree_self_test()
    print(f"layer_check self-test: {failures} failures")
    return 1 if failures else 0


def tree_self_test():
    """run() on throw-away trees: a clean tree passes; an injected SDL include in a sim source is
    reported with its file and line (Done when); a synthetic link-list violation is reported; the
    real tree (this checkout) passes both checks."""
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

    def make_clean_tree(root):
        make(
            root,
            {
                "src/formats/include/anno/formats/a.hpp": "#pragma once\n",
                "src/formats/src/a.cpp": '#include "anno/formats/a.hpp"\n',
                "src/sim/include/anno/sim/b.hpp": "#pragma once\n#include <anno/formats/a.hpp>\n",
                "src/sim/src/b.cpp": "#include \"anno/sim/b.hpp\"\n",
                "src/sim/tests/b_test.cpp": (
                    '#include "anno/sim/b.hpp"\n'
                    '#include "anno/test/game_dir.hpp"\n'
                    '#include "anno/sim_load/scenario.hpp"\n'
                ),
                "src/sim_load/include/anno/sim_load/c.hpp": "#pragma once\n#include <anno/sim/b.hpp>\n",
                "src/sim_load/src/c.cpp": '#include "anno/sim_load/c.hpp"\n',
            },
        )

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        clean = tmp / "clean"
        make_clean_tree(clean)
        check(scan_includes(clean) == [], "a clean synthetic tree has no include violations")
        # the required failing case: an injected SDL include in a sim source.
        bad_include = tmp / "bad_include"
        make_clean_tree(bad_include)
        make(
            bad_include,
            {"src/sim/src/b.cpp": '#include "anno/sim/b.hpp"\n#include <SDL3/SDL.h>\n'},
        )
        violations = scan_includes(bad_include)
        check(
            any(v[0] == "src/sim/src/b.cpp" and v[1] == 2 and v[3] == "SDL" for v in violations),
            "an injected SDL include is reported with its file and line",
        )
        # the required failing case: a synthetic link list with anno_render under anno_sim.
        bad_link = check_links(
            parse_link_list("anno_formats:\nanno_sim:anno_formats;anno_render\nanno_sim_load:anno_sim\n")
        )
        check(
            ("anno_sim", "anno_render") in bad_link,
            "a synthetic link list with anno_render under anno_sim must fail",
        )
        # malformed/empty input is handled, not crashed on.
        check(parse_link_list("") == {}, "an empty link list parses to nothing")
        check(
            parse_link_list("anno_formats:\n") == {"anno_formats": []},
            "a library with no links parses to an empty list",
        )

    # the real tree (this checkout) must pass the include check outright.
    repo_root = Path(__file__).resolve().parent.parent
    real_violations = scan_includes(repo_root)
    check(real_violations == [], f"the real tree has no include violations: {real_violations}")
    return failures


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--root", default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument("--link-list", default=None)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    messages = run(args.root, args.link_list)
    for m in messages:
        print(m)
    if messages:
        print(f"layer_check: {len(messages)} violation(s)")
        return 1
    print("layer_check: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
