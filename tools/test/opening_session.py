#!/usr/bin/env python3
"""opening_session.py: the complete ship-start opening through the production window, then its replay
(W4-OPENING-1 I5).

    python -I tools/test/opening_session.py <anno1503 executable> [<new work dir>] [--gpu] [--adapter N]

Stage 1 launches `--ship-start lan5-ship-a --seat 0`, selects the seat's ship with the pointer, orders it
to the coast of the first island (about 550 tiles), waits for the sim's own arrival event and quick-saves.
Stage 2 resumes that save and, with the pointer and keys only, tries a founding the sim refuses (twice),
founds the warehouse, unloads every hold of the ship (the last one limited by the full store, then refused
for it), and builds HANDWERK and PIONIER with the building tool, checking the money and goods ledger at each
step (tools/test/viewer_wiring/opening_sail.script, opening_resume.script; no grant, no relocation).
Each window runs through tools/test/viewer_wiring.py, so a successful exit alone is not a pass.
Stage 3 resumes the save stage 2 wrote after its builds (opening_resume2.script: stores, hold, ship and building tool).
Then each recording (session/commands.txt, hashes.txt, outcomes.txt, session.txt) is replayed in a fresh
process with the same manifest, seat, rules and start state (`--replay-hashes`): every checkpoint and every
outcome must agree; the same recording with one unload's quantity changed (and another's goods) must
diverge at a recorded checkpoint. Exit: 0 pass, 1 fail, 2 bad arguments, 77 skipped (no game files)."""
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
WIRING = HERE / "tools" / "test" / "viewer_wiring.py"
MANIFEST = HERE / "data" / "maps" / "candidates" / "ship-start-v2" / "lan5-ship-a.json"


def run(cmd, env=None, timeout=3000):
    p = subprocess.run(cmd, env=env, capture_output=True, text=True, encoding="utf-8", errors="replace",
                       timeout=timeout)
    return p.returncode, p.stdout, p.stderr


def wiring(exe, case, out, flags, extra_app=()):
    app = ["--ship-start", str(MANIFEST), "--seat", "0", "--speed", "200", *extra_app]
    cmd = [sys.executable, "-I", str(WIRING), exe, case, str(out), *flags]
    for a in app:
        cmd += ["--app-arg", a]
    return run(cmd)


def replay(exe, session, resume=None):
    cmd = [exe, "--ship-start", str(MANIFEST), "--seat", "0", "--speed", "200",
           "--commands", str(session / "commands.txt"), "--replay-hashes", str(session / "hashes.txt")]
    if resume:
        cmd += ["--resume", str(resume)]
    return run(cmd, env=dict(os.environ))


def main(argv):
    flags, pos = [], []
    it = iter(argv)
    for a in it:
        if a == "--gpu":
            flags.append(a)
        elif a == "--adapter":
            flags += [a, next(it, "0")]
        else:
            pos.append(a)
    if not pos or not Path(pos[0]).is_file():
        print(__doc__, file=sys.stderr)
        return 2
    exe = str(Path(pos[0]).resolve())
    if not os.environ.get("ANNO1503_GAME_DIR"):
        print("skipping: ANNO1503_GAME_DIR is not set (the opening needs the original game files)")
        return 77
    work = Path(pos[1]) if len(pos) > 1 else Path(tempfile.mkdtemp(prefix="opening_session_"))
    work.mkdir(parents=True, exist_ok=True)
    failures = []

    def check(cond, text):
        print(("ok   " if cond else "FAIL ") + text)
        if not cond:
            failures.append(text)
        return cond

    # ---- stage 1: launch, select, sail, arrive, save ------------------------------------------
    a = work / "stage1" / "run"
    rc, out, err = wiring(exe, "opening_sail", a, flags)
    print(out.strip().splitlines()[-1] if out.strip() else err)
    if not check(rc == 0 and "PASS opening_sail" in out, "stage 1 (launch, select, sail, arrive, save) passed the wrapper"):
        print(out[-3000:])
        return 1
    save = a.parent / "run.saves" / "quicksave-seat0.sav"
    if not check(save.is_file() and save.stat().st_size > 1000, f"stage 1 wrote its save ({save})"):
        return 1
    kept = work / "stage1" / "arrived.sav"
    shutil.copy(save, kept)

    # ---- stage 2: resume, found, unload, build, ledger ----------------------------------------
    b = work / "stage2" / "run"
    rc, out, err = wiring(exe, "opening_resume", b, flags, ("--resume", str(kept)))
    print(out.strip().splitlines()[-1] if out.strip() else err)
    if not check(rc == 0 and "PASS opening_resume" in out, "stage 2 (resume, found, unload, build, ledger) passed the wrapper"):
        print(out[-3000:])
        return 1

    # ---- stage 3: the save stage 2 wrote after founding, unloading and building ---------------------
    save2 = b.parent / "run.saves" / "quicksave-seat0.sav"
    if not check(save2.is_file() and save2.stat().st_size > 1000, f"stage 2 wrote its save ({save2})"):
        return 1
    kept2 = work / "stage2" / "built.sav"
    shutil.copy(save2, kept2)
    c = work / "stage3" / "run"
    rc, out, err = wiring(exe, "opening_resume2", c, flags, ("--resume", str(kept2)))
    print(out.strip().splitlines()[-1] if out.strip() else err)
    if not check(rc == 0 and "PASS opening_resume2" in out,
                 "stage 3 (resume the built city: stores, hold, ship, building tool) passed the wrapper"):
        print(out[-3000:])
        return 1

    # ---- the recordings: what the window issued, replayed in a fresh process -------------------
    for name, session, resume in (("stage 1", a, None), ("stage 2", b, kept)):
        commands = (session / "commands.txt").read_text(encoding="utf-8")
        check(commands.strip() != "", f"{name}: the window recorded its commands")
        rc, out, err = replay(exe, session, resume)
        agree = re.search(r"(\d+) checkpoints agree.*?(\d+) outcomes agree", err)
        check(rc == 0 and agree is not None, f"{name}: a fresh process replays every checkpoint and outcome ({err.strip().splitlines()[-1] if err.strip() else rc})")
    cmd2 = (b / "commands.txt").read_text(encoding="utf-8").splitlines()
    kinds = [line.split()[2] for line in cmd2]
    check(kinds[:1] == ["found"] or "found" in kinds, "stage 2 issued a FoundCmd through the window")
    check(kinds.count("unload") >= 4, f"stage 2 issued its unloads through the window ({kinds.count('unload')})")
    check(kinds.count("buildc") == 2, "stage 2 issued two builds through the window")
    cmd1 = (a / "commands.txt").read_text(encoding="utf-8").splitlines()
    check(any(line.split()[2] == "move" for line in cmd1), "stage 1 issued its MoveCmd through the window")

    # ---- a changed command diverges at a recorded checkpoint -----------------------------------
    for label, edit in (("one ton fewer unloaded", lambda f: f[:-1] + [str(int(f[-1]) - 1)]),
                        ("another goods unloaded", lambda f: f[:-2] + [str(int(f[-2]) + 1), f[-1]])):
        tampered = work / "tampered" / label.replace(" ", "_")
        shutil.rmtree(tampered, ignore_errors=True)
        tampered.mkdir(parents=True)
        for name in ("hashes.txt", "outcomes.txt", "session.txt"):
            shutil.copy(b / name, tampered / name)
        changed, lines = False, []
        for line in cmd2:
            fields = line.split()
            if not changed and fields[2] == "unload":
                fields = edit(fields)
                changed = True
            lines.append(" ".join(fields))
        (tampered / "commands.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
        rc, out, err = replay(exe, tampered, kept)
        check(changed and rc == 1 and "differs" in err, f"the recording with {label} diverges ({err.strip().splitlines()[-1] if err.strip() else rc})")

    # ---- a recording is only replayed against what it was made from -------------------------------
    def broken(label, mutate, expect):
        d = work / "refused" / label.replace(" ", "_")
        shutil.rmtree(d, ignore_errors=True)
        d.mkdir(parents=True)
        for name in ("commands.txt", "hashes.txt", "outcomes.txt", "session.txt"):
            shutil.copy(b / name, d / name)
        mutate(d)
        rc, out, err = replay(exe, d, kept)
        check(rc == 1 and expect in err, f"the replay refuses a recording {label} ({err.strip().splitlines()[-1] if err.strip() else rc})")

    def edit_session(prefix, new):
        def run(d):
            lines = (d / "session.txt").read_text(encoding="utf-8").splitlines()
            hit = [i for i, line in enumerate(lines) if line.startswith(prefix + " ")]
            assert len(hit) == 1, (prefix, hit)
            lines[hit[0]] = prefix + " " + new
            with open(d / "session.txt", "w", encoding="utf-8", newline="\n") as out:
                out.write("\n".join(lines) + "\n")
        return run

    manifest_hash = re.search(r"^manifest-hash (\S+)$", (b / "session.txt").read_text(encoding="utf-8"), re.M).group(1)
    broken("of another manifest", edit_session("manifest-hash", manifest_hash[:-1] + ("0" if manifest_hash[-1] != "0" else "1")),
           "another manifest")
    broken("of another seat", edit_session("seat", "1"), "another seat")
    broken("of other rules", edit_session("rules-digest", "0000000000000000"), "another rules")
    broken("of another start state", edit_session("start-hash", "0000000000000000"), "another start state")
    broken("without its session.txt", lambda d: (d / "session.txt").unlink(), "no session.txt")
    broken("that names no ship-start session",
           lambda d: (d / "session.txt").write_text("map something.txt\nspeed 200\n", encoding="utf-8"),
           "names no ship-start session")
    print(f"session: {work}")
    if failures:
        print(f"opening_session: FAIL ({len(failures)}): " + "; ".join(failures))
        return 1
    print("opening_session: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
