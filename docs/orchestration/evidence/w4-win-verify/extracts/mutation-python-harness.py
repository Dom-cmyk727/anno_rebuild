"""W4-WIN-VERIFY: mutation proofs for the Python wrapper/validator, run on scratch copies outside tools/.

Each mutant copies tools/test (with its fixtures) to cache/v/mut/<name>/tools/test, applies one source edit, runs the
copy's viewer_wiring_contract_test.py and records which tests fail. The control (no edit) must pass all of them.
    python -I mutate_py.py [name ...]"""
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(r"E:\anno1503-worktrees\anno1503-W4-3")
PY = r"C:\Users\altin\AppData\Local\Programs\Python\Python314\python.exe"
OUT = ROOT / "cache" / "v" / "mut"

MUTANTS = {
    "control": [],
    "no-sanitizer-scan": [("viewer_wiring.py", "    if reports:\n        print(f\"viewer_wiring: FAIL {case}: the window wrote sanitizer diagnostics ",
                           "    if False:\n        print(f\"viewer_wiring: FAIL {case}: the window wrote sanitizer diagnostics ")],
    "stdout-not-scanned": [("viewer_wiring.py", 'sanitizer_reports({"stdout": out_path, "stderr": err_path})',
                            'sanitizer_reports({"stderr": err_path})')],
    "stdout-discarded": [("viewer_wiring.py", "stdout=so, stderr=se", "stdout=subprocess.DEVNULL, stderr=se")],
    "own-sanitizer-pattern": [("viewer_wiring.py", "m = ctest_verdict.SANITIZER.search(line)",
                               "m = __import__('re').search('runtime error:', line)")],
    "scan-after-expect-stderr": [("viewer_wiring.py", "    if reports:\n        print(f\"viewer_wiring: FAIL {case}: the window wrote sanitizer diagnostics ",
                                  "    if reports and not flags['stderr']:\n        print(f\"viewer_wiring: FAIL {case}: the window wrote sanitizer diagnostics ")],
    "no-ubsan-default": [("viewer_wiring.py", 'env.setdefault("UBSAN_OPTIONS", "halt_on_error=1:print_stacktrace=1")', "pass")],
    "ubsan-default-overrides-caller": [("viewer_wiring.py", 'env.setdefault("UBSAN_OPTIONS", "halt_on_error=1:print_stacktrace=1")',
                                        'env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"')],
    "identity-compare-removed": [("viewer_session.py", "        elif identities[0] != want_id:", "        elif False:")],
    "identity-first-argument-only": [("viewer_session.py", "return f\"{IDENTITY_PREFIX} steps={len(steps)} fnv1a64={fnv1a64(data):016x}\"",
                                      "return f\"{IDENTITY_PREFIX} steps={len(steps)} fnv1a64={fnv1a64(canonical_text([(n, v, a[:1]) for n, v, a in steps]).encode()):016x}\"")],
    "identity-waits-dropped": [("viewer_session.py", "    steps = script_steps(script)\n    data = canonical_text(steps)",
                                "    steps = script_steps(script)\n    data = canonical_text([s for s in steps if s[1] != 'wait'])")],
    "identity-not-required": [("viewer_session.py", "    if not full and not legacy_ok:", "    if False:")],
    "legacy-accepted-by-wrapper": [("viewer_wiring.py", "viewer_session.problems(script.read_bytes(), log_text)",
                                    "viewer_session.problems(script.read_bytes(), log_text, legacy_ok=True)")],
    "legacy-reported-valid": [("viewer_session.py", "if match and legacy:", "if False:")],
    "trace-first-argument-only": [("viewer_session.py", "(a if full else a[:1])", "a[:1]")],
    "ran-records-compare-first-argument-only": [
        ("viewer_session.py", "found.append((int(m.group(1)), m.group(2)))",
         "found.append((int(m.group(1)), ' '.join(m.group(2).split(' ')[:2])))"),
        ("viewer_session.py", "want = expected_records(script, full=full)", "want = expected_records(script, full=False)")],
    "log-read-with-splitlines": [("viewer_session.py", 'lines = [line[:-1] if line.endswith("\\r") else line for line in log_text.split("\\n")]',
                                  "lines = log_text.splitlines()")],
    "long-line-cut-before-match": [("viewer_wiring.py", 'excerpt = line if len(line) <= 300 else "..." + line[max(0, m.start() - 100):m.start() + 200]',
                                    "excerpt = line[:300]")],
    "log-echo-after-scan": [("viewer_wiring.py", '    for line in (log_text or "").splitlines():\n        print(line)\n    # Child diagnostics',
                             '    # Child diagnostics')],
    "identity-line-anywhere": [("viewer_session.py", "        if len(identities) != 1 or first != identities[0]:",
                                "        if len(identities) < 1:")],
}


def run(name):
    d = OUT / name
    shutil.rmtree(d, ignore_errors=True)
    shutil.copytree(ROOT / "tools" / "test", d / "tools" / "test", ignore=shutil.ignore_patterns("__pycache__"))
    for fname, old, new in MUTANTS[name]:
        p = d / "tools" / "test" / fname
        t = p.read_text(encoding="utf-8")
        assert t.count(old) == 1, f"{name}: pattern not unique in {fname}: {old[:60]!r}"
        p.write_text(t.replace(old, new), encoding="utf-8", newline="")
    r = subprocess.run([PY, "-I", str(d / "tools" / "test" / "viewer_wiring_contract_test.py")],
                       capture_output=True, text=True, timeout=600)
    failed = sorted(set(re.findall(r"^(?:FAIL|ERROR): (\w+)", r.stdout + r.stderr, re.M)))
    tail = [l for l in (r.stderr + r.stdout).splitlines() if l.startswith(("Ran ", "OK", "FAILED"))]
    (d / "result.txt").write_text(f"mutant {name}\nedits: {[(f, o[:50]) for f, o, _ in MUTANTS[name]]}\n"
                                  f"exit {r.returncode}\n{' / '.join(tail)}\nfailing tests:\n" + "\n".join(failed) + "\n",
                                  encoding="utf-8")
    print(f"{name:34} exit={r.returncode} {' / '.join(tail)}")
    for f in failed:
        print(f"      {f}")


if __name__ == "__main__":
    for n in (sys.argv[1:] or MUTANTS):
        run(n)
