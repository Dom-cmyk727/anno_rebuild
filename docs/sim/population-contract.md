# Population input pipeline: the bound contract (MAP-POP-IMPL-1, P0)

Status: **bound design** for packages A-F of MAP-POP-IMPL-1 (2026-11-03). It applies the nine binding decisions of the [MAP-POP-READY-1 acceptance](../orchestration/reviews/2026-11-03-map-pop-ready1.md) to this branch and overrides the proposal wording of [`population-readiness.md`](population-readiness.md) and [`population-inputs.md`](population-inputs.md) wherever they differ (those two stay the evidence record: addresses, derivations, shipped rows). Nothing here enables a live House tick: `tick_building` keeps `case TickKind::House: return;`.

Evidence labels as in the inputs document: **D** disassembly, **P** parsed from a shipped file, **E** established by an accepted audit, **A** approved adaptation (a safe-domain choice, not a fidelity claim), **O** open.

## 1. Identity record (decision 9)

| Item | Value |
|---|---|
| Start | `2f876af5a44fc3395819134e79a9595a13d9f7d9` on the accepted forestry `f40a7602` (stream 126, PRNG timeline 4) |
| Branch use of 127 | `git grep kStateHashVersion` shows 126 as the last value; no other owned work on this branch uses 127. **Stream 127 is reserved for A+B+C together.** |
| PRNG timeline | stays 4 (no draw site is added) |
| Command wire | unchanged |
| Increment point | The constant moves 126 -> 127 in the first commit that changes created-world bytes (package A). Package B changes the canonical bytes of 127 again (four config fields in the config scope, `GoodsInfo::{bau_first, id_sec}` in the registry digest); **package C changes none** (derived tables are not hashed). Version 127 is unpublished until the orchestrator accepts the bundle. |
| Re-baselining | Every package that moves bytes regenerates, with the independent `tools/sim_stream.py` as oracle where it can: `goldens.txt`, `ship_holds.txt`, `cargo_rules_bytes.txt`, the recorded logs `g2`-`g6.log` (by `ANNO_UPDATE_GOLDEN=1`, delta = version word and hashes only, sizes unchanged), `scenario_corpus.txt` (version header and hash column; counts unchanged) and the hard-coded version assertions (`sim_test`, `config_test`, `island_state_test`, `ship_nav_state_test`, `ship_state_test`). Gates of A, B and C are therefore green at their own package; the **final** F gate runs on the last layout. Recordings made at stream 126 stay at their original pins and are not claimed to replay on this build. |
| Derived caches, the pure kernel | no further bump |

## 2. The nine decisions as rules of this branch

Labels: **[D]** original instruction fact, **[A]** approved safe-domain adaptation (not a fidelity claim). Decisions 3, 4, 5 and 7 are [A]; decision 8's slot-0 read is [D] and the empty-group refusal [A]; the lookup sized by `goods_count` (the original's table has 96 entries, AnnoGame `0x10014afc`) is [D] for the rule and [A] for the size.

1. **Own saves and resync are exact (OD-1).** `House::last` is saved, hashed and restored as is. No reset option and no load-kind split. The original-format import keeps `last = 0` (Load never writes it, E).
2. **Create's timing stays evidence-only (OD-2).** Construction stamps are unchanged (`stamp_placement` still skips `House`). Package E reads the callers of `0x10024010`; nothing live depends on the answer.
3. **Widened, checked arithmetic (OD-3).** The kernel computes in `std::int64_t`/`std::uint64_t` and returns a typed result when an output cannot be represented in the state field it would be committed to (`residents_64` i32, `e` u32, stocks u32). The original's 16-bit stores appear only as labelled diagnostics (`orig16_*`), never as the value.
4. **Undefined means NotEvaluated (OD-4).** Negative smoothed `E'`, a reached zero divisor, a mood index outside the 21-entry tables or the 5-entry growth table, a missing required input: each gives `HouseKernelStatus::NotEvaluated` with one specific `NotEvaluatedReason`. No clamp, no wrap, no native fault. `E' = 0` is not a blanket error: mood returns 0 before it reads any table; the need loop faults only if a **selected** (stage-enabled) need reaches its divide.
5. **Previous-group capacity is an explicit input (OD-5).** `std::optional<std::uint32_t> previous_wohnmax`. The registry view offers `previous_wohnmax(group_index)` for index >= 1 and `nullopt` for index 0; the caller may supply 0 explicitly for a synthetic or modded group 0. When a path needs it (the floor with neither `NoDowngrade` nor mood flags `0x30`) and none is available: `NotEvaluated(PreviousCapacityUnavailable)`. Adjacent memory is never read.
6. **Configurable roles, bound view (OD-6).** Section 3.
7. **Declared parser domain.** Unknown stage names, `GOTT`/`SPIELER`, indices outside the 32-bit mask, unresolved goods names (`Ware`, `IdSec`), `UNUSED`/goods 0 as an active need and zero demand (`Menge` 0) are diagnostic refusals for the new population inputs. No goods<=95, needs<=16/255, infrastructure<=32 limit. Absent metadata keeps the original defaults (0). These are **A**, not claims about the original's untraced error handling.
8. **Slot 0 is positional.** The kernel reads need slot 0 unconditionally when `E <> 0` (mood, `0x10075fe9`); no NAHRUNG-id branch, no reordering. An empty group has valid derived metadata (`goods_end = infra_end = 0`, lookup all sentinel 0); a calculation that needs slot 0 on it returns `NotEvaluated(MissingFirstNeed)`. The below-64 correction belongs to the downgrade arm only; the removal arm bypasses it (`0x10076547`-`0x10076551`).
9. **One identity change** - section 1.

## 3. Registry and config binding (OD-6)

**Config.** `SimConfig` gains four `std::uint32_t` fields with the original values as defaults, serialised and hashed in the config scope (`state_fields.hpp`):

| Field | Default | Original meaning (D) |
|---|---|---|
| `population_infra_first` | `0x4c` | first infrastructure goods id (WIRTSHAUS), `cmp $0x4c` at `0x1001487e`, `0x10014b25` |
| `population_infra_last` | `0x5a` | last infrastructure goods id, `0x10014889`, `0x10014b2a` |
| `population_tavern_goods` | `0x4c` | the tavern need that earns the alias, `0x10014b60`; also mood's tavern test `0x100760d9` |
| `population_tavern_alias_goods` | `0x1d` | ALKOHOL, the lookup slot the first tavern may take, `0x10014b65`-`0x10014b6d` |

**Forced-infrastructure set.** `HouseGroupDerived::forced` is a dynamic bit vector of `infra_last - infra_first + 1` bits (bit `ware - infra_first`), not a u32; the original's u32 at `+0xc4` (`1 << (ware - 0x4c)`, AnnoGame `0x10014b49`-`0x10014b5a`) is the shipped special case. Only the player stage mask is a 32-bit domain. The kernel reads each need's own `force` flag (as mood does), not this set.

**No-tavern mods.** A tavern role outside `[first, last]` is intentionally refused by `SimConfig::validate()` (the roles describe one original mechanism); a mod without a tavern keeps the roles and simply has no need on that goods id.

Validation lives in `SimConfig::validate()` only for the **ordering that never needs a goods table** (`first <= last`, `tavern_goods` inside `[first, last]`). The checks against the goods domain (`last < goods count`, `alias < goods count`, `alias` outside the range) run where a goods table exists: `TypeRegistry::build` and `validate_against(cfg)`, and **only for a registry that has house groups**. A registry without groups (every synthetic non-population registry, the empty registry) accepts any config, so original default ids that its small goods table does not contain never make it fail.

**Binding.** The derived view is data of the **registry**, built once inside `TypeRegistry::build` from (needs, goods `id_sec`, the `SimConfig` it was given):

* the registry stores `PopulationBinding { infra_first, infra_last, tavern_goods, tavern_alias_goods, goods_count }` - the exact roles it was derived under - and `std::vector<HouseGroupDerived>`, one per group;
* `validate_against(cfg)` (already run by `World::create`, `World::load` and the header read with the *world's* config, because a registry is shared) fails with a message naming the field when `cfg`'s roles differ from the binding and the registry has groups. A world therefore cannot run with tables derived under other roles;
* `TypeRegistry::population_view(group_index, cfg)` returns a small view (`group`, `derived`, `bau_first(goods)`) **only** when `cfg`'s roles equal the binding, else an error. The kernel takes that view, not raw tables, so a stale table cannot reach it. Keying by both identities is thereby unnecessary: one registry carries one binding, and a different config needs a different registry (the same pattern as `build_goods_first`).
* `digest()` covers the **inputs** (goods `bau_first`/`id_sec`, group needs); the roles are hashed through the config; the derived tables are not hashed (a pure function of hashed inputs).
* The derived data is never serialised; a loaded world gets it from its registry.

## 4. Typed pure kernel (package D), summary

`house_kernel(const HouseKernelInputs&) -> HouseKernelResult` in `house_kernel.hpp/.cpp`: no `World`, no mutation, no I/O, no PRNG, not called from the driver. Inputs: the bound view, `player_mask` (u32), `now` and `last` (the gate uses the low 16 bits of `now - last`), `gate_period` (from `ClassInfo`), residents (`i32`), `E` (`u32`), stocks (span), mood flags, `previous_wohnmax` (optional), walker facts (optional: count and type maximum). Output: `status` (`Skipped`, `Evaluated`, `NotEvaluated`) with a reason, numeric predictions (`last'`, `E'`, `stock'`, `lowest`, `mood`, residents after growth, the two truncated counter integers, `cap_crossed`, `arm`, the signed below-64 result), `spawn_condition` only when walker facts are supplied, and **labelled deferred effects** (`Upgrade`, `Downgrade`, `Remove`, `WalkerSpawn`) that are *not* performed and never claimed as performed.

## 5. Rule-to-proof table

Format: rule -> primary evidence -> smallest wrong reading -> distinguishing input -> intended assertion. Expected values are hand-derived from the instruction spans and the raw files (the inputs and readiness documents carry the full derivation); a value copied into both the C++ and its oracle is not proof, so each row names where its *independent* expectation comes from.

| Id | Rule | Evidence | Smallest wrong reading | Distinguishing input -> expectation | Independent expectation from |
|---|---|---|---|---|---|
| P-A1 | Mask low 23 bits | D `0x1001d397` | default 1 stays | HEAD+0x48 `0x1f`, +0x5c 0 -> created player's flags `0x1f` | hand constants in the test; raw-file histogram script |
| P-A2 | High reconstruction | D `0x1001d39c`-`0x1001d3d5` | low bits only; another bit map | (`0x1`,`0x3c`) -> `0x69000001`; (`0x1f`,`0x20`) -> `0x4000001f`; each of bits 2,3,4,5 alone | same; mutants "no high" and "no assignment" must fail the *intended* check after a passing low-mask control |
| P-A3 | Ignored bits | D, bit 23 and the other packed bits | leakage | (`0xffffffff`,`0xc3`) -> `0x7fffff`; (`0x800000`,0) -> 0 | hand constants |
| P-A4 | Optional, zero is explicit | decision, E | "0 means unset" | stored 0 -> 0; spec without a mask (fixed map, ship start, synthetic) -> 1 | hand constants |
| P-B1 | `BauFirst` name map 0-31 | D `AnnoGame 0x10024bd0` entries 0-33 | alphabetical or table-position index | 23 shipped keys -> the indices of inputs doc B1; maximum on a need 21 | a Python reader of the raw `BGruppen.dat` and the transcribed name list, not the C++ |
| P-B2 | Absent == explicit EVER == 0 | D `0x10014c3b` | absent = 31 | goods row without key -> 0; `INFRA_EVER` -> 0 | hand constants |
| P-B3 | Domain refusals | A (decision 7) | silent 0 | `INFRA_STUFE_9Z`, `GOTT`, `SPIELER`, unresolved `IdSec` name -> error naming the line | hand text |
| P-B4 | Identity moves | repo convention | field not hashed | each of `bau_first`, `id_sec`, each of the four roles moves the digest it belongs to | perturbation + a Python re-encoding (`tools/sim_stream.py`) |
| P-B5 | Old version refused at the version check | convention | rejection by checksum/truncation/missing prerequisite | a current 127 stream beside the same stream re-sealed as 126: the 126 one fails with the version text | sim_stream.py builds both |
| P-C1 | Lookup has **goods_count** entries, sentinel `n` | D `0x10014afc`-`0x10014b09`, `0x10014b1d` | length `n`; empty for zero needs | PIONIER (n 6): `lookup[76] = 4`; zero needs with nonempty goods: all-zero table of goods_count entries | an independent Python transcription of the post-pass on the raw file |
| P-C2 | Endpoints do not read Force | D `0x10014b33` before `0x10014b39` | Force decides `infra_end` | [NAHRUNG F, WIRTSHAUS] -> `goods_end 1`, `infra_end 2`, mask 0; PIONIER mask 0 yet `infra_end 6` | hand rows C1-C5 |
| P-C3 | Last duplicate wins, first tavern gets the alias | D `0x10014b1b`, `0x10014b65` | first duplicate wins; alias follows last | C2, C6, C7 | hand rows |
| P-C4 | One ascending substitution pass | D `0x10014b78`-`0x10014bab` | descending or fixed point | C8 (KIRCHE before KATHETRALE), C9 (higher substitute not followed) | hand rows |
| P-C5 | No original-count limits | decision 7 | arrays of 16/96/255/32 | synthetic registry with > 255 needs, > 32 infrastructure ids, non-original role ids | construction + digest of the synthetic table |
| P-C6 | Config change cannot reuse a table | section 3 | stale derived data | same registry + a config with another `population_infra_last` -> `validate_against` error and `population_view` error | direct call |
| P-D1 | Unsigned u16 gate, one step per call | D `0x1007624b`, `0x10076259` | signed compare; `last = t` | D1/D1b rows: (2499,0) skip; (2500,0) -> 2500; (65535,0) take; (65536,0) skip; t 5000 last 0 -> take, take, skip | Python reference, hand vectors |
| P-D2 | Smoothing toward zero, stored widened | D `0x10076286`-`0x100762a5` | floor, or none | D2 rows; D3 negative sum -> NotEvaluated | reference |
| P-D3 | Only stage-enabled needs decay, full 32-bit test | D `0x100762db`-`0x100762e2` | decay all; 23-bit mask | D4, D5 | reference |
| P-D4 | Satisfaction on the decayed stock, 64-bit | D `0x100762f8`-`0x10076314` | 32-bit product | D6: pre-decay 2818221, E' 128, Menge 12 -> 160 (u32 gives 0) | reference |
| P-D5 | Mood precedence | D `0x10075fe9`, `0x100761b1`-`0x10076220` | flags override E=0; always average | D12 rows | reference |
| P-D6 | Cap and floor signed strict; below-64 only on the downgrade arm | D `0x10076468`, `0x10076540`-`0x1007654f`, remove-arm bypass `0x10076547`-`0x10076551` | `>=`; correction on the remove arm; unsigned word | D8-D11, plus two inputs with the **same** below-64 residents and different arm (NoDowngrade vs not) | reference |
| P-D7 | Smallest caller cadence | readiness F-8 | gate consumes time | `d = 0` call, partial `last`, partition 5000/5000/skip, save/reload of `last` | hand vectors; save/reload test |
| P-D8 | Nominal-success function | evidence contract | returns success without calculating | each reference vector must equal a nontrivial expected; a "success-only" mutant fails it | tiny public-command contract of the CLI |
| P-D9 | Overflow is typed, not wrapped (decision 3) | A | product or sum that does not fit `u32`/`i32` | stock 0xFFFFFFFF with a taken gate; residents near `INT32_MAX` + growth -> `NotEvaluated(Overflow)`; the same inputs one step smaller evaluate | hand vectors |
| P-D10 | `E'` = 0 paths (decision 4) | D `AnnoWelt 0x10075fe9` (mood), `0x10076305`-`0x10076318` (divide) | blanket error for E'=0 | E' 0 with a selected need -> `NotEvaluated(ZeroDivisor)`; E' 0 with every need unselected -> evaluated, mood 0; E 0 for mood alone -> 0 before any table read | hand vectors |
| P-D11 | Previous capacity explicit (decision 5) | D `AnnoWelt 0x1007651d`-`0x10076533` | adjacent memory read; silent 0 | floor path with `nullopt` -> `NotEvaluated(PreviousCapacityUnavailable)`; explicit 0 -> floor 0; `NoDowngrade` or flags `0x30` -> floor 64 and the input unused; group index 0 via the registry view -> `nullopt` | hand vectors; floor value 64 vs `48*prev` asserted |
| P-D12 | Slot 0 positional (decision 8) | D `AnnoWelt 0x10076113` | NAHRUNG-id branch; reordering; Force test on slot 0 | a group whose slot 0 is LEDER (not NAHRUNG) is read at slot 0; slot 0 unforced is still read, slots >= 1 only when forced; empty group with `E <> 0` -> `NotEvaluated(MissingFirstNeed)`, with `E = 0` -> mood 0 | hand vectors |
| P-D13 | Bound roles in the kernel | D `AnnoWelt 0x100760d9` | literal `0x4c`/`0x1d`/`0x5a` | a registry with `population_tavern_goods`, `infra_first`/`infra_last`, alias set to non-default ids: mood's tavern test, infra range and alias follow them; the same needs under default roles differ | synthetic registry |
| P-X1 | Construction stamps unchanged (decision 2) | board | stamping `House` | `stamp_placement` still leaves `House::last` 0 for a created house | existing create test, asserted explicitly |
| P-E1 | Real-input canary | task E.2 | unchanged world hash taken as proof | calculations counted, domain outcomes counted, at least one skipped and one taken gate, nontrivial mask | independent reference run on extracted inputs |
| P-E2 | Dispatch stays a no-op | board | kernel called from the tick | one focused test: a taken-gate house in a ticked world keeps `last`, residents, `e`, stock | existing G7 case + one new case |

**Independence of the numeric rows (D).** `fixtures/synthetic/sim/house_kernel_vectors.txt` is seeded from the hand-derived rows D1-D14 of `population-readiness.md` section 7 and the rows above, and is committed **before** `house_kernel.cpp` exists. `tools/house_kernel_ref.py` is written from the instruction spans and that file without reading the C++, and runs standalone. Its CLI exits non-zero for: a vector file with zero vectors, a vector whose expected value is wrong, and malformed input; child time is bounded. A contract test registered in CTest runs a reference that merely echoes the expected result and requires it to fail. Evidence cells name the module: addresses `0x1001d3xx` and `0x1007xxxx` are `AnnoWelt.dll`; `0x10014xxx` and `0x10024bd0` are `AnnoGame.dll`.

## 6. Partition and ownership

| Package | Owns | Shared files (parent only) |
|---|---|---|
| A | `scenario_formats.cpp` (`decode_player_flags`), `scenario.hpp/.cpp`, tests | `world.hpp` (version), `tools/sim_stream.py` |
| B | `[WAREN]` reader, `GoodsInfo`, `goods_table.cpp`, `original_registry.cpp`, config fields | `config.hpp/.cpp`, `state_fields.hpp`, `types.cpp` digest, `tools/sim_stream.py` |
| C | `house_groups.hpp/.cpp` derived struct, `types.cpp` build/binding | `types.hpp` |
| D | `house_kernel.hpp/.cpp`, `tools/house_kernel_ref.py`, `fixtures/synthetic/sim/house_kernel_vectors.txt`, contract test | CMake registration |
| E | `tests/population_canary_test.cpp`, `docs/re/population.md` section 16 | |
| F | gate logs under `cache/`, REPORTS | |

A research or reference helper may write only the files in its row; the parent keeps headers, config, digest, version and the final merge of results. Complexity (task E.4): registry metadata is built once; the lookup is indexed by goods id; a kernel call is O(needs) and reads no world; there is no per-house rebuild and no whole-world scan.

## 7. Delivered (MAP-POP-IMPL-1; the frozen-gate record is the REPORTS entry the orchestrator receives and, locally, cache/map2-pop-impl1/final/)

| Package | API and files |
|---|---|
| A | `ScenarioSpec::PlayerSpec::flags` (optional u32), `decode_player_mask` in `scenario_formats.cpp`, assignment in `World::create`; oracle `tools/player_mask_ref.py` + `fixtures/synthetic/sim/player_masks.txt` |
| B | `GoodsInfo::{bau_first, id_sec}`, `goods_metadata.hpp` (`stage_index_of`, `apply_goods_metadata`), `SimConfig::population_{infra_first,infra_last,tavern_goods,tavern_alias_goods}`, registry validation; oracle `tools/waren_ref.py` + `waren_metadata.txt` |
| C | `PopulationRoles`, `HouseGroupDerived`, `derive_group`, `TypeRegistry::{derived_groups, population_roles, roles_of, population_view}`, `PopulationView`; oracle `tools/group_derived_ref.py` + `group_derived.txt` |
| D | `house_kernel.hpp` (`house_kernel`, `house_mood_combine`, typed results); `tools/house_kernel_ref.py`, `house_kernel_ref_contract_test.py`, `house_kernel_vectors.txt` (70 vectors, hand-derived before the C++) |
| E | `population_canary_test.cpp`; research in `docs/re/population.md` section 16 |

Config: `docs/sim/state.md` 3.1 is not edited here; this document and `config.hpp` are the field list of the four `population_*` fields. No driver or tick change: House dispatch is still `return;`.

## 8. Limits

This document defines the delivered API; live effects remain open. Arithmetic is widened by decision 3, so "faithful" holds only within the original's 16-bit ranges (E, stocks u16, residents i16, `Wohnmax << 6` in a 16-bit shift); every shipped value is inside them (largest Wohnmax 42), but vectors such as D6 (stock 2818221), O-over/O-ctrl and a Wohnmax of 1024 or more are states the original cannot hold. `HouseGroupDerived::lookup`/`forced` are verified against the oracle and the disassembly but no kernel step reads them yet (mood reads each need's own Force, as the original does). The real-input canary evaluated 541 houses (masks 0x1, 0x3, 0xf only; every imported `last` is 0; no NotEvaluated outcome occurred on real data), so Overflow, ZeroDivisor and the high stage bits are covered by synthetic vectors only. The Create stamp (OD-2), the zero-resident count, below-64 reachability and every live-effect rule (walker spawn and draw, upgrade/downgrade/removal, taxes, unlocks) stay open exactly as in the readiness document. Windows/MSVC, Linux/WSL, original-runtime and GPU checks are NOT_RUN.
