# WILDLIFE-SPECIES-LOAD-1 evidence

2026-11-19 — READY FOR REVIEW; final code/tests await orchestrator review.
[Contract, primary addresses and API](../../sim/wildlife-species-load.md).
Base 901a8467f766825ae9683cb41dee33522a9aff39; pinned dispatch
470b400a32456c59ddbb03f3414336f40458d6f9, no board/main merge.
Tested code 4acf5545303964ed30440105bdef9cd7a6803496,
tree aa42defb5b91795f64411788922c19c338f43851. Publication adds docs only.

## Source and selector proof

D/P: Welt helper 0x10092e9e–0x10092eb8 /0x10092ed6–0x10092eed reads
u16 at 0x100ca700+2*(32*climate+index). Tutorial/01Discover01 first island INFO
climate is 2. Supported row RVA 0xca780 through 0xca7bf is completely backed in
actual .rdata (VA 0xc1000, virtual extent 57995, raw extent 61440, flags 0x40000040).
Checked existing GOG stamp/size/machine/base and strict unique initialized readonly
window. Only GogClimate2 is supported; no inference of seven species rows or
arbitrary climate bytes from the unrelated kernel. SHA identifies supplied bytes,
not authenticity. Zero/unknown ring IDs remain raw until explicit source validation.

Independent private oracle uses pefile, tools/figuren.py and tools/insel.py, not
new C++ readers: all 32 ordered u16 entries, 9 distinct types, kinds 49/50 and
numeric yields 0/137/138. Requested parsing may deduplicate; slot reconstruction
never does. Records and segmented AnnoGame names are independently read at runtime.
No original table/row/listing is published. `species-oracle.tsv` holds all 32
index/id/kind/goods rows privately; C++ compares every row when the private oracle
path is provided and always checks the independent whole-row digest. Future real
suite runs need no private path; digest remains a nonempty independent guard.

Private `selection-oracle.json/tsv` uses unchanged wildlife_ref.evaluate on those
raw facts and authored draws/providers: ordinary position2/index1/inspected1/draws2;
cave direct index31/inspected1/draws1, unsupported kind 50; cave saved137 wraps to0
in2 inspections/1 draw; ordinary saved138 from17 reaches23 after7 inspections/2
draws; unmatched saved142 from31 stops16 after18 inspections/1 draw. Inputs are
supplied, not original runtime draw-history or cave-provider observations.
The C++ real case asserts these exact positions/indices/counts/dispositions and
all facts; it is source-backed planning, not actual animal creation or a canary.

Fresh WARE source gap and focused independent primary disposition:
[review a2454542](https://github.com/Dom-cmyk727/anno_rebuild/blob/a245454279236b7e80e188c9d9e15c1c577a748f/docs/orchestration/reviews/2026-11-19-wildlife-species-ware-review.md).
Slot 31 type 131/kind 50 has safe unmatched WARE. Original 0x10010436–0x10010445
returns without +0x100 store; fresh constructor 0x10011259 initialized 0 (aligned
entry 0x10011190, ECX zero at 0x100111a8). Only match 0x1001044a writes. Focused
review APPROVED this narrow predicate, not final code. Reused-descriptor branch
0x10002228 makes unknown→0 invalid for overlays; no such catalogue is modeled.
Generic facts distinguish absent, matched, unmatched-default via present/matched
flags; unsafe/duplicate/ambiguous input refuses. Sealed New binder retains its
stricter unknown-WARE refusal and previous error ordering. No ordinal invented,
no filtering/reroll and no claim that unsupported selection is creation success.

## Authored and causal proof

Four final synthetic cases execute 121 assertions. Source-defined 60001 fits the
original u16 ring and kind 51; existing wider 90001 binder controls stay separate.
Ordinary/cave order, unused suffixes, exhausted boundaries/counts, OQ-574 consumed 0
under accepted helper refusal, wrap/duplicates/18-inspection no-match, unsupported
selected/inspected kind 50, profile/climate/backing/overlap refusals, zero/missing
species, safe unmatched/matched/absent WARE and unsafe/duplicate siblings checked.
Original input image/raw records/draws remain unchanged; repeat deterministic.
Owned plan survives all source owners; its sealed descriptor can drive accepted
creation projection only with separately authored geometry, never generated positions.

`causal-final.py` compiles actual changed production TUs into private executables
before library resolution. Replacement counts asserted; intended assertion text
and child exit checked. Legal 4/121 passes. Final mutants:

| Actual mutation | Failed cases | Failed assertions |
|---|---:|---:|
| Sort ring | 3 | 17 |
| Wrong row offset | 4 | 26 |
| Wrong numeric goods | 2 | 14 |
| Extra cave position draw via wrapper | 3 | 17 |
| Wrong unmatched retained default | 1 | 2 |

Each is a semantic assertion failure, not a crash/compile failure or empty filter.
Canonical source/binary unchanged by mutants. Final test source is archived beside
causal artifacts. Earlier causal controls are historical, not final-only coverage.
Unchanged sim::wildlife_create, public wildlife_ref and 74-vector fixture are reused;
no new public reference/runner or generator is introduced. Canonical C++ vectors
execute all 74 vectors, including accepted create behavior, at the final binary.

## Failures preserved and corrected

- Initial private oracle stopped at unmatched WARE with ValueError; first script,
  traceback and raw diagnostic probe retained. Outer compound command ended with
  cat; no separate initial Python exit receipt is claimed. Corrected oracle files,
  complete metadata and five reference results are preserved and checked by C++.
- Exploratory constructor range starting 0x10011230 is misaligned and explicitly
  excluded from primary evidence; aligned 0x10011190 is the cited read.
- First build had three doctest syntax/decomposition errors (ternary message and
  stream extraction); build-first.log and test-first-build.cpp retained. Corrected.
- First authored run 3 passes/1 failure: duplicate WARE was added to unreferenced 130
  rather than referenced 131, so its expected refusal was wrong. First causal legal
  control also stopped at that failed assertion before running any wrong variant;
  causal-first/ and initial logs retained. Fix targets requested entry 131.
- Corrected authored 4/119 and successful intermediate causal files retained. Final
  added unsafe-WARE guard gives 4/121 and a new final causal directory; old logs are
  not overwritten or relabeled. Only the changed test TU needed repeated strict
  warnings. First ASan rebuild preceded the final test edits; final incremental
  build completed before freezing/gates. No prior build image is claimed tested.

## Canonical gates and source receipts

Cache root `cache/wildlife-species-load-1/`. Source/binaries frozen before gates,
1024 entries rechecked afterwards. Jobs 1, threads 2, timeout 180, required original
files and both private oracle paths; memory admission enabled through host access.
No renderer/benchmark change, so no render auto-contract. ASAN_OPTIONS disables
unsupported Mac LeakSanitizer only; ASan/UBSan enabled. No gate/memory/timeout rerun.

`debug.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=27 executed=27 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

`asan.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=27 executed=27 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

29 unique entries/profile, not merely 27: required species and old creation
original cases execute separately once, then 27 main cases. Breakdown: 5 species,
6 creation, 4 release, 2 leaf legal/refusal, 1 all-family vectors, 11 legacy formats
controls. The unchanged ground kernel original case does not enlarge species
profile coverage. Real checks are local original-data proof, not hosted CI assets.

Apple Clang 21/GCC 15.3 strict O3/Werror: annowelt.cpp, wildlife_create.cpp,
wildlife_species.cpp and test TU. Production checks reused unchanged; final test
TU separately checked after corrections. GCC deliberate shadow control rejected.
clang-format 23.1.3 dry-run/lint_sim/diff pass. Builds 0 compiler warnings; inherited
duplicate anno_sim linker warning remains. Host M5 Max/18 logical/48 GiB, macOS 27.0.1
(26A434), Spotlight active; conservative jobs, no performance claim. Host log retained.

178-entry recursive private manifest `closeout-sha256.json` SHA256
`c17eeca1a3812f892e27d06c062fbf7fb2d8f8ca8350928c43d142c9c0e21f5b`.
It pins raw ranges, oracle scripts/rows, actual first failures, build logs, commands,
strict objects, all causal variants/binaries/results and retained canonical images.
No original bytes, decoded rows or listings enter git.

| Receipt | SHA256 |
|---|---|
| Debug gate | f4ee4bc3240c561910bf200a647e837868d9bad9cf92176e423efb0bd485bfd8 |
| ASan gate | 34b5f7cda19917dc1a083a306863c1111fd9543762d5a5e65ca811b0e3d532a2 |
| Debug retained sim binary | adbda6fe32ffe06716776825f81e36b95ea9c23b7d596d693a72f15d3e1c27cc |
| ASan retained sim binary | ab1ee626de74f297c2d0361490319bcc800c0e8c35187bb31090b4acb7d7c5ab |
| Debug retained formats binary | 96c23cccda1ac5b4145a6e3a7f3ddfd531ca3cbe38d768024df0cbd98057a85e |
| ASan retained formats binary | 7fc9920d562a5c6f9d07e27299c0d495d740ec369520695069c19219ed5ba606 |
| 1024 frozen entries | a03fd7a3cf235b5852f20ea5816e3afabdf5441eb04625b4f16140ddafcd7b71 |
| 32 raw-source oracle rows | fda833fca3e31245f1b9ce6088015ac6b0379a37fde6687ed2602b914617b0cc |
| Five independent selections | 2a9033ccdf39e93b56b35723b088085a002f8a8a6f7bf769d95b0ce41c5e17d7 |
| Oracle input identities/profile | 1eaa51dbd2f758b956189e35ad97fda6997e83b2a4d18091e1298bd76a90697e |
| Final causal commands/results | 4169097602b9ff2eed5d253c4691657beff47752751012886852c5956cbac020 |
| Final test Apple strict | 78be5912f066b6ac5a3b45e42f035c24d69a1c84178b9ab66972a40d9ccecdb4 |
| Final test GCC strict | 7f7ae2197df87a5984aee3cefffbdb5bb3370c5ffe8eaa2e9eac8886bffcf569 |
| 292 unchanged tracked fixtures | 76a3095c0c56ede5bd923b95d39ea3b64114648532b3c5c0941fd06b5bdc322b |

Prior release 83-entry and creation 209-entry manifests independently rechecked
intact. Tracked synthetic files 292; prior physical 293 included ignored Python
bytecode, not a new tracked fixture. Selector/ref/vector/pins/default/state unchanged.

NOT_RUN/unproved: other climate rows, reused catalogue/overlays, fractional position/
height generation, cave provider/zero-entrance behavior, actual manager/lifecycle,
World/scheduler/global PRNG provenance, full OQ-578/579/584, HE/original runtime,
LeakSanitizer, local Windows/WSL, broad/full/heavy/opening/pins. No state/tag/Rules/
default/identity/PRNG/navigation/raster/graphics changes. Other jobs/coordinators,
originals and owner awake assertion untouched. Hosted CI read after ordinary own-
branch push; orchestrator owns exact-head terminal closeout. Final review pending.
