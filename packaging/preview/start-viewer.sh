#!/bin/sh
# ANNO 1503 rebuild - island viewer (preview build @NAME@), Linux x86_64.
# Needs an installed/extracted ANNO 1503 (GOG "A.D." 2.0.0.5); the folder must
# contain Anlagen.dat.
# Usage: ./start-viewer.sh [<game folder> [<scenario or island file>]]
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
scene=${2:-"$game/Scenes/0200Single/Single_01.sze"}
exec "$here/linux/anno1503" --view "$scene" --game-dir "$game"
