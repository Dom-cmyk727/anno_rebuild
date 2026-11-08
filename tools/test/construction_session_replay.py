#!/usr/bin/env python3
"""construction_session_replay.py: W4-UI-2 V2, the construction menu's full interaction proof
(construction_full_proof.script: every category tab, a locked-cell click, an armed placement that
builds, a demolish) replayed headlessly in a fresh process from its own recording, the same
mechanism opening_session.py already uses for the ship-start flow (--commands/--replay-hashes,
main.cpp's own replay_hashes - no window, no SDL).

    python -I tools/test/construction_session_replay.py <anno1503 executable>

A negative control proves the mechanism actually checks something: the same recording with the
build command's type changed must diverge at a recorded checkpoint (a different building placed
means a different state hash from the same turn on).
Exit: 0 pass, 1 fail, 2 bad arguments, 77 skipped (no game files)."""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
WIRING = HERE / "tools" / "test" / "viewer_wiring.py"
MAP = HERE / "data" / "maps" / "lan5.txt"
DATA_UI = HERE / "data" / "ui"


def run(cmd, env=None, timeout=600):
    p = subprocess.run(cmd, env=env, capture_output=True, text=True, encoding="utf-8",
                       errors="replace", timeout=timeout)
    return p.returncode, p.stdout, p.stderr


def main(argv):
    if len(argv) != 1:
        print(__doc__)
        return 2
    (exe,) = argv
    game_dir = os.environ.get("ANNO1503_GAME_DIR")
    if not game_dir:
        print("skipping: ANNO1503_GAME_DIR is not set (needs the original game files)")
        return 77
    if not Path(game_dir, "Anlagen.dat").is_file():
        print(f"skipping: {game_dir} is not the original game folder (no Anlagen.dat)")
        return 77

    with tempfile.TemporaryDirectory(prefix="anno_construction_replay_") as tmp:
        session = Path(tmp) / "session"
        app_args = ["--play", str(MAP), "--construction-menu", "on", "--layout", str(DATA_UI)]
        cmd = [sys.executable, "-I", str(WIRING), exe, "construction_full_proof", str(session)]
        for a in app_args:
            cmd += ["--app-arg", a]
        code, out, err = run(cmd, env=dict(os.environ, ANNO1503_GAME_DIR=game_dir))
        if code != 0:
            print(f"the window session itself failed (exit {code})\nstdout:\n{out}\nstderr:\n{err}")
            return 1

        commands = session / "commands.txt"
        hashes = session / "hashes.txt"
        if not commands.is_file() or not hashes.is_file():
            print(f"the session did not record {commands} / {hashes}")
            return 1

        replay_cmd = [exe, "--play", str(MAP), "--game-dir", game_dir, "--commands", str(commands),
                      "--replay-hashes", str(hashes)]
        code, out, err = run(replay_cmd, env=dict(os.environ))
        if code != 0:
            print(f"fresh-process replay disagreed (exit {code})\nstdout:\n{out}\nstderr:\n{err}")
            return 1
        print(f"fresh-process replay agrees: {err.strip()}")

        # Negative control: the same recording, but the built type is wrong (501 -> 502, a
        # different Anlagen id) - from the turn it is applied on, the state hash must differ.
        mutated_text = commands.read_text(encoding="utf-8").replace("build 501 ", "build 502 ")
        if mutated_text == commands.read_text(encoding="utf-8"):
            print("negative control did not change anything - commands.txt format moved")
            return 1
        mutated = Path(tmp) / "commands_mutated.txt"
        mutated.write_text(mutated_text, encoding="utf-8")
        neg_cmd = [exe, "--play", str(MAP), "--game-dir", game_dir, "--commands", str(mutated),
                  "--replay-hashes", str(hashes)]
        code, out, err = run(neg_cmd, env=dict(os.environ))
        if code == 0:
            print(f"negative control did NOT fail (it should have): {out.strip()}")
            return 1
        print(f"negative control correctly disagrees: {out.strip() or err.strip()}")

    print("construction_session_replay: pass")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
