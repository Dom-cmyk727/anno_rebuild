# WILDLIFE-DISCOVER-1 — frozen review evidence

READY FOR REVIEW; independent orchestrator review pending.
Tested source `ddab0993589f04524c61bdc329f58a401e9f24c6`, tree `83ff22247f4f9207083e5606047a5b8c9d1b9177`.
Base ANBAU `02111ea2139cfddf1e4018bcc229fd55d918056e`;
dispatch `74e978beb8030c74f8281ac9ced5342f19d1cd8e`, read without merging.
Publication adds only this evidence and REPORTS; full publication head/tree/CI
accompany direct handoff. No predecessor refs overwritten, no main/board merge.
Original bytes/listings and absolute host paths remain private in cache.

## Contract and discriminating acceptance

[Primary contract](../../sim/wildlife-discover.md),
[ordered prefix](../../sim/wildlife-prefix.md),
[previous ANBAU contract](../../sim/wildlife-anbau.md).
D = GOG instructions/data, P = original parsed input, C = authored execution.

| Requirement / primary | Executed evidence / smallest wrong reading | Independent disposition |
|---|---|---|
| D low word / 0x100899ab,b6 | 0xabcd8001 -> 32769, not full 2882371585; 0x1234ffff -> 65535, no player-slot clamp | pending |
| D replacement / 0x100899b6 | Header 6 = 32769 then header 8 = 2; final 2 rather than wrong union 32771 | pending |
| D no write on absence | Optional absent versus completed zero, maximum and nonzero with asymmetric upper bits | pending |
| Defensive length | 0,1,2,3,5,8-byte payloads refuse with preceding scalar retained; failing header has no completion marker | pending |
| Ownership and chronology | Input unchanged then destroyed, scalar and source evidence owned; ore/BERGWERK/ANBAU/fertility/HIRSCH effects and draw assignments preserved | pending |
| C actual-TU causal check | Legal 63/63; one actual assignment changed to union, exact intended final-mask assertion fails with 62/63 passing | pending |
| P same original once | Independent Python parse 4 bytes, low=0 upper=0; C++ derives same independently, next54/KONTORID, earlier effects/cursor unchanged | pending |

## Finite primary closure

AnnoWelt header arm 0x10089977–0x100899cb compares DISCOVER at 0x100dcec8
through 0x100b4b20–0x100b4b6c. Raw helper 0x100b4c40–0x100b4c5f takes declared
length from header+0xc (0x100b4b10–0x100b4b13) and calls supplied stream read+4.
Count local initialized0, reported bytes added to caller accumulator; scratch LOW
WORD is loaded0x100899ab and replaces island+0xa6 at 0x100899b6, then returns1.
No direct other island/resource/occurrence/terrain store, callback or random draw.
Stream I/O semantics on every malformed/short input are not established: this arm
checks neither reported count nor return. Defensive exact-four-byte framing is
not an original memory-safety or transactional-I/O equivalence claim.

Saver 0x1008a669–0x1008a6a0 emits four bytes from island+0xa6. Upper adjacent
word is present in the file but ignored by this load arm. Getter0x10087c62–
0x10087c6b zero-extends the scalar. Reset 0x1008c740–0x1008c7c0 clears it at
0x1008c7b9; reset is not an observed header completion. Optional widened u32
represents imported low16 without imposing a modern player limit. No bit meaning,
visibility algorithm, diplomatic consequence or later recount is invented.
The existing source-order caller/delegation proof is reused, with no new lookup,
deduplication or callback. Normal scenario player-slot validation is unchanged.
INFO citation corrected through 0x10089802, store 0x100897ee, without runtime edit.
Historical frozen ANBAU evidence/source hashes are preserved unchanged.

## Causal proof and original continuation

The two direct authored cases pass342 assertions. Private actual adapter TU is
compiled/linked against the normal test runner after a legal63/63 control. Exactly
one assignment `prepared.discover_mask = mask` is replaced with prior-mask union;
mutant exit1 fails `is.discover_mask == 2`,62 controls still pass. No canonical
source mutation; rebuilt canonical binary matches the legal-control identity.
Strict GCC negative shadow control rejects; no new evidence framework.

Same raw Tutorial/01Discover01 island ordinal0/slot11, origin24,24,128x128;
supplied stamp500/mode0/raw draw7 remains controlled input, not original global
cursor. Header 53 DISCOVER imports0 with zero draws.54 headers complete, plus
labelled failed header54 KONTORID in55 header evidence rows.0 spawns; HIRSCH 71
unreached.4760 terrain accepts/0refused,4589 resource draws,16 terrain epilogues,
fertility49 raw16384/effective8160 changes; BERGWERK50 has9 entries/20kind1 nodes,
OCCUR51 has1 ore/1kind6node, Q512000/reserve1,20masks17,21sharednodes/8 chunks;
ANBAU52 bytes135/tokens13/seed61449/mask65535/ignored0. All retained assertions
execute. No global scene initialization, complete island or resumable prefix claim.

## Gates, unchanged dependencies and limits

Mac17,6 / M5 Max,18 logical CPUs,48 GiB RAM; macOS 27.0.1 build26A434,
AppleClang21.0.0, GCC 15.3.0 cached read-only/network-off container.1.5 TiB free;
contention checked, no competing test process observed. Jobs1,2 test threads,
2048MiB per thread, host memory admission enabled; no coordinator/owner job stopped.
ASAN detect_leaks=0:halt_on_error=1, UBSAN halt_on_error=1:print_stacktrace=1.
No memory refusal, timeout, canonical failure or repeated whole gate.
Debug and ASan each have18 main+3separate required contracts=21 executed passes.
Original Debug1/1 once. No skips, empty, incomplete or sanitizer diagnostics.
Doctest filter exclusions are not skips. Raw counters follow without alteration.

`cache/wildlife-discover-1/debug-final.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=18 executed=18 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

`cache/wildlife-discover-1/asan-final.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=18 executed=18 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

`cache/wildlife-discover-1/original-final.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

Both affected TUs pass Apple/GCC O3 strict Werror; GCC shadow negative rejects.
clang-format 23.1.3, sim lint, link resolution and diff check pass. Incremental
Debug/ASan links retain the inherited Apple duplicate-library warning; no claim
of a warning-free whole build.276tracked fixture files byte-equal base, including
pins; this is a byte audit, not a claimed full pins.py gate.

Reuse prior resource/stone/cliff/rock/fertility/BERGWERK/OCCUR/ANBAU algorithms and
long campaigns: no changes to those algorithms, metadata reader, normal loader,
World, formats framing, native/save, scheduler or gameplay consumers. Affected
prefix integration is rerun; required cases include malformed prerequisites,
new malformed framing and causal ownership. No new public evidence tool or picture
change; automatic picture contract omitted deliberately for headless import scope.
NOT_RUN: full suite/pins.py, target Windows/WSL, original runtime, History Edition,
post-island/global callbacks, visibility/diplomacy, World publication, wildlife
creation/tick, derived-cache rebuild consumers, HIRSCH on this original prefix.
Exact publication CI is pending at handoff, never represented as accepted.

## Decisions and reasoning

- Retain widened scalar and per-header optional evidence: absence, explicit zero,
  completion and diagnostic boundary stay distinct without new persistent state.
- Four-byte framing is defensive; do not emulate unsafe/unchecked original reads.
- Do not copy normal-loader player policy into the raw importer or OR duplicates.
- Stop at actual KONTORID 54, without skipping ahead to wildlife/HIRSCH.
- Reuse unchanged algorithms/long gates and preserve predecessor frozen receipts.
- Correct citation only in current contract, preserving historical source evidence.

QUESTIONs: none in this finite arm. Next header needs a separate release.
Independent review pending, no executor self-approval.

## SHA-256 receipts (raw artifacts stay in cache)

| Artifact | SHA-256 |
|---|---|
| `cache/wildlife-discover-1/debug-final.log` | `6b086cb6f6c4522af64d3a917141a51781d31616ea2acc1ee406262d5809e1fc` |
| `cache/wildlife-discover-1/asan-final.log` | `93f5c6a2b53ec5e06b64749ddc68289ceadfb4fa5f8fcd3afa36a1453f602c12` |
| `cache/wildlife-discover-1/original-final.log` | `d53682f7b265e6d7c4b8732aa08df0f0459e06121888477ece0d3b9638d84e34` |
| `cache/wildlife-discover-1/debug-final.log.inventory.json` | `d54efc888216d837c47503ce0d73114735123250988e4ae162434faef4d6de66` |
| `cache/wildlife-discover-1/asan-final.log.inventory.json` | `b9b5a354cc167bd63b6d0925e9dc11340971d26b33a58510424bd6ec5aa2f117` |
| `cache/wildlife-discover-1/original-final.log.inventory.json` | `3111c066b2ed5bc8ba78cec67f0107b8b9c6b87374e3abb38ecf9aa56ce5a46e` |
| `cache/wildlife-discover-1/fixture-audit.json` | `57cbe1e96e463041c57b053e6f0b3bb996e91c63d43ca819befabfaf9fb62929` |
| `cache/wildlife-discover-1/independent-header.py` | `97d42249b09cdaffebc33221ed434c1ae45ac7ba8473f244337f84b5d4c923d0` |
| `cache/wildlife-discover-1/independent-header.json` | `461463a8d556acf15b1ff04be39137f426653151bde0c14a5afc588ee994922f` |
| `cache/wildlife-discover-1/independent-header.log` | `23af156630bb0ba4d0bf56505055e7f864c742acc79497e2ddc924471c5ada48` |
| `cache/wildlife-discover-1/discover-arm.private` | `9d40f9d81edb264c1a3077a330eede831f96a362f451b9692153b0b507454b0e` |
| `cache/wildlife-discover-1/raw-read.private` | `e3453b80cb4691c40f31fd0e6b2a8d6a2532632bdd2cbdfafdbeb9fbbf2c1b23` |
| `cache/wildlife-discover-1/declared-length.private` | `eff8571d32285762d575c927e2a5f02a61b43608c0be3befd705ca1025f1228f` |
| `cache/wildlife-discover-1/tag-compare.private` | `f3a1aec93254c2b26c1dfd2b297bd07368dc528d57ce9e3d989226edeca9cfcd` |
| `cache/wildlife-discover-1/discover-save.private` | `8d8fef0600df6dc0550b2fa0e72b267d7c5cf8fcdc3a62d169c6d7e58349c8d6` |
| `cache/wildlife-discover-1/scalar-getter.private` | `8ff123d90ef540ac4f0c0aa99487b1a873f47e63274979affe16bcf37a2a1cb1` |
| `cache/wildlife-discover-1/island-reset.private` | `36b21404f6175fbe0755361cdcd84edbc22926a036fca86ad683bc5b382b5f20` |
| `cache/wildlife-discover-1/reset-identities.json` | `47ab39cf7fa25d9f74d2f22af68a62130aa185542b95c39b0626b48683bfcb6e` |
| `cache/wildlife-discover-1/reset-legal.log` | `94391b958df8c2eb4838f985a1ad551a3e415ca1f5a7d0c8b507fa776a04983f` |
| `cache/wildlife-discover-1/reset-mutant.log` | `2b48f9c0ea498973907a9f49b3971ef685f444c4d2262f1e098cf15352033a50` |
| `cache/wildlife-discover-1/mutation-proof.py` | `31abd6b8aaef08d1f4e39dde049761aa7930de3e5f473422b766a07c3e82ea5d` |
| `cache/wildlife-discover-1/mutation.log` | `79f14f42f8c20446e5fd5030b51ffecf526ca5ab182b93b47b7099cb879b4657` |
| `cache/wildlife-discover-1/build-first.log` | `777a9046029e138cace7600b3b56ac033e68c193fae47d92b5f8b9f5db233294` |
| `cache/wildlife-discover-1/build-controls.log` | `4e1d40078c7f77e54d0772f083c5b4d5b43551d7f8338853dda914476e824e5a` |
| `cache/wildlife-discover-1/build-canonical.log` | `c2ed93ffd93c840755c0e91392ae593b1b52de9b7aaf6c208123feaceb1dde2e` |
| `cache/wildlife-discover-1/build-asan.log` | `777a9046029e138cace7600b3b56ac033e68c193fae47d92b5f8b9f5db233294` |
| `cache/wildlife-discover-1/apple-warnings.py` | `89ed2ea6451cd4864a7e19382cb185713a289f6a29b63cd61f7c756024a0177b` |
| `cache/wildlife-discover-1/clang-warnings.json` | `23d128ee82d80478b964eb9456347066850bd22e271d4bf4d390e6f5125e4a46` |
| `cache/wildlife-discover-1/apple-warnings.log` | `923b631138bf1be975b9f8d2727e5d12afe236a6a0c6b8bf48a990f4e02382ac` |
| `cache/wildlife-discover-1/target-warnings.sh` | `434c63bb5f96f4549b9cc6644522810f8c4dd6e58603179f63b494a25d7ffe0d` |
| `cache/wildlife-discover-1/gcc-warnings.log` | `e029815c43244dcd9c829e399c8cfe3b664363751f161c9b3263ec1ee9cb330b` |
| `cache/wildlife-discover-1/controls-first.log` | `95e4d8c45c09ba598f9083f1c1597b7c46b62bcb97a1fb91b52ae203e33503c3` |
| `cache/wildlife-discover-1/lint.log` | `c90ca6eba541545a82beb573bfcbcc47531df735edd4ff68c814ab50296e6948` |
| `cache/wildlife-discover-1/format.log` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `build/dev/src/sim/anno_sim_tests` | `e3d9c8ffe5283c22ffbbd3fb55f51c5e25e1f84d2b44119024f1b21b7e95ad8f` |
| `build/asan/src/sim/anno_sim_tests` | `11b720948a5a94c3501c72ac481c9e1997e69b033116412e7852fadaacfece5c` |
| `docs/sim/wildlife-anbau.md` | `a67f9284dbfae69e27219e62a435e5e9705d869891f216ea8b0cceab53772061` |
| `docs/sim/wildlife-discover.md` | `cd16341d1a7d4936faad47657d82b8c9ed5b10387d82f1cca551fa45689e29ab` |
| `src/sim/tests/wildlife_prefix_test.cpp` | `b6ec760ac0a9eab7eabf2c13fb53b013703023676826baf7ddd0d19547c003c4` |
| `src/sim_load/include/anno/sim_load/wildlife_prefix.hpp` | `5d234fb9f0357de812692980924d3c8040cea8388aa20b17e5241ba796c3f285` |
| `src/sim_load/src/wildlife_prefix.cpp` | `697e3f0ec15785de2ff8b4e8061049dd9485aecadde996c713a484fdaf181839` |
| `game/app/AnnoWelt.dll` | `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2` |
| `game/app/AnnoGame.dll` | `ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc` |
| `game/app/Scenes/Tutorial/01Discover01.sze` | `225ed50b005743e00d8d3ffe18667d4565dc781ef45bd6d9b767fd2fe056c0f6` |
