#!/bin/sh
# Builds share/anno1503-preview-<name>.zip from the C++ build: Windows and
# Linux x86_64 binaries (anno1503, bench_frame, anno-dump), start scripts,
# README and a source snapshot of HEAD. No game files are included.
#
# Cross-compiles on a macOS or Linux host with:
#   - MinGW-w64 (x86_64-w64-mingw32-g++, e.g. Homebrew mingw-w64): Windows,
#     linked statically (cmake/toolchains/mingw-w64-x86_64.cmake);
#   - Docker: Linux x86_64 in ubuntu:22.04 (glibc 2.35) with GCC 13 from
#     the toolchain PPA, libstdc++ and libgcc linked statically.
# CMake fetches the pinned SDL3 and wgpu-native (MinGW and Linux archives).
#
# Usage: packaging/package-preview.sh <name>      e.g. 2026-10-18a
set -eu
name=${1:?usage: packaging/package-preview.sh <name>}
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
if [ -n "$(git status --porcelain -- src cmake CMakeLists.txt CMakePresets.json packaging)" ]; then
  echo "uncommitted changes in src/, cmake/ or packaging/; commit first so the package matches its commit" >&2
  exit 1
fi
commit=$(git rev-parse --short HEAD)
dir=anno1503-preview-$name
out=target/package/$dir
rm -rf "$out"
mkdir -p "$out/windows" "$out/linux"

# Windows (MinGW-w64, static).
cmake -S . -B build/package-windows -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake -DANNO_BUILD_TESTS=OFF
cmake --build build/package-windows --target anno1503 bench_frame anno_dump
for b in app/anno1503 tools/bench_frame/bench_frame tools/anno_dump/anno-dump; do
  x86_64-w64-mingw32-strip -o "$out/windows/$(basename "$b").exe" "build/package-windows/src/$b.exe"
done

# Linux x86_64 (Docker; the source is the committed tree).
src=target/package/source-$commit
rm -rf "$src"
mkdir -p "$src"
git archive --format=tar HEAD -- . ':!share' | tar -x -C "$src"
docker run --rm --platform linux/amd64 -v "$root/$src:/src:ro" -v "$root/$out/linux:/out" \
  ubuntu:22.04 bash -c '
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq >/dev/null
# GCC 13 from the Ubuntu toolchain PPA: GCC 12 (the release default) stops
# on false -Wrestrict warnings in std::string code (GCC bug 105329).
apt-get install -y -qq software-properties-common >/dev/null
add-apt-repository -y ppa:ubuntu-toolchain-r/test >/dev/null
apt-get update -qq >/dev/null
apt-get install -y -qq g++-13 python3-pip ninja-build ca-certificates libx11-dev libxext-dev \
  libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev libxkbcommon-dev \
  libdrm-dev libgbm-dev libegl1-mesa-dev libgl1-mesa-dev libwayland-dev wayland-protocols \
  libdecor-0-dev libasound2-dev libpulse-dev libdbus-1-dev libudev-dev >/dev/null
pip3 install -q "cmake>=3.29"
CC=gcc-13 CXX=g++-13 cmake -S /src -B /tmp/b -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DANNO_BUILD_TESTS=OFF -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc"
cmake --build /tmp/b --target anno1503 bench_frame anno_dump
for b in app/anno1503 tools/bench_frame/bench_frame tools/anno_dump/anno-dump; do
  strip -o "/out/$(basename "$b")" "/tmp/b/src/$b"
done'
rm -rf "$src"

for f in README.txt start-viewer.sh start-viewer.bat run-benchmark.sh run-benchmark.bat; do
  sed "s/@NAME@/$name/g; s/@COMMIT@/$commit/g" "packaging/preview/$f" > "$out/$f"
done
chmod +x "$out/start-viewer.sh" "$out/run-benchmark.sh" "$out"/linux/*
git archive --format=zip -o "$out/anno1503-source.zip" HEAD -- . ':!share'

mkdir -p share
rm -f "share/$dir.zip"
(cd target/package && zip -qr "$root/share/$dir.zip" "$dir")
echo "share/$dir.zip ($(du -h "share/$dir.zip" | cut -f1)), commit $commit"
