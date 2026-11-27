#!/usr/bin/env python3
"""Keeps anno_formats/anno_sim/anno_sim_load free of app, render, SDL and GPU code (CLAUDE.md:
determinism, "Separate libraries by responsibility", the dependency direction of the former
crates; LAYER-CHECK-1).

The rule, stated here and nowhere else (the docs cite this file, they do not copy it). A header is
allowed for a library's own (non-test) source/header files if it is:
  - one of the C++20 standard library headers (STD_HEADERS below; cppreference's library header
    index, cited there rather than reproduced here);
  - its own anno/<lib>/... prefix, or the anno/<lib>/... prefix of a library strictly below it in
    the stack formats < sim < sim_load (ALLOWED_ANNO_PREFIXES);
  - a quoted header ("...") that resolves to a real file next to the file that includes it, or
    inside that library's own include/ directory (QUOTED is resolved by path, not by name);
  - a NAMED_EXCEPTION: every other header any file in the tree actually uses, with its reason and
    the exact file it is scoped to (so a new file cannot silently reuse someone else's exception).

Test files (src/<lib>/tests/) additionally get doctest/doctest.h and anno/test/... (test_support's
real header path; "anno_test_support" is the library name, not the include prefix). anno_sim's own
tests also get anno/sim_load/... (the scenario corpus case reads it, docs/sim/state.md S14 stage
2 - CMake already links anno_sim_tests to anno_sim_load) and the quoted-header allowance for their
own tests/ directory as their "library include directory".

Everything else is refused, including (named explicitly, since these are what the task asks a
synthetic case to catch): SDL (<SDL.h>, <SDL3/...>), WebGPU/wgpu (<webgpu.h>, <wgpu.h>,
<wgpu/...>), Vulkan (<vulkan/...>), <windows.h>, and anno/render/, anno/app/, anno/assets/,
anno/net/, anno/ui/ - the layers CLAUDE.md says these three must stay below.

Two checks:
  include  every #include directive in the three libraries' source and header files (<...> and
           "..." forms, spaces after '#' allowed), against the allow list above.
  link     each of the three targets' real CMake LINK_LIBRARIES (read from a file CMake itself
           generates at configure time with $<TARGET_PROPERTY:...,LINK_LIBRARIES>, --link-list;
           never a hand-written copy of the graph), against the same stack's "may link" column
           (a library may link only a library strictly below it).

Usage: layer_check.py --root REPO --link-list FILE   check the real tree and link graph, exit 1
                                                      on any violation
       layer_check.py --self-test                    check the rule and the scanner on synthetic
                                                       trees/link lists, including the required
                                                       failing cases (an injected SDL/Vulkan/
                                                       windows.h include, a link list with
                                                       anno_render under anno_sim)
"""
import argparse
import re
import sys
import tempfile
from pathlib import Path

# C++20 standard library headers (cppreference's "C++ Standard Library header files" index,
# https://en.cppreference.com/w/cpp/header - cited, not reproduced in full prose here). Includes
# the C-compatibility headers (<c...>) the standard also specifies. A header this project never
# happens to use today is still listed, so a later, legitimate use of it does not need a new rule.
STD_HEADERS = frozenset(
    """
    algorithm any array atomic barrier bit bitset
    cassert cctype cerrno cfenv cfloat charconv chrono cinttypes ciso646 climits clocale
    cmath codecvt compare complex concepts condition_variable coroutine csetjmp csignal
    cstdarg cstddef cstdint cstdio cstdlib cstring ctgmath ctime cuchar cwchar cwctype
    deque exception execution expected filesystem forward_list fstream functional future
    initializer_list iomanip ios iosfwd iostream istream iterator latch limits list locale
    map memory memory_resource mutex new numbers numeric optional ostream queue random
    ranges ratio regex scoped_allocator semaphore set shared_mutex source_location span
    sstream stack stdexcept stop_token streambuf string string_view strstream syncstream
    system_error thread tuple type_traits typeindex typeinfo unordered_map unordered_set
    utility valarray variant vector version
    """.split()
)

# Library name -> (its own header prefix, non-test source directories relative to repo root).
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
TEST_DOCTEST = "doctest/doctest.h"

# Header prefixes/names that are always refused, wherever they appear: the upper layers CLAUDE.md
# says formats/sim/sim_load must stay below, and third-party libraries that pull in a window/GPU/
# OS dependency. Named explicitly (not just "not in the allow list") so the reason is specific.
FORBIDDEN_PATTERNS = [
    ("SDL", re.compile(r"^(SDL3?/|SDL\.h$|SDL3?\.h$)")),
    ("webgpu/wgpu", re.compile(r"^(webgpu\.h$|wgpu\.h$|wgpu/)")),
    ("Vulkan", re.compile(r"^vulkan/")),
    ("windows.h", re.compile(r"^windows\.h$", re.IGNORECASE)),
    ("anno/render", re.compile(r"^anno/render/")),
    ("anno/app", re.compile(r"^anno/app/")),
    ("anno/assets", re.compile(r"^anno/assets/")),
    ("anno/net", re.compile(r"^anno/net/")),
    ("anno/ui", re.compile(r"^anno/ui/")),
]

# Every other header a file in the tree actually uses, scoped to the one file it is needed for
# (relative to the repo root, forward slashes), with its reason. A header not in STD_HEADERS, not
# an anno/ prefix the rule allows, not a quoted header that resolves by path, and not named here is
# refused. Adding a name here without a matching file is a self-test failure (keeps the list from
# rotting the way an unscoped allow-list would).
NAMED_EXCEPTIONS = {
    ("src/sim/tests/founding_assets_test.cpp", "sys/resource.h"): (
        "POSIX getrusage() for the test's own peak-RSS report; no Windows equivalent header, "
        "guarded in the source by #ifndef _WIN32"
    ),
    ("src/sim/tests/founding_assets_test.cpp", "freight_original.inc"): (
        "a generated fixture (original-data component projection), not a header; included once, "
        "textually, the same as a fixture array literal would be written inline"
    ),
}

SUFFIXES = (".hpp", ".cpp", ".h", ".inl", ".ipp")
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^">]+)[>"]')


def allowed_anno_prefixes(lib, is_test):
    """The anno/ header prefixes `lib`'s own files (or, with is_test, its tests) may use."""
    index = STACK.index(lib)
    prefixes = [LIBS[name][0] for name in STACK[: index + 1]]
    if is_test:
        prefixes = prefixes + TEST_COMMON_PREFIXES + TEST_EXTRA_PREFIXES.get(lib, [])
    return prefixes


def classify_include(header, is_quoted, lib, is_test, including_file=None, repo_root=None):
    """None if `header` is allowed for `lib` (or its tests, when is_test); else the broken rule's
    name. `including_file`/`repo_root` are needed only to resolve a quoted header by path."""
    for name, pattern in FORBIDDEN_PATTERNS:
        if pattern.search(header):
            return name
    if header.startswith("anno/"):
        if any(header.startswith(p) for p in allowed_anno_prefixes(lib, is_test)):
            return None
        return "outside the allowed layers"
    if is_test and header == TEST_DOCTEST:
        return None
    if header in STD_HEADERS:
        return None
    if is_quoted and including_file is not None and repo_root is not None:
        if (including_file.parent / header).is_file():
            return None
        lib_dirs = LIBS[lib][1] + ([TEST_DIRS[lib]] if is_test else [])
        for d in lib_dirs:
            if (Path(repo_root) / d / header).is_file():
                return None
    if including_file is not None:
        rel = Path(including_file).as_posix()
        if repo_root is not None:
            try:
                rel = Path(including_file).resolve().relative_to(Path(repo_root).resolve()).as_posix()
            except ValueError:
                pass
        if (rel, header) in NAMED_EXCEPTIONS:
            return None
    return "not a standard header, not this library's own area, and not a named exception"


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
                    is_quoted = m.group(1) == '"'
                    header = m.group(2)
                    rule = classify_include(header, is_quoted, lib, is_test, path, root)
                    if rule is not None:
                        rel = path.relative_to(root).as_posix()
                        violations.append((rel, lineno, header, rule))
    return violations


def check_named_exceptions_are_used(root):
    """[message] for a NAMED_EXCEPTIONS entry whose file:header no longer appears in the tree (the
    list must not rot into claims about files that no longer need them)."""
    root = Path(root)
    messages = []
    for (rel, header), _reason in NAMED_EXCEPTIONS.items():
        path = root / rel
        if not path.is_file():
            messages.append(f"named exception for {rel}: that file does not exist")
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        needle_angle = f"#include <{header}>"
        needle_quote = f'#include "{header}"'
        if needle_angle not in text.replace(" ", "") and needle_quote not in text.replace(" ", ""):
            # A looser substring check tolerates spacing differences the exact needles do not.
            if header not in text:
                messages.append(f"named exception for {rel}:{header}: not included there anymore")
    return messages


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
    messages += check_named_exceptions_are_used(root)
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

    # classify_include: the rule table itself (no including_file/repo_root: these cases never
    # need the quoted-path or named-exception resolution).
    check(classify_include("vector", False, "anno_formats", False) is None, "std headers (<>) are allowed")
    check(
        classify_include("optional", True, "anno_formats", False) is None,
        "std headers are allowed quoted too (the standard does not care about the delimiter)",
    )
    check(
        classify_include("anno/formats/figuren.hpp", True, "anno_formats", False) is None,
        "a library may include its own headers",
    )
    check(
        classify_include("anno/formats/figuren.hpp", True, "anno_sim", False) is None,
        "sim may include formats (below it)",
    )
    check(
        classify_include("anno/sim/world.hpp", True, "anno_formats", False) is not None,
        "formats may not include sim (above it)",
    )
    check(
        classify_include("anno/sim_load/scenario.hpp", True, "anno_sim", False) is not None,
        "sim may not include sim_load (above it) outside its own tests",
    )
    check(
        classify_include("anno/sim_load/scenario.hpp", True, "anno_sim", True) is None,
        "sim's own tests may include sim_load",
    )
    check(
        TEST_EXTRA_PREFIXES.get("anno_sim_load", []) == [],
        "sim_load's tests get no extra exception (only sim's own tests do, into sim_load)",
    )
    check(
        classify_include("anno/test/game_dir.hpp", True, "anno_sim", False) is not None,
        "non-test sim code may not include test support",
    )
    check(
        classify_include("anno/test/game_dir.hpp", True, "anno_sim", True) is None,
        "sim's tests may include test support",
    )
    check(
        classify_include(TEST_DOCTEST, True, "anno_sim", False) is not None,
        "doctest is a test-only allowance",
    )
    check(
        classify_include(TEST_DOCTEST, True, "anno_sim", True) is None,
        "sim's tests may include doctest",
    )
    for header in ["SDL3/SDL.h", "SDL.h", "webgpu.h", "wgpu.h", "wgpu/wgpu.h", "vulkan/vulkan.h", "windows.h"]:
        check(
            classify_include(header, True, "anno_formats", False) is not None,
            f"{header} is refused everywhere (F1: an allow list refuses everything not named)",
        )
    for area in ["render", "app", "assets", "net", "ui"]:
        check(
            classify_include(f"anno/{area}/x.hpp", True, "anno_sim_load", False) is not None,
            f"anno/{area}/ is refused everywhere",
        )
    # F1: a header this rule does not know is refused even if it looks innocuous (a deny list
    # would have missed this; an allow list refuses it correctly).
    check(
        classify_include("GLFW/glfw3.h", True, "anno_sim", False) is not None,
        "an unlisted third-party header is refused, not passed through by default",
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
    """run() on throw-away trees: a clean tree passes; an injected SDL/Vulkan/windows.h include in
    a sim source is reported with its file and line (Done when); a synthetic link-list violation
    is reported; a quoted header resolving by path is allowed; the real tree (this checkout)
    passes both checks."""
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
                "src/formats/include/anno/formats/a.hpp": "#pragma once\n#include <vector>\n",
                "src/formats/src/a.cpp": '#include "anno/formats/a.hpp"\n',
                "src/sim/include/anno/sim/b.hpp": "#pragma once\n#include <anno/formats/a.hpp>\n",
                "src/sim/src/b.cpp": "#include \"anno/sim/b.hpp\"\n",
                "src/sim/src/helper.hpp": "#pragma once\n",
                "src/sim/src/uses_helper.cpp": '#include "helper.hpp"\n',
                "src/sim/tests/b_test.cpp": (
                    '#include "anno/sim/b.hpp"\n'
                    '#include "anno/test/game_dir.hpp"\n'
                    '#include "anno/sim_load/scenario.hpp"\n'
                    '#include <doctest/doctest.h>\n'
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
        check(
            (clean / "src/sim/src/helper.hpp").is_file(),
            "the quoted-header-resolves-next-to-its-includer case is set up",
        )
        # the required failing cases: SDL, Vulkan, windows.h injected in a sim source.
        for header, rule_name in [
            ("SDL3/SDL.h", "SDL"),
            ("vulkan/vulkan.h", "Vulkan"),
            ("windows.h", "windows.h"),
        ]:
            bad = tmp / f"bad_{rule_name}"
            make_clean_tree(bad)
            make(
                bad,
                {"src/sim/src/b.cpp": f'#include "anno/sim/b.hpp"\n#include <{header}>\n'},
            )
            violations = scan_includes(bad)
            check(
                any(v[0] == "src/sim/src/b.cpp" and v[1] == 2 and v[3] == rule_name for v in violations),
                f"an injected {header} include is reported with its file and line",
            )
        # an unresolvable quoted header (no such file next to it, not in the library's own
        # include dir) is refused, not silently passed.
        bad_quote = tmp / "bad_quote"
        make_clean_tree(bad_quote)
        make(bad_quote, {"src/sim/src/b.cpp": '#include "anno/sim/b.hpp"\n#include "nonexistent.hpp"\n'})
        check(
            any(v[1] == 2 for v in scan_includes(bad_quote)),
            "a quoted header that resolves to no real file is refused",
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
        # a named exception for a file that is not in the tree at all is reported, not assumed ok.
        stale_root = tmp / "stale_exception"
        check(
            any("does not exist" in m for m in check_named_exceptions_are_used(stale_root)),
            "a named exception for a file that does not exist in the tree is reported",
        )

    # the real tree (this checkout) must pass the include check outright, and every named
    # exception must still be exactly where it says it is.
    repo_root = Path(__file__).resolve().parent.parent
    real_violations = scan_includes(repo_root)
    check(real_violations == [], f"the real tree has no include violations: {real_violations}")
    stale = check_named_exceptions_are_used(repo_root)
    check(stale == [], f"every named exception is still used where it says: {stale}")
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
