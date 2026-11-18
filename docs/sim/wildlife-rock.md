# Rock import class — WILDLIFE-ROCK-1

2026-11-19. D = matching original instructions, P = parsed original data,
C = executed proof, A = bounded API adaptation, U = unresolved. This contract
and hand-derived acceptance table precede implementation. Continue the locally
reviewed [cliff](wildlife-cliff.md) and [ordered prefix](wildlife-prefix.md).
No World/native/default/tag/registry identity changes belong here.

## Acceptance table

| Exact primary predicate | Hand-derived legal result / distinguishing alternative | Proof / review |
|---|---|---|
| D Game 0x10009048–0x10009095: unsigned quad minimum, clear low 4 bits; 0x10009105–0x10009109: mode 0 selects highest of those bytes | 2 × 1 window 35,63,79 / 47,55,65 gives cell minima 35/55 -> 32/48, result 48 | Mode 2 gives 32; origin mean gives 50; max vertex & 0xf0 gives 64; unrounded max minimum 55. Authored vectors/C++; pending |
| D same mode branch / 0x10008f9f | For a 1 × 1 footprint modes 0 and 2 coincide (one quantized cell minimum) | Use two cells to distinguish modes instead of asserting an invented 1-cell difference; pending |
| D Game 0x10007d80–0x10007d86: zero-extended u16 flag word; 0x10009111–0x1000911a: only class-test bit 4 | 0x8018 remains 0x00008018, mode 0, no class flatness test | No sign-extension, no implicit mode 2 or raw file-height substitution; pending |
| D Welt 0x1007c88a–0x1007cb48 and 0x1007cbdf | Same full overlap, per-world-block coverage and group 0x141 level bypass as cliff | Nonflat/all rotations, low result, chunk crossing, compatible/refused objects and partial anchors; pending |
| D Game 0x10008923–0x10008a28 | Existing 0x8018 lacks 0x20, so ordinary resource after rock refuses; rock after resource and same-group overlays may pass | Legal installation and both orders, unchanged draw/variation behavior on rock; pending |
| D complete fresh wrapper/writer path, Welt 0x1008ec90–0x1008ee59 / 0x10082040–0x10082b0a | Neither bit 0x8000 nor 0x08 selects another reached arm; flags stay intact | Reuse concrete getters/mask sampler/observer bypass with finite reread, no new representation; pending |
| P predecessor 42/39/type 1512 | Independently derive its admission/height/final writes in the same raw HHbb stream with supplied stamp 500/mode 0/draw 7 | One original Debug continuation, exact next unsupported boundary and separate counts; pending |

## Narrow path and height source

D: extend only exact flags 0x8018 / parsed kind group 0x141. Keep exact 0x3a
and 0x12 plus ordinary 0x09/0x29 behavior. No content-id test and no normalization
of stored type flags. Original u16 getter 0x10007d80–0x10007d86 clears EAX then
reads AX from type +0x12; the upper flag is bit 15, not a sign-extended caller
bit 31. Island admission ORs the flags at 0x1007c88a–0x1007c898.

D: each cell reads four unsigned corner bytes, takes the minimum, clears its
low nibble at 0x10009081, and tracks low/high cell minima at
0x10009083–0x10009095. The low two combined flag bits select mode 0/high at
0x100090e0–0x10009109, rather than mode 2/low. A shared window-level helper
must serve both, preserving old mode-2 values. The result is a HEIGHT-unit byte;
island admission writes descriptor byte 8 at 0x1007cbbf, and the lower writer
stores that computed byte at 0x10082260. The original raw state high byte is not
trusted; low phase bits still follow the existing writer.

D: for one cell, low and high coincide. Quantizing every minimum then taking
the maximum equals quantizing the maximum cell minimum, so do not invent a
rounding-order distinction. It differs from an unquantized result, from maximum
vertex height, from the origin mean and from the minimum across multiple cells.
The class-test mask is 0x1000004 (0x10009111), which excludes 0x8000; class bits
are 0x7000 (0x1000912f / Welt 0x1007cbc2), also excluding 0x8000. With bit 4
clear, nonflat terrain does not enter that separate class test. Group 0x141 still
bypasses only the final 32 gate, not bounds/compatibility/coverage.

## Upper bits, existing objects and write reuse

D: admission's object window at island +0x88 is concrete
0x10083c80–0x10083d85, a clipped copy of each tile's u16 id without type-flag
filter/callback. Compatibility Game 0x10008920 reads EXISTING bit 0x20 first,
then incoming/existing kind groups and incoming bit 0x10 as already documented
for cliffs. Rock has no 0x20: later ordinary resources cannot silently replace
it. Same-group overlays still obey the existing kept-anchor coverage rule.

D: finite reread of admission, height, compatibility, wrapper and writer finds
no 0x8000/0x08 arm on this fresh-import path. Incoming bit 0x10 chooses overlay;
existing bit 0x10 plus anchor chooses skipped object writes. Type +0x94 and
+0x84 remain the addressed walk-code byte +0x75 / mask +0x88 getters from the
[cliff contract](wildlife-cliff.md), not reads of flag word +0x12. Bit 31 is clear,
+c8 remains zero and import 0x04000000 bypasses observer/final getter paths.
No flag-specific PRNG draw is introduced. The internal writer callbacks still
execute and update derived caches, as the dated correction below establishes;
the wrapper bypass is not complete callback suppression. Active graphics caches
and other original callers remain outside this profile.

D/C: reuse the same tile record/partial writer, compatibility, prepared facts,
Session scratch and header water-mask epilogues. No second overlay plane.
Retain the predecessor's original coverage/offset-domain restrictions; wider
layouts remain unsupported. Group 0x141 rocks are nonresource, so consume no
selector draw and leave the separate variation plane alone. Failed sessions
remain poisoned and diagnostic partial results cannot become complete success.

## Original continuation baseline

P: previous diagnostic stops at Tutorial/01Discover01 island ordinal 0/slot 11,
GELAENDE header 42 record 39, type 1512 FELSEN kind 321 flags 0x8018, 2 × 1,
world (59,90). It has 2,387 accepts / 2,333 resource draws / 29 stones / 25 cliffs /
9 completed epilogues / 0 spawns. These are predecessor values, not assumed new
outputs. Continue only supported classes in original HHbb order, with the same
explicit stamp 500, mode 0 and bounded supplied draw 7; stop the next boundary.

## Measured continuation and causal proof

P/C: corrected independent HHbb parsing composed with original `place()` and
ordered writes predicts, and the direct C++ adapter confirms, 4,760 completed /
accepted 4,760 / refused 0 / resource-class 4,589 / resource draws 4,589 /
84 stones / 63 cliffs / 24 rocks / 0 spawns. All 16 GELAENDE headers 33–48
complete their water-mask epilogues. The next unsupported input is header 49,
`FRUCHTBAR`, before any effect is applied. Boundary record 0 is the header's
source convention, not a decoded placed-object ordinal. HIRSCH header 71 remains
unreached; a diagnostic partial is not complete scene/World initialization.

P/C: former boundary 42/39/type 1512 accepts at world (59,90), rotation 0,
computed height 48, 2 writes, no resource draw/variation, cursor still 2,333.
The actual two quantized cell minima are 32/48, distinguishing mode 0 from 2.
Both final cells retain id 1512, phase 0, height 48, dx 0/1, dy 0, rotation 0.
Their final anchors are both 1 after later partial placements: these final fields,
not just the first Accepted outcome, are asserted from the independent raw-order
reference. Earlier stone/cliff fields, first resource variant 6/height 48/draw 1,
and header 33/34 water-mask counts 1,024/977 remain asserted.

The old 4,589 candidate figure is now verified as 4,589 accepted resource draws
in this particular supported local GELAENDE prefix. It still does not prove the
original world's global cursor or any preceding scene/player/actor callbacks.
Supplied stamp 500/mode 0/draw 7 remain explicit controlled inputs.

C: nine additional hand-derived rows extend the same reference to 44 vectors;
all 35 predecessor rows remain unchanged. The public CLI contract has 23 checks,
including an imported-module mode-0-as-mode-2 mutation in a fresh private directory
with bytecode disabled from the first child. Prior isolation failures remain in
the predecessor evidence, not reclassified as executed wrong-code proof.

C: one private actual production-TU branch mutation selects low instead of high
for mode 0 only. Legal control passes 80 assertions, including ordinary mode 1
and both old mode-2 classes. The mutant fails exactly 3 intended height assertions
(32 versus 48) while 77 pass; it still reaches legal admission and writes the
object/phase without a resource draw. Canonical source/binary remain untouched
and are checked after the post-mutation build.

NOT_RUN: original/History Edition runtime, new Unicorn emulation, full-world
publication, actual global PRNG alignment and unknown FRUCHTBAR/later callbacks.
No extra storage, state number, numeric tag, default or registry identity is added.

## Writer callback correction (2026-11-19)

D: focused re-review found that the prior read ending at 0x10082637 omitted the
writer tail. Actual AnnoWelt 0x10082040 continues to `ret` at 0x10082b0a.
The fresh wrapper bypass at 0x1008eccb–0x1008ecd3 and 0x1008ee33–0x1008ee52
is unchanged, but does not skip the following calls inside the lower writer:

- At 0x10082ab9, island slot +0x19c, concrete vtable 0x100cac2c pointer
  0x100cadc8 -> 0x10084150–0x10084360, refreshes derived chunk-height bounds.
  It uses terrain extrema and anchored item height; the type +0x58 target is
  pure AnnoGame 0x10007d50–0x10007d53, with the addressed pure float conversion
  0x100b5f90. Writes at 0x10084323/0x10084331 target bytes 3/2 of island +0x94
  chunk records, not tile records or the separate variation plane.
- At 0x10082ad8, island slot +0x188, pointer 0x100cadb4 -> 0x1008f2d0,
  receives event 0xcb. Event minus 0xc9 gives index 2, target 0x1008f2f0;
  0x1008f41e increments chunk byte 5. The World +0xbc branch at 0x1008f31c
  belongs to event 0xcc / index 3, not this call.
- The tail's separate World +0xf8 call at 0x10082afd is conditional on caller
  class bits 0x2000/0x4000. This profile passes 0x04000000, so that branch is
  not reached. No represented tile/variation write or PRNG call occurs in the
  two reached derived-cache callbacks.

D: empty-tile fill 0x100928a0 is island slot +0x1a0, pointer 0x100cadcc.
It is not the +0x19c call made at 0x10082ab9. The inherited transport attribution
is corrected separately; this does not establish its other callers or authorize
interpreting FRUCHTBAR as effect-free/draw-free.

A/C: this prepared prefix projects tile/variation/draw effects. It deliberately
omits the original derived chunk-height-bound and dirty-byte cache updates;
TileLayer's own dirty tracking is not claimed to reproduce those original bytes.
The projected fields and measured counts are unchanged. Before future World/render
consumers depend on those caches, their required initialization/rebuild must be
provided under a separate reviewed integration contract. Neither complete callback
bypass nor reproduction of the complete original writer state is claimed.

Matching modules: AnnoWelt SHA256
`f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`;
AnnoGame SHA256
`ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc`.
Frozen source 26899502 and prior numerical/gate receipts remain historical and
unchanged. This prose correction adds no runtime behavior and reruns no C++ gate.
