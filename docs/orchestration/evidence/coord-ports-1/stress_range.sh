#!/bin/bash
# stress_range.sh <build-dir> <log> <range-lo> <range-hi> [ctest args...]
# Narrows ip_local_port_range for one ctest run of anno_test_support_tests, then restores it.
set -u
BUILD=$1; LOG=$2; LO=$3; HI=$4; shift 4
ORIG=$(cat /proc/sys/net/ipv4/ip_local_port_range)
restore() { echo "$ORIG" > /proc/sys/net/ipv4/ip_local_port_range; echo "restored: $(cat /proc/sys/net/ipv4/ip_local_port_range | tr '\t' ' ')"; }
trap restore EXIT
while pgrep -f anno_budget_coord >/dev/null; do sleep 2; done
echo "$LO $HI" > /proc/sys/net/ipv4/ip_local_port_range || exit 2
echo "range during run: $(cat /proc/sys/net/ipv4/ip_local_port_range | tr '\t' ' ')"
cd "$BUILD" && ctest -R '^anno_test_support_tests\.' -j 4 --timeout 600 -O "$LOG" "$@" > /dev/null 2>&1
echo "ctest exit $?"
grep -E "tests passed|\(Failed\)|\(Timeout\)" "$LOG"
