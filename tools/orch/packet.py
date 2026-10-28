"""Build one task packet for the executor (orchestration tooling, round ct).

The packet is the single file the executor reads at the start of a task:
the orchestrator's detailed instructions (a Markdown file it writes), then the
binding board text, cut verbatim out of docs/orchestration/TASKS.md at one
commit. The executor no longer searches the board for the blocks.

    python -I tools/orch/packet.py --repo E:/anno1503 --at HEAD \
        --instructions <file.md> --out <packet.md> \
        --preflight "<the exact preflight command>" \
        round:cs item:1.4 item:1.12 section:2 task:T10c block:T10c:F3 ...

Selectors (each extract is copied unchanged, between marker lines):
  round:<id>          the "**Round <id> (" block, up to the next round or "## " heading
  section:<n>         the whole "## <n>." section
  item:1.<n>          item <n> of "## 1." (the "<n>. " paragraph and its indented lines)
  task:<id>           a "### <id> " or "#### <id> " entry, up to the next heading of that level or higher
  block:<id>:<label>  inside task <id>, the top-level bullet that starts "- **<label>", up to the
                      next top-level bullet (also "- **<label>" ... acceptance bullets must be selected
                      on their own, e.g. block:T10c:F3 acceptance)
A selector that matches nothing, or more than one place, is an error: the
packet is never built from a guess. "@k" picks the k-th match where the board
repeats a heading on purpose (two items of section 1 carry the number 7:
item:1.7@2 is the steps rule).
"""
from __future__ import annotations

import argparse
import hashlib
import re
import subprocess
import sys
from pathlib import Path

BOARD = "docs/orchestration/TASKS.md"


def board_text(repo: str, at: str) -> tuple[str, str]:
    commit = subprocess.run(["git", "-C", repo, "rev-parse", "--verify", at + "^{commit}"],
                            capture_output=True, text=True, check=True).stdout.strip()
    text = subprocess.run(["git", "-C", repo, "show", "%s:%s" % (commit, BOARD)],
                          capture_output=True, check=True).stdout.decode("utf-8")
    return commit, text.replace("\r\n", "\n")


def heading_level(line: str) -> int:
    m = re.match(r"^(#{1,6}) ", line)
    return len(m.group(1)) if m else 0


def find_one(lines: list[str], pred, what: str, k: int = 0) -> int:
    """The index of the one line that matches; with k > 0, of the k-th match."""
    hits = [i for i, line in enumerate(lines) if pred(line)]
    if k == 0 and len(hits) == 1:
        return hits[0]
    if 0 < k <= len(hits):
        return hits[k - 1]
    raise SystemExit("packet: selector %s matches %d places in %s (need exactly 1, or name one with @k)"
                     % (what, len(hits), BOARD))


def cut_round(lines: list[str], rid: str, k: int = 0) -> list[str]:
    start = find_one(lines, lambda s: s.startswith("**Round %s (" % rid), "round:" + rid, k)
    end = start + 1
    while end < len(lines) and not lines[end].startswith("**Round ") and heading_level(lines[end]) == 0:
        end += 1
    return lines[start:end]


def cut_section(lines: list[str], n: str, k: int = 0) -> list[str]:
    start = find_one(lines, lambda s: s.startswith("## %s. " % n), "section:" + n, k)
    end = start + 1
    while end < len(lines) and heading_level(lines[end]) not in (1, 2):
        end += 1
    return lines[start:end]


def cut_item(lines: list[str], spec: str, k: int = 0) -> list[str]:
    sec, _, n = spec.partition(".")
    body = cut_section(lines, sec)
    start = find_one(body, lambda s: s.startswith("%s. " % n), "item:" + spec, k)
    end = start + 1
    # The item runs over its indented and blank lines; the next unindented line ends it.
    while end < len(body) and (body[end].startswith(" ") or body[end] == ""):
        end += 1
    while end > start + 1 and body[end - 1] == "":
        end -= 1
    return body[start:end]


def cut_task(lines: list[str], tid: str, k: int = 0) -> list[str]:
    pat = re.compile(r"^#{3,4} %s( |$)" % re.escape(tid))
    start = find_one(lines, lambda s: bool(pat.match(s)), "task:" + tid, k)
    level = heading_level(lines[start])
    end = start + 1
    while end < len(lines):
        lv = heading_level(lines[end])
        if lv and lv <= level:
            break
        if lines[end].startswith("**Current executor order") or lines[end].startswith("**Waiting for the owner"):
            break
        end += 1
    return lines[start:end]


def cut_block(lines: list[str], spec: str, k: int = 0) -> list[str]:
    tid, _, label = spec.partition(":")
    task = cut_task(lines, tid)
    start = find_one(task, lambda s: s.startswith("- **" + label), "block:" + spec, k)
    end = start + 1
    while end < len(task) and not task[end].startswith("- **"):
        end += 1
    while end > start + 1 and task[end - 1] == "":
        end -= 1
    return task[start:end]


CUTTERS = {"round": cut_round, "section": cut_section, "item": cut_item, "task": cut_task, "block": cut_block}


def extract(lines: list[str], selector: str) -> list[str]:
    kind, _, arg = selector.partition(":")
    if kind not in CUTTERS or not arg:
        raise SystemExit("packet: unknown selector %r" % selector)
    k = 0
    if "@" in arg:
        arg, _, occ = arg.rpartition("@")
        if not occ.isdigit() or int(occ) == 0:
            raise SystemExit("packet: bad occurrence in %r" % selector)
        k = int(occ)
    return CUTTERS[kind](lines, arg, k)


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--repo", required=True)
    ap.add_argument("--at", default="HEAD")
    ap.add_argument("--instructions", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--preflight", required=True, help="the exact preflight command the executor runs first")
    ap.add_argument("selectors", nargs="+")
    a = ap.parse_args(argv)
    commit, text = board_text(a.repo, a.at)
    lines = text.split("\n")
    digest = hashlib.sha256(text.encode("utf-8")).hexdigest()[:16]
    instructions = Path(a.instructions).read_text(encoding="utf-8").replace("\r\n", "\n").rstrip("\n")
    out = [
        "<!-- generated by tools/orch/packet.py from %s at %s (sha256 %s); do not edit by hand -->" % (BOARD, commit, digest),
        "",
        "## 0. Start (two calls)",
        "",
        "1. Run the preflight. Every row must be PASS. If one fails, stop and report the table as a QUESTION.",
        "",
        "   ```",
        "   " + a.preflight,
        "   ```",
        "2. Read this whole packet. It already contains the binding board text (section 3), cut verbatim from",
        "   `%s` at `%s`. Do not search the board for these blocks again. Open other files only when a step" % (BOARD, commit),
        "   needs them, and batch independent reads into one parallel turn.",
        "",
        "Claude Code loads `CLAUDE.md` by itself; `AGENTS.md` is the same text for Codex (never edit or commit it).",
        "",
        instructions,
        "",
        "## 3. Binding board text (verbatim, `%s` at `%s`)" % (BOARD, commit[:12]),
        "",
        "Where sections 1–2 and these excerpts differ, the excerpts win; note the difference in your report.",
        "",
    ]
    for sel in a.selectors:
        block = extract(lines, sel)
        out += ["<!-- board: %s -->" % sel, ""] + block + ["", "<!-- end: %s -->" % sel, ""]
    Path(a.out).parent.mkdir(parents=True, exist_ok=True)
    Path(a.out).write_text("\n".join(out).rstrip("\n") + "\n", encoding="utf-8")
    print("packet: %s, %d selectors, board %s (sha256 %s), %d lines" % (a.out, len(a.selectors), commit[:12], digest, len(out)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
