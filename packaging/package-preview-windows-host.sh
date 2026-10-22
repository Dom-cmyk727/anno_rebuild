#!/bin/sh
# Builds share/anno1503-preview-<name>.zip on a Windows host: the same
# package as package-preview.sh (Windows and Linux x86_64 binaries of
# anno1503, bench_frame and anno-dump, start scripts, README and a source
# snapshot of HEAD; no game files), with both builds in Docker so nothing
# but Git Bash and Docker Desktop (Linux containers) is needed on the host:
#   - Windows x86_64: MinGW-w64 (GCC 13, posix threads) in ubuntu:24.04,
#     cmake/toolchains/mingw-w64-x86_64.cmake, everything linked statically
#     (the executables import only system DLLs);
#   - Linux x86_64: ubuntu:22.04 (glibc 2.35) with GCC 13 from the toolchain
#     PPA, libstdc++ and libgcc linked statically, as in package-preview.sh.
# The two toolchain images are built once (docker build, tags
# anno1503-pkg-mingw:24.04 and anno1503-pkg-linux:22.04) and reused by later
# runs; `docker image rm` them to refresh the packages. The builds themselves
# start from scratch inside the containers each time. CMake fetches the
# pinned SDL3 and wgpu-native (MinGW and Linux archives).
#
# Text files come from the commit (git show / git archive with
# core.autocrlf=false), so a CRLF checkout does not leak into the package.
#
# Usage (Git Bash): packaging/package-preview-windows-host.sh <name>   e.g. 2026-10-22a
set -eu
name=${1:?usage: packaging/package-preview-windows-host.sh <name>}
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
if [ -n "$(git status --porcelain -- src cmake CMakeLists.txt CMakePresets.json packaging)" ]; then
  echo "uncommitted changes in src/, cmake/ or packaging/; commit first so the package matches its commit" >&2
  exit 1
fi
# Docker gets Windows paths; MSYS must not rewrite the container-side ones.
export MSYS_NO_PATHCONV=1
winpath() { cygpath -w "$1"; }
commit=$(git rev-parse --short HEAD)
dir=anno1503-preview-$name
out=target/package/$dir
rm -rf "$out"
mkdir -p "$out/windows" "$out/linux" share

# Toolchain images (cached by Docker after the first run).
docker build -q --platform linux/amd64 -t anno1503-pkg-mingw:24.04 - <<'EOF'
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq \
 && apt-get install -y -qq --no-install-recommends g++-mingw-w64-x86-64-posix \
    gcc-mingw-w64-x86-64-posix binutils-mingw-w64-x86-64 mingw-w64-x86-64-dev \
    cmake ninja-build ca-certificates >/dev/null \
 && update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix \
 && update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix \
 && rm -rf /var/lib/apt/lists/*
EOF
# GCC 13 from the Ubuntu toolchain PPA: GCC 12 (the release default) stops on
# false -Wrestrict warnings in std::string code (GCC bug 105329).
docker build -q --platform linux/amd64 -t anno1503-pkg-linux:22.04 - <<'EOF'
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq \
 && apt-get install -y -qq software-properties-common >/dev/null \
 && add-apt-repository -y ppa:ubuntu-toolchain-r/test >/dev/null \
 && apt-get update -qq \
 && apt-get install -y -qq g++-13 python3-pip ninja-build ca-certificates zip libx11-dev \
    libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
    libxkbcommon-dev libdrm-dev libgbm-dev libegl1-mesa-dev libgl1-mesa-dev libwayland-dev \
    wayland-protocols libdecor-0-dev libasound2-dev libpulse-dev libdbus-1-dev libudev-dev >/dev/null \
 && pip3 install -q "cmake>=3.29" \
 && rm -rf /var/lib/apt/lists/*
EOF

# The source is the committed tree (copied into the container: builds on a
# Windows bind mount are slow).
src=target/package/source-$commit
rm -rf "$src"
mkdir -p "$src"
git -c core.autocrlf=false archive --format=tar HEAD -- . ':!share' | tar -x -C "$src"

# Windows (MinGW-w64, static).
docker run --rm --platform linux/amd64 -v "$(winpath "$root/$src"):/src:ro" \
  -v "$(winpath "$root/$out/windows"):/out" anno1503-pkg-mingw:24.04 bash -c '
set -e
cp -r /src /tmp/src
cmake -S /tmp/src -B /tmp/b -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/tmp/src/cmake/toolchains/mingw-w64-x86_64.cmake -DANNO_BUILD_TESTS=OFF
cmake --build /tmp/b --target anno1503 bench_frame anno_dump
for b in app/anno1503 tools/bench_frame/bench_frame tools/anno_dump/anno-dump; do
  x86_64-w64-mingw32-strip -o "/out/$(basename "$b").exe" "/tmp/b/src/$b.exe"
done'

# Linux x86_64.
docker run --rm --platform linux/amd64 -v "$(winpath "$root/$src"):/src:ro" \
  -v "$(winpath "$root/$out/linux"):/out" anno1503-pkg-linux:22.04 bash -c '
set -e
cp -r /src /tmp/src
CC=gcc-13 CXX=g++-13 cmake -S /tmp/src -B /tmp/b -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DANNO_BUILD_TESTS=OFF -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc"
cmake --build /tmp/b --target anno1503 bench_frame anno_dump
for b in app/anno1503 tools/bench_frame/bench_frame tools/anno_dump/anno-dump; do
  strip -o "/out/$(basename "$b")" "/tmp/b/src/$b"
done'
rm -rf "$src"

# sed -b: MSYS sed would otherwise drop the CRs of the .bat files' CRLF.
for f in README.txt start-viewer.sh start-viewer.bat run-benchmark.sh run-benchmark.bat; do
  git show "HEAD:packaging/preview/$f" | sed -b "s/@NAME@/$name/g; s/@COMMIT@/$commit/g" > "$out/$f"
done
git -c core.autocrlf=false archive --format=zip -o "$out/anno1503-source.zip" HEAD -- . ':!share'

# Zipped in Linux so the archive carries Unix modes (executable scripts and
# Linux binaries), as the packages built on macOS did.
docker run --rm --platform linux/amd64 -v "$(winpath "$root/target/package"):/pkg:ro" \
  -v "$(winpath "$root/share"):/share" -e DIR="$dir" anno1503-pkg-linux:22.04 bash -c '
set -e
cp -r "/pkg/$DIR" /tmp/
cd /tmp
find "$DIR" -type d -exec chmod 755 {} +
find "$DIR" -type f -exec chmod 644 {} +
chmod 755 "$DIR/start-viewer.sh" "$DIR/run-benchmark.sh" "$DIR"/linux/* "$DIR"/windows/*
rm -f "/share/$DIR.zip"
zip -qr "/share/$DIR.zip" "$DIR"'
echo "share/$dir.zip ($(du -h "share/$dir.zip" | cut -f1)), commit $commit"
