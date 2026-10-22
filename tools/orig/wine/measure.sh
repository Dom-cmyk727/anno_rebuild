#!/bin/bash
# SUPERSEDED by agent.py and tools/orig/timing.py (T16, docs/testing/original-wine.md
# sections 3 and 4); kept for reference only.
# Spike measurements under Wine: PRNG table, cities, speed keys (F7 400 %, F6 200 %, F8 50 %, F5 100 %).
cd /root/anno/tools
export DISPLAY=:91
echo "== prng"
python3 wmem.py prng | grep -E '"pid"|mismatch'
echo "== cities"
python3 wmem.py city | python3 -c 'import json,sys; d=json.load(sys.stdin); [print(c) for c in d.get("cities",[])]'
for k in F7 F6 F8 F5; do
  xdotool key "$k"
  sleep 2
  r=$(python3 wmem.py world --seconds 12 | python3 -c 'import json,sys; d=json.load(sys.stdin); print(d.get("ms_per_s"), d.get("tick_eq_counter_shr1_all"))')
  echo "== $k: game ms per real s, invariant = $r"
done
