# Source-backed wildlife creation plans

WILDLIFE-SPECIES-LOAD-1, 2026-11-19. Boundary/acceptance before C++.
Build on [decisions](wildlife-decisions.md), [creation](wildlife-create-effects.md),
[primary helper](../re/wildlife.md#2-creation-helper-0x10092ca0), and
[prefix](wildlife-prefix.md). Review pending orchestrator.

D: Welt 0x10092e9e–0x10092eb8 and 0x10092ed6–0x10092eed read island climate
byte +0xc0, multiply by 32, add selected index, then read u16 species at
0x100ca700 + 2*(32*climate+index). No species/type byte exists in the spawn.
D: original species mask 31 fixes the decoded ring length 32, not an engine limit.
Catalogue +0x6c and type +0x80 supply numeric goods through the creation receivers.
Existing selector preserves direct draws and bounded 18-inspection behavior.

P: selected first Tutorial/01Discover01 island INFO byte 0 is climate 2. This
packet supports only GogClimate2: exact row RVA 0xca780, 64 initialized bytes,
within the actual readable/nonwritable .rdata section of GOG Welt. Validate the
existing machine/stamp/base/image-size profile and unique full file backing.
This proves that addressed row, not total table extent, all climate bytes or seven
rows from another table. Climate 0/1/3/255 are explicitly unsupported here.
Names/facts still come from the addressed Game mappings and parsed FigurenHeader,
not a copied decoded ring, route name or modern registry default kind.

| Requirement | Hand expectation / positive control | Distinguishing negative |
|---|---|---|
| Raw ring | preserve all 32 slots, order and repeated IDs | sorting/filtering changes selection |
| Source facts | kind 49/51/50 and numeric yields, absent WARE ->0 | missing/duplicate/unsafe fields refuse before selection |
| Supported New capability | synthetic 60001 fits source u16, kind 51 allowed | valid kind 50 selection keeps index/count, unsupported creation |
| Draw order | ordinary [6,33,999] ->position2/index1/consumed2; cave [31,999] ->position-1/index31/consumed1 | exhausted ordinary0/1 retain consumed0/1; cave0 no draw available |
| Search | duplicate types retained; index31 wraps to0 to match; unmatched saved stops index16 after18 from31 | no reroll or filtered indices |
| Missing cave entrance | OQ-574 and consumed0, not manager null | no invented provider/position |
| Profile | only original climate2 row, distinct synthetic neighbouring row | wrong climate/image/overlap/backing refuses |
| Ownership | selected facts/capability/source identities own buffers | inputs unchanged and repeat deterministic after source destruction |
| Real source | independent PE/records oracle checks all32 entries/kind/goods and authored selections | never original runtime/draw-history/World proof |

API: formats owned ring reader; sim_load source-backed preparation returns an
owned ring/facts, unchanged selector decision/count, selected fact, and optional
sealed creation descriptor. Explicit typed unsupported-creation disposition for
selected other valid kind, not invalid source or a replacement choice. Source
errors occur before draws; selector errors retain its exact fault/count. Stop
at plan; no fractional coordinates, cave provider or actual manager. Generic
numeric species facts are separate from the sealed New-only creation capability.

## Fresh non-New WARE clarification (2026-11-19)

D: Game constructor entry 0x10011190 zeroes ECX at 0x100111a8 and initializes
byte +0x100 to 0 at 0x10011259. Fresh factory 0x1000222a ->0x1000641f
->0x10011dd0 establishes that object. WARE loop 0x100103d0–0x10010445 scans
ordinals 0..142; only match 0x1001044a writes +0x100. Safe unmatched input
returns success without a store, retaining 0 for this fresh-descriptor profile.
Reused catalogue entries can bypass construction at 0x10002228: this is NOT a
zero-default contract for overlays/reused entries. No catalogue object is executed.

P: climate 2 slot 31 references type 131/kind 50 with a safe unmatched WARE name.
Its numeric yield is 0, not an invented ordinal or reason to filter it. Generic
facts keep `ware_present` and `ware_matched`: absent=false/false, matched=true/true,
unmatched retained default=true/false. Duplicate entries/records, unsafe lengths
and unterminated names refuse. New kinds 49/51 retain conservative unknown-name
refusal, as does the existing sealed binder, with its previous error order.
Generic facts also retain HP/default/source identity fields for small shared parsing;
these are values, not New capability or a manager/loaded-descriptor simulation.

The orchestrator's focused independent primary review APPROVED this predicate only:
[WARE review at a2454542](https://github.com/Dom-cmyk727/anno_rebuild/blob/a245454279236b7e80e188c9d9e15c1c577a748f/docs/orchestration/reviews/2026-11-19-wildlife-species-ware-review.md).
Full new code/tests remain pending package review. The initial private oracle's
unknown-WARE ValueError and its diagnostic probe are preserved; the corrected
oracle follows the newly closed default path. The exploratory constructor dump
starting 0x10011230 is misaligned, not primary evidence; use aligned 0x10011190.

## Implemented boundary

Formats `read_wildlife_species_ring` owns all 32 u16 entries, profile climate/RVA/
raw offset and whole supplied-image hash. This hash is identity, not authenticity.
Shared strict readonly PE-window checks are unchanged. `prepare_wildlife_creation_plan`
binds distinct requested IDs once, then reconstructs all original slots including
duplicates before calling unchanged `sim::wildlife_create`. Generic facts remain
separate from the sealed capability; supported selection is rebound from the same
immutable source, never caller-forged kind/source facts. Unselected non-New kinds
still participate in numeric goods search. Selected non-New remains a typed
UnsupportedCreationKind plan with the exact choice/count and no capability.
Source-stage errors precede selector draw evaluation; selector-stage errors carry
its exact fault and consumed count, including OQ-574 before any draw. No manager
failure is inferred. Result owns ring, slot facts, choice, selected facts and optional
capability. A composed projection uses separately authored geometry only.
