# WIN-NAV-EDITIONS-1: coarse navigation heights in GOG and the History Edition

Status: **READY FOR REVIEW.** The independent review (N5) is the Windows orchestrator's.

Packet: `docs/orchestration/tasks/windows-nav-editions-1.md` at `56ff354a`. This is a research and differential
verification only: no production code, identity, default, pin or rule changed.

Branch `task/WIN-NAV-EDITIONS-1` starts from `origin/main` `b5c0ada209d94a0082bc88d793c917c7222e6a21` (tree
`5aae031dbf84e489ba6cd832598f0af830faf9e1`).

Private material is git-ignored under `cache/win-nav-editions-1/`:
- listings and scripts;
- emulator and probe sources, binaries and logs;
- scan outputs.

No original bytes, listings, decoded tables or host paths are in this note.

**Labels:** **D** static instruction proof (module + address), **E** isolated original-code emulation (unicorn
2.1.4, the module's own bytes), **R** rebuild execution (private probe linked against the current `anno_sim`),
**H** independent hand or exact-rational arithmetic, **P** parsed shipped file, **L** likely (argued, not
executed), **NOT_RUN**.

## 0. Answer

- **The sum/clamp defect is CONFIRMED, and both editions agree on the rule.**
  - GOG `AnnoWelt.dll`, HE's 32-bit `AnnoWeltAddOn.dll` and HE's 64-bit `AnnoWelt.dll` (the one the HE game runs)
    all compute `candidate = (converted & 255) + (tile base byte)` with a widened sum. They keep the full maximum
    (0..510) and store `max >> 1` with no clamp (D, E).
  - The rebuild masks the sum and clamps at 255 (D, R).
  - On the six discriminating rows the three originals give 9/9, 9/127, 9/128, 9/135, 127/255, 9/9 (E). The
    rebuild gives 9/128 → 9/9, 9/135 → 9/9 and 127/255 → 127/127 (R, three mismatches).
- **The editions differ in one place: the height conversion.**
  - GOG divides on the x87 (`fdiv` by f32 0.04, `__ftol`).
  - HE86 divides in single precision (`divss`, `cvttss2si`).
  - HE64 multiplies by 25.0f in single precision (`mulss`, `cvttss2si`).
  - Over all 921 shipped object types this changes the converted component of **five** types by +1 in HE:
    FELSEN 1511/1522, KLIPPE 1552/1555 and PIONIER 3960 (H, confirmed by E for FELSEN and KLIPPE).
  - The rebuild's exact conversion equals GOG at 53- or 64-bit precision. At 24-bit x87 precision, GOG would equal
    HE (OQ-032's open question).
  - **This difference can never change a coarse class or shore:** these components are ≥ 44, so the block maximum
    is ≥ 44 in both editions, above both thresholds (H).
- **Shipped content reaches the overflow range, but no coarse class or shore changes (P, block census).**
  - 337 placed objects in GOG scenes (580 in HE's larger scene set) have a candidate above 255, up to 344.
  - In 186 GOG blocks (325 HE) the stored maximum byte differs from the rebuild's.
  - **0 blocks** change class or own-block shore.
  - With shipped metadata (component ≤ 200), a class or shore flip needs a base well above the block's vertex
    maximum (§5). Route impact is therefore **potential**, for authored or modded metadata, not demonstrated for
    shipped content.
- **Recommendation:** the faithful correction (component mask before the add, no sum mask, no clamp). No edition
  policy is needed for routing. The owner decides the ADR 0006 identity boundary (§6).

## 1. N0: identities and the frozen question

| Edition | Module or data file | SHA-256 | Arch / base |
|---|---|---|---|
| GOG 2.0.0.5 | `AnnoWelt.dll` | `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2` (matches the Mac evidence) | x86, `0x10000000` |
| GOG | `AnnoGame.dll` | `ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc` (matches) | x86, `0x10000000` |
| GOG | `Anlagen.dat` | `b2317a5c24b8c9fe796cf40f3fedaefcfa30ad3f185df9e69b2dca6a03d8dd75` (matches) | data |
| GOG | `1503Startup.exe` | `0cd03e300d1f437c6066409a2961c6b4d0ced578d81dad0ef594dc081c62e9b4` | x86 |
| HE (Steam) | `AnnoWelt.dll` (loaded by `Anno1503.exe`) | `95951b5712848cd005f009115fafddb10e2c79ae5cf79501753a9938503ae4dc` | x64, `0x180000000` |
| HE | `AnnoWeltAddOn.dll` (32-bit twin, RTTI) | `87cf9e3a1495dca01b21f0155fbb6f29dfaf5805685ab67a4c590d0e860d1e54` | x86, `0x10000000` |
| HE | `AnnoGame.dll` | `06af5111316276c73efe9dde355b06c479fa36d617478722a9700c36225b15bf` | x64 |
| HE | `AnnoGameAddOn.dll` | `e6cf3a96954fe056e52eeaa07333a70f12535e30b4b9eadb9cca4d3005586be5` | x86 |
| HE | `Anlagen.addon` (the file HE loads) | `b2317a5c…` = GOG `Anlagen.dat` | data |
| HE | `Anlagen.dat` (older variant, not loaded by HE) | `bcbaf29c584bbd4d4a892500e28bfe3249c809f2c00a1463ae62804602faaec4` | data |
| HE | `Anno1503.exe` | `6429fc8656ff03940217994b2512be62f1e3f62f22aec8bf7afff365bff61568` | x64 |

**Address forms:** all addresses below are preferred VAs (image base + RVA). No runtime relocation was observed.
HE's addresses were found from HE's own code, never copied from GOG:
- HE86 through its RTTI tables: `tInselServer` `0x1014b00c` and `tWeltServer` `0x10153e9c`;
- HE64 through its own tables (`tWeltServer` `0x1800ff650`) and by content. HE64's table slots do not follow the
  x86 order (`docs/re/forestry.md` §8).

**Rebuild baselines:**
- Main `b5c0ada2` is the merge base of the Mac production base `889143ba`. That base is 68 commits ahead: wildlife
  `sim_load` work and `resource_import.cpp`.
- The coarse files are **blob-identical** in both: `route_world.cpp` `2e9f43cc`, `route_world.hpp` `99b6a010`,
  `route_coast.cpp` `55afa6d3`, `route_cache.cpp` `c91540bb`, `route_plan.cpp` `01c80d54`, `ship_mover.cpp`
  `86b5999a`. So the "current" and "reviewed" production arithmetic is the same code; there is no delta to explain.
- Main has no `resource_import`, so the legal-import path of the Mac probe cannot run on main (§4 note).
- Mac refs inspected without merging: `origin/orch/mac-lane` `56ff354a`, research head `74d50bfe`.

**Claims to settle, frozen from the packet before any probe:**

| Claim | Separating control |
|---|---|
| (a) The component is masked to 8 bits before the add | The component-256 row must stay 9/9 |
| (b) There is no sum mask | Sums 256 and 270 must give 128 and 135 |
| (c) There is no clamp before the halving | Sum 510 must give 255 |
| (d) The thresholds are 19 and 32, compared in this order on doubled halves | Flat 19/20/31/32, far corner 32/34 |
| (e) HE equals GOG | Each row run per edition |
| (f) Shipped content reaches the separating range | Census |

## 2. N1: the side-by-side table

| Claim | GOG (`AnnoWelt` f220…) | HE86 (`AnnoWeltAddOn` 87cf…) | HE64 (`AnnoWelt` 9595…) | Result | Status |
|---|---|---|---|---|---|
| Producer = island slot `+0x19c`, 8-tile blocks of the clipped rectangle | `0x10084150`–`0x10084360` | `0x10033730`–`0x100339a4` | `0x1800448e0`–`0x180044c1c` | same loops and extents | D |
| 9 × 9 unsigned vertices, stride W + 1, min starts 255, max 0 | `0x10084208`–`0x10084259` | `0x100337f0`–`0x100338bb` (row unrolled) | `0x1800449e0`–`0x180044acb` (row unrolled) | same | D |
| 8 × 8 tiles; anchor = record byte 3 bit 7 | `test [rec+3],0x80` `0x10084287` | `cmp byte [rec+3],0; jge` `0x100338e8` | `cmp byte [rec+3],0; jge` `0x180044b09` | same | D |
| Type lookup, then the height getter | registry `+0x70`, type `+0x58` → Game `0x10007d50` `fld [desc+8]` | registry `+0x70`, type `+0x58` → GameAddOn `0x10003bc0` `fld [desc+8]` | registry `+0xe8`, type `+0xb8` → Game x64 `0x1800010c0` `movss [desc+0xc]` | same value | D |
| HEAD `[0x14]` → descriptor | Game `0x10009372`/`0x10009384` → `+8` | GameAddOn `0x10003d96`/`0x10003de8` → `+8` | Game x64 `0x180002aa6`/`0x180002aac` → `+0xc` | same raw f32 | D |
| Conversion | `fdiv f32 0.04` (`0x100c9c20`) `0x100842a3`, `__ftol` `0x100b5f90` | `fstp` f32, `divss f32 0.04` (`0x10147884`) `0x1003390c`, `cvttss2si` | `mulss 25.0f` (`0x1800fff24`, loaded `0x1800449a2`) `0x180044b28`, `cvttss2si` | **differs in rounding** (§3) | D, E, H |
| Component masked before the add | `and eax,0xff` `0x100842b6`, `add` `0x100842bb` | `movzx ecx,al`, `add` `0x10033918`/`0x1003391e` | `movzx ecx,al`, `add` `0x180044b30`/`0x180044b36` | same | D, E |
| Sum widened, unsigned max, no sum mask | `cmp/jbe` 32-bit `0x100842bd` | `cmp/cmova` 32-bit `0x10033920` | `cmp/cmova` 32-bit `0x180044b38` | same | D, E |
| Halve without clamp; byte stores | `shr 1` both, store +3 max, +2 min (`0x1008431f`–`0x10084331`) | `shr 1` both, stores `0x1003995c`–`0x1003397c` | `shr 1` both, stores `0x180044bbe`–`0x180044bc7` | same | D, E |
| Chunk record | 16 bytes, `[island+0x94]`, cols `[+0x8c]`, no bounds check | 16 bytes, same fields | 24 bytes, `[+0x118]`, index bounds-checked against `[+0x110]·[+0x114]` | layout only | D |
| Null registry | not checked | not checked | skipped (`0x180044b04`) | no game effect | D |
| Coarse builder | world slot `+0xf8` `0x100aae00` | `tWeltServer` slot `+0xf8` `0x100ae6d0` | `tWeltServer` `0x1800dc0e0` | | D |
| Class: `T1 ≥ 2·max` → water, then `T1 < 2·min` → land, else mixed | `0x100ab0b8`–`0x100ab0e7`; T1 = `ftol(W/0.04)&255` | `0x100ae9e9`–`0x100aea15`, constant `0x13` | `0x1800dc4cc`–`0x1800dc4f4`, constant `0x13` | same order | D |
| Shore: `2·min < T2 < 2·max` strict | `0x100ab94b`–`0x100ab966`; T2 = `ftol(G/0.04)&255` | `0x100aec20`–`0x100aec34`, constant `0x20` | `0x1800dc7d4`–`0x1800dc7e2`, constant `0x20` | same | D |
| Threshold values | static initializers: A = 0.04·16 (`0x100a3b40`), G = 2A (`0x100a3ba0`), W = G − 0.5 (`0x100a3bf0`); T2 = 32, T1 = 19 | 19, 32 compiled in | 19, 32 compiled in | equal | D + H |

The fine query's byte add (GOG `0x100904e7`) is a different consumer. It is not compared here and must not be
changed by this repair.

## 3. The conversion difference (H, E)

- **The forms:** with `D = f32(0.04)`:
  - GOG computes `trunc(round_p(h / D))`, where `p` is the x87 precision control in force (53 by MSVC default,
    24 under a Direct3D FPU change, see OQ-032);
  - HE86 computes `trunc(round_24(h / D))`;
  - HE64 computes `trunc(round_24(h · 25))`;
  - the rebuild's `object_top_units` computes `floor(h / D)` exactly.
- **The six rows:** all variants give the same component on the six discriminating rows. The packet chose them
  away from conversion boundaries.
- **Shipped heights:** over all 921 shipped `HEAD[0x14]` values, only five differ: HE gives +1 for 1511 and 1522
  (1.8f: 44 → 45), 1552 and 1555 (2.6f: 64 → 65) and 3960 (3.6f: 89 → 90).
  - GOG at 24-bit precision equals HE.
  - GOG at 53 and 64 bits equals the rebuild.
  - No height is negative or non-finite.
- **Emulation (E):** FELSEN and KLIPPE with base 17 give max halves 30 and 40 on GOG at PC 53 and 64. They give 31
  and 41 on GOG at PC 24, on HE86 (independent of PC) and on HE64. The unicorn emulator honours the x87 precision
  control.
- **No class or shore effect:** each candidate is ≥ component ≥ 44, so `2·max_half ≥ 44`. That exceeds both T1 = 19
  and T2 = 32 in both editions, and the shore test then depends on `min` alone, which the conversion does not
  touch (H).

## 4. N2/N3: the boundary cases and the controls

**Rig:**
- **Emulation (E):** a 16 × 16 island at (0, 0), block (0, 0), object 700 anchored at local (3, 3). It runs each
  module's own producer bytes. Two stubs mirror the D-read bodies: the registry lookup returns the authored type,
  and the getter is `fld [ecx+8]` (x86) or `movss xmm0,[rcx+0xc]` (x64). GOG and HE86 were run at x87 PC 53, 24 and
  64.
- **Rebuild (R):** the Mac rig. A 24 × 24 island at (8, 8) in a 40 × 40 world; cell (1, 1); object 700 at
  (11, 11). Its object id, anchor and item-height byte are set with `TileLayer`'s setters and checked before the
  query.
- **Expected values** were written into the probe source by hand before it ran.
- **Class and shore:** for E they come from the stored bytes by the D-read rules (§2). For R they come from
  `classify_cell` and the coast index's own type word.

| Row (f32 bits) | Expected halves / class / own shore | GOG E (PC 53/24/64) | HE86 E | HE64 E | Rebuild canonical R | Sum-mask-only R | Corrected R | No component mask R |
|---|---|---|---|---|---|---|---|---|
| c2 + b16, `3da3d70a` | 9/9 water no | = | = | = | = | = | = | = |
| c239 + b16 = 255, `4118f5c3` | 9/127 mixed yes | = | = | = | = | = | = | = |
| c240 + b16 = 256, `4119999a` | 9/128 mixed yes | = | = | = | **9/9 water no** (first failure) | 9/127 mixed yes | = | = |
| c254 + b16 = 270, `41228f5c` | 9/135 mixed yes | = | = | = | **9/9 water no** | 9/127 mixed yes | = | = |
| c255 + b255 = 510, flat 255, `41233333` | 127/255 land no | = | = | = | **127/127 land no** | 127/127 | = | = |
| c256 masked + b16, `4123d70a` | 9/9 water no | = | = | = | = | = | = | **9/136 mixed yes** |
| Flat 19 / 20 / 31 / 32 | 9/9 water, 10/10 land, 15/15 land, 16/16 land; no shore | = | = | = | = | = | = | = |
| Far corner 32 / 34 on flat 18 | 9/16 mixed no, 9/17 mixed yes | = | = | = | = | = | = | = |
| Flat 18, no object | 9/9 water no | = | = | = | = | = | = | = |

**Totals (R):** canonical 13 rows, 3 mismatches (exit 1). Sum-mask-only correction: 3 (exit 1). Corrected: 0
(exit 0). No component mask: 1 (exit 1). This is the pattern the packet predicted.

**Notes:**
- In R, the coast index's derived shore type follows the own-block straddle in every row.
- The variants are private copies of `route_world.cpp` with exact one-time replacements, linked ahead of the
  unchanged library. Canonical source never changed.
- **Setup legality:** main has no `resource_import`, so the decisive tile is set directly. Two sources prove the
  profile reachable: the Mac's real-import proof at `889143ba` (not re-executed here), and §5. Shipped scenes
  contain real anchored objects with candidates 256..344.
- **Runtime of the original games:** NOT_RUN. The HE session belongs to the owner, and no GOG runtime
  observation was set up. The verdicts rest on D + E per edition.

## 5. Shipped-content scan (P, H)

**Scope:**
- GOG: every `.scp`/`.sze` under `Scenes`: 515 files, 1,133 islands, 4,585,381 placed items.
- HE: `Scenes` and `Languages/*/Scenes` (English and German, the multiplayer scenes included): 571 files, 1,842
  islands, 7,458,405 items.

**Predicates:**
- **Anchors:** every `GELAENDE`/`PROD2`/`FLUESSE` item's top-left tile is an anchor with base = the item's stored
  height byte (`state >> 8`, `docs/formats/insel.md`).
- **Approximation:** items that a later item overwrites are still counted.
- **Vertices:** the `HEIGHT` grid with zero right column and bottom rows (`docs/re/ship-navigation.md` §9.2).
- **Blocks:** island-local 8 × 8.
- **Components:** GOG at 53-bit precision, HE64, and the rebuild's arithmetic.

**Items:**
- GOG: 337 items have candidate > 255, maximum 344, e.g. FELSEN 1533 (component 112) at base 160–176.
- HE: 580.
- The count is the same under every conversion variant.

**Blocks:**
- 186 GOG blocks (325 HE) have an original maximum above 255. In each, the rebuild's stored max byte differs.
- **0 blocks change class, and 0 change own-block shore.** That is GOG original against the rebuild, and GOG
  against HE.
- GOG and HE stored bytes differ in 2 GOG blocks (4 HE).

**Bound for later placements (L):** components in shipped metadata are at most 200.
- **Water flip:** the rebuild's wrapped maximum must be ≤ 19. That needs vertex max ≤ 19 and a candidate in
  256..275, so a base ≥ 56 on a block whose vertices are all ≤ 19.
- **Shore flip:** needs a base ≥ 23 above a block whose vertices are all ≤ 33.
- **Placement bases** from the D-read placement modes are a corner value of the anchor or footprint:
  - mode 1: the origin quad;
  - mode 2: the quantized footprint minimum (`docs/sim/wildlife-cliff.md` at the Mac research head `74d50bfe`; not
    on main).
- These bases do not exceed the anchor tile's corners, which are inside the block. So shipped metadata cannot
  produce a flip through these modes.
- Authored or modded metadata, or file-stored bases, can (the Mac's KLIPPE profile does).

## 6. N4: compatibility and recommendation

**Chain (D in the current code):**
1. `WorldTileSource::block_range` (`route_world.cpp`) feeds `classify_cell` and `CoastIndex::range_at` /
   `derived_shore` (`route_coast.cpp`, own straddle of the cell and its 8 neighbours).
2. The coast index lives in `RouteCache` (`world.hpp` `route_cache_`: derived, host-facing, not saved or hashed),
   synced by `sync_once` in `ShipMover::visit`.
3. `WorldPlanner` → `plan_route` (`ship_mover.cpp` 150–159, 271–272).
4. `NavMotion`: 10 fields, saved and hashed (`state_fields.hpp` 1183).
5. No draw site sits in `block_range`.

**Compatibility:**
- **Byte layout:** no change (derived bytes only).
- **Behaviour:** any changed class or shore changes masks and types, then routes, then `NavMotion` and later
  timing. For shipped content, §5 shows no class or shore change, so shipped scenes as loaded would plan the same
  routes. A difference needs authored or modded metadata or stored bases outside the bound.

**Recommended remedy:** the editions agree on the arithmetic, so take the faithful correction, the Mac's proposed
`WILDLIFE-NAV-REPAIR-1`:
- In `route_world.cpp` `block_range`: `top = (object_top_units(...) & 0xff) + base`, and `max_half = mx >> 1`.
- Correct the header comment.
- Add the 13 rows above as permanent independent expectations in `route_world_test.cpp`.
- Do not touch `object_tile_blocks` or the fine rule.
- Keep `object_top_units` exact. That equals GOG at 53/64-bit precision, and the one-unit HE difference on five
  types has no class or shore effect (§3).

**Owner decisions** (none is taken here):
1. **The identity boundary.** Under ADR 0006 item 4, a change of default behaviour moves a section base version.
   Main has `kSectionVersions`, all at 1. The candidates are `header` (it carries `Rules`/`NavRules`, so peers
   would refuse before planning) or `figures` (owner of `NavMotion`). The follow-ups are the section table,
   `state.md`, the identity and refusal tests, and reviewed pins or replays only if authorized. No tag or version
   is allocated here.
2. **Rebaselining.** Given §5, a rebaseline should show no change on shipped-scene trajectories. That can be
   checked by the repair packet's real-data canary rather than assumed.
3. **Edition policy for the conversion:** not needed for routing (§3). It only matters if the unsaved halves must
   ever match HE byte for byte, and that would be an optional rule, not a default.

The secondary world raster's FP capture (`docs/testing/wildlife-raster-fp-capture.md`) is a separate question and
not a prerequisite for this verdict.

## 7. Verdict per edition

| Edition | Sum mask / clamp defect in the rebuild | Conversion |
|---|---|---|
| GOG | **Confirmed** (D + E, all 13 rows; shipped overflow exists, no shipped class/shore change) | Rebuild exact = GOG at 53/64-bit; at 24-bit GOG = HE (OQ-032 open) |
| HE (HE64 runs; HE86 twin) | **Confirmed** (D + E, same 13 rows) | Single precision; +1 on five shipped types, no class/shore effect |

## 8. Limits and NOT_RUN

- Original runtime observation: NOT_RUN in both editions.
- The full route witness through `plan_route`: not run. The impact stays potential.
- The census counts overwritten items as anchors (an approximation). It treats scenario islands at their local
  8-block grid. Misaligned islands use tile classification in the rebuild.
- Readers of chunk bytes `+2`/`+3` other than the builder were not surveyed in the originals.
- The C++ acceptance gates are not required for this diff (docs only); the private probe is not a gate.
- Hosted CI is read after the push.

## 9. Private receipts (`cache/win-nav-editions-1/`, SHA-256)

| Receipt | SHA-256 |
|---|---|
| `emu_coarse.py` | `6c68925a1a9ef4c0c76659eac7c3cd94bfc4b4e300f7ffb743991498d2d30d86` |
| `emu_coarse.log` | `cb02f610e6310640f72a529b5781630d0cb2364352f76534ba4af2d77941a46d` |
| `gog_producer.lst` | `82f34f45075cef1a5b2818af2de3edbaaf2fb212affdff08113c867ec8c21339` |
| `he86_producer.lst` | `8b33227a0b222f7c1fd9686e8f94d099d7bc5f2f356348bdbdd83ef96a125d5b` |
| `he64_producer.lst` | `aa60ec62a0b772dee0a24991c7ac93d3e76e937f51547f6645fc32d767b0a53c` |
| `gog_builder.lst` | `3eb4ce349b859126d7997ff980e744933b1c8215ea1c4af3d6bbf15db4e5be12` |
| `he86_builder.lst` | `da71f327c1597adcc30b9d4a07b3309531df87850ec61bae93f4f495a28cae8a` |
| `he64_builder.lst` | `85543d8ed2bf64a5c612cc8b8243071bc2c26fe5e8e20ed4a9134be8befe3ee1` |
| `navconv.py` | `5fe5d10e05b613e62d312b05069277b2ae500d522a3415011142694d9a60baa6` |
| `navitems.py` | `a7fc46378770f3d014be2c80562b2245dd9c83f2401dcbf8ea2e7dbed7aad8a9` |
| `navblocks.py` | `279ee325f9ae23527f8a61f80d126d8c22d45d23b572117b539ae037ead67135` |
| `navscan_gog.txt` | `a8aaba76f944becde5ff657c8f78262351e4f6b12209a8b7a1730f91bd86c1e2` |
| `navitems_gog.txt` | `c25b98a6e0407fda92ea5c133999429444a8311e53bf603f363a54993f87ca17` |
| `navitems_he.txt` | `ecef9e1eb5aa4f38ef2cffd2893b9b0b71b76b37d4555aaed6ce175a5e46be22` |
| `navblocks_gog.txt` | `3383f6ec4cce624f3d3c5b2eac5ea03eaa7bc703c34d1089628504df2102be5e` |
| `navblocks_he.txt` | `a29ab54878ec4d79b0d6497f9833879f86f3735b330d8174debf0d71c0f3cc5b` |
| `probe/probe.cpp` | `6c7d0427ed9624d598b14fd23938e5d0dc17988b7aa54d24372575437e777d69` |
| `probe/route_world_sum_only.cpp` | `4d792c9da8bc73b57d389028cdf1784fcf44cdad7d75550fe5a04e3a5dab174a` |
| `probe/route_world_corrected.cpp` | `7d22ffb6c588fc90637187952e69472527e10c91ad9e023c3b8795815f8ea696` |
| `probe/route_world_no_component_mask.cpp` | `97c91d5a24495cef3cb0858614025b6082878dffac9d859f35a826f8a93c4429` |
| `probe/run-canonical.log` | `5176fd725cab250c269b8d4f92b35001aaa835a8e21c9a7232e88b8e3636260b` |
| `probe/run-sum_only.log` | `df8069b7c5be4f69b69abb2bb56baf3541c37db9ec2abdb46c98a2c8745c193d` |
| `probe/run-corrected.log` | `b91537c6d0c84285dffa699ddfbb0e34fc99cbd3896d4afbd1e707bc230cf35c` |
| `probe/run-no_component_mask.log` | `c44d51dda3af25ee32482cf63b3aa5ace3827718d821a8450fc14178293d6029` |
| `probe-canonical.exe` | `d40d409b305d5aa0cdebf66f96fb2db5c93d17fe78198dab2fe8dc08fe41004b` |
| `anno_sim.lib` (Release, this branch's base) | `d0ff6fa9cacb4c47afdb2538459bfcbdc672eb3032a2d9b09fce4e91bd98072b` |
| Rebuild `route_world.cpp` blob (main and `889143ba`) | git `2e9f43cc5943ff4a1cc0cffc783522dafb1f1993` |

Tools: system Python 3.14 with capstone 5.0.7 and pefile 2024.8.26; a private venv with unicorn 2.1.4; MSVC 2022
Release for `anno_sim` and the probe; the Windows PC alone.
