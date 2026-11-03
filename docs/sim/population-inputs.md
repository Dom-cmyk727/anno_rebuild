# Population: the immutable-input contract for the first calculation slice

MAP-POP-READY-1 (2026-11-03). Companion to [`population-readiness.md`](population-readiness.md), which holds the prerequisite table, the owner decisions (OD-n) and the packages (A-E) this document refers to. **Draft contract only**: it changes no layout, no version and no code. Stream 126 and PRNG timeline 4 are untouched; where a later package would change an identity this document says so (section 6).

Evidence labels: **D** disassembly (module, address; re-read for this task), **P** parsed from a shipped file (re-derived by an independent script), **E** established by the earlier accepted audit (T32.5 F1, `docs/re/population.md`), **A** adaptation (ours), **O** open. Modules: `AnnoGame.dll` sha256 `ac721220…7bc`, `AnnoWelt.dll` `f220a878…6ae2`, `BGruppen.dat` `63e876fd…7849e3b`.

## 1. What is in the slice, and what is not

In: the player's stage mask as an *input* (its import is package A); per-goods `BauFirst` and `IdSec` (package B); the derived per-group endpoints, lookup, forced mask and alias (package C); the configuration that names the infrastructure and tavern goods (OD-6). These are exactly what the bounded kernel (package D) reads and what the importer already reads (`Menge`, `Wohnmax`, tables, flags, needs).

Out: prices, climates, `Forsch`, `Bonus`/`Malus`, `BauLast`, group-level `BauFirst`/`BauLast`/`Hiscore`/`Wissen`/`Priority` (no P2 reader); `[BAUSTUFEN]` and the unlock pass (P4); upgrade material and queue data (P3); walker type facts (P5).

**Ownership classes used below.** *Parsed* = a value the loader writes from the file. *Post-pass* = written by the loader's group post-pass (substitution, alias, max). *Derived* = a pure function of parsed values and configuration, computed once when the registry is built. *Registry* = immutable, in the registry digest, never saved. *State* = hashed and saved (players, houses).

## 2. Field contract

Each card: source and ownership / width, sign, units / default, absent, explicit zero / validation and supported domain / configurability and modding / digest and persistence / first consumer / unsupported.

### 2.1 Player stage mask (`Player::flags`) — state; imported by package A

* **Source:** original `SPIELER` HEAD `+0x48` (u32) and `+0x5c` (u8): `flags = (HEAD48 & 0x7fffff)` plus `0x01000000` if `+0x5c & 0x08`, `0x08000000` if `& 0x04`, `0x20000000` if `& 0x10`, `0x40000000` if `& 0x20` (**D** AnnoWelt `0x1001d389`-`0x1001d3df`). Other bits of HEAD `+0x5c` and bit 23 of HEAD `+0x48` are dropped. The OQ-460 OR at `0x1001bda3` is **not** imported (open).
* **Width:** u32 (existing `Player::flags`, `player.hpp:100`); bit `b` set = the player has reached stage index `b`. The tick tests `1 << BauFirst` against the full dword (**D** `0x100762db`-`0x100762e2`).
* **Default / absent / zero:** constructor value 1 (`INFRA_EVER` only). A synthetic or ship-start/fixed-map player has no head and keeps 1. An original head that stores 0 gives **0** (not 1): no stage active, not even `INFRA_EVER`. The import field is therefore optional (`absent` ≠ `0`).
* **Validation:** none beyond the type; every u32 is representable.
* **Configurability:** the stage-index domain is 32 because the mask is one u32; widening is a state-format change (future identity, not part of this slice).
* **Digest / persistence:** already hashed and saved; package A changes which value an import writes, not the layout.
* **Consumer:** decay and `lowest` in the tick (kernel), unlock (P4).
* **Unsupported:** high bits are unread by every shipped need (maximum shipped `BauFirst` index 21, section 5) but the full dword must still be kept and tested.

### 2.2 Goods `BauFirst` (stage index) — parsed; package B

* **Source:** `[WAREN]` key `BauFirst: INFRA_STUFE_…`, the name looked up in the name-pointer table at `AnnoGame 0x10024bd0` (**D**, entries 0-33 re-read; whether the table continues was not read: indices 0 `INFRA_EVER`, 1 `1A`, 2 `1B`, 3 `1C`, 4 `2A` … 9 `2F`, 10 `2G`, 11-16 `3A`-`3F`, 17-22 `4A`-`4F`, 23-28 `5A`-`5F`, 29 `TRIUMPH`, 30 `DENKMAL`, 31 `FORSCH`; entries 32/33 are `GOTT`/`SPIELER`). Stored at goods record `+2`. Read by the tick as `goods + 26·ware + 2` (**D** `0x100762c2`-`0x100762d3`).
* **Width / units:** u8 in the original (a shift count, `cl & 31` in `shll`). Engine: `std::uint8_t stage_index` on `GoodsInfo`, supported domain **0..31**.
* **Default / absent / zero:** the `[WAREN]` start clears the table (`population.md` §1 cites `0x10014c3b`; that address was not re-read here, but the tick reads the byte unconditionally and no other writer exists in the parse path I read), so an absent key is **0**, which is the same value as an explicit `INFRA_EVER`. The two are indistinguishable at runtime in the original; the registry stores the value only, no presence flag (**A**).
* **Validation (A):** the name must be one of the 32 stage names (indices 0-31); an unknown name, `GOTT` or `SPIELER` is a load error. What the original does with an unknown stage name or with indices 32/33 is **untraced (O)**, so this is a deliberate adaptation, not a fidelity claim.
* **Configurability:** the 32-name mapping is a compiled name table (precedent: the orchestrator's CLOSE-1 decision accepting the exact route-name table as a mapping to supported behaviours). New content reuses the stage names with new goods; no goods id is named in code.
* **Digest / persistence:** appended to the registry digest per goods in goods order; not saved.
* **Consumer:** the tick's mask test (kernel, package D). **Unsupported:** a good that is *never* active (no value maps to it); stage names beyond the 32.

### 2.3 Goods `IdSec` (substitute goods) — parsed; package B

* **Source:** `[WAREN]` key `IdSec: <goods name>`, stored at goods record `+1` (**D** read by the post-pass at `0x10014b7f`: object `+0x7bd` for id `0x4c`, stride `0x1a`).
* **Width:** u8 goods id in the original; engine `GoodsId` (u32), "none" = 0.
* **Default / absent / zero:** absent = 0 = none. In the post-pass `lookup[0]` is the sentinel (no need has ware 0, section 2.5), so IdSec 0 never copies.
* **Validation (A):** the name must resolve in the goods table; the id must be below the goods count. Shipped values: `KLEIDUNG→SEIDESTOFFE`, `KIRCHE→KAPELLE`, `HOCHSCHULE→SCHULE`, `KATHETRALE→KIRCHE` (**P**, 4 of 56 goods rows).
* **Consumer:** the group post-pass substitution only reads ids in the infrastructure range; `KLEIDUNG`'s IdSec is outside it, so its first reader is not the group pass (delivery/trade: P5 or later, unread). Mood and decay do not use the lookup.
* **Digest / persistence:** as 2.2. **Unsupported:** chains deeper than the one pass handles (section 3, example C9).

### 2.4 Configuration of infrastructure goods — configuration; package B/C (OD-6)

* **Fields (proposal):** `infra_first` (original `0x4c` = `WIRTSHAUS`), `infra_last` (`0x5a`), `tavern_goods` (`0x4c`), `tavern_alias_goods` (`0x1d` = `ALKOHOL`), all `GoodsId`/u32 in `SimConfig`, defaults the original values (**D**: `cmp $0x4c`/`$0x5a` at `0x1001487e`-`0x10014889`, `0x10014b25`-`0x10014b2a`; tavern/alias `0x10014b60`-`0x10014b6d`; mood tavern test `0x100760d9`).
* **Validation:** `infra_first ≤ infra_last < goods count`; `tavern_goods` within the range; `tavern_alias_goods < goods count` and outside the range.
* **Digest / persistence:** config is hashed in the header; adding the fields changes the canonical bytes (section 6). Not saved separately.
* **Why config:** the original's numeric ids are properties of its goods table; PLAN §0.1 forbids encoding them as engine constants. The count of infrastructure goods (15 in the original) is data, not a limit: the forced mask is a bit set over `infra_last − infra_first + 1` entries with no 32-bit cap.

### 2.5 Group need vector and ware — parsed; exists

* **Source:** `Ware`, `Force`, `Menge` per need in file order (`HouseGroup::needs`); **D** `population.md` §1.
* **Width:** `ware` `GoodsId` (u32); `force` bool; `menge` u16, the loader integer `(h·4096 + 5000)/10000` for a value written `h/100` (equals `trunc(Menge·40.96 + 0.5)`), e.g. 1.0 → 41, 0.4 → 16, 0.2 → 8 (**P**).
* **Validation (existing):** the goods name must exist; `Menge` and `Force` required. **Proposed additions (A, package C):** `ware ≠ 0` (goods 0 is `UNUSED`; the original would accept it as a need and then every infrastructure id with `IdSec = 0` would copy its slot because `lookup[0]` would no longer be the sentinel); `menge > 0` (a zero divisor is the undefined divide of OD-4, and a zero demand is meaningless).
* **Original-count facts, not engine limits:** the original record has 16 need slots, a byte `n` (low 8 bits of slot+1, stored for every accepted `Ware`), and a Ware-name scan that stops before id `0x60` (so ids `0x5b`-`0x5f` share the UNUSED literal with id 0 and are unreachable). The engine stores a vector of any length and resolves names over the whole goods table; none of those numbers becomes a cap.
* **Consumer:** importer, mood, decay; derived data of 2.6-2.9.

### 2.6 Derived: `goods_end` (`+0xcc`) — post-parse; package C

* **Rule (**D** `0x1001486b`-`0x100148a6`, executed per accepted `Ware`, `n` = slot + 1):** if `ware < infra_first`: `goods_end = n`; if `infra_first ≤ ware ≤ infra_last`: unchanged; if `ware > infra_last`: `goods_end = n` only when it is still 0. **Force is not read.**
* **Width / units:** slot index boundary; engine u32 (original u8).
* **Default / absent:** 0 (a group with no consumer-class need).
* **Digest:** none (derived); recomputed identically at every registry build. **Consumer:** mood (the goods range is slots `1 … goods_end−1`) and the loop split. **Unsupported:** `ware > infra_last` is an instruction fact with no shipped example (the name scan cannot reach it in the original); the engine keeps the rule for modded goods tables.

### 2.7 Derived: `infra_end` (`+0xcd`) — post-pass; package C

* **Rule (**D** `0x10014b2f`-`0x10014b33`, `0x10014bad`-`0x10014bbd`):** for every need with `infra_first ≤ ware ≤ infra_last`, `infra_end = slot + 1` — **forced or not**; after the loop `infra_end = max(infra_end, goods_end)` (unsigned compare). A group with no infrastructure need ends with `infra_end = goods_end`.
* **Consumer:** mood's infrastructure range `goods_end … infra_end−1`.

### 2.8 Derived: forced-infrastructure mask (`+0xc4`) — post-pass; package C (optional in the first slice)

* **Rule (**D** `0x10014b39`-`0x10014b5a`):** for an infrastructure need with Force ≠ 0, set bit `ware − infra_first`. Force **only** affects this mask (and the per-need flag mood reads at tick time).
* **First reader:** none in the P2 tick (mood reads each need's own `force`); the mask is first read by P3's `UpgradeCheck` (`0x10075dd0`, unread; **O**). It is built in the same pass because the pass is one function and a second construction would risk a drifted rule; its test is against a separate transcription.
* **Width:** a bit set over the configured range (no 32-bit cap); shipped values `0x0 / 0x41 / 0x91 / 0xa3 / 0x286` (**P**, re-derived).

### 2.9 Derived: id→slot lookup and tavern alias — post-pass; package C

* **Rule (**D** `0x10014afc`-`0x10014b76`):** `lookup` has one entry per goods id, initialised to the sentinel `n`; for need slots `j = 0 … n−1` in order `lookup[ware(j)] = j` (a repeated ware overwrites: the **last** slot wins, both slots stay in the vector and `n` counts both). If `ware(j) == tavern_goods` and `lookup[tavern_alias_goods]` is still the sentinel, `lookup[tavern_alias_goods] = j` (the first tavern wins; a real alias-goods need blocks it).
* **Width / sentinel:** slot index u32, sentinel = `n`; `lookup` sized by the registry's goods count (the original: a 96-byte table).
* **Consumer:** delivery (`0x1007664c`, P5). Mood recognises a tavern by the need's ware, not by `lookup[alias]`; decay does not use the lookup. So the lookup is **not** an input of the kernel; it is built in package C because it comes out of the same pass.

### 2.10 One-pass `IdSec` substitution — post-pass; package C

* **Rule (**D** `0x10014b78`-`0x10014bab`):** for ids `infra_first … infra_last` **ascending**, if `lookup[id]` is the sentinel and `lookup[id_sec(id)]` is not, copy that slot. Exactly one pass: a substitute with a lower id (already processed) is visible in the same pass; a chain whose substitute has a higher id is not followed.

### 2.11 Inputs the kernel takes from existing registry data and state

* `Wohnmax` (u16, units: residents per house, cap = `Wohnmax·64` in 1/64 residents), `NoUpgrade`/`NoDowngrade`, the three 21-entry i8 satisfaction tables and the 5-entry i8 `VermehrTab` (index mood −2…+2 at `group + 0x110 + mood`, **D** `0x1007642c`). The tables' length 21 follows from the caps (128 and 160 give indices ≤ 20); a longer table is not read.
* **Previous group's `Wohnmax`:** the floor input for the downgrade arm; group 0 reads layout-adjacent bytes in the original (OD-5; derived 0). The kernel takes it as an explicit input.
* **House state:** `last` (u16), `residents_64` (i32, signed, units 1/64 resident), `E` (u32), per-need stock (u32, units: 1/16 ton scaled by the satisfaction formula; `s = trunc(1536·stock/(E·Menge))`); walker count and type maximum only if supplied.
* **Mood flags** (`house +0x0c & 0x30`): input, default 0 (OQ-462 open).

## 3. Reproducible construction order (package C)

1. Resolve the goods table, then the group needs (existing parse).
2. Per group, per need in file order with `n = slot + 1`: update `goods_end` (2.6).
3. Per group: initialise `lookup` to the sentinel (the vector length `n`, value `n`); walk the needs: set `lookup[ware] = slot`, update `infra_end` (2.7) and the forced bit (2.8), apply the tavern alias (2.9).
4. Per group: the one-pass `IdSec` substitution over the infrastructure range (2.10).
5. Per group: `infra_end = max(infra_end, goods_end)`.
6. Groups are processed independently; an unnamed hole between named slots is the cleared, empty group (n = 0) and processes to `goods_end = infra_end = 0`.

The result is a pure function of (needs, goods `id_sec`, config). Because it is derived it needs no digest of its own and cannot drift from its inputs; a test recomputes it twice and against a separate transcription.

## 4. The corrected endpoint-versus-Force distinction

The earlier round-cy note said both endpoints were post-pass products decided by Force. Re-read:

| Value | Written by | Decided by | Force? |
|---|---|---|---|
| `goods_end` (`+0xcc`) | **parse**, per `Ware` | the goods id class of each need and its position | **no** |
| `infra_end` (`+0xcd`) | **post-pass**, at `0x10014b33` | the position of the last infrastructure need, then `max` with `goods_end` | **no** (the Force test is the next statement, `0x10014b39`) |
| forced mask (`+0xc4`) | post-pass | Force of infrastructure needs | yes |
| mood's forced slots | tick time | each need's own Force flag within the ranges above | yes, only there |

Shipped proof: PIONIER has no forced infrastructure (mask 0) yet `infra_end = 6 = n`.

## 5. Data-level acceptance table (hand-derived; B = goods metadata, C = group derivation)

Shipped rows come from an independent script over the raw `BGruppen.dat` (it reproduces every row of T32.6 §1.3); synthetic rows apply the same rules to invented need lists and are not file examples. Goods ids are the original table (NAHRUNG 26, LEDER 27, SALZ 28, ALKOHOL 29, STOFFE 33, WIRTSHAUS 76 = `0x4c`, SCHULE 80, HOCHSCHULE 81, KAPELLE 82, KIRCHE 83, KATHETRALE 84). `F` = forced.

| Id | Rule (evidence) | Smallest wrong reading | Input → expected | Future assertion / reviewer disposition |
|---|---|---|---|---|
| B1 | Shipped `BauFirst` (**P**): NAHRUNG, LEDER, STOFFE `EVER`(0); WIRTSHAUS `1B`(2); ALKOHOL, SALZ, KAPELLE `1C`(3); SCHULE `2B`(5); GEWUERZE, TABAKWAREN `2C`(6); KIRCHE `2F`(9); SEIDESTOFFE, HOCHSCHULE `3C`(13); LAMPENOEL, BADEHAUS `3D`(14); KLEIDUNG, SCHMUCK, WEIN, PELZE, HEILKRAEUTER `4C`(19); THEATER, PARK `4E`(21); KATHETRALE `5B`(24); 23 goods carry the key | table-position or alphabetical index | the name→index map above, checked against the name table; maximum on a *need* is 21 | registry digest-independent transcription |
| B2 | Absent = explicit `INFRA_EVER` = 0 (`population.md` §1, `0x10014c3b`) | absent = "never" or 31 | a goods row without the key → 0; `INFRA_EVER` → 0; `INFRA_STUFE_1A` → 1 | runtime indistinguishable; digest moves only with the value |
| B3 | Unknown stage name | silently 0 | `BauFirst: INFRA_STUFE_9Z` → load error naming the line (adaptation) | refusal test |
| B4 | Index domain | accept 32/33 | `GOTT`, `SPIELER` → load error; `INFRA_STUFE_FORSCH` → 31 accepted | |
| B5 | `IdSec` shipped (**P**) | read for consumer goods in the group pass | 4 rows; `KLEIDUNG→SEIDESTOFFE` is outside the infra range and never copied by the group pass | |
| B6 | Digest coverage | metadata not hashed | change one `bau_first` or one `id_sec` → registry digest moves | mutation |
| C1 | Five shipped groups (**P**; ids above; derivation 2.6-2.10) | any shipped-order shortcut | PIONIER n6 `goods_end` 4 `infra_end` 6 mask 0 alias→slot 4; SIEDLER 10/6/10 `0x41` alias 6; BUERGER 12/7/12 `0x91` alias 7; KAUFLEUTE 11/7/11 `0xa3` alias 7; ARISTOKRATEN 8/4/8 `0x286` alias sentinel 8 | independent script; slot-for-slot |
| C2 | Duplicate wares: last slot wins; `n` counts both (**D** `0x10014b1b`) | first wins, or de-duplicated | [NAHRUNG, SALZ, NAHRUNG] → n 3, `lookup[NAHRUNG] = 2`, `goods_end` 3 | order-sensitive |
| C3 | `goods_end` is "the n of the last consumer-class need", not "first infrastructure index" (`0x1001487e`) | first infra slot | [WIRTSHAUS, NAHRUNG] → n 2, `goods_end` 2, `infra_end` 2, `lookup[ALKOHOL] = 0` (the tavern is slot 0) | |
| C4 | `infra_end` moves without Force (`0x10014b33` before `0x10014b39`) | only forced infrastructure | [NAHRUNG F, WIRTSHAUS] → `goods_end` 1, `infra_end` 2, mask 0 | the case a Force-based rule fails |
| C5 | A later consumer need rewrites `goods_end` past a tavern | `goods_end` set once | [NAHRUNG, WIRTSHAUS F, LEDER F] → n 3, `goods_end` 3, `infra_end` 3, mask `0x1` | the infra loop is empty; the forced tavern is in the goods range |
| C6 | Second tavern: `lookup[tavern]` takes the later slot, the alias stays on the first (`0x10014b65`-`0x10014b6d`) | alias follows the last | [WIRTSHAUS, WIRTSHAUS] → `goods_end` 0, `infra_end` 2, `lookup[WIRTSHAUS] = 1`, `lookup[ALKOHOL] = 0` | |
| C7 | A real alias-goods need blocks the alias | alias overwritten | [ALKOHOL, WIRTSHAUS] → `lookup[ALKOHOL] = 0` | |
| C8 | One-pass substitution, ascending: KIRCHE (83) is processed before KATHETRALE (84) (`0x10014b78`-`0x10014bab`) | direct needs only, or descending | PIONIER: KIRCHE→KAPELLE slot 5, KATHETRALE→that same slot 5 (shipped); a descending or single-lookup pass leaves KATHETRALE at the sentinel | |
| C9 | A substitute with a higher id is **not** followed | two passes | modded `IdSec`: HOCHSCHULE(81)→KIRCHE(83), KIRCHE→KAPELLE, need [KAPELLE] only → `lookup[81]` stays the sentinel; `lookup[83] = lookup[84] = 0` | synthetic, not a file row |
| C10 | `Ware` naming goods 0 | accepted | refused at registry build (adaptation: `lookup[0]` is the IdSec-none sentinel) | refusal test |
| C11 | Empty group | crash or garbage | no needs → n 0, `goods_end` 0, `infra_end` 0, empty lookup | |
| C12 | `ware > infra_last` arm | n unconditional | with `goods_end` 0 → `n`; with `goods_end` non-zero → unchanged (instruction fact; no shipped example) | synthetic only |

Timed behaviour does not occur in this document: all of it is registry construction. The timed gate, its smallest interval (a call with d = 0 returns unchanged), the one-step-per-call rule and the single partial state `last` are in `population-readiness.md` (package D).

## 6. Future identity changes (none made here)

| Change | Effect | Needed by |
|---|---|---|
| `GoodsInfo::{stage_index, id_sec}` appended to the registry digest | registry digest ⇒ canonical bytes ⇒ `kStateHashVersion` increment | package B |
| four `SimConfig` fields (OD-6) | config serialisation ⇒ header bytes ⇒ increment; a content hash in the lockstep handshake already covers the registry | package B/C |
| `PlayerSpec::flags` imported | created players of masks ≠ 1 hash differently ⇒ increment; layout unchanged | package A |
| derived group data | none | package C |
| the kernel | none | package D |
| a live tick, walker spawn, upgrade | new draw sites ⇒ `kPrngTimelineVersion`, possible new state | not proposed |

Windows delta when any of these lands: the stream number, the corpus and golden fixtures, and the player/registry identity in recordings.

## 7. Limits

No C++ was built; no original runtime was run. The corpus histograms are those of T32.5/P2.1 (373 heads; not re-run). `BauFirst` behaviour for an unknown name or index 32/33 and the `UpgradeCheck` inputs are unread. Windows/MSVC, Linux/WSL, original-runtime and GPU checks are NOT_RUN.
