#!/bin/sh
# ANNO 1503 rebuild - frame benchmark (preview build @NAME@), Linux x86_64.
# Measures the viewer's frame pipeline (GPU and CPU) without a window and
# writes benchmark-results.md next to this script. Takes 1-3 minutes.
# Usage: ./run-benchmark.sh [<game folder>]
set -e
here=$(cd "$(dirname "$0")" && pwd)
game=${1:-$ANNO1503_GAME_DIR}
if [ -z "$game" ]; then
  printf 'Path of your ANNO 1503 folder (contains Anlagen.dat): '
  read -r game
fi
if [ ! -f "$game/Anlagen.dat" ]; then
  echo "Anlagen.dat not found in \"$game\"." >&2
  exit 1
fi
out="$here/benchmark-results.md"
{
  echo "CPU: $(grep -m1 'model name' /proc/cpuinfo 2>/dev/null | cut -d: -f2-), $(nproc 2>/dev/null) threads"
  echo
  "$here/linux/bench_frame" "$game" "$game/Scenes/0200Single/Single_01.sze" \
    --res 1920x1080,2560x1440,3840x2160 --renderer gpu,cpu --chunks async --check --quick
} > "$out" 2>&1
echo "Done: $out"
