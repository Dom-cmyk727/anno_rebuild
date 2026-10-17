#!/bin/sh
# Builds share/anno1503-preview-<name>.zip: Windows and Linux x86_64 binaries
# (anno1503, anno-extract, anno-dump), start scripts, README and a source
# snapshot of HEAD. No game files are included.
#
# Cross-compiles on any host with:
#   - rustup targets x86_64-pc-windows-gnu and x86_64-unknown-linux-gnu
#   - mingw-w64 (Windows linker), zig + cargo-zigbuild (Linux, glibc 2.28)
#
# Usage: packaging/package-preview.sh <name>      e.g. 2026-10-17b
set -eu
name=${1:?usage: packaging/package-preview.sh <name>}
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
if [ -n "$(git status --porcelain -- crates Cargo.toml Cargo.lock)" ]; then
  echo "uncommitted changes in crates/; commit first so the package matches its commit" >&2
  exit 1
fi
commit=$(git rev-parse --short HEAD)

cargo build --release --target x86_64-pc-windows-gnu -p anno1503 -p anno-tools
cargo zigbuild --release --target x86_64-unknown-linux-gnu.2.28 -p anno1503 -p anno-tools

dir=anno1503-preview-$name
out=target/package/$dir
rm -rf "$out"
mkdir -p "$out/windows" "$out/linux"
for b in anno1503 anno-extract anno-dump; do
  cp "target/x86_64-pc-windows-gnu/release/$b.exe" "$out/windows/"
  cp "target/x86_64-unknown-linux-gnu/release/$b" "$out/linux/"
  chmod +x "$out/linux/$b"
done
for f in README.txt start-viewer.sh start-viewer.bat; do
  sed "s/@NAME@/$name/g; s/@COMMIT@/$commit/g" "packaging/preview/$f" > "$out/$f"
done
chmod +x "$out/start-viewer.sh"
git archive --format=zip -o "$out/anno1503-source.zip" HEAD -- . ':!share'

mkdir -p share
rm -f "share/$dir.zip"
(cd target/package && zip -qr "$root/share/$dir.zip" "$dir")
echo "share/$dir.zip ($(du -h "share/$dir.zip" | cut -f1)), commit $commit"
