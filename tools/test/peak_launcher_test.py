#!/usr/bin/env python3
"""Contract of the opt-in peak-memory launcher (W4-NEXT-2 part M, C-3).

    peak_launcher_test.py <anno_peak_launcher> <anno_peak_alloc>

1. It reports a known allocation: the peak of a 256 MiB child minus the peak of a 0 MiB child
   is within 10 % of 256 MiB (of 288 MiB when the helper is an AddressSanitizer build: its
   shadow memory, one byte per eight, is resident too).
2. It changes no result: for an exit of 0, an exit of 7 and an abort, the launched run has the
   same exit status, byte-identical stdout (arguments with spaces, quotes and backslashes
   included) and the same stderr apart from exactly one appended "[anno-test peak]" line, whose
   exit= field agrees.
3. Its own cost: the median wall time of 7 launched runs minus 7 direct runs is printed (not a
   pass condition); a launch that adds more than 2 s fails.
4. The test name is the value of a --test-case= argument.

Exit 0 when every check passed, 1 otherwise.
"""
import os
import re
import statistics
import subprocess
import sys
import time

LINE = re.compile(
    r"^\[anno-test peak\] peak_bytes=(\d+) source=(\S+) exit=(-?\d+) wall_ms=(\d+) test=(.*)$")
MIB = 1024 * 1024


def run(argv):
    start = time.perf_counter()
    done = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=120)
    return done, time.perf_counter() - start


def split_peak(stderr):
    """(stderr without peak lines, list of parsed peak lines)."""
    text = stderr.decode("utf-8", "replace").replace("\r\n", "\n")
    kept, peaks = [], []
    for line in text.split("\n"):
        m = LINE.match(line)
        if m:
            peaks.append(m)
        elif line.startswith("[anno-test peak]"):
            peaks.append(None)  # malformed
        else:
            kept.append(line)
    return "\n".join(kept), peaks


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    launcher, alloc = sys.argv[1], sys.argv[2]
    failures = []

    def check(ok, what):
        print(("ok: " if ok else "FAIL: ") + what)
        if not ok:
            failures.append(what)

    # 1. A known allocation. An AddressSanitizer build of the helper also makes its shadow
    # (1 byte per 8) resident: it says so, and the expected difference is then 9/8 of it.
    peaks = {}
    asan = False
    for mib in (0, 256):
        done, _ = run([launcher, alloc, str(mib)])
        asan = asan or b"build asan" in done.stdout
        _, lines = split_peak(done.stderr)
        check(done.returncode == 0 and len(lines) == 1 and lines[0] is not None,
              "%d MiB child: exit 0 and one well-formed peak line" % mib)
        if lines and lines[0] is not None:
            peaks[mib] = int(lines[0].group(1))
    if len(peaks) == 2:
        delta = peaks[256] - peaks[0]
        expected = 256 * MIB * (9 / 8 if asan else 1)
        print("peak 0 MiB child: %d bytes, 256 MiB child: %d bytes, difference %.1f MiB"
              " (helper build: %s, expected %.0f MiB)"
              % (peaks[0], peaks[256], delta / MIB, "asan" if asan else "plain",
                 expected / MIB))
        check(abs(delta - expected) <= 0.10 * expected,
              "difference within 10 %% of %.0f MiB (%.1f MiB)" % (expected / MIB, delta / MIB))

    # 2. No result change.
    echo = ["with space", 'with "quote"', "trailing\\", "back\\\\slash \\\"mix", ""]
    cases = [("exit 0", ["0", "0"] + echo), ("exit 7", ["0", "7"] + echo),
             ("abort", ["0", "abort"])]
    for what, args in cases:
        direct, _ = run([alloc] + args)
        launched, _ = run([launcher, alloc] + args)
        rest, lines = split_peak(launched.stderr)
        direct_err, _ = split_peak(direct.stderr)
        check(launched.returncode == direct.returncode,
              "%s: exit status %r launched, %r direct" % (what, launched.returncode,
                                                          direct.returncode))
        check(launched.stdout == direct.stdout, "%s: stdout byte-identical" % what)
        check(rest == direct_err, "%s: stderr identical apart from the peak line" % what)
        check(len(lines) == 1 and lines[0] is not None, "%s: exactly one peak line" % what)
        if what == "abort" and direct.returncode == 0:
            check(False, "abort: the helper did not fail")
        if lines and lines[0] is not None and what != "abort":
            check(int(lines[0].group(3)) == direct.returncode,
                  "%s: the peak line's exit= is the child's" % what)

    # 3. Its own cost.
    direct_t, launched_t = [], []
    for _ in range(7):
        direct_t.append(run([alloc, "0"])[1])
        launched_t.append(run([launcher, alloc, "0"])[1])
    added = statistics.median(launched_t) - statistics.median(direct_t)
    print("median wall: direct %.1f ms, launched %.1f ms, added %.1f ms"
          % (statistics.median(direct_t) * 1e3, statistics.median(launched_t) * 1e3, added * 1e3))
    check(added < 2.0, "the launcher adds less than 2 s per run")

    # 4. The test name.
    done, _ = run([launcher, alloc, "0", "0", "--test-case=a name, with: punctuation"])
    _, lines = split_peak(done.stderr)
    check(len(lines) == 1 and lines[0] is not None
          and lines[0].group(5) == "a name, with: punctuation",
          "test= is the --test-case= value")
    done, _ = run([launcher, alloc, "0"])
    _, lines = split_peak(done.stderr)
    base = os.path.basename(alloc)
    check(len(lines) == 1 and lines[0] is not None and lines[0].group(5) == base,
          "test= is the program's file name without --test-case=")

    # 5. A program that cannot start is the launcher's own failure (125), not the child's.
    missing = os.path.join(os.path.dirname(os.path.abspath(alloc)), "no-such-program-peak")
    done, _ = run([launcher, missing])
    check(done.returncode == 125, "a missing program ends with 125 (got %r)" % done.returncode)

    print("RESULT %s (%d failure(s))" % ("PASS" if not failures else "FAIL", len(failures)))
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
