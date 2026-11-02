#!/usr/bin/env python3
"""viewer_session.py: does a play-window session record prove that its script ran? (W4-WIN-FINAL F1)

    python -I tools/test/viewer_session.py <script file> <session dir>      # prints PASS or the problems
    python -I tools/test/viewer_session.py --revalidate <viewer_wiring script dir> <session dir>...

The window (src/app/src/main.cpp, write_session) saves the scripted session's log to <session>/script.log:
one `ran line N: VERB[ ARG0]` record for every step the script executed except `wait` (a wait
ends silently), `checkpoint NAME: ...` lines, and `FAIL line N (VERB): ...` for a failed step. This module
compares that record with the script that was invoked: the log must exist and not be empty, hold exactly the
script's non-wait steps in the script's order (line number, verb and first argument), end with the script's
last step, and hold no FAIL line. A child that exits 0 without producing such a record has not run the
script, whatever its exit status says. The comparison reads the script the way DevScript::parse does
(`#` starts a comment, a step is the first word and the words after it)."""
import re
import sys
from pathlib import Path

RAN = re.compile(r"^ran line (\d+): (\S+)(?: (\S+))?")


def script_steps(text):
    """[(line number, verb, first argument or None)] of the script's steps, as DevScript::parse reads them."""
    steps = []
    for number, line in enumerate(text.splitlines(), start=1):
        words = line.split("#", 1)[0].split()
        if words:
            steps.append((number, words[0], words[1] if len(words) > 1 else None))
    return steps


def expected_records(text):
    """The ran-records a complete execution leaves: every step but `wait`, in order."""
    return [(n, v, a) for n, v, a in script_steps(text) if v != "wait"]


def problems(script_text, log_text):
    """What is wrong with a session log (None: the file is absent) against the script that was invoked."""
    if log_text is None:
        return ["no script.log in the session directory: the window did not record a script run"]
    lines = log_text.splitlines()
    if not any(line.strip() for line in lines):
        return ["script.log is empty"]
    found = []
    out = []
    for line in lines:
        if line.startswith("FAIL"):
            out.append("a step failed: " + line[:200])
        m = RAN.match(line)
        if m:
            found.append((int(m.group(1)), m.group(2), m.group(3)))
    want = expected_records(script_text)
    if not want:
        out.append("the invoked script has no steps")
    elif want[-1][1] != "quit":
        out.append("the invoked script does not end with quit")
    for i, (w, f) in enumerate(zip(want, found)):
        if w != f:
            out.append(f"record {i + 1} is {f}, the script's step is {w} (the log is not of this script, or out of order)")
            break
    else:
        if len(found) < len(want):
            out.append(f"truncated: {len(found)} of the script's {len(want)} non-wait steps were recorded; "
                       f"the next is {want[len(found)]}")
        elif len(found) > len(want):
            out.append(f"{len(found) - len(want)} more records than the script has steps")
    if want and not out and found[-1] != want[-1]:
        out.append("the last step of the script was not recorded")
    return out


def check_session(script_path, session_dir):
    log = Path(session_dir) / "script.log"
    text = log.read_text(encoding="utf-8", errors="replace") if log.is_file() else None
    return problems(Path(script_path).read_text(encoding="utf-8"), text)


def main(argv):
    if len(argv) >= 3 and argv[0] == "--revalidate":
        scripts, bad = Path(argv[1]), 0
        for session in argv[2:]:
            name = Path(session).name
            # The case a session ran: from its script.log's first record against the known scripts.
            log = Path(session) / "script.log"
            text = log.read_text(encoding="utf-8", errors="replace") if log.is_file() else None
            match = [s for s in sorted(scripts.glob("*.script")) if not problems(s.read_text(encoding="utf-8"), text)]
            if match:
                print(f"VALID    {session}  = {match[0].name}")
            else:
                bad += 1
                print(f"INVALID  {session}: no script in {scripts} matches this log")
        return 1 if bad else 0
    if len(argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    found = check_session(argv[0], argv[1])
    print("PASS" if not found else "\n".join(found))
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
