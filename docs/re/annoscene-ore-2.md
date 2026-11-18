# Link placement: ore quantities, resource masks and treasure sites

ANNOSCENE-ORE-2, 2026-11-18. Baseline `b5c0ada209d94a0082bc88d793c917c7222e6a21`.
**D** = GOG 2.0.0.5 code/control-flow read; **P** = original data parsed;
**C** = explicit derived audit/scenario; **U** = unproved. Addresses are AnnoScene unless
prefixed Welt (AnnoWelt) or Game (AnnoGame). Original runtime and HE are **NOT_RUN**.
Methods: Python 3.14.6, Capstone 5.0.7, pefile 2024.8.26, existing `tools/disasm.py`,
`tools/insel.py` and `tools/figuren.py`; private diagnostics/audit under `cache/annoscene-ore-2/`.
No original tables, layouts, listings or assets accompany this note.

Predecessor: [ANNOSCENE-ORE-1 §5.1 at its accepted pin](https://github.com/Dom-cmyk727/anno_rebuild/blob/ea023e18386c465e795c33cac6d1f33f46b2a405/docs/re/annoscene.md#51-ore-markers-and-info-capacity-annoscene-ore-1-2026-11-16).
The dispatch said it was absent from main; direct `git ls-tree` shows baseline and predecessor
have identical blobs for annoscene.md, annoscene_ore_ref.py and its contract
(`53666414…`, `0b781ca8…`, `b88c518d…`). No merge, extraction or source replacement was needed.
Use [placement §5](annoscene.md#5-island-selection-at-erze-annoscenedll-0x10009550),
[salt source semantics](../sim/salt-extraction.md) and
[mine follow-ups](mine-deposit-followups.md) as related reads, with the corrections below.
OQ-151 is narrowed here, not closed wholesale.

| Original module | SHA-256 |
|---|---|
| AnnoScene.dll | 5b521f9541332846d32b57c93eaaee9c9d8de2ac2404cd75da0fe93e0e36fd38 |
| AnnoWelt.dll | f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2 |
| AnnoGame.dll | ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc |

## 1. Corpus and the three different objects

**P/D.** This copy has 465 island .scp files: 15 PIRAT and 21 *DUMMY exclusions leave
429 candidates, all parsed and all agreeing with INFO[5], totalling 1,590 ore markers.
INFO[4] is zero throughout. Re-read the 428 built-in paths at `0x10031554` (80-byte stride):
428 unique matches, no network-only path; disk additionally has
`Dschungel/D_080x080_0015_2B_Suedsee.scp`. This confirms this installation, not another build's
464/428 inventory. No input was removed to force equality.

**P/D.** OCCUR ENTRY2's packed first word names a **figure type**, not a mine building;
Welt `0x10095f63–0x10095f9d` extracts its type with shift 14 and mask 0xfff. All 1,590
ore payloads here are 24 bytes. The loader reads the 16-byte prefix, creates/attaches the
occurrence, then calls its +0x64 reader on the remaining bytes (`0x100960d3–0x10096119`).
The ore reader at `0x100634e0` accepts an eight-byte remainder. That gives tons from its
first u16, i.e. payload offset 16. Three direct file-offset samples independently checked
this against the reference parser; no extracted payload is published.

**D.** Marker goods come from occurrence +0x84 (`0x10063640`), forwarding type +0x80;
Game `0x1000fc90` returns type byte +0x100. The marker holds quantity/reserve separately
(Welt `0x100637a0`). A later mine looks up and consumes that source under its own rules;
marker count/INFO capacity proves no amount, and nearby resource-mask data is a third concept.

## 2. Quantity and the marker that receives no assigned ore

**D — loaded amount.** Welt constructor `0x100637a0–0x100637c4` sets stored quantity
Q = 512,000 raw (2,000 tons at 256 raw/ton), reserve byte R = 1. Reader
`0x1006351f–0x10063565` overwrites Q with `file_tons << 8`; for goods 1..8 with R != 0
and Q < 409,600 (1,600 tons), it resets Q to 512,000. Equality is not reset. Thus a
400-ton file entry does **not** establish a 400-ton normal loaded source.

**D — assigned type.** `0x10008683–0x1000877e` walks ore kinds 0..6 in order. For each
marked kind while unused candidates remain, draw modulo the **original candidate count**,
then probe null slots forward/wrap without another draw. One candidate pointer is cleared;
the mark is cleared after retyping/updating. Only goods 1..6 and property 4 == 0 entered
the candidate set (`0x10008622–0x1000864b`). Container enumeration order has not been closed
here, so this note does not supply exact world-draw-to-marker reference vectors.

Retyping is occurrence +0x28 (`0x10008714` → Welt `0x10063750`). The special type **1661**
sets Q = 102,400 raw (400 tons) and clears R (`0x1006377d–0x1006378b`). Other assigned
ore types preserve the loaded Q/R. This is replacement, never accumulation with an old
marker amount. The kind-0/kind-6 iron distinction therefore matters despite identical goods.
The 0xd00 argument in this attach is the **figure-type descriptor**, not the island:
Game type-kind getter `0x10010a40` returns 0xd00; occurrence then stores that pointer at +4.
A later eight-byte load record may overwrite constructor/attach quantity; scene retyping
occurs after that load and applies the special reset again.

**D — unassigned, corrected after independent review (P2, 2026-11-18).**
`0x1000879f–0x100087df` does not delete or retype unused markers. It clears R through
+0x74(1,0), then pushes two values 1 before +0x88. Getter `0x10063650–0x1006366c` ends with
ret 4, leaving the other 1 as the take flag when Scene pushes Q and calls +0x8c. That ABI
reading is correct, but **the flag test is branch-local**: Welt `0x10063678` jumps directly
to `0x100636b8` when R == 0, bypassing the flag test at `0x1006367a`. This is the path the
setter has selected. World property 14 bit value 2 then controls depletion: set, subtract
min(request,Q); clear, return the request without changing Q (`0x100636b8–0x100636e8`).
Thus take(Q,1) after clearing reserve leaves Q = **0 with world flags 2**, Q unchanged with
world flags 0. The earlier interpretation incorrectly lifted the R != 0 flag guard across
that jump; its corpus-02 and corresponding negative control are superseded failed evidence.

For R != 0, flag bit 0 set returns the request without depletion. With that flag clear,
world bit value 2 clear still preserves Q; set, the unsigned comparison at `0x100636a1`
protects the 409,600 reserve on the supported requests used below. These are different paths,
not one global flag rule. World property 14 reads +0x69dc via Welt `0x100a344e`.

**D-derived/C — branch matrix**, initial Q = request = 512,000 raw. Each cell is
`remaining Q / returned amount`; R = 1 represents the nonzero branch.

| R | World flags | Take flag 0 | Take flag 1 |
|---:|---:|---:|---:|
| 0 | 0 | 512,000 / 512,000 | 512,000 / 512,000 |
| 0 | 2 | 0 / 512,000 | 0 / 512,000 |
| 1 | 0 | 512,000 / 512,000 | 512,000 / 512,000 |
| 1 | 2 | 409,600 / 102,400 | 512,000 / 512,000 |

Additional hand boundaries in the R = 1 / flag 0 / world 2 branch: requests 102,399,
102,400 and 102,401 leave Q 409,601, 409,600 and 409,600 respectively, returning 102,399,
102,400 and 102,400. The R = 0 branch also pins an oversized request and an empty source.
No new general rule for out-of-domain reserved-source requests is claimed.

**D — stored versus extractable.** With R != 0 and getter flag bit 0 clear, +0x88 returns
Q − 409,600; otherwise Q. At Q = 512,000/R = 1, shallow availability is 400 tons while
stored amount is 2,000 tons. At 1661's Q = 102,400/R = 0 it is also 400 tons, but there
is no reserved portion. Those equal observations must not erase the different state/rules.
No complete mining-production rule is derived from this placement read.

### Nearby resource-mask refresh is not tonnage assignment

**D.** Both paths invoke island container +0x10 over a 15 × 15 rectangle beginning
`trunc(marker_position * scale) − 7` (`0x10008717–0x10008763`, `0x100087e5–0x1000882b`).
Coordinates are stored as u16 before the container receives them; do not invent a signed
edge clamp. Scale `0x100433c4` is initialized to 1 / 1 = 1.0 at `0x10007900–0x10007912`;
these are tile coordinates, not a quantity multiplier.

Welt visitor `0x100785a0–0x100787ae` clips/visits 8 × 8 chunks and resource-record anchors,
then calls `0x1007c0d0` at `0x10078758`. That callback through its return at `0x1007c27a`
**resets resource byte +0x0a to 0x10**, queries nearby occurrence class mask 1, and ORs
`1 << (goods−1)` for eligible goods 1..8. It reads neither source amount nor reserve.
Zero-quantity markers still participate in this mask calculation.

Its record's packed +8 byte provides signed-nibble anchor offsets; +9 rotation chooses
footprint 3 × 4 or 4 × 3 (`0x1007c11b–0x1007c170`). Generated radius-7 row bounds from
`0x1007c0de–0x1007c115` are 7,7,7,6,6,5,4,2 for nonnegative row distance. Negative distances
are adjusted by `1 − distance − footprint_extent`, with row clamped to zero and >7 rejected;
column is compared to the selected row bound (`0x1007c1f5–0x1007c253`). This is the callback's
geometry, not a claim that the visitor's u16 boundary behavior was ported by the audit.
Overlaps rebuild a bitwise union; quantities are not added, and the old mask is not retained.

**Hand discriminators (D-derived/C).** Normal file amount 400/R1 loads to 2,000 tons;
file 1,600/R1 stays 1,600. Retype the first to 1661 → 400/R0; retype to salt → 2,000/R1.
Unassign it with world flags 2 → 0/R0, or world flags 0 → 2,000/R0; the cleared reserve bypasses the carried take-flag test. Two nearby goods 1/3 give
mask 0x15 after reset; two goods-1 sources give 0x11, not an arithmetic sum. At an isolated
anchor with rotation 0, source offset (−7,−2) contributes, (−7,−3) does not. These checks
separate the plausible capacity/accumulation/mask rules without executing the original.

## 3. Treasure scan and draws

**D — exact predicate.** `0x10008cc0–0x10008fc3` uses island primary table `0x100cac2c`:
+0x50/+0x54 are origin x/y, +0x58/+0x5c width/height, +0x98 is Welt `0x10084c20`.
That getter samples **one corner**, not a cell mean, from island +0x14 with stride width+1,
then indexes height table `0x100e2d90`. Its initializer `0x1007c760–0x1007c780` repeatedly
adds f32 0.04. Scene `0x10007930–0x10007998` initializes the threshold to 2 × (16 × f32 0.04).
For all 256 byte heights, the strict original comparison is equivalent to **height > 32**;
32 is not high. No object, city, claim, route, entrance or accessibility predicate is called
by this candidate scan. Final y/up interpolation through +0xa4 is not a second legality test.

**D — traversal.** Start at origin + trunc((dimension−1)/2) on each axis. For radii **0..127**,
visit directions 0..7: +x, −x, +y, −y, +x+y, −x−y, +x−y, −x+y
(jump table `0x10008fc8`). Per direction, remember whether a high corner was seen. The
first low/equal or out-of-bounds point closes that direction permanently. Record that
transition point **only if high was seen earlier**. There is no search beyond the first
low point: a low centre closes all directions at radius 0.

**D — selection/failure.** If no direction ever saw high, return false without a draw
(`0x10008e82–0x10008e88`). Otherwise draw once, use `raw & 7`, and probe subsequent direction
slots cyclically without another draw until a nonzero coordinate pair is found or the bounded
probe fails (`0x10008e9a–0x10008ed0`). A high ray that never ends by radius 127 has no recorded
transition; high-seen is not guaranteed success. Move the chosen transition **8 steps inward**
along that ray (`0x10008eea–0x10008f2a`), then reject if outside the island. There is no retry
with another direction on that final failure, nor a fresh height/object/accessibility test.
Return coordinates and interpolate their y/up at `0x10008f63–0x10008fb7`; failure leaves the
caller without a created treasure, not a fallback to the centre.

| Order | World draw | Condition / selection |
|---|---|---|
| Ore assignment, ascending kind | raw modulo original collected marker count | One per marked kind while an unused marker exists; null-slot probes consume none, `0x100086ad` |
| Unassigned accessor calls / mask refresh | none in the resolved paths | Accessors, subtraction and resource union above |
| Native-name branch between ore and treasure | Separate existing branch, not recounted here | Do not infer whole-placement draw totals from ore/treasure rows |
| Treasure kind | raw & 3 | Only when flagged and climate 4/5/6; zero selects 1551, otherwise 1550; before scanning, `0x10008b69` |
| Treasure direction | raw & 7 | Only when any high was seen, even if no usable transition/final site results; `0x10008e9a` |
| Treasure facing / construction | No separate facing draw | Caller has no facing argument. Welt `0x10095100–0x100952a0`, treasure constructor `0x100637f0`, ore/base constructors, attach and position setter introduce no world draw on this path. |

**D.** Caller `0x10008b25–0x10008bb5` does the optional kind draw before scan and creates
nothing when scan returns false. The occurrence factory's treasure arm at Welt
`0x10094e6b–0x10094e9f` selects constructor `0x100637f0`; this read claims creation only,
not the later treasure's reward behavior.

**Hand cases (C).** A 17 × 17 grid of 32 has no high and consumes zero direction draws.
All 33 with selector 0 ends its +x ray outside at (17,8), returns (9,8), and draws once;
the answer is neither first-high nor centre. A 256 × 256 all-high grid records no ray
transition by radius 127: it consumes a direction draw and fails. A selection-stage array
with only direction 2's point (16,8) probes from raw 7 to 2 and yields (16,0), with one draw.
These distinguish threshold equality, radius bound, direction order and empty/one-site behavior.
They do not assert player reachability or actual original runtime parity.

## 4. Bounded audit and implementation boundary

**P/C.** Every input is accounted once: 465 = 429 evaluated + 36 excluded; unsupported 0,
marker/INFO mismatches 0. File amounts: 1,585 markers at 400 tons, 5 at 2,000; all 1,590
normal markers derive loaded Q = 512,000/R1. Explicit **all-unassigned** audit scenarios:
world flags 2 leave all 1,590 at 0/R0; flags 0 leave them at 512,000/R0. The flag argument does not guard the R = 0 branch. A separate
**retype-to-1661 accessor scenario** yields 102,400/R0 for each. These are labelled inputs,
not an intrinsic generated ore total per island or a reconstruction of arbitrary ERZE draws.

Treasure: all eight fixed selector inputs on 429 templates give 3,432 evaluated outcomes:
3,208 sites on 401 templates and 224 no-high failures on 28. No final-outside or
no-transition exceptions occurred on this installed corpus; synthetic controls cover both
absence and the latter bound. Complete per-template/no-site diagnostics and the input-hash
manifest remain private. No reachable/free-building interpretation is attached to a site.

Corrected audit controls: 41 assertions/exits, including legal/restored synthetic corpus, missing,
zero-candidate, malformed, wrong expected result, and four applied wrong-rule mutations (including a global take-flag guard and removal of its
actual branch-local guard).
The full 8-case R/take-flag/world-flag matrix and 5 request boundaries are included.
Three direct original-offset samples and all 256 height-boundary projections remain unchanged.
These are private audit checks, not CTest/gameplay coverage. Reproduction commands and exact
hashes are frozen in the report/handoff; no audit scripts are installed in tools/. Corrected
corpus-03-P2 changes only unassigned_flags2_raw on 356 templates containing 1,590 markers;
all other per-template fields, inputs and treasure results match corpus-02. The old projection
fails the corrected world-2 assertion while its world-0 legal control still passes.

**U / next boundary.** Exact marker-container enumeration for RNG index identity (container table `0x100caf34`: +0x14
`0x10095770`, +0x10 `0x10095ad0`), spatial-query transitive effects (+8 `0x10095480`), arbitrary
edited kind-4 assignment, native setup/draw closure, full resource visitor edge semantics,
complete mining consumption and treasure rewards remain outside this result. OQ-151 remains
open for those edges. The next smallest contract is ore retyping/unassigned-state and treasure-site
reference vectors with explicit world flags/inputs; a full placement port still needs marker
ordering and the caller's world PRNG timeline contract. Spatial query/enumeration callees are not a closed transitive draw proof; the draw table
counts resolved sites only and does not assert a whole-placement total. No generated ore, map change, state
field, default or PRNG version is authorized by this read.
