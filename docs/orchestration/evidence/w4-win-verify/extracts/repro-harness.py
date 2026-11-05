"""W4-WIN-VERIFY: the review's counterexamples run through the public wrapper, before and after.

    python -I repro.py <tools dir holding viewer_wiring.py> <label> <old|new>

Runs the same fixture program as tools/test/viewer_wiring_contract_test.py (imported from the task worktree) with
the wrapper of <tools dir>. `old` feeds the logs the published window (dad27da) wrote, `new` the logs of the
corrected window. Prints one line per probe: its name, the wrapper's exit status and its verdict line."""
import sys
from pathlib import Path

WORKTREE = Path(r"E:\anno1503-worktrees\anno1503-W4-3")
sys.path.insert(0, str(WORKTREE / "tools" / "test"))
import unittest  # noqa: E402

import viewer_wiring_contract_test as c  # noqa: E402

tools, label, kind = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
c.WIRING = tools / "viewer_wiring.py"
old = kind == "old"
GOOD = c.LEGACY if old else c.GOOD  # what a window of that era records for tiny.script


class Probe(c.ViewerWiringContract):
    pass


def line(p):
    out = [l for l in (p.stdout + p.stderr).splitlines() if "viewer_wiring: " in l or l.startswith("  ")]
    return " | ".join(l.strip()[:200] for l in out[:3])


def main():
    case = Probe("test_control_a_complete_matching_record_passes")
    Probe.setUpClass()
    tiny_text = c.TINY.read_text()
    look999 = case.variant("look999", tiny_text.replace("look 1 2", "look 1 999"))
    wait3 = case.variant("wait3", tiny_text.replace("wait frames 2", "wait frames 3"))
    probes = [
        ("clean complete record", dict(record=GOOD)),
        ("stderr only: UBSan runtime error, child exit 0", dict(record=GOOD, stderr=c.UBSAN + "\n")),
        ("stderr only: ASan report, child exit 0", dict(record=GOOD, stderr=c.ASAN + "\n")),
        ("stdout only: UBSan runtime error, child exit 0", dict(record=GOOD, stdout=c.UBSAN + "\n")),
        ("invoked `look 1 999`, log of `look 1 2`", dict(record=GOOD, script=look999)),
        ("invoked `wait frames 3`, log of `wait frames 2`", dict(record=GOOD, script=wait3)),
    ]
    print(f"== {label}: wrapper {c.WIRING}")
    for name, kw in probes:
        p, _ = case.run_case("complete", **kw)
        verdict = "PASS" if p.returncode == 0 else "FAIL"
        print(f"{verdict:4} exit={p.returncode}  {name}\n       {line(p)}")


main()
