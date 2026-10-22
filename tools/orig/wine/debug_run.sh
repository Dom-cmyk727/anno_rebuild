#!/bin/bash
# Spike diagnostics: start the launcher with Wine's error output, watch the processes for a while.
DISP=:92
export DISPLAY=$DISP WINEPREFIX=/root/anno/prefix WINEARCH=win32
unset WAYLAND_DISPLAY
export WINEDEBUG=${WINEDEBUG:-err+all,fixme-all}
Xvfb "$DISP" -screen 0 1024x768x24 -nolisten tcp >/tmp/xvfb92.log 2>&1 &
XPID=$!
sleep 1
cd /root/anno/game
wine explorer /desktop=anno,1024x768 1503Startup.exe >/tmp/wine92.log 2>&1 &
for i in 1 2 3 4 5 6 7 8; do
  sleep 3
  echo "== t=$((i*3))s"
  ps -eo pid,ppid,pcpu,args --cols 200 | grep -i -E '\.exe|wineserver' | grep -v grep | cut -c1-160
done
DISPLAY=$DISP scrot -o /tmp/s92.png 2>/dev/null && cp /tmp/s92.png "$1/s92.png"
echo "== wine log (last 40 lines)"
grep -v '^$' /tmp/wine92.log | tail -40
wineserver -k 2>/dev/null
kill $XPID 2>/dev/null
