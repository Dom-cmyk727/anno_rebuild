#!/usr/bin/env python3
"""viewer_session.py: does a play-window session record prove that its script ran? (W4-WIN-FINAL F1, W4-WIN-VERIFY V2)

    python -I tools/test/viewer_session.py <script file> <session dir>      # prints PASS or the problems
    python -I tools/test/viewer_session.py --revalidate <viewer_wiring script dir> <session dir>...

The window (src/app/src/main.cpp, write_session) saves the scripted session's log to <session>/script.log:
  * `script identity: steps=N fnv1a64=HEX`, the first line: the window's own record of the script it parsed
    (DevScript::identity_line): the number of steps and FNV-1a 64 over the canonical text, one
    `LINE VERB ARG...` line per step after comments and whitespace are gone, `wait` steps and every argument
    included;
  * `ran line N: VERB ARG...` for every step the script executed except `wait` (a wait ends silently);
  * `checkpoint NAME: ...` lines, and `FAIL line N (VERB): ...` for a failed step.
This module compares that record with the script that was invoked: the log must exist and not be empty, carry an
identity equal to the invoked script's, hold exactly the script's non-wait steps in the script's order (line
number, verb and every argument), end with the script's last step, and hold no FAIL line. A child that exits 0
without producing such a record has not run the script, whatever its exit status says. The comparison reads the
script the way DevScript::parse does, byte for byte (lines end at LF, `#` starts a comment, a step is the first
word and the words after it, ASCII whitespace separates the words).

A log without the identity line is LEGACY evidence (recorded before W4-WIN-VERIFY: first argument only, no `wait`
values): `problems(..., legacy_ok=True)` and `--revalidate` check it in the reduced form and name it legacy; they
never report it as a verified full identity. The wrapper (viewer_wiring.py) accepts only the full identity: the
binary it runs records it.

This detects accidental mismatch (the wrong script revision, a truncated or reordered run); it does not
authenticate the window that wrote the log."""
import re
import sys
from pathlib import Path

IDENTITY_PREFIX = "script identity:"
IDENTITY = re.compile(r"^script identity: steps=(\d+) fnv1a64=([0-9a-f]{16})$")
RAN = re.compile(r"^ran line (\d+): (.*)$")


def _bytes(script):
    return bytes(script) if isinstance(script, (bytes, bytearray)) else script.encode("utf-8")


def script_steps(script):
    """[(line number, verb, [arguments])] of the script's steps, as DevScript::parse reads them: lines end at LF,
    `#` starts a comment, a step is the first word and the words after it (bytes.split() splits on exactly the
    ASCII whitespace the parser's stream does). An `expect title` text stays words joined by one space, which is
    how the window prints it as well."""
    steps = []
    for number, raw in enumerate(_bytes(script).split(b"\n"), start=1):
        words = [w.decode("utf-8", errors="surrogateescape") for w in raw.split(b"#", 1)[0].split()]
        if words:
            steps.append((number, words[0], words[1:]))
    return steps


def canonical_text(steps):
    """The text the identity hashes: `LINE VERB ARG...` and a newline for every step, wait steps included."""
    return "".join(" ".join([str(n), v] + a) + "\n" for n, v, a in steps)


def fnv1a64(data):
    h = 0xCBF29CE484222325
    for byte in data:
        h = ((h ^ byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def identity_line(script):
    steps = script_steps(script)
    data = canonical_text(steps).encode("utf-8", errors="surrogateescape")
    return f"{IDENTITY_PREFIX} steps={len(steps)} fnv1a64={fnv1a64(data):016x}"


def expected_records(script, full=True):
    """The ran-records a complete execution leaves: every step but `wait`, in order, as (line, `VERB ARG...`).
    full=False: the reduced legacy form, the first argument only."""
    return [(n, " ".join([v] + (a if full else a[:1]))) for n, v, a in script_steps(script) if v != "wait"]


def problems(script, log_text, legacy_ok=False):
    """What is wrong with a session log (None: the file is absent) against the script that was invoked.
    legacy_ok: a log without the identity line is checked in its reduced legacy form instead of refused."""
    if log_text is None:
        return ["no script.log in the session directory: the window did not record a script run"]
    lines = log_text.splitlines()
    if not any(line.strip() for line in lines):
        return ["script.log is empty"]
    out = []
    identities = [line for line in lines if line.startswith(IDENTITY_PREFIX)]
    full = bool(identities)
    if not full and not legacy_ok:
        out.append("no `script identity:` line in script.log: the record does not identify the whole script the "
                   "window parsed (a legacy record, or a window without the full identity)")
    elif full:
        first = next(line for line in lines if line.strip())
        want_id = identity_line(script)
        if len(identities) != 1 or first != identities[0]:
            out.append(f"the record must open with exactly one `{IDENTITY_PREFIX}` line, found {len(identities)}")
        elif IDENTITY.match(identities[0]) is None:
            out.append(f"malformed identity line: {identities[0][:120]!r}")
        elif identities[0] != want_id:
            out.append(f"script identity mismatch: the window parsed [{identities[0][len(IDENTITY_PREFIX):].strip()}], "
                       f"the invoked script is [{want_id[len(IDENTITY_PREFIX):].strip()}] "
                       "(a different script, or one changed after the window read it)")
    found = []
    for line in lines:
        if line.startswith("FAIL"):
            out.append("a step failed: " + line[:200])
        m = RAN.match(line)
        if m:
            found.append((int(m.group(1)), m.group(2)))
    want = expected_records(script, full=full)
    if not want:
        out.append("the invoked script has no steps")
    elif want[-1][1].split(" ")[0] != "quit":
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


def check_session(script_path, session_dir, legacy_ok=False):
    log = Path(session_dir) / "script.log"
    text = log.read_text(encoding="utf-8", errors="replace") if log.is_file() else None
    return problems(Path(script_path).read_bytes(), text, legacy_ok=legacy_ok)


def main(argv):
    if len(argv) >= 3 and argv[0] == "--revalidate":
        scripts, bad = Path(argv[1]), 0
        for session in argv[2:]:
            log = Path(session) / "script.log"
            text = log.read_text(encoding="utf-8", errors="replace") if log.is_file() else None
            legacy = text is not None and not any(l.startswith(IDENTITY_PREFIX) for l in text.splitlines())
            match = [s for s in sorted(scripts.glob("*.script")) if not problems(s.read_bytes(), text, legacy_ok=True)]
            if match and legacy:
                print(f"LEGACY   {session}  = {match[0].name}  (checked: the ordered first-argument trace only; the "
                      "record has no full script identity, so wait values and later arguments are not verified)")
            elif match:
                print(f"VALID    {session}  = {match[0].name}  (full script identity and ordered full-argument trace)")
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
