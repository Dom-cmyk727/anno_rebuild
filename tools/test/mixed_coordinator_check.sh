#!/bin/bash
# mixed_coordinator_check.sh: one holder, one restart onto another coordinator build, one exit.
#
#   tools/test/mixed_coordinator_check.sh <first-coord> <successor-coord> <probe> <port> <work-dir> [<request-probe>]
#
# 1. <first-coord> listens on <port> with its own journal in <work-dir>.
# 2. <probe> (the holder; <request-probe> asks for the 80, default the same binary) holds 50 of 100 bytes (a client of the probe's own build: its compat version and
#    owner namespace are what it was built with).
# 3. <first-coord> is stopped and <successor-coord> starts on the SAME journal.
# 4. The holder is killed (only that pid).
# 5. A request for 80 is tried for ~12 s. A successor that recovers the dead owner admits it.
# Prints the binaries' hashes, the coordinators' LISTENING lines, the journal before and after,
# and "RESULT admitted=yes|no". Never touches the shared port or journal, and stops only the
# processes it started. Exit 0 when the request was admitted, 1 when not, 2 on a setup failure.
set -u
FIRST=$1; SUCC=$2; PROBE=$3; PORT=$4; DIR=$5; PROBE2=${6:-$3}
mkdir -p "$DIR" || exit 2
J="$DIR/mixed.journal"; B="$DIR/mixed.budget"; : > "$J"
echo "first     $(shasum -a 256 "$FIRST" | cut -c1-16) $FIRST"
echo "successor $(shasum -a 256 "$SUCC" | cut -c1-16) $SUCC"
echo "probe     $(shasum -a 256 "$PROBE" | cut -c1-16) $PROBE"
echo "request   $(shasum -a 256 "$PROBE2" | cut -c1-16) $PROBE2"
export ANNO_TEST_BUDGET_COORD=local ANNO_TEST_BUDGET_PORT=$PORT ANNO_TEST_BUDGET_ID=mixed \
       ANNO_TEST_BUDGET_RECHECK_MS=100 ANNO_TEST_BUDGET_SPAWN=0 ANNO_TEST_ADMIT_WAIT_S=20
wait_for() { # file pattern seconds
    for _ in $(seq 1 $(($3 * 10))); do grep -q "$2" "$1" 2>/dev/null && return 0; sleep 0.1; done; return 1
}
C1= ; C2= ; HP=
cleanup() { for p in $HP $C1 $C2; do [ -n "$p" ] && kill "$p" 2>/dev/null; done; exec 7>&-; }
trap cleanup EXIT
"$FIRST" "$PORT" local --journal "$J" > "$DIR/coord1.out" 2>&1 & C1=$!
wait_for "$DIR/coord1.out" LISTENING 15 || { echo "setup: first coordinator did not listen"; exit 2; }
echo "first LISTENING: $(grep -m1 LISTENING "$DIR/coord1.out")"
rm -f "$DIR/hold.fifo"; mkfifo "$DIR/hold.fifo"; exec 7<>"$DIR/hold.fifo"
"$PROBE" holder "$B" 50 <&7 > "$DIR/holder.out" 2>&1 & HP=$!
wait_for "$DIR/holder.out" "READY 1" 20 || { echo "setup: holder not admitted: $(cat "$DIR/holder.out")"; exit 2; }
echo "journal with the live holder:"; sed 's/^/  /' "$J"
kill "$C1"; wait "$C1" 2>/dev/null; C1=
"$SUCC" "$PORT" local --journal "$J" > "$DIR/coord2.out" 2>&1 & C2=$!
wait_for "$DIR/coord2.out" LISTENING 15 || { echo "setup: successor did not listen"; exit 2; }
echo "successor LISTENING: $(grep -m1 LISTENING "$DIR/coord2.out")"
kill -KILL "$HP"; wait "$HP" 2>/dev/null; HP=
echo "holder killed; journal now:"; sed 's/^/  /' "$J"
ok=no
for i in $(seq 1 24); do
    line=$("$PROBE2" try "$B" 80 2>&1 | grep -m1 'claim=')
    echo "try $i: $line"
    case "$line" in *claim=1*) ok=yes; break;; esac
    sleep 0.5
done
echo "journal at the end:"; sed 's/^/  /' "$J"
echo "RESULT admitted=$ok"
[ "$ok" = yes ]
