#!/bin/bash
# SUPERSEDED by agent.py (T16, docs/testing/original-wine.md section 4); kept for reference only.
# Do not run it: it uses `pkill -f`, which the T16 rules forbid (`pkill -x` only), and it
# runs the game in the base prefix instead of a disposable copy.
echo "run_game.sh is superseded by tools/orig/wine/agent.py; see its header" >&2
exit 1
# Spike: run the original ANNO 1503 under Wine on an invisible X display (Xvfb).
# Runs in the AnnoHarness WSL distro as root. Game copy /root/anno/game, prefix /root/anno/prefix.
DISP=${1:-:91}
export DISPLAY=$DISP WINEPREFIX=/root/anno/prefix WINEDEBUG=-all WINEARCH=win32
unset WAYLAND_DISPLAY
pkill -f "Xvfb $DISP" 2>/dev/null
Xvfb "$DISP" -screen 0 1024x768x24 -nolisten tcp >/tmp/xvfb.log 2>&1 &
XPID=$!
sleep 1
cd /root/anno/game
echo "xvfb=$XPID display=$DISP" > /tmp/annorun.txt
# A Wine virtual desktop of exactly 1024x768, so the game never switches the display mode.
wine explorer /desktop=anno,1024x768 1503Startup.exe >/tmp/wine.log 2>&1
echo "launcher exited $?" >> /tmp/annorun.txt
# The launcher hands over to the game and exits: wait for every Wine process of this prefix.
wineserver -w
echo "wineserver done" >> /tmp/annorun.txt
kill $XPID 2>/dev/null
