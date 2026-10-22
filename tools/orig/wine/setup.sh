#!/bin/bash
# One-time setup of the isolated runner (docs/testing/original-wine.md), run as root inside the
# AnnoHarness WSL distro. On Windows first (once):
#   wsl --install Ubuntu-24.04 --name AnnoHarness --location E:\wsl\AnnoHarness --no-launch
#   wsl -d AnnoHarness -u root -- bash /mnt/c/Users/altin/anno1503/tools/orig/wine/setup.sh
# The game copy lives only inside the distro (never in git).
set -e
GAME_SRC=${1:-/mnt/c/Users/altin/anno1503/game/app}
export DEBIAN_FRONTEND=noninteractive
dpkg --add-architecture i386
apt-get update -qq
# Wine (32-bit game), the invisible X server and X input/capture, and Mesa's software OpenGL:
# the game asks for a Direct3D device, which Wine implements on OpenGL (llvmpipe here).
apt-get install -y -qq --no-install-recommends \
  wine wine64 wine32:i386 xvfb xdotool x11-apps x11-utils scrot python3 python3-pil procps cabextract \
  libgl1 libgl1:i386 libglx-mesa0 libglx-mesa0:i386 libgl1-mesa-dri libgl1-mesa-dri:i386 libegl1:i386 \
  libvulkan1 libvulkan1:i386 mesa-vulkan-drivers mesa-vulkan-drivers:i386 mesa-utils
mkdir -p /root/anno/tools
[ -d /root/anno/game ] || cp -a "$GAME_SRC" /root/anno/game
cp "$(dirname "$0")"/*.sh "$(dirname "$0")"/*.py /root/anno/tools/ 2>/dev/null || true
sed -i 's/\r$//' /root/anno/tools/*
chmod +x /root/anno/tools/*.sh
Xvfb :99 -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 &
XP=$!
sleep 1
DISPLAY=:99 WINEPREFIX=/root/anno/prefix WINEARCH=win32 WINEDEBUG=-all wineboot -i
WINEPREFIX=/root/anno/prefix wineserver -w
kill $XP
DISPLAY=:99 true
echo "setup done: $(wine --version), game $(du -sh /root/anno/game | cut -f1), prefix $(du -sh /root/anno/prefix | cut -f1)"
