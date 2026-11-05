#!/bin/bash
# sample_memory.sh: numeric memory samples of the processes whose command matches a pattern (round dw, DW6c).
#
#   tools/test/sample_memory.sh <pattern> <seconds> <csv>
#
# One row per interval (5 s): unix time, the matching pids, their summed RSS (KiB), their summed physical
# footprint (bytes, from the `footprint` tool's "Footprint: N KB|MB|GB" line; 0 when unavailable), the free
# memory percentage (memory_pressure -Q) and the used swap (MiB). Every value is numeric; an unavailable
# value is written as an empty field, never as text. Stops after <seconds> or when no process matches.
set -u
PATTERN=$1; SECONDS_TOTAL=$2; CSV=$3
echo "t,pids,rss_kib,footprint_bytes,free_pct,swap_used_mib" > "$CSV"
to_bytes() { # "N KB" / "N MB" / "N GB" -> bytes
    awk '{ v=$1; u=$2; if (u=="KB") m=1024; else if (u=="MB") m=1048576; else if (u=="GB") m=1073741824; else m=1; printf "%d", v*m }'
}
end=$(( $(date +%s) + SECONDS_TOTAL ))
while [ "$(date +%s)" -lt "$end" ]; do
    pids=$(pgrep -f "$PATTERN" | tr '\n' ' ' | sed 's/ *$//')
    [ -z "$pids" ] && { sleep 1; continue; }
    rss=0; fp=0
    for p in $pids; do
        r=$(ps -o rss= -p "$p" 2>/dev/null | tr -d ' ')
        rss=$(( rss + ${r:-0} ))
        f=$(footprint -p "$p" 2>/dev/null | grep -m1 -o 'Footprint: [0-9.]* [KMG]B' | sed 's/Footprint: //')
        [ -n "$f" ] && fp=$(( fp + $(echo "$f" | to_bytes) ))
    done
    free=$(memory_pressure -Q 2>/dev/null | grep -o '[0-9]*%' | head -1 | tr -d '%')
    swap=$(sysctl -n vm.swapusage 2>/dev/null | grep -o 'used = [0-9.]*M' | grep -o '[0-9.]*')
    echo "$(date +%s),\"$pids\",$rss,$fp,${free:-},${swap:-}" >> "$CSV"
    sleep 5
done
