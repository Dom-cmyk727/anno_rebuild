"""W4-WIN-VERIFY: mutation proofs for the C++ identity producer (dev_script.cpp), in place with a restore.

Each mutant edits src/app/src/dev_script.cpp, rebuilds anno_app_input_tests, runs its `dev_script` suite and records the
failing assertions; the file is restored from git after every mutant and the tree must be clean at the end."""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(r"E:\anno1503-worktrees\anno1503-W4-3")
SRC = ROOT / "src" / "app" / "src" / "dev_script.cpp"
EXE = ROOT / "build" / "win-a29b079" / "src" / "app" / "Release" / "anno_app_input_tests.exe"
OUT = ROOT / "cache" / "v" / "mut-cpp"
OUT.mkdir(parents=True, exist_ok=True)

MUTANTS = {
    "control": [],
    "identity-not-pushed": [("    script.log_.push_back(identity_line(script.steps_));\n", "")],
    "waits-dropped-from-identity": [("    for (const Step& s : steps) {\n        text += std::to_string(s.line)",
                                     "    for (const Step& s : steps) {\n        if (s.verb == \"wait\") {\n            continue;\n        }\n        text += std::to_string(s.line)")],
    "later-arguments-dropped-from-identity": [("        for (const std::string& a : s.args) {\n            text += \" \" + a;\n        }",
                                               "        for (const std::string& a : std::vector<std::string>(s.args.begin(), s.args.begin() + (s.args.empty() ? 0 : 1))) {\n            text += \" \" + a;\n        }")],
    "line-numbers-dropped-from-identity": [("        text += std::to_string(s.line) + \" \" + s.verb;", "        text += s.verb;")],
    "ran-records-first-argument-only": [("        for (const std::string& a : s.args) {\n            ran += \" \" + a;\n        }",
                                         "        if (!s.args.empty()) {\n            ran += \" \" + s.args[0];\n        }")],
}


def build():
    r = subprocess.run(["cmake", "--build", str(ROOT / "build" / "win-a29b079"), "--config", "Release", "--parallel", "12",
                        "--target", "anno_app_input_tests"], capture_output=True, text=True)
    return r.returncode, r.stdout[-600:]


def restore():
    subprocess.run(["git", "-C", str(ROOT), "checkout", "--", "src/app/src/dev_script.cpp"], check=True)


def main(names):
    env_game = r"E:\anno1503\game\app"
    for name in names:
        text = SRC.read_text(encoding="utf-8", newline="")
        crlf = "\r\n" in text
        t = text.replace("\r\n", "\n")
        for old, new in MUTANTS[name]:
            assert t.count(old) == 1, f"{name}: pattern not unique: {old[:60]!r}"
            t = t.replace(old, new)
        SRC.write_text(t.replace("\n", "\r\n") if crlf else t, encoding="utf-8", newline="")
        rc, tail = build()
        if rc != 0:
            line = f"{name:42} did NOT compile (rc {rc}) {tail[-200:]!r}"
        else:
            r = subprocess.run([str(EXE), "-ts=dev_script"], capture_output=True, text=True,
                               env={**__import__("os").environ, "ANNO1503_GAME_DIR": env_game})
            out = r.stdout + r.stderr
            failed = re.findall(r"dev_script_test\.cpp\((\d+)\): (?:FATAL )?ERROR: (CHECK[A-Z_]*|REQUIRE[A-Z_]*)\(", out)
            summary = [l for l in out.splitlines() if l.startswith("[doctest] test cases")]
            line = f"{name:42} exit={r.returncode} failing assertions at lines {sorted({int(n) for n, _ in failed})} {summary}"
            (OUT / f"{name}.txt").write_text(out, encoding="utf-8")
        restore()
        print(line, flush=True)
    rc, tail = build()
    status = subprocess.run(["git", "-C", str(ROOT), "status", "--short"], capture_output=True, text=True).stdout.strip()
    print(f"restored: rebuild rc={rc}; git status: {status or 'clean'}")


if __name__ == "__main__":
    main(sys.argv[1:] or list(MUTANTS))
