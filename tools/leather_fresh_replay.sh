#!/bin/sh
# MAP-NEEDS-CLOSE-EXTRACT-1 C2: produce the four leather-fixture replay logs from the CURRENT test
# binary, then replay each in a fresh anno-sim-replay process and check the dispositions.
# usage: tools/leather_fresh_replay.sh <anno_sim_tests> <anno-sim-replay> <out-dir>
# needs ANNO1503_GAME_DIR. Exit 0 only if: genuine reproduces (9/9), changed legal command diverges
# at entry 101, changed checkpoint diverges at entry 505, refused-command log reproduces (1/1).
set -eu
tests="$1"; replay="$2"; out="$3"
mkdir -p "$out"
rm -f "$out"/leather_fixture*.log
ANNO_POP_LIVE_OUT_DIR="$out" "$tests" -tc="synthetic mechanics fixture: recorded log*,synthetic mechanics fixture: refused*" >/dev/null
fail=0
check() { # name expected_exit expected_text
  r=$("$replay" run "$out/$1.log" 2>&1) && rc=0 || rc=$?
  echo "== $1 sha256 $(shasum -a 256 "$out/$1.log" | cut -c1-16)"; echo "$r"; echo "exit $rc"
  [ "$rc" = "$2" ] || { echo "UNEXPECTED exit"; fail=1; }
  echo "$r" | grep -q "$3" || { echo "UNEXPECTED text (wanted: $3)"; fail=1; }
}
check leather_fixture 0 "checkpoints 9/9"
check leather_fixture_changed_command 1 "DIVERGED at entry 101"
check leather_fixture_changed_checkpoint 1 "DIVERGED at entry 505"
check leather_fixture_refused 0 "checkpoints 1/1"
exit $fail
