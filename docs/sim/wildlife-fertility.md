# Ordered transient FRUCHTBAR import

Scope: one additional local island header in the diagnostic
[wildlife prefix](wildlife-prefix.md), after the reviewed
[rock projection](wildlife-rock.md). No World publication, gameplay fertility,
crop policy, native field, identity or default changes. D = original GOG
AnnoWelt instructions; P = parsed original file; C = executed authored check.
Independent orchestrator review pending.

## Header contract established before code (2026-11-19)

D: dispatcher `0x10089130`, FRUCHTBAR comparison `0x100894ca–0x100894d8`
(string `0x100dcefc`). Width and height are unsigned 16-bit island fields at
+8/+0xa. `0x100894de–0x10089520` reads width bytes per row and addresses the
island +0x18 byte plane at local `y * width + x`. No block header, world-origin
addition, transpose, scale or mean occurs. At `0x1008952b–0x10089539`, the exact
byte operation is `(old & 0xf0) | (source & 0x0f)`. The preceding GELAENDE
resource branch `0x1008948c–0x1008949d` writes the opposite nibble of that same
plane. All source bytes 0–255 are valid for this merge; observed 0–10 is no cap.

D: `0x10089549–0x10089562` advances row/consumed bytes; `0x10089565` sets island
byte +0xc2 to 1, then returns at `0x1008957a`. This packet records completion and
raw-column presence, not the later meaning/lifecycle of +0xc2 in a live island.
Concrete island table `0x100cac2c`, +0xdc -> `0x100946c0`, delegates at
`0x10094848` and only updates byte bookkeeping before return `0x10094873`.
Per-child caller `0x100a2fb0–0x100a2fbc` continues source order. Thus duplicates
merge in order; the final raw column is the last successful payload, while each
header's trace remains visible. No directly reached world-random call, resource
selection or tile/code write occurs in this arm/delegation. Archive +4 is the
already used byte-read interface; malformed original I/O behavior is not claimed.

D: empty-tile fill +0x1a0 -> `0x100928a0` (pointer `0x100cadcc`) is NOT called
by this path. Post-island callbacks at `0x100a2fc8`, `0x100a2fd7`, `0x100a2fdf`
only occur after the entire child loop and remain outside the local adapter.
No whole-scene draw/callback closure is inferred. The rock contract's omitted
writer-derived caches and future rebuild obligation remain unchanged.

## Acceptance table (authored before implementation)

| Requirement / evidence | Positive / distinguishing counterexample | Disposition |
|---|---|---|
| D byte nibble merge | Existing high 4 then raw 255 yields 79, not 255; later high 2 plus raw 255 yields 47 | authored C++ controls pass; review pending |
| D row-major, no origin | Asymmetric 3 × 2 island at (7,11), raw [0,17,2,255,128,15] -> low [0,1,2,15,0,15] | authored C++ controls pass; review pending |
| D source chronology | Resource / F1 / resource / F2 / HIRSCH; F1 changes 4 cells, F2 [255,7,128,16,254,18] changes 6; exactly 3 supplied draws | authored C++ controls pass; review pending |
| D no immediate fill | Empty low terrain plus fertility leaves all tile/code fields empty; occupied fields stay unchanged | authored C++ controls pass; review pending |
| Defensive length / geometry | Exactly W × H required; 0/5/7 bytes refuse before the 6-byte effect, huge invalid geometry refuses before allocation | authored C++ controls pass; review pending |
| Owned output and digest | Raw final F2 column retained after input destruction, digest equals independently constructed raw terrain; borrowed height grid valid through later resource/header use | authored C++ controls pass; review pending |
| Causal interpretation | Private actual production-TU whole-byte-copy mutant fails the named nibble assertion after legal setup | authored C++ controls pass; review pending |
| P original continuation | Apply raw header 49 once, validate every low nibble/raw column and preserved high variant, stop at next unknown header | original check pending; review pending |

## Representation and defensive boundaries

The Session owns effective byte variation. Prepared Terrain owns the last raw
fertility payload, using the same raw-column convention as the existing normal
scenario adapter (`scenario_formats.cpp`, fertility import); that production path
is unchanged. Raw bytes and effective low nibble are distinct representations,
not two persistent fertility models. The trace's optional fertility-update count
is engaged only after successful completion, including zero changes. No saved
+c2 field is added; trace completion represents the observed header load marker
within this transient result only.

Immutable terrain backing stays alive throughout the Session's borrowed height
view. Prepared terrain is rebuilt once when extracting the result, with the raw
fertility column and recomputed digest; no whole-map copy per object/header and
no mutation through shared const storage. Duplicate payloads replace only the
owned raw-column staging vector. Length is checked before changing variation;
malformed input is defensively refused without claiming original transactional
I/O. Earlier accepted prefix state remains diagnostic, never complete success.

## Pre-gate causal proof

The public adapter's authored chronology test executes 69 assertions successfully.
One private production-TU replacement changes only the nibble expression to the
whole source byte. The same test then fails exactly three intended assertions
(4-cell change count and both variation vectors), with 66 passing. Canonical
source remains unchanged and the normal target was rebuilt afterward. The initial
mutation harness stopped before linking because Ninja was absent from PATH; that
attempt is retained, followed by the corrected existing-tool-path receipt.
