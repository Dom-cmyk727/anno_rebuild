#!/usr/bin/env python3
"""selected_house_session_replay.py: W4-NEXT-2 K's own scripted session
(selected_house_draw_check.script: build, select, deselect, select another, demolish while
selected) replayed headlessly in a fresh process from its own recording - the same mechanism
construction_session_replay.py uses for construction_full_proof.script. Selection is viewer-only
state (selection.hpp's own doc comment: "selecting and deselecting writes no sim state"), so this
checks only what the recording actually carries: the two builds and the demolish, by their
resulting state hashes - not the click-to-select UI path, which a fresh headless replay (no
window, no SDL) cannot exercise at all.

    python -I tools/test/selected_house_session_replay.py <anno1503 executable>

A negative control proves the mechanism actually checks something: the same recording with the
second build's type changed must diverge at a recorded checkpoint.
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

    with tempfile.TemporaryDirectory(prefix="anno_selected_house_replay_") as tmp:
        session = Path(tmp) / "session"
        app_args = ["--play", str(MAP), "--construction-menu", "on", "--layout", str(DATA_UI)]
        cmd = [sys.executable, "-I", str(WIRING), exe, "selected_house_draw_check", str(session)]
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

        # Negative control: the same recording, but the second build's type is wrong (501 -> 502)
        # - from the turn it is applied on, the state hash must differ. The first build's own
        # "build 501" also matches this text; replace only the LAST occurrence (the second house),
        # since mutating both would still place two houses, just of a different, still-consistent
        # type, and might not diverge where the demolish-while-selected step expects type 501.
        original = commands.read_text(encoding="utf-8")
        idx = original.rfind("build 501 ")
        if idx < 0:
            print("negative control did not find a second build to mutate")
            return 1
        mutated_text = original[:idx] + "build 502 " + original[idx + len("build 501 "):]
        mutated = Path(tmp) / "commands_mutated.txt"
        mutated.write_text(mutated_text, encoding="utf-8")
        neg_cmd = [exe, "--play", str(MAP), "--game-dir", game_dir, "--commands", str(mutated),
                  "--replay-hashes", str(hashes)]
        code, out, err = run(neg_cmd, env=dict(os.environ))
        if code == 0:
            print(f"negative control did NOT fail (it should have): {out.strip()}")
            return 1
        print(f"negative control correctly disagrees: {out.strip() or err.strip()}")

    print("selected_house_session_replay: pass")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
