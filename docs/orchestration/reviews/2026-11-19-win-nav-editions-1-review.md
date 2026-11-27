# Review: WIN-NAV-EDITIONS-1 (N5, independent, read-only)

Reviewed: branch `task/WIN-NAV-EDITIONS-1` head `adb430b85a96a9771684ddc068165c8f8c937fed` (evidence note and
REPORTS entry), against the packet at `56ff354a` and the rebuild at `b5c0ada2`. Both original editions were
inspected from their installed bytes, read-only. My own scripts and outputs are private (git-ignored cache).

**Labels (mine):** **D** my own disassembly read, **H** my own hand or exact-rational arithmetic, **P** my own parse
of shipped metadata through the repo's chunk reader. I ran no emulation (no **E**) and no rebuild probe (no **R**).

## Verdict: ACCEPTED WITH NOTES

The diagnosis holds. All three original producers compute `max = max(vertex max, (component & 255) + base)` as a
widened unsigned value, keep the full 0..510 maximum and store `max >> 1` and `min >> 1` with no sum mask and no
clamp. The rebuild at `b5c0ada2` masks the sum and clamps. Both editions agree on this rule, so the faithful
correction needs no edition policy. The notes below fix one typo, sharpen two labels and record two limits. None
of them changes the verdict or the recommendation.

## 1. Independently verified

**Hashes (all match the evidence note):** GOG `AnnoWelt.dll` `f220a878…`, `AnnoGame.dll` `ac721220…`,
`Anlagen.dat` `b2317a5c…`; HE `AnnoWelt.dll` `95951b57…` (x64, base `0x180000000`), `AnnoWeltAddOn.dll`
`87cf9e3a…` (x86), `AnnoGame.dll` `06af5111…`, `AnnoGameAddOn.dll` `e6cf3a96…`, `Anlagen.dat` `bcbaf29c…`,
`Anlagen.addon` `b2317a5c…` (byte-identical to GOG `Anlagen.dat`), `Anno1503.exe` `6429fc86…` (x64). The
x64 `AnnoFrame.dll`/`AnnoScene.dll` name `AnnoWelt.dll`; a 64-bit `Anno1503.exe` cannot load the x86 AddOn twin,
so HE64 is the producer HE runs (D, import strings and PE machine fields).

**How I found the HE producers.** I swept each module's `.text` for references to f32 0.04 and 25.0 and,
separately, for a `shr …,1` next to byte stores at record `+2` and `+3`. The store pattern gave exactly one
cluster per module: GOG `0x10084323`, HE86 `0x1003397c`, HE64 `0x180044bc7`. I took the function bounds from the
prologue and `ret` (HE86) and from the x64 unwind table, including its chained entries (HE64). No GOG address
was transplanted.

| Fact | GOG (D) | HE86 (D) | HE64 (D) | Note's address |
|---|---|---|---|---|
| Function bounds | `0x10084150`–`0x10084360` | `0x10033730`–`0x100339a4` | `0x1800448e0`–`0x180044c1c` | agree |
| 9×9 unsigned vertices, min 255, max 0 | `0x10084208`–`0x10084259` | `0x100337f0`–`0x100338bb` | `0x1800449e0`–`0x180044acb` | agree |
| Anchor = record byte 3 bit 7 | `test …,0x80` `0x10084287` | `cmp …,0; jge` `0x100338e8` | `0x180044b09` | agree |
| Lookup → height getter | `+0x70`, `+0x58`; Game `0x10007d50` `fld [ecx+8]` | `+0x70`, `+0x58`; GameAddOn `0x10003bc0` `fld [ecx+8]` | `+0xe8`, `+0xb8`; Game `0x1800010c0` `movss [rcx+0xc]` (vtable slot `+0xb8`, three vtables) | agree |
| Constant | `0x100c9c20` = bits `0x3d23d70a` | `0x10147884` (0.04f) | `0x1800fff24` (25.0f), loaded `0x1800449a2` | agree |
| Conversion | `fdiv` `0x100842a3`; `0x100b5f90` = `__ftol` (RC truncate, `fistp` qword, low dword) | `fstp` f32, `divss` `0x1003390c`, `cvttss2si` | `mulss` `0x180044b28`, `cvttss2si` | agree |
| Component masked before the add | `and eax,0xff` `0x100842b6`, `add` `0x100842bb` | `movzx ecx,al` `0x10033918`, `add` `0x1003391e` | `movzx ecx,al` `0x180044b30`, `add` `0x180044b36` | agree |
| Base = record byte +4, unsigned | `and ecx,0xff` `0x100842b0` | `movzx` `0x1003391b` | `movzx` `0x180044b33` | agree |
| 32-bit unsigned max, no sum mask | `cmp/jbe` `0x100842bd` | `cmp/cmova` `0x10033920` | `cmp/cmova` `0x180044b38` | agree |
| Halve, no clamp, byte stores (+3 max, +2 min) | `0x1008431f`–`0x10084331` | `0x1003395c`–`0x1003397c` | `0x180044bbe`–`0x180044bc7` | **typo, note 1** |
| Class: `19 ≥ 2·max` water, then `19 < 2·min` land | `0x100ab0b8`–`0x100ab0e7` | `0x100ae9e9`–`0x100aea15`, imm `0x13` | `0x1800dc4cc`–`0x1800dc4f4`, imm `0x13` | agree |
| Shore: `2·min < 32 < 2·max` | `0x100ab94b`–`0x100ab966` | `0x100aec20`–`0x100aec34`, imm `0x20` | `0x1800dc7ce`–`0x1800dc7e2`, imm `0x20` | agree |
| Descriptor store of the raw f32 | Game `0x10009384` → `+8` | not checked | Game `0x180002aa6`/`0x180002aac` → `+0xc` | partly checked |

**Hand-derived rows (H)** (`orig` = `max(v, (c & 255) + b) >> 1`; vertices flat at `v`):

| Row (c, b, v) | Original | Sum wrapped to 8 bits | Sum clamped at 255 | Component mask removed | Rebuild `b5c0ada2` |
|---|---|---|---|---|---|
| 2, 16, 18 (control) | 9/9 | 9/9 | 9/9 | 9/9 | 9/9 |
| 239, 16, 18 | 9/127 | 9/127 | 9/127 | 9/127 | 9/127 |
| 240, 16, 18 | 9/128 | 9/9 | 9/127 | 9/128 | **9/9** |
| 254, 16, 18 ("270") | 9/135 | 9/9 | 9/127 | 9/135 | **9/9** |
| 255, 255, 255 | 127/255 | 127/127 | 127/127 | 127/255 | **127/127** |
| 256, 16, 18 ("component 256") | 9/9 | 9/9 | 9/9 | 9/136 | 9/9 |

- **What each row separates.** The 256 and 270 rows separate wrapping and clamping from the original, and the
  510 row separates them even though the class (land) agrees. The component-256 row is the only row that catches
  a deleted component mask.
- **The rebuild's code.** It computes `(c + b) & 0xff` and then `min(max, 255) >> 1`. Its mask on the whole sum
  happens to equal the original on the component-256 row, so that row cannot catch the rebuild's defect.
- **The rebuild's class.** For the 256 and 270 rows the rebuild returns `AllWater` (19 ≥ 18) where the original
  returns Mixed with shore.
- **The flat controls.** Flat 19/20/31/32 and far corner 32/34 match the note's expected column under the D-read
  rules.
- **The f32 bit patterns.** The six patterns convert to 2/239/240/254/255/256 under every variant: exact, x87 PC
  24/53/64, `divss` and `mulss` 25 (H).

**Conversion difference (P, H).** I computed every shipped type's `HEAD[0x14]` exactly in three files: GOG
`Anlagen.dat` (921 types), HE `Anlagen.dat` (909) and HE `Anlagen.classic` (894).
- **Five types differ.** HE86 `divss` and HE64 `mulss` (equal to each other on all shipped values) give +1
  against GOG at PC 53/64 on 1511, 1522 (44→45), 1552, 1555 (64→65) and 3960 (89→90). x87 PC 24 equals HE.
- **The rebuild's conversion.** The exact `object_top_units` equals GOG PC 53/64 on all types.
- **No shipped range problem.** No height is negative; the maximum component is 200.
- **FELSEN 1533 is 112** under every variant.
- These match the note.

## 2. Discrepancies and notes

1. **Typo:** §2 gives the HE86 halving stores as `0x1003995c`–`0x1003397c`. The `shr ebx,1` is at `0x1003395c`.
2. **The "sum-mask-only" label.** The private variant (diffed against canonical; source hashes match the
   receipts) moves the mask onto the component and keeps the 255 clamp. A literal "remove only the sum mask"
   variant (component unmasked, clamp kept) gives 9/127 on the component-256 row: 4 mismatches, not 3. The
   repair packet's test names should say "component mask, clamp kept".
3. **The census does not err in only one direction.** The report says counting overwritten items as anchors
   "can only add anchors, so 0 class/shore changes is not made easier". A phantom anchor can raise the rebuild's
   wrapped maximum above 19 or 32 and so hide a real flip. The 0-flip result is therefore P with an approximation
   that can go either way. The §5 bound (L) is the stronger support. The repair packet's real-data canary should
   use the true surviving anchors.
4. **x87 precision in GOG.** OQ-032 asks about load time. The coarse producer runs during play.
   - GOG `AnnoDisplay.dll` imports DirectDraw and carries the `IID_IDirect3D7` interface id (D, presence only;
     cooperative-level flags not traced).
   - So a single-precision control word at producer time is plausible. GOG's value for the five types is
     therefore unresolved, not "= rebuild".
   - This cannot change any class or shore: every candidate is ≥ 44, so `2·max_half ≥ 44 > 32`, and the shore
     then depends on `min` only (H, agreeing with §3).
   - It affects only the stored max byte. Suggest extending OQ-032 to this producer.
5. **HE64's bounds check.** On an out-of-range index HE64 sets the record base to 0 and still stores (to address
   2 and 3). "Bounds-checked" means it faults rather than skips. This has no effect on valid input.
6. **HE classic data.** `Anlagen.classic` adds 2246 and 2247 (64→65) to the five, seven types in all. I did not
   verify which file HE loads in which mode. It is immaterial to routing for the same reason as note 4.
7. **Extreme heights (repair-packet scope).** The domains differ for absurd authored heights:
   - `object_top_units` saturates at 2^40 before the component mask;
   - GOG keeps the low byte of a 64-bit `fistp`;
   - `cvttss2si` returns `0x80000000` (low byte 0) once the quotient reaches 2^31.

   Shipped heights are far below this. The repair should state the supported domain rather than claim parity
   there.
8. **Hygiene.** The committed diff (`b5c0ada2..adb430b8`) touches only `REPORTS.md` and the evidence note.
   - No host paths, no hex dumps, no listings, nothing under `cache/`.
   - Three metadata heights are quoted (1.8f, 2.6f, 3.6f). They are small single values, acceptable in my
     view; the orchestrator may reduce them to the integer components.

## 3. Not verified by me

- Emulation (E) rows and rebuild probe runs (R). I re-derived their expected values (H) and checked the variant
  sources only.
- The scene census counts (515/571 files, 337/580 items, 186/325 blocks, 0 flips). Not re-run.
- GOG's threshold initializers. The threshold float lies in zero-initialised `.data` in the file, so T1 = 19 and
  T2 = 32 rest on the note's initializer reading. HE's immediates `0x13`/`0x20` I did read.
- The HEAD-buffer → stack-slot read before the descriptor copies.
- Readers of block bytes +2/+3 other than the builder.
- Original runtime in either edition. It stays NOT_RUN.

## 4. Assessment of the recommendation and owner decisions

- **The recommended correction is right and minimal.** It is
  `top = (object_top_units(..) & 0xff) + base`, `max_half = mx >> 1`, with the fine-rule `object_tile_blocks`
  untouched (GOG `0x100904e7` is a separate byte add).
- **Leave `object_top_units` exact.** The edition conversion difference cannot alter class or shore on shipped
  types.
- **The 13 rows should become permanent tests.** Include the component-256 row (the only guard against deleting
  the component mask) and the 510 row (the only guard against the clamp when the class agrees).
- **The owner decisions are correctly left open.**
  - The ADR 0006 section to bump (`header` vs `figures`).
  - Rebaselining, gated on a real-data canary.
  - An edition policy for the conversion: only as an optional rule, and only if byte parity with HE is ever
    wanted.
- **Compatibility is separated correctly.** Byte layout is unchanged, behaviour changes only for flips, and
  shipped content shows no flips under an approximate census (note 3).
