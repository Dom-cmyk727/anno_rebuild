# WILDLIFE-POSTLOAD-FILL-1 — frozen isolated first-call evidence

READY FOR REVIEW. Tested source `db2240a656f50e65d8557667d528a4c9d56a07e2`,
tree `795a25a7b8562a7767e2fb10b0c0c9666bf0798d`, base INPUTS `263958ed`.
Final publication changes contract/report/evidence only; head/tree/CI accompany handoff.
Dispatch `13f2f7536d1266ce7c3b0bf3e3d077743631d45b` read, no board/main merge.
[Acceptance and supported inputs](../../sim/wildlife-postload.md#11-isolated-first-call-apply--wildlife-postload-fill-1).
Primary bindings remain [INPUTS](../../re/wildlife-postload-inputs.md); raw module/span
identities are frozen in its receipt and POSTLOAD, not replaced by new self-comparisons.

## Reference before implementation

Commit `26e92579` precedes C++: 19 authored fill rows append to 12 byte-identical
GFX rows. Full fields, signed rectangle, immutable raw/effective planes and draw
suffix are inputs/expected outputs. Python computes decisions from inputs only.
31 vectors agree; 42 CLI controls execute child statuses including wrong output
exit 1, zero/missing/malformed exit 2, completion/ground/profile/geometry errors.
First control edit matched HEIGHT twice (input and expected output), refused to
mutate, and failed the contract. Correction (2026-11-19, NAV-INPUTS intake):
controls-first.log is empty, with the unchanged SHA-256 e3b0c442… listed below;
it does not retain that failure text. No retained stderr/console receipt was
found in this task's logs; only the historical note survives. The empty file
and final successful evidence are preserved; the failure was not recreated.
The corrected replacement anchors only the input and all 42 controls pass.
No reference algorithm change was needed to fix that control.

Raw aux abcdefaa splits into retained aux aa0000aa, code ef, mask 1cd. Grass mean32
with code2 yields retained aux aa000020/code2/mask0 (raw aa000220); fish mean15
keeps code/mask and yields retained aux aa00000f (raw abcdef0f). The implementation
does not duplicate code bits into aux. Original V1–V5 field arithmetic was checked
against the frozen complete928a0–92b32 read and the now-bound mean/getter profile.

## Capability, ownership and causal proof

Private move-only capability: complete prefix/island/header provenance paired to
exact scene SHA; raw INSEL5 geometry, matching Terrain digest and reconstructed
source HEIGHT/GFX; last successful FRUCHTBAR including zero completion count;
raw/effective low-nibble identity. Immutable role facts cross-check loaded unique
HEAD/id/flags against registry head_raw, zero-extension and actual walk_code(0).
Alternate 9001/9002 roles with code3 and a short36-byte HEAD/default4 use this same
seam. No missing facts or unsupported mean profile acquire guessed defaults.
All checks precede the one owned island/tile clone; apply moves it into the result.

Source proof uses existing strict format accessors: complete, nonoverlapping
HEIGHT/GFX blocks only. Partial/overlapping setup cannot be validated here and
refuses, even if another transient prefix can represent it. No silent fallback
or source relocation. Validation cost is source/entry/selected-plane linear;
apply is clipped row/column linear with four corner reads per candidate, no
per-cell map copies. Raw Terrain/ground/digest stay unchanged, dirty tile digests
use existing setters and compare with independently streamed literal expected fields.

Success/refusal source equality, zero-valued completed ground, exact full-field
vectors, mutated/destroyed caller lifetime, deterministic repetition, occupancy
idempotence, missing/mismatched provenance and facts, and moved-capability reuse
are tested. New prepare/apply callers are definitions/header/tests ONLY
(callers.txt). Prefix/normal load/World do not call them; no navigation/raster/GFX
composition or core/state/registry/default/tag changes.

Private production-TU legal control: 4 cases/41185 assertions pass. Masked occupancy
mutant: exit1,4 intended failures/41181 passes. Signed fertility: exit1,4 intended
failures/41181 passes. Grass-style fish writer: exit1,20 intended failures/41165
passes. Valid setup executes in each; actual full-cell and digest assertions fail,
not metadata setup. Replacement spans verified once; compile/link/source/binary
identities in causal.json. Canonical source never changed; hash equals legal TU,
and canonical rebuild preceded gates. No private mutant entered git.
First original-canary compile used nonexistent TileLayer::rect(); retained one
compiler error in build-canary.log, fixed before freeze. No canonical gate failed.

## Canonical gates

Debug `cache/wildlife-postload-fill-1/debug-final.log`:
```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=13 executed=13 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```
Bounded original `cache/wildlife-postload-fill-1/original-final.log`:
```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```
ASan/UBSan `cache/wildlife-postload-fill-1/asan-final.log`:
```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=13 executed=13 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```
Per synthetic profile: 13 main =4 fill +3 retained GFX +5 VertexGrid +1 agreement,
plus1 separately required reference contract, no double count. Original is1/1
separately. No failed/empty/skipped/incomplete/timeout/memory refusal or sanitizer
line; no whole-gate repeat. Jobs1, threads2,2048 MiB/thread, admission unchanged;
host access as authorized. ASan detect_leaks0, LeakSanitizer NOT_RUN.

## Original-input leaf, independently derived

Scene Tutorial/01Discover01, SHA225ed50b005743e00d8d3ffe18667d4565dc781ef45bd6d9b767fd2fe056c0f6,
128x128 at original origin24,24. Normal reviewed local prefix on separate copy,
no injected stock/actors/position changes. Supplied stamp500/mode0/draw7, not a
reconstructed global stream. Frozen4760 accepted records/4589 resource draws plus
156 HIRSCH draws ->4745 before leaf. All73 headers complete,156 spawns retained.

independent-leaf.py derives raw footprint occupancy union, valid because the
reviewed all-accepted import only overwrites/skips nonzero cells, never clears.
No new full admission campaign: reuse is explicit. Union5001 cells; raw HEIGHT and
last FRUCHTBAR yield fish8486/grass1559/unchanged6339. Action-map SHA
`a66cff880b8ca148261ba7b61a9e288d9f4a4a90717546014386c4c2f0971487`
was computed BEFORE C++ canary and its output. C++ re-derives and checks this plan,
then checks every output field against the plan plus preserved prior-prefix
fields, independently streamed expected chunk digests, source immutability,
Terrain/ground/GFX/cursor preservation. 83270 assertions pass,12.55s CTest wall.
```text
LEAF_ONLY fish=8486 grass=1559 unchanged=6339 prefix_consumed=4745 new_draws=0 postload_order=NOT_RUN original_runtime=NOT_RUN
```
This is not original runtime, full ordered postload, arbitrary catalogue reuse or
global cursor parity. Ground-preprocess presence comes from completed FRUCHTBAR,
not a constant injected to make the check run. No first/second-call ordering claim
beyond the isolated apply boundary.

## Efficient checks, reuse and limits

Format23.1.3/lint/diff pass. Three affected TUs strict O3/Werror AppleClang21 and
GCC15.3 clean; GCC shadow negative control rejected. Compiler warnings0; inherited
Apple duplicate-library LINKER warning retained, no full warning sweep claim.
Apple M5Max/18 logical/48GiB/macOS27.0.1,1.5TiB free; Syncthing was using one CPU
at intake, left untouched. Other jobs/coordinator/awake assertion untouched.
276 prior synthetic files byte-identical; the existing 12 GFX rows also unchanged,
only19 new rows append. No pin/recording changes, no full pins.py claim. Core
sim runtime/public headers, prefix adapter and GFX production helper byte-identical.
Unrelated sailing/render/corpus/long/PE campaigns reused by unchanged dependency;
GFX tests rerun because their shared-file reader now skips the named fill type.

Decisions and reasoning:
- Bind completion to exact source/planes rather than trusting a Boolean on unrelated terrain.
- Copy validated facts and one owned snapshot; retain const backing, no temporary borrowed pointers.
- Preserve original split-word effects and width/unsigned rules with nonzero full-field controls.
- Retain independent raw-input action plan and distinguish prior-prefix reuse from original execution.
- Keep both postload helpers UNWIRED; leave all later callbacks/global/state work separate.

NOT_RUN: original runtime/HE, c2-zero preprocessing, navigation/raster/V9 repair,
GFX composition/ordered postload, global RNG/World/native/lifecycle, unrelated full
suites/corpus/performance, full pins/warnings, Windows/WSL, LeakSanitizer.
QUESTIONs: none blocking this bounded helper; independent orchestrator review and
exact-head CI pending at publication. Submit without waiting CI per packet.

## SHA-256 receipts

Private originals/listings/action rows stay ignored; no original bytes/script/pixels
are committed. Log sidecars retain exact workflow/source/command/config identities.

| Artifact | SHA-256 |
|---|---|
| `docs/sim/wildlife-postload.md` | `f4a415e6dfd27d44ceaff38fcd201da1f9e1de264bf43999ec8bd22f25afb3f5` |
| `src/sim_load/include/anno/sim_load/wildlife_postload_fill.hpp` | `4b7a71eb96ddde31f107dcf1a417912190e24c8063390bc44653a74dcfb99eb4` |
| `src/sim_load/src/wildlife_postload_fill.cpp` | `59a97f540e2caec8ab14d2481c66edb070bd053155a41af6daf9eabd9a1bdb17` |
| `src/sim/tests/wildlife_postload_fill_test.cpp` | `0709e10463492ff1af1c9a9ccce4e2e4a2f8b308219c5d15ef2193528601825a` |
| `src/sim/tests/wildlife_graphics_test.cpp` | `3c8c0d0647a28a2b1de7b47a05b10f3476706ac89d98e2282d33b9b02f3abeb1` |
| `src/sim/CMakeLists.txt` | `08c76b0d92e46b5b5962236d7384761a6ba40852d3ee7b34f700ae37e7c42a66` |
| `src/sim_load/CMakeLists.txt` | `9300fac52ca77fc6da5fa44faebd9eb9bc30560c86b89c1c816d344259b904f2` |
| `tools/wildlife_postload_ref.py` | `dcc5de2ae58c93a2b4f00fa76a38e81bdfe51c830493f05c561c77b4a39e752a` |
| `tools/wildlife_postload_ref_contract_test.py` | `3c514c796b7012ebb476896a142a2d95cd823758bec7463919acb16d2307d42a` |
| `fixtures/synthetic/sim/wildlife_postload_vectors.txt` | `e946e0d7ca42c18ea3961d5aeed4c476a9bcf7cd98b32f623b21067ab124ee80` |
| `build/dev/src/sim/anno_sim_tests` | `98a759e9707f18efeb229fa64963c9de83228d9dd63d1e061d4ff99a625f3da3` |
| `build/asan/src/sim/anno_sim_tests` | `0ba181e097e9ecdef135c900594801f8a5df1be366b348b1417c28976546e5ae` |
| `cache/wildlife-postload-fill-1/author-vectors.py` | `62f15dbbfe9271c4e43898fc4706a580ea49b7c98570e977e5e70af73334686a` |
| `cache/wildlife-postload-fill-1/reference-before-cpp.log` | `e082475e35f5a03fd69d0b6d7b8d6029b2ddfaf355a243b171e76d5215395124` |
| `cache/wildlife-postload-fill-1/controls-first.log` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `cache/wildlife-postload-fill-1/controls-corrected-before-cpp.log` | `150e33c5d5c9dc790c880eb6c4c4d1c6bcf3eeb758ae6f430489b4e1f129a88e` |
| `cache/wildlife-postload-fill-1/build-first.log` | `facd9e5eeb9b0fb4c7a6fb6501e447fb3b785735b0cbe1dde2685df9e75e8dd8` |
| `cache/wildlife-postload-fill-1/build-canary.log` | `7fb96d1fa564d6dad14c3a4c7d1c6143f169c6246d5fa5802f4d283b4e360a45` |
| `cache/wildlife-postload-fill-1/build-corrected.log` | `4781e0d91a93b4370dfd11d5ac31182846cbe1da4b38c51bc3725bcb370d5a2a` |
| `cache/wildlife-postload-fill-1/build-final-controls.log` | `d6814e35b865887d8b7cb2e3fec5861811560f64db3408e1acefe8ebad9ec3fa` |
| `cache/wildlife-postload-fill-1/build-canonical.log` | `c2ed93ffd93c840755c0e91392ae593b1b52de9b7aaf6c208123feaceb1dde2e` |
| `cache/wildlife-postload-fill-1/build-asan.log` | `c6234fb10e8262b4b62bd7c0133974c674193e20f7fb6c6bb46155853c9186db` |
| `cache/wildlife-postload-fill-1/causal.py` | `b56c1304ad3c2fe7b1583e657474e72c94cb2bc2ab73ae6be88391638657f94b` |
| `cache/wildlife-postload-fill-1/causal.json` | `6756d565bb09698fab4aa0e3fbf7e6ce1932d29a415d35aff3060337091e6569` |
| `cache/wildlife-postload-fill-1/causal-console.log` | `755ed556a5b0880e589a22ff6530ae7cbbb1557904b6a7c00d03f1585cd86efb` |
| `cache/wildlife-postload-fill-1/legal.cpp` | `59a97f540e2caec8ab14d2481c66edb070bd053155a41af6daf9eabd9a1bdb17` |
| `cache/wildlife-postload-fill-1/occupancy.cpp` | `c69dd6047a9768971e5c78923a04ef0176a82919c7085bc1b42235378aca7638` |
| `cache/wildlife-postload-fill-1/signed.cpp` | `91a902229f2d32c341741c7e2c891e36edf1527f998830d98dfada0b2baec62b` |
| `cache/wildlife-postload-fill-1/fish_writer.cpp` | `02ad96f0fa3001672cbc0b7fba5c0edffb5190cb494ff472ceec8852ea6691b5` |
| `cache/wildlife-postload-fill-1/legal-run.log` | `3701080632a57ce33878543241e78141b9f6e6f1157c9f293d9b698a9cbe3d44` |
| `cache/wildlife-postload-fill-1/occupancy-run.log` | `77182d40a64a535808408086fac36b908f73957830e439a98ce65b09dae1bf85` |
| `cache/wildlife-postload-fill-1/signed-run.log` | `7bed8fb1080c7eac349d773aa233b9c4016275e4df6938b022964383d68cb52d` |
| `cache/wildlife-postload-fill-1/fish_writer-run.log` | `9a3bf56342ac9dea61bba39699fb33799949cc5209400deaa7707c7505583876` |
| `cache/wildlife-postload-fill-1/apple-warnings.json` | `434297e181a5c01b5184abba36939e9ab58a1f5044a9916b1554fb13efa7d02f` |
| `cache/wildlife-postload-fill-1/apple-warnings.log` | `9a0df522c17e97fdfbc9368b8202b9aa79f1b36c7680d87c54f814e26ac29c3b` |
| `cache/wildlife-postload-fill-1/gcc-warnings.log` | `e49b6024a2361cdcfa64212611676873e5d0cb228b03e8dbac5138f1ee317fc4` |
| `cache/wildlife-postload-fill-1/target-warnings.sh` | `beb393792e4b1664b07d7a9ae822d40ac0ea5fe68a005302861047575c0d7f23` |
| `cache/wildlife-postload-fill-1/lint.log` | `c90ca6eba541545a82beb573bfcbcc47531df735edd4ff68c814ab50296e6948` |
| `cache/wildlife-postload-fill-1/callers.txt` | `ad6a7ad1b86bcbba1599792ac40295c91d9d7786a19a2bf3058b1cd06835209c` |
| `cache/wildlife-postload-fill-1/scope.json` | `31f2458be6d42bcc1deb02a80f9c85f04f6d2636983482729ac89a13d8ddaf84` |
| `cache/wildlife-postload-fill-1/independent-leaf.py` | `3b0e07c75755f47f98ac94e46483c59cf321d6e41f13ea06c6e017c72f075c07` |
| `cache/wildlife-postload-fill-1/independent-leaf.json` | `40f3249ddd7c9bc7119ca623b052160f897cea6d4e2acc177c082082d1c9a352` |
| `cache/wildlife-postload-fill-1/independent-leaf.log` | `6123b56c18a3e688245989751507d22ae335626d8aed8e1164a4ed7280c50dee` |
| `cache/wildlife-postload-fill-1/expected-actions.private` | `a66cff880b8ca148261ba7b61a9e288d9f4a4a90717546014386c4c2f0971487` |
| `cache/wildlife-postload-fill-1/debug-final.log` | `dfefbe717f5a4a6f001b43e4c9bd04853dcbb0c8bae98f5d08c1eac893fee39e` |
| `cache/wildlife-postload-fill-1/debug-final.log.inventory.json` | `710b7240b966f511cb6803c11b4f4b3a70aed5e33bb63b487a628a17d39d416f` |
| `cache/wildlife-postload-fill-1/asan-final.log` | `eaad1df39c65a5579360e30360f8e3147850e5185b3c22b1a1694caab56574c0` |
| `cache/wildlife-postload-fill-1/asan-final.log.inventory.json` | `ba6189e4807f82d84174e47b8d2aee76fe3f0118cf595fe51813d0cd45240a93` |
| `cache/wildlife-postload-fill-1/original-final.log` | `fb2cc305d76756ea15aeb611348eb935d595092d6f10ee16aea97d5deaf9fa4c` |
| `cache/wildlife-postload-fill-1/original-final.log.inventory.json` | `a50ef361d9b60a57ca92d3f3fbab40e7215e2398b16ade7310b3a1e5f17c41a3` |
| `cache/wildlife-postload-fill-1/docs-check.log` | `d41df85dad2bd05683ab42a2b36aeeacd20764e260fa051d10ebb9548842dc53` |

Final docs check, after the 50 source/artifact receipts:

```text
DOC_CHECK files_or_sections=3 links=17 hashes=50 missing=0 status=PASS
SCOPE: frozen runtime/tools/fixtures/CMake unchanged after gates; report/contract/evidence only
```
