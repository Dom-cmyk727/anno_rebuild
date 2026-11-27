# MAC-HEAVY-2 — review (ORCH III, 2026-11-27)

**Packet:** `task/MAC-HEAVY-2` head `2abe83c2` (one REPORTS.md entry on main `62f67abc`), hosted CI
46061801399 success (docs gate). Verdict: **ACCEPTED** as verification evidence. The packet changes no code; it
lands with the next board merge.

## What it proves

- Main `62f67abc` (candidates 6-13) on Apple M5 Max / Metal: Release three times, ASan+UBSan once, nine doctest
  binaries in default order and seeds 1/2/3. Each Release run: `executed=2889 failed=1 refused=0`; ASan: `executed=2890
  failed=1`, `sanitizer_lines=0`. 152 artifacts sealed (`SHA256SUMS` `09f0c0ff…`).
- The ten gpu skips are exactly the cases pinned to the PC's RTX 4090 / Radeon by name; the other three skips are
  Pillow and the two unset GOG shot folders. Both are acceptable on this host (the GOG compares passed 5/5 on the PC
  for candidate 13).
- No sanitizer diagnostic anywhere. MH-001 is therefore **not** a sanitizer finding: under the slower ASan build the
  minimap case sees the wrong zoom (`tile_w 42`, expected 84) and passes alone, so it is timing or order.

## Findings and where they go

| ID | Kind | Packet |
|---|---|---|
| MH2-001 `viewer_wiring_status_icon_zoom_gpu`, script line 25 `wait frames 10` after `look` while paused, 30 s timeout; all three Release runs and alone; ASan passed | New on candidate 13 (UI-HARVEST-ICON-1 R1 `09120398`), Mac-only so far (PC full run passed it). Lead: a paused window draws only when something changes; if `look` does not move the camera on this host, no frame comes | MAC-FIX-ASAN-TRANSPORT-1 (renamed in the prompt to MAC-FIX-VIEWER-TRANSPORT-1); UI lane told |
| MH2-002 (MH-001) minimap `tile_w` 42 vs 84 under full ASan, passes alone | Timing/order, not UB | same packet |
| MH2-003..005, MH2-007 reclaimer and picture counters (`SceneReclaimer::shared()`, `render::payload_counts()`, `picture.commit`) fail in some orders, pass alone | Shared process-wide state between cases | MAC-FIX-ORDER-1 |
| MH2-006 transport stats opt-in: `script offset 475: unsupported action: SET_DIPLOMACY` | The opt-in test predates strict action dispatch (`9d658712`); never measured since | same packet as MH2-001; implementing SET_DIPLOMACY is a sim rule, so a QUESTION, not a fix |

MH-005 did not reproduce in four full orders; it stays closed unless MAC-FIX-ORDER-1's extra seeds bring it back.

## NOT_RUN (accepted)

Windows adapters, the GOG shot compares and Pillow on the Mac; LeakSanitizer (unsupported on this host).
