# House-tick inputs, lifecycle and calculation contract

T32.6 F1 (round di). This note replaces the round-cy audit at `cfd85ec59d38f986bf1f0bde7d2ddcc5e6e2db88`. Inspected engine code is that tree (P1 integrated; `TickKind::House` still returns at `driver.cpp`). Stage-mask facts are the accepted T32.5 F1 text at `1fc598eb0b57f0d487fb50d315d7c4ad9c0d8af2` (`docs/re/population.md` §15, including §15.7 S9a / OQ-185 and §15.8 S9b / OQ-462), read only. Original files were read under `E:\anno1503\game\app` and were not copied. Scratch disassembly and the arithmetic script are under this worktree's ignored `cache/t326-f1/`. No source, test, golden, shared OQ or spec edit, and no P2 implementation.

The round-cy note is wrong in four places this document corrects. Both need endpoints are not post-pass products, and Force does not decide them. The proposed kernel must not return a "would fire" flag for a transition whose inputs it does not have. Mood overrides are not unconditional. A missing billing cross-reference does not make every callee store-free.

Round dj S1: the below-64 correction is a signed compare. AnnoWelt `0x1007654b` is `cmp bp, 0x40` and `0x1007654f` is `jge`. Resident −1 falls through that correction. The earlier unsigned reading of the same branch is withdrawn.

## 1. What the loader actually writes

AnnoGame `BGruppen` parser, image base `0x10000000`. Group records are 6 × `0x118` bytes at object `+0x9c4`, cleared when `[BGRUPPE]` starts (`0x10013ef5`–`0x10013f06`, `0x1a4` dwords). The current group pointer is `[esp + 0x14]`. The need index lives at `[esp + 0x20]` and is set to `-1` when a `BGruppeNr` name selects a slot (`0x100140df`–`0x10014105`: slot × `0x118`, byte `+0` = slot, `+0x114` = goods table). That index is not the file cursor: the cursor is a different stack slot, saved around the key parse at `0x10013f4b` before `add esp, 0x10`.

`this` for the parser is the text reader (`edi = [ebp + 8]` at `0x10013f21`). The group record is the pointer above, not `this`.

### 1.1 `+0xcc` is written while parsing `Ware` (`0x1001486b`–`0x100148a6`)

On a goods-name match, `ebx` is the goods id (`0`…`0x5f`). `ecx` is the need index. The match path does `inc ecx`, stores the id at `[group + ecx*4 + 0x64]` (slot 0 on the first need), saves `ecx`, then `inc cl` and stores that byte at `[group + 2]`. So `n` is the low 8 bits of (slot + 1), written for every accepted `Ware`, including infrastructure.

`+0xcc` is then updated from that new `n` by the id alone. The post-pass never stores `+0xcc`.

| Goods id | Effect on `+0xcc` |
|---|---|
| `< 0x4c` | `+0xcc = n` |
| `0x4c`…`0x5a` | unchanged |
| `> 0x5a` | `+0xcc = n` only when `+0xcc` is still 0; otherwise unchanged |

Force is not read here. A later `max` updates `+0xcd` only.

On the shipped file, consumer goods precede infrastructure and every infrastructure id is in `0x4c`…`0x5a`, so `+0xcc` ends as the index of the first infrastructure need. That is a data invariant, not the rule. Filtering "goods versus infra" or "forced versus not" does not reproduce the three rows above.

### 1.2 Post-pass (`0x10014afc`–`0x10014bd9`)

`edi` starts at object `+0x9c4` and steps by `0x118` while it is strictly below the last group pointer plus `0x118` (`0x10014ad8`–`0x10014aeb`, compare at `0x10014bd3`). The record at that end address is not processed. A hole between the base and the last named slot is processed, and it is still the cleared zeros. Per group:

1. **Sentinel.** Bytes `[group + 3 + id]` for `id = 0`…`0x5f` are filled with `n` (`0x10014afe`–`0x10014b09`). `n` means "not a direct need".
2. **Direct lookup, in slot order** `j = 0`…`n-1` (`0x10014b1b`–`0x10014b76`). `lookup[Ware(j)] = j`. A duplicate id overwrites; both slots remain in the need array and `n` counts both.
3. **Infrastructure side effects, only when `Ware(j)` is in `0x4c`…`0x5a`.** `+0xcd = j + 1` (`0x10014b2f`–`0x10014b33`) for every such need, forced or not. Force is tested afterwards (`0x10014b39`): a non-zero Force byte ORs bit `(id − 0x4c)` into the dword at `+0xc4` (`0x10014b40`–`0x10014b5a`). Force does not decide whether `+0xcd` moves.
4. **WIRTSHAUS alias** (`0x10014b60`–`0x10014b6d`). If this need's id is `0x4c` and `lookup[0x1d]` is still the sentinel `n`, set `lookup[0x1d] = j`. A real `ALKOHOL` need, or an earlier tavern that already took the alias, blocks the write. The second tavern in one group therefore updates `lookup[0x4c]` and leaves the alias on the first tavern slot.
5. **One substitution pass** over infrastructure ids `0x4c`…`0x5a` (`0x10014b78`–`0x10014bab`). The id's `IdSec` is the byte at goods-record `+1` (object `+ 0x7bd` for id `0x4c`, stride `0x1a`). If `lookup[id]` is still `n` and `lookup[IdSec]` is not `n`, copy that slot. Ids ascend, so a chain whose substitute has a lower id is visible in the same pass. The pass does not walk further on its own.
6. **`+0xcd = max(+0xcd, +0xcc)`** (`0x10014bad`–`0x10014bbd`), unsigned byte compare. A group with no infrastructure id keeps `+0xcd` at the cleared 0 until this raises it to `+0xcc`.

`IdSec` and `BauFirst` are `[WAREN]` fields (goods `+1` and `+2`). `parse_house_groups` does not read them. `GoodsInfo` holds `name` and `forsch` only (`types.hpp`).

### 1.3 Shipped groups

Derived by applying §1.1–§1.2 to `Data/BGruppen.dat` `[BGRUPPE]` / `[WAREN]`. Goods ids are the AnnoGame name table (`0x10024bd0` is the stage-name table, kind `0x21`; goods names are the table `goods_table.cpp` already records). `Menge` uses the loader integer `(h·4096 + 5000) / 10000`. Stage index is the slot of the `BauFirst` name in the 32 names at `0x10024bd0` (`INFRA_EVER` = 0 … `INFRA_STUFE_FORSCH` = 31). `NoUpgrade` / `NoDowngrade` absent means the cleared bit stays 0.

| Group | `n` | `+0xcc` | `+0xcd` before max | `+0xcd` | Forced-infra mask | `lookup[ALKOHOL]` |
|---|---|---|---|---|---|---|
| PIONIER | 6 | 4 | 6 | 6 | `0` | slot 4 (WIRTSHAUS), even though that need is not forced |
| SIEDLER | 10 | 6 | 10 | 10 | `0x41` (WIRTSHAUS, KAPELLE) | slot 6 |
| BUERGER | 12 | 7 | 12 | 12 | `0x91` (WIRTSHAUS, SCHULE, KIRCHE) | slot 7 |
| KAUFLEUTE | 11 | 7 | 11 | 11 | `0xa3` (WIRTSHAUS, BADEHAUS, HOCHSCHULE, KIRCHE) | slot 7 |
| ARISTOKRATEN | 8 | 4 | 8 | 8 | `0x286` (BADEHAUS, THEATER, KIRCHE, PARK) | sentinel 8 (no tavern, no alcohol need) |

`Wohnmax`: 8, 15, 28, 42, 30. `NoDowngrade` is set for PIONIER and ARISTOKRATEN only. `NoUpgrade` is set for KAUFLEUTE and ARISTOKRATEN only. Stored `VermehrTab` (the published integers, recomputed from the file's two-decimal values): PIONIER −16, −9, 0, 12, 32; SIEDLER −25, −12, 0, 25, 51; BUERGER −32, −16, 0, 32, 64; KAUFLEUTE −38, −19, 0, 38, 76; ARISTOKRATEN −51, −25, 0, 51, 102. `Menge` matches the published list in `population.md` §P1 (NAHRUNG 41/37/37/37/37 and the rest of that row).

Score-table ranges (min, max), all 21 entries: every group Nahrung `(−2, 2)`; Bedarf PIONIER `(1, 2)`, every other group `(−2, 2)`; every Infra `(−2, 2)`. Pioneers' goods table cannot go negative. Every other group can, so a negative table byte is a shipped input, not a mod-only case.

Substitution actually taken (id not itself a need, `lookup` copied from `IdSec`):

| Group | Copied |
|---|---|
| PIONIER | KIRCHE → KAPELLE slot 5; KATHETRALE → that same slot, because KIRCHE was copied first |
| SIEDLER | HOCHSCHULE → SCHULE slot 8; KATHETRALE → KIRCHE slot 9 |
| BUERGER, KAUFLEUTE | KATHETRALE → KIRCHE's own slot |
| ARISTOKRATEN | KATHETRALE → KIRCHE slot 4 |

`IdSec` observed: HOCHSCHULE → SCHULE, KIRCHE → KAPELLE, KATHETRALE → KIRCHE. Empty `IdSec` stays 0 and does not copy, because `lookup[0]` is the sentinel. Decay does not use this map. Delivery does (`0x1007664c`), so it is a P5 input. Mood recognizes a tavern by the need's ware id `0x4c`, not by `lookup[ALKOHOL]`.

`BauFirst` of each shipped need, as a bit index the tick shifts (`0x100762d3`–`0x100762e2`):

| Name | Index | Name | Index |
|---|---|---|---|
| INFRA_EVER | 0 | INFRA_STUFE_2F | 9 |
| INFRA_STUFE_1B | 2 | INFRA_STUFE_3C | 13 |
| INFRA_STUFE_1C | 3 | INFRA_STUFE_3D | 14 |
| INFRA_STUFE_2B | 5 | INFRA_STUFE_4C | 19 |
| INFRA_STUFE_2C | 6 | INFRA_STUFE_4E | 21 |

NAHRUNG, LEDER and STOFFE are `INFRA_EVER` except where a later group says otherwise (SIEDLER/BUERGER STOFFE is still EVER; KAUFLEUTE STOFFE is EVER; KLEIDUNG / SCHMUCK / WEIN are `4C`). SALZ and KAPELLE are `1C`. WIRTSHAUS is `1B`. A player mask of 1, the constructor value, decays only `INFRA_EVER` needs. Force does not gate decay. `BauFirst` does not gate mood.

### 1.4 Synthetic counterexamples

These are not shipped rows. They are the rule applied to invented need lists. `F` is Force.

| Needs in order | `n` | `+0xcc` | `+0xcd` | What a shipped-order shortcut gets wrong |
|---|---|---|---|---|
| WIRTSHAUS, then NAHRUNG | 2 | 2 | 2 | The tavern is slot 0. `+0xcc` is not "the first infra index". The infra loop `cc`…`cd-1` is empty. Slot 0 is also the food slot mood always reads. |
| NAHRUNG `F`, WIRTSHAUS not `F` | 2 | 1 | 2 | `+0xcd` moved even though Force is 0. The mask bit stays clear. |
| NAHRUNG, WIRTSHAUS `F`, LEDER `F` | 3 | 3 | 3 | The later consumer good rewrites `+0xcc` to 3, past the tavern. The tavern sits in the goods range. The infra loop is empty, so the special tavern add never runs; a forced tavern is only in the goods mean. |
| WIRTSHAUS, WIRTSHAUS, and no ALKOHOL | 2 | 0 | 2 | `lookup[0x4c]` is slot 1. `lookup[ALKOHOL]` stays slot 0. |
| ALKOHOL, then WIRTSHAUS | 2 | 1 | 2 | Alias skipped. `lookup[ALKOHOL]` stays the alcohol slot. |
| One id `0x60`, then WIRTSHAUS | 2 | 1 | 2 | The high id writes `+0xcc` because it was still 0. It is not an infrastructure id and not a consumer id. |
| NAHRUNG, then id `0x60` | 2 | 1 | 1 | The high id does not move `+0xcc` once it is non-zero. |
| No needs | 0 | 0 | 0 | Sentinel fill is 96 zeros. Mood's slot-0 food divide is still reached if a house points at this record (`E ≠ 0`), and `Menge` is 0. |

A scalable engine stores the need vector and runs this rule. It should reject `n > 255`, a goods id outside `0`…`0x5f`, or more needs than the house stock vector can hold, and say so. The original would wrap `n` to 8 bits and write past the 16 × 4-byte need area. Reproducing that wrap is not the proposal. Sixteen is the original record's slot count, not a new engine limit.

`parse_house_groups` (`house_groups.cpp`) keeps slot order, `Ware`, `Force`, `Menge`, `Wohnmax`, the four tables and the two flag bits. It skips every other key, including group `BauFirst`. It does not build `+0xcc`, `+0xcd`, the mask or the lookup. Those omissions are P2.2 dependencies. They are not unread original facts anymore: the assigned loader range has been read. The old OQ-472 deferral is closed here.

## 2. Input and effect contract

House tick `0x10076230`–`0x100765b9`. `this` = house (`esi = ecx` at `0x10076239`, vtable slot `+0x8c`). One stack argument, the tick. Mood `0x10075fd0`–`0x10076224`. `this` = house (`ebx = ecx` at `0x10075fd4`), no stack argument, called at `0x100763ed`. City unlock `0x10029610` is a different object and a different stamp (§4).

Widths below are the original's. The engine widens `residents_64` (i16 → i32), `E` (u16 → u32) and stocks (u16 → u32) on entry (P1 decision 6, OQ-452). Division is toward zero, including the MSVC bias-and-`sar` form used for `/64` and `/128`. A negative numerator truncates toward zero (`-65 / 64 = -1`, `-10 / 64 = 0`). The kernel proposed below uses that rounding on the widened values and reports the original 16-bit store as a separate labeled result. It does not wrap the widened fields.

Supported domain for a defined result: `E ≥ 0`, every `Menge > 0` for a slot that is divided, every stock `≥ 0`, `BauFirst` in `0`…`31`. Outside that domain the original divides by zero, shifts by `cl & 31`, or indexes a 21-byte table at 21 or beyond. Those are labeled undefined. No fallback is invented.

### 2.1 Direct numerical steps, in order

1. **Gate.** `diff = (u16)(tick − last)`. If `diff < 2500`, return. The other outputs are not evaluated. Else `last = (u16)(last + 2500)`. One step, not a loop, and not `last = tick`.
2. **Mask.** House `+0x10` → city, city slot `+0x1c` = owner (`0x1008c680`), player `GetProperty` `0x21` copies `player +0x90` to dest+4 (`0x10020a6f`, T32.5 §15.1). The tick tests that dword.
3. **`E`.** `E' = trunc(E·127/128) + trunc(residents_64/64)`, then the original stores the low 16 bits. The next original read zero-extends those bits, so a negative sum becomes a value above 32767. The widened result keeps the signed sum.
4. **Needs** `j = 0`…`n-1`. Let `bit = BauFirst(goods[Ware(j)])`. If the mask has `1 << bit`, `stock[j] = trunc(stock[j]·127/128)` and `lowest = min(lowest, min(s_j, 160))` with `s_j = trunc(stock·1536 / (E·Menge))` using the **new** stock and the **new** `E`. A clear bit leaves that stock unchanged and out of `lowest`. `lowest` starts at `0x7fffffff`. Force is not consulted. `E·Menge = 0` is undefined.
5. **Spawn condition, not a spawn.** If `lowest < 102` and the number of non-null pointers at house `+0x18`…`+0x24` is `<` type slot `+0xb8(0)`, the original draws the world PRNG (`0x10076397`) and calls house slot `+0x118` with job `0x35`. Without the walker count, the type maximum and the variant count, this predicate is **not evaluated**. It is not false, and it is not "would fire".
6. **Mood** (§2.2), from the stocks and `E` after step 4.
7. **Counters, then growth, then counters.** `sub(group, trunc(residents_64/64))`, then `residents_64 += VermehrTab[mood]` (i8, already in 1/64 units), then `add(group, trunc(new/64))`. The original add is a u16 `add` and wraps. The widened add does not. The city counter object is not stored yet (P1 decision 10). The kernel can return the two truncated integers and the new residents. It does not write a city.
8. **Branch on the mood byte, signed.**
   - `mood ≥ 0` and `residents_64 > Wohnmax·64` (strict: equality does not enter): counter-correct by `trunc(residents/64) − Wohnmax`, set residents to `Wohnmax·64`, then `UpgradeCheck` (`0x10075dd0`). A true result calls city slot `+0x98`. Inputs still missing for that call: next group's forced needs and tables, queue count, kind block, materials `0x29`–`0x2d`. **Upgrade dispatch is not evaluated** without them. Crossing the cap is a number and can be returned on its own.
   - `mood < 0`: if `NoDowngrade` or flags `& 0x30`, floor = 64 and the removal arm is selected; else floor = `48 · Wohnmax(previous)` (`group − 0x50` at `0x10076527`, which is the previous record's `+0xc8`) and the downgrade arm is selected. The floor test is signed and strict: `movsx eax, bp` then `cmp eax, edx` / `jge` at `0x10076540`–`0x10076545`. Equality does not enter. On the downgrade arm only, the original then compares the same residents word as a signed i16 with 64 (`cmp bp, 0x40` / `jge` at `0x1007654b`–`0x1007654f`). A word `< 64`, including every negative i16, falls through: the counter add is `trunc(residents/64) − 64` (the truncation is the `movsx` value already in `eax`, bias-and-`sar` at `0x10076553`–`0x10076559`, then `sub eax, 0x40`) and residents is stored as 64 (`0x1007656a`), then city slot `+0x9c` is called. A signed word `≥ 64` skips that add and that store, and still calls. This is not an unsigned test of the low 16 bits: i16 −1 is below 64. The widened kernel uses the same signed `< 64` test on the widened integer. It does not reread those bits as an unsigned word, and it does not wrap the widened field to 16 bits before the test (OQ-452). On the removal arm the `jge` block is skipped (`0x10076549`) and the call is island slot `+0xcc(x, y, 0, 0, 0)`. The call is observed. That it removes the house stays **likely** (unchanged). Downgrade's type change, material move and PRNG variant are P3 effects and are **not evaluated** here. The floor test, the arm select, and the below-64 test are separate. None of them is inferred from the deferred call.

`UpgradeCheck` and both city calls can change the city store (materials `0x29`–`0x2d` on upgrade, `population.md` §5). The tick body has no call to billing `0x1002dac0`. That only means decay, mood and growth do not take stock. It does not mean a taken upgrade is store-free. The kernel must not report "city store unchanged" for a result whose transition is still deferred.

### 2.2 Mood

`E` is read as u16. `E == 0` returns 0 immediately (`0x10075fde`–`0x10075fef`), before tables and before flags. Flags cannot override that 0.

Otherwise, with the group pointer at house `+0x28`:

- **Food.** Always slot 0, Force not tested. `food = min(s_0, 128)`. `f = NahrungTab[trunc((food+4)/8)]`.
- **Goods mean.** Forced slots `1`…`+0xcc−1`, each contributing `min(s_j, 128)`. A forced tavern (`Ware == 0x4c`) that lies in `+0xcc`…`+0xcd−1` is added here too, also capped at 128, and it is not also in the first range when `+0xcc` is its slot. Unforced needs add nothing. If the count of contributions is 0, the goods satisfaction is 128, not a mean of an empty set. `b = BedarfTab[trunc((that+4)/8)]`.
- **Infra.** `infra` starts at 160. Forced slots `+0xcc`…`+0xcd−1` take `min` with `min(s_j, 160)`. No forced slot leaves 160. The same forced tavern also updates this minimum. `i = InfraTab[trunc((infra+4)/8)]`.
- **Index.** `trunc((s+4)/8)` on a non-negative `s`. Caps 128 and 160 land on indices 16 and 20. The tables have 21 bytes (0…20). Satisfaction 164 would be index 21, which the caps prevent. Dropping the cap is an undefined table read.

`f`, `b` and `i` are signed i8.

- If any of them is `< 0`: flags `& 0x20` returns −2. Otherwise the result is `min(f, b, i)`. Bit `0x10` does **not** force −1 on this path.
- If all are `≥ 0` (0 included): flags `& 0x20` returns −2; else flags `& 0x10` returns −1; else `trunc((f+b+i+1)/3)`. Both bits set returns −2.

The round-cy sentence "flags `& 0x30` force −1/−2" is only the all-non-negative path. Pioneers cannot get a negative goods score (Bedarf is 1…2). They can get a negative food or infra score. Every later group can get a negative score from any of the three tables.

### 2.3 What a partial kernel may return

Inputs that are enough, when the supported-domain constraints hold:

`tick`, `last`, `e`, `residents_64`, `stock[]`, `mood_flags`, player mask, and the group: `n`, `+0xcc`, `+0xcd`, per-need `ware` / `force` / `menge` / `BauFirst`, `Wohnmax`, previous `Wohnmax` (0 at group 0), `NoDowngrade`, the four tables.

| Output | When it is a number | When it is not evaluated |
|---|---|---|
| `acted` | gate result | — |
| `last'` | the u16 sum on a taken gate; unchanged on a skip | — |
| `e'`, `stock'`, `residents'` | gate taken; widened, no 16-bit wrap | gate skipped |
| `lowest` | gate taken; `0x7fffffff` if no bit matched | gate skipped |
| `spawn_condition` | gate taken **and** walker count, type max supplied: the comparison only | either input missing. Not a spawn, no PRNG, no job |
| `mood` | gate taken and `E'` defined | gate skipped, or a divided `Menge` is 0 |
| `counter_sub`, `counter_add` | gate taken: the two truncated resident counts | gate skipped. Not a city write |
| `cap_crossed` | `mood ≥ 0` and `residents' > Wohnmax·64` | gate skipped |
| `below_floor`, `arm` (`downgrade` or `remove`) | `mood < 0` and previous `Wohnmax` known (or `NoDowngrade` / flags select the 64-floor) | gate skipped |
| upgrade happened, downgrade happened, house removed, PRNG draws, city-store delta, type id, materials | never in this kernel | always deferred to P3 or P5 |

A test that expects `upgrade == true` from this kernel is testing the wrong contract. The mutant that turns "not evaluated" into `true` fails the deferred vector in §3.

## 3. Numerical vectors

Worked in integers, independently of any future C++ kernel (`cache/t326-f1/derive.py` only checked the shipped rows; the arithmetic below was recomputed with toward-zero division). Original-width results are labeled `u16` / `i16`. Proposed engine results are the widened column.

### 3.1 Gate, wrap, stamp

| Inputs | `diff` (low 16) | Result | `last'` |
|---|---|---|---|
| tick 2499, last 0 | 2499 | skip | 0 |
| tick 2500, last 0 | 2500 | take | 2500 |
| tick 65535, last 0 | 65535 | take | 2500 |
| tick 65536, last 0 | 0 | skip | 0 |
| tick 68036, last 0 | 2500 | take | 2500, not 68036 |
| tick 3000, last 0 | 3000 | take | 2500, not 3000 |
| tick 66500, last 64000 | 2500 | take | 964 (`(64000+2500) mod 65536`) |

City unlock (`0x10029610`–`0x10029626`, store at `0x10029908`, T32.5 §15.5): the same 2500 compare, then `city +0x10 = tick`. Tick 3000 with city last 0 stores 3000. House and city must not share a helper that always adds 2500 or always stores `now`.

A signed 16-bit compare of the difference treats 65535 as −1 and skips the tick-65535 row. That mutant fails this vector. An outer timeout is not the failure.

### 3.2 Mask and stock

`E = 128`, `Menge = 12`, so `s = stock` before the cap (`128·12 = 1536`).

| Case | Stock before | Mask bit | Stock after | Contribution to `lowest` |
|---|---|---|---|---|
| bit clear | 128 | off | 128 | none |
| bit set | 128 | on | 127 | `min(127, 160) = 127` |
| bit set | 1 | on | 0 | 0 |
| bit set | 127 | on | 126 | 126 |
| bit set | 0 | on | 0 | 0 |
| satisfaction 101 | 101 | on | `trunc(101·127/128) = 100` | 100, which is `< 102` |
| satisfaction 102, measured **before** decay on a stock that decays to 102 | — | — | — | see next row |
| raw `s` after decay = 102 | stock whose new value is 102 | on | 102 | 102, not `< 102` |
| raw 200 | 200 | on | `trunc(200·127/128) = 198` | `min(198, 160) = 160` |

The 101/102 threshold is on `lowest` after the cap of 160, compared with 102 (`0x10076341`). 101 passes the spawn **condition** only when walker inputs exist (§2.3). 102 does not. Goods-mood cap of the same raw 200 is 128, not 160 (§3.4).

Decay of a clear bit fails the "stock stays 128" row.

### 3.3 `E` and residents

| `E` | `residents_64` | Widened `E'` | Original u16 store |
|---|---|---|---|
| 640 | 320 (exactly 5) | `trunc(81280/128)+5 = 640` | 640 |
| 128 | 100 | `127 + 1 = 128` | 128 |
| 100 | 63 | `99 + 0 = 99` | 99 |
| 128 | −10 | `127 + 0 = 127` | 127 |
| 128 | −65 | `127 + (−1) = 126` | 126 |
| 0 | −65 | −1 | 65535, and the next u16 read is 65535, not −1 |

Skipping the smooth leaves `E = 100` in the third row. That is the mutant.

Growth, after the counter subtract of the old truncation and before the add of the new one. `VermehrTab` values are the stored i8, not the file's decimals.

| Residents before | Add | Residents after (widened) | `trunc` before | `trunc` after |
|---|---|---|---|---|
| 100 | +32 | 132 | 1 | 2 |
| 64 | −16 | 48 | 1 | 0 |
| 63 | +1 | 64 | 0 | 1 |
| 32767 | +1 | 32768 widened; i16 wrap −32768 | 511 | widened 512; i16 `trunc(−32768/64) = −512` |

The i16 row is outside the shipped `Wohnmax` (maximum 42 · 64 = 2688). It is the OQ-452 boundary, not a default the kernel should apply to the widened field.

### 3.4 Satisfaction caps, index, tavern, empty classes

`E = 128`, `Menge = 12`, stock already the post-decay value.

| Stock | Raw `s` | Cap 128 (food / goods mean) | Cap 160 (`lowest` / infra) | Index `trunc((capped+4)/8)` |
|---|---|---|---|---|
| 0 | 0 | 0 | 0 | 0 |
| 3 | 3 | 3 | 3 | 0 |
| 4 | 4 | 4 | 4 | 1 |
| 101 | 101 | 101 | 101 | 13 |
| 102 | 102 | 102 | 102 | 13 |
| 123 | 123 | 123 | 123 | 15 |
| 124 | 124 | 124 | 124 | 16 |
| 128 | 128 | 128 | 128 | 16 |
| 159 | 159 | 128 | 159 | 16 or 20 |
| 160 | 160 | 128 | 160 | 16 or 20 |
| 200 | 200 | 128 | 160 | 16 or 20 |
| 164 with the cap removed | 164 | — | index 21 | undefined table read |

Tavern, synthetic group, not a shipped row. `+0xcc = 2`, `+0xcd = 4`, slots: 0 food, 1 a forced consumer good, 2 WIRTSHAUS, 3 another infra need. `E` and `Menge` as above, so `s = stock`.

| Slot 1 | Tavern stock | Tavern Force | Slot 3 Force / stock | Goods satisfaction | Infra satisfaction |
|---|---|---|---|---|---|
| 80, forced | 200 | forced | forced / 40 | `trunc((80+128)/2) = 104` | `min(160, 40) = 40` |
| 80, forced | 200 | not forced | forced / 40 | 80 (tavern omitted) | 40 |
| none forced, tavern not forced | 200 | not forced | not forced | 128 (empty mean) | 160 (none) |
| 80, forced | 200 | forced, but the need was placed **below** `+0xcc` by the interleaved counterexample | — | counted once, cap 128, in the goods range | not also in the infra range |

### 3.5 Mood precedence

Table bytes are the scores `f`, `b`, `i` after indexing. `E ≠ 0` unless the row says so.

| `f, b, i` | Flags | Mood |
|---|---|---|
| any | any, `E = 0` | 0 |
| 2, 2, 2 | 0 | `trunc(7/3) = 2` |
| 0, 0, 0 | 0 | `trunc(1/3) = 0` |
| 1, 1, 1 | `0x10` | −1 |
| 1, 1, 1 | `0x20` | −2 |
| 1, 1, 1 | `0x30` | −2 |
| −1, 5, 5 | 0 | −1 (min, not `trunc(10/3) = 3`) |
| −1, 5, 5 | `0x10` | −1 (bit 4 does not force a different value; min already −1) |
| −1, 2, 5 | `0x10` | −1 |
| −1, 2, 5 | `0x20` | −2 |
| 1, −4, 2 | 0 | −4 |

The mutant that always returns `(f+b+i+1)/3` fails the −1, 5, 5 row (it yields 3). The mutant that returns −1 whenever bit `0x10` is set fails two rows: `E = 0` with flags `0x10` (yields −1 instead of 0), and scores −4, −4, 0 with flags `0x10` (yields −1 instead of the min −4). Scores −1, 2, 5 with flags `0x10` do not catch it, because the min is already −1.

### 3.6 Floor, cap, overflow

`Wohnmax = 8` so the cap is 512. Previous `Wohnmax = 8` so the downgrade floor is `48·8 = 384`. Settlers' previous cap is 8, so 384 is the shipped settler floor. Citizens use `48·15 = 720`.

| Residents after growth | Mood | Flags / `NoDowngrade` | Result |
|---|---|---|---|
| 512 | ≥ 0 | — | not above the cap; no clamp, upgrade not entered |
| 513 | ≥ 0 | — | cap crossed; residents become 512; `trunc(513/64) − 8 = 0` so the counter correction is 0; upgrade **not evaluated** |
| 576 | ≥ 0 | — | cap crossed; correction `9 − 8 = 1`; upgrade not evaluated |
| 384 | < 0 | neither, floor 384 | `384 < 384` is false; arm not entered; the below-64 test is not reached |
| 383 | < 0 | neither, floor 384 | arm = downgrade; signed `383 < 64` is false, so no correction and residents stay 383; the city call is observed and its effects are not evaluated |
| −1 | < 0 | neither, floor 384 | arm = downgrade; signed `−1 < 64`, so the counter add is `trunc(−1/64) − 64 = −64` and residents become 64 before the call. Effects of the call are not evaluated |
| 0 | < 0 | neither, floor 384 | arm = downgrade; signed `0 < 64`; add `−64`; residents become 64; call effects not evaluated |
| 63 | < 0 | neither, floor 384 | arm = downgrade; signed `63 < 64`; add `trunc(63/64) − 64 = −64`; residents become 64; call effects not evaluated |
| 64 | < 0 | neither, floor 384 | arm = downgrade; signed `64 < 64` is false, so no correction and residents stay 64; call effects not evaluated |
| 65 | < 0 | neither, floor 384 | arm = downgrade; signed `65 < 64` is false; residents stay 65; call effects not evaluated |
| −1 | < 0 | `NoDowngrade` or flags `& 0x30` | floor 64; signed `−1 < 64`, so the removal arm is entered; the set-to-64 block is not this arm |
| 0 | < 0 | `NoDowngrade` or flags `& 0x30` | floor 64; removal arm; set-to-64 is not this arm |
| 63 | < 0 | `NoDowngrade` or flags `& 0x30` | floor 64; removal arm; set-to-64 is not this arm |
| 64 | < 0 | `NoDowngrade` or flags `& 0x30` | `64 < 64` is false; removal arm not entered |
| 65 | < 0 | `NoDowngrade` or flags `& 0x30` | `65 < 64` is false; removal arm not entered |

Shipped `VermehrTab` cannot step from a value at or above 384 to a value below 64 in one update (largest drop is 51). The below-64 block is still a real instruction for a residents value that is already there, including a modded table. The division above was checked with the same bias-and-`sar` the instruction uses (`cache/t326-f1-corr/vectors.py`). An unsigned reading of `cmp bp, 64` gets the −1 row wrong: it treats the word as 65535 and skips the add. The signed path adds −64 and stores 64.

Widened multiply. `stock = 2796203`, `E = 128`, `Menge = 12`. True `stock·1536 = 4294967808`, `s = 2796203`, capped 160. The same multiply in u32 is 512, `s = 0`. Original u16 stocks cannot reach this (65535·1536 fits in 32 bits). The widened kernel's intermediate has to be wider than 32 bits. A u32-multiply mutant fails this row. This is a supported engine input and an impossible original u16 input; the vector says which.

Direct decay does not change a city-store quantity. A mutant that subtracts from the store on the decay path fails a vector whose transition outputs are not evaluated. A vector must not also expect the store to stay unchanged once upgrade dispatch is admitted: that effect is P3 and is outside this kernel.

### 3.7 Mutants to vectors

| Mutant | Failing vector |
|---|---|
| Skip `E` smoothing | §3.3, `E = 100`, residents 63, result 100 instead of 99 |
| Decay a need whose mask bit is clear | §3.2, stock 128 stays 128 |
| Signed gate compare | §3.1, tick 65535 / last 0 must take |
| Apply `VermehrTab` before mood | Residents 100 and a post-decay mood of +2 (PIONIER stored entry +32). The add is 132. Adding the mood-0 entry (0) first leaves 100 |
| Take city store inside the kernel | §3.6 last paragraph, decay row |
| Treat a missing upgrade input as `would_fire = true` | §2.3, residents 576, upgrade not evaluated |
| Extend `+0xcd` only for forced infra | §1.4, unforced WIRTSHAUS still sets `+0xcd = 2` |
| Derive `+0xcc` in the post-pass from Force | §1.4, WIRTSHAUS-then-NAHRUNG yields `+0xcc = 2` with no Force involved |
| Always average the three scores | §3.5, scores −1, 5, 5 yield 3 instead of −1 |
| Let bit `0x10` override `E = 0` | §3.5 first row |
| Read `cmp bp, 64` as unsigned | §3.6 resident −1: the unsigned word 65535 skips the add; the signed path adds −64 and stores 64 |

### 3.8 Later tests, not written now

When an implementation packet exists: G7 (`synthetic::g7_scenario`, hash `p1_g7_create` stays the create hash) gains N-update checks of `E`, stocks and residents against §3, with the city store unchanged while upgrade and the walker stay deferred. Own-save / resync: hash equal after save/load under the implemented exact `last` (§4). A rules option that resets `last` must say the hash changes and must be in the lockstep handshake, or peers diverge. Multi-peer: same kernel inputs, same checksum. Version: this kernel writes no new stream field and draws no PRNG, so `kStateHashVersion` and `kPrngTimelineVersion` stay. A live tick that first draws `HouseWalkerTarget`, `HouseUpgradeVariant` or `HouseDowngradeVariant` bumps the timeline (state.md draw table). Goldens are not rewritten here.

## 4. Lifecycle

Three paths, plus the city stamp which is not one of them.

| Path | Original | This tree |
|---|---|---|
| Play create | Base ctor `0x100734ca` stores 0 at `+0x0a`. House ctor `0x10077220` then sets residents `0x140` and `E` `0x280` and zeroes 16 stock words. Create's shared tail `0x1002469b`–`0x100246b2` calls slot `+0x4c` with key `0x23`. The setter `0x100733a4`–`0x100733ac` writes the u16 at buffer+4 to `+0x0a`. | `make_payload` leaves `last = 0`, residents `0x140`, `E` `0x280`. `stamp_placement` (`create.cpp`) skips `House` (P1 decision 8). P2 play-create has to stamp. The value Create copies is `[esp + 0xe4]` at `0x1002469b`. This pass did not finish the push count from `sub esp, 0xbc` at `0x10024025` through that site, so it is not re-proved here that the value is the world tick. Next read: that frame. Until then, "property `0x23` stores `now`" remains the existing population.md claim, not a new proof. |
| Original-format import | Load `0x10076e60`–`0x1007711b` writes residents and `E` (`+0x2c`, `+0x2e` at `0x10076f04` / `0x10076f0b`) and does not write `+0x0a`. Scenario buildings go through Create (`annowelt.md`), which does call property `0x23` before Load. Load does not clear a stamp Create already wrote. | `scenario_formats.cpp` builds the payload and sets `residents_64 = r<<6`, `e = r<<7`, stocks from `house_need_stock`. It does not set `last`, so `last` stays 0. That matches original Load's non-write. It matches Create's stamp only when the stamp value is already 0. A non-zero stamp at import is not reproduced. This is a qualifier, not a new default. |
| Own save / resync | The original house record does not contain `last` (`population.md` §11.2). OQ-185: world save `0x100ae700` still has no `call [reg+0x80]` (T32.5 §15.7). Next address stays the player writer `0x1001e920` / city writer `0x1002ec00`. | `fields(House)` visits `u16 last`, then `residents_64`, `e` and the stock vector (`state_fields.hpp`). Save then load keeps `last`. That is P1 decision 4. |

**OQ-450 stays an owner choice.** Owner question 1 (2026-10-25) says keep catch-up for the original load (`last = 0` on that path). P1 decision 4 says our own save is the runtime form, and it explicitly leaves the catch-up question open. Quoting only "the save is exact" does not close it. Quoting only question 1 would reset exact saves, which this audit does not do.

Implemented today: exact `last`. Outstanding choice: whether a load of our own save should keep that `last` or reset it so the house catches up. Recommendation, not a new default: keep the exact field. Resetting it changes the state hash across save/load (OQ-132) and desynchronises a peer that did not reset. If the owner wants catch-up later, it has to be an explicit rules option in the handshake, with a test that shows the hash change. Consequences of keeping it: original-format import can still start at 0; an own-save reload does not replay missed 2500-steps. Consequences of resetting: one load can run many single steps of `last += 2500` (the tick is not a loop), and two peers with different choices diverge.

City `+0x10` is the unlock stamp. T32.5 §15.5: after a taken pass it stores the tick, and the constructor range that was read does not initialise it (OQ-461). House `last += 2500` is not that store. The kernel must not advance a city with the house formula.

## 5. Dependency proposal

Producer → first reader. "Have" means the engine already stores it. "Derive" means P2.2 computes it with §1 and keeps it on the group, immutable, in the registry digest.

| Input | Producer | First reader | Stage |
|---|---|---|---|
| `last` | ctor 0; Create property `0x23` (play); import leaves 0; own-save visits it | gate | P2 play-create stamp. OQ-450 for own-save. |
| Player mask | ctor 1; `HEAD` load reconstructs it (T32.5 §15.4). Import does not. OQ-460's extra OR is unread | decay bit test | **P2.1** |
| `BauFirst` | `[WAREN]`, goods `+2` | decay | **P2.2** |
| `n`, ware, Force, `Menge`, tables, `Wohnmax`, flags | parser already | mood, growth, floor | have |
| `+0xcc`, `+0xcd`, mask `+0xc4`, lookup | §1, not the post-pass alone | mood uses `cc`/`cd` and ware `0x4c`; mask is not a mood input; lookup is delivery | **P2.2** for `cc`/`cd` (and the mask if a later reader wants it). Lookup and `IdSec` are **P5** unless P2.2 builds them in the same pass because the pass is one function. The partial kernel does not need lookup. |
| `E`, residents, stocks | payload | smooth, decay, mood | have |
| Mood flags | omitted; owner Q7 / OQ-462 default 0. Writer still unread (`0x1002b700` / `0x1002b840`, T32.5 §15.8) | mood override | default 0 until that read |
| Previous `Wohnmax` | previous group slot | floor | have if groups stay slot-ordered |
| Next group, materials, queue, kind | not on the house | upgrade | **P3** |
| Walker count, type max, PRNG | figures / type / world | spawn | **P5** |
| City counters | not stored (P1 decision 10) | the two truncations | **P3/P4** cache. The kernel only returns the integers. |
| City store | `CityStock` | not the direct tick | unchanged by the kernel. **P3** if upgrade is wired. **P5** stall take. |

**P2.1** is the mask import from `HEAD +0x48` / `+0x5C`, using the T32.5 reconstruction. No invented bits. OQ-460 stays out.

**P2.2** is `BauFirst` on goods, plus `n` / `+0xcc` / `+0xcd` (and, in the same derivation, the mask and the lookup) from §1. Not a second meaning of Force.

**Partial kernel** is §2.3. Tests are the vectors in §3. It does not replace `tick_building`. `driver.cpp` stays a return.

**Live P2.3** is blocked. Wiring the kernel into the tick while upgrade or spawn can be true, and then skipping the call and the PRNG, is not a faithful live tick. A later packet can wire a **subset** whose tests prove the spawn condition is false and `cap_crossed` / `below_floor` are false, so those calls are not reached. That subset still needs the walker inputs and the floor inputs in the test, or the proof is missing. Full live behaviour waits for P3 and P5 and for the OQ-450 choice. This note does not approve P2.3.

No stream bump and no golden rewrite fall out of this note. `kStateHashVersion` stays.

## 6. Candidate questions

Kept in this note only. Not written to `docs/open-questions.md`.

| Id | Status after this read |
|---|---|
| OQ-470 | Still the P2.2 task: put `[WAREN]` `BauFirst` on goods data. The tick site is `0x100762d3`. Not an unread formula. |
| OQ-471 | Still P2.1: import the reconstructed mask. Histogram is T32.5 §15.4 (263 of 373 full values are 1; 78 have a non-zero high half). |
| OQ-472 | **Closed here.** `+0xcc` is the parser rule in §1.1. `+0xcd`, the mask, the alias and the substitution are §1.2. Force does not extend `+0xcd`. |
| OQ-473 | Create's property-`0x23` value: name the frame slot `[esp + 0xe4]` at `0x1002469b` by counting pushes from `0x10024025`. Until that read, do not claim every import leaves ctor 0. |
| OQ-474–479 | unused |

Still owned elsewhere, and not reopened: OQ-450 (owner, §4), OQ-451 (load-phase draw order), OQ-452 (whether a rules option should reproduce the 16-bit wrap; the kernel's widened arithmetic does not), OQ-460, OQ-461, OQ-462, OQ-185.

## 7. Decisions and reasoning

1. **Treat §1 as the endpoint rule, and retire the Force shortcut.** Alternative: keep "derive both ends from Force and the infra id range" and leave OQ-472 unread. Rejected. The parser writes `+0xcc` at `0x1001487e`–`0x100148a0` with no Force test. The post-pass writes `+0xcd` at `0x10014b33` before the Force test at `0x10014b39`. Evidence: those instructions, re-read, and the shipped table in §1.3 which has `+0xcd = n` for pioneers even though the infra mask is 0.
2. **Withdraw would-fire outputs.** Alternative: keep the round-cy `HouseTickOut` flags for walker, upgrade, downgrade and remove. Rejected. Spawn needs walker count, type max and a PRNG draw (`0x10076397`). Upgrade needs `UpgradeCheck` inputs and then a store-changing call. The comparison `lowest < 102` and `residents > Wohnmax·64` can be numbers. Calling the missing ones "would fire" asserts a transition the inputs do not decide.
3. **Split mood overrides by the sign of the table bytes, and let `E == 0` win.** Alternative: leave "flags `& 0x30` force −1/−2". Rejected. `0x10075fe9` returns 0 before the flag tests. `0x100761b1` takes the min path when any score byte is negative, and that path tests only bit `0x20` (`0x100761fe`). Shipped Bedarf/Nahrung/Infra ranges include negatives (§1.3), so the path is not theoretical. Pioneers' Bedarf `(1, 2)` is the exception for goods only.
4. **Keep OQ-450 open and recommend exact `last` without changing the default.** Alternative: treat owner question 1 as resetting own-save `last`. Rejected. Question 1 is the original load. P1 decision 4 and OQ-450 say the own-save choice is still open. Exact visit is what `fields(House)` does. Resetting it would change hashes and multi-peer checksums. The recommendation is to leave that code in place until the owner picks an option.
5. **Do not claim Create's stamp is re-proved, and do not claim import `last = 0` equals every original import.** Alternative: repeat "Load leaves 0, so import matches". Load's non-write is re-checked (`0x10076e60`–`0x1007711b` has stores at `+0x2c` / `+0x2e` and none at `+0x0a`). Create still calls property `0x23` at `0x100246b2`. The copied value's frame slot was not counted (OQ-473). Our importer never stamps, so it matches the original only when that value is 0.
6. **Widen the multiply, and label the 16-bit wrap.** Alternative: specify u32 intermediates, or make the kernel wrap like i16/u16. The u32 multiply of stock `2796203` changes `s` from 160 to 0 (§3.6). Original u16 cannot hold that stock. OQ-452 stays the owner's wrap question. The proposed numbers are the widened ones, with the original store written beside them.
7. **Leave lookup and `IdSec` on the P5 side of the kernel boundary.** Alternative: require the full lookup before any mood test. Mood's tavern test is the ware id `0x4c` (`0x100760d9`), and the ranges are `+0xcc` / `+0xcd`. The alias changes `lookup[0x1d]`, which delivery reads. P2.2 should still compute the lookup in the same pass, because the pass is one function and splitting it would risk a second, drifted rule. The kernel does not take it as an input.
8. **Treat the below-64 correction as signed, on the widened integer.** Alternative: keep the unsigned reading, where a negative word whose low 16 bits are `≥ 64` skips the add. Rejected. `0x1007654f` is `jge` after `cmp bp, 0x40`, and `eax` at the following `cdq` is the `movsx` of that word (`0x10076540`). Resident −1 adds −64 and is stored as 64 (§3.6). Wrapping the widened field to 16 bits before the test would reintroduce the original narrow width; OQ-452 still owns that choice, and this note does not take it.
