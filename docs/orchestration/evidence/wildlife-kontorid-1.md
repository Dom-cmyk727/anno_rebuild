# WILDLIFE-KONTORID-1 — frozen review evidence

READY FOR REVIEW; independent orchestrator review pending.
Tested source `c199cf1f382fb40012ea4c660b763d51530f56f4`, tree `b6a3ad124a415301f1c5cb3c9ab65a087fe10e8e`.
Base `8d97b6956189faf9483f6d9944b42676362d1c7d`;
dispatch `c55b88413b5bf16d453afd1939a5b647aa74622f`, read without merging.
Publication changes only this evidence and REPORTS; direct handoff names full
head/tree/exact CI. Previous branches, source pins and raw logs remain intact.

## Primary and acceptance

[Prewritten primary contract](../../sim/wildlife-kontorid.md),
[ordered prefix](../../sim/wildlife-prefix.md),
[preceding DISCOVER contract](../../sim/wildlife-discover.md).
D = GOG instructions/data, P = original parsed input, C = authored execution.

| Requirement / primary | Executed distinguishing control | Independent disposition |
|---|---|---|
| D u16 LE replacement / 0x10089bee,bf4 | Bytes 2,129 -> 33026, not byte 2 or swapped 641; maximum 65535 stays unsigned | pending |
| D no increment/union | First 33026 then 7 -> 7, not 33031 or 8 | pending |
| D initialization separate | Optional absent versus engaged zero after nonzero; no normal-loader zero refusal or fabricated default 1 | pending |
| Defensive exact framing | Lengths 0,1,3,4,8 refuse exact KONTORID boundary at header 5; preceding 33026 retained, no completion marker | pending |
| Different record identity | KONTID is unsupported, not silently treated as island KONTORID; preceding counter retained | pending |
| C owned chronology and unaffected state | Destroyed inputs, per-header values/provenance and final counter remain owned; surrounding resource, ore, discovery, ANBAU, fertility and HIRSCH unchanged | pending |
| C actual adapter-TU cause | Legal 67/67; exactly one replacement-to-union change fails final counter == 7, 66 others pass | pending |
| P same original once | Raw header 54 length 2/value 1, no draws; next 55 PROD2 length 6, all preceding effects retained | pending |

Finite original arm: AnnoWelt 0x10089bb1–0x10089c09, not the truncated range
ending at the store. Tag string 0x100dceb4 is KONTORID. Exact comparator
0x100b4b20–0x100b4b6c and raw reader 0x100b4c40–0x100b4c5f are reused.
Reader obtains declared size at header +0xc through 0x100b4b10, calls stream +4.
Before pushes, count pointer = [esp+0x10], payload pointer = [esp+0x18].
Store 1 at 0x10089bd5, after three pushes [esp+0x24], initializes PAYLOAD
scratch at original [esp+0x18], not count. Reported bytes enter accumulator at
0x10089be2–0x10089beb. Word loaded at 0x10089bee replaces island +0xc6 at
0x10089bf4. Immediate success return through 0x10089c09. No allocation, increment,
nonzero test, resource mutation, gameplay callback or world draw in this arm.

Saver 0x1008a6aa–0x1008a6e3 selects 2 bytes and writes island +0xc6. Reset
0x1008c7fd and constructor 0x1008cbe9 independently store word 1. An absent
record leaves initialization alone in the original; optional absence reports no
observed transfer rather than manufacturing an engine value. Explicit zero
replaces initialization here. This does not change normal-loader nonzero policy,
founding arithmetic or a market building's KONTID. Encoded width is not an engine
population/counter-allocation capacity. No arithmetic overflow occurs in a copy.
Exact-size refusal is defensive: unchecked original malformed I/O is not emulated.

The requested DISCOVER P3 prose is corrected in contract and current evidence:
0x1008999b zeroes payload scratch; after three pushes [esp+0x20] means original
[esp+0x14], while count pointer is original [esp+0x10]. Historical ddab0993
receipts are retained and explicitly labelled for revision-based verification.
No raw log/disassembly bytes or tested-source pins changed; no gate repeated for
that citation correction.

## Causal checks and original continuation

Two direct authored cases pass 344 assertions. Legal chronology passes 67/67,
then the actual adapter assignment is changed exactly once in a private TU to
union the previous counter. The intended final-counter assertion fails, exit 1,
66/67 assertions still pass. Canonical source never mutated; its rebuilt binary
matches legal-control identity. No new evidence framework or oracle generated
from engine output. Independent Python parse reads the original payload directly.

Same Tutorial/01Discover01 ordinal 0/slot 11, origin (24,24), 128x128;
supplied stamp 500/mode 0/raw draws 7 retain controlled-input provenance, not an
original global cursor. KONTORID 54 imports 1 with zero draws. There are now
55 completed headers and 56 evidence rows including failed boundary 55 PROD2.
No extra header is executed; HIRSCH 71 remains unreached, 0 spawns.
Previous assertions remain active: 4760 terrain accepts, 0 refused, 4589 draws,
16 terrain epilogues, fertility 16384 bytes/8160 effective changes, BERGWERK
9 entries/20 kind-1 nodes, OCCUR 1 ore/1 kind-6 node/Q512000/R1/20 masks17,
21 shared nodes in 8 chunks, ANBAU 135 bytes/13 tokens/61449->65535/ignored 0,
DISCOVER 4 bytes/low 0/upper 0. No global scene initialization claim.

## Gates and limits

Debug 20 main + 3 separately required = 23 executed passes. ASan/UBSan same 23.
Original Debug 1/1 once. Every lane has failed/empty/skipped/incomplete/sanitizer
counts 0. No refused-memory case, timeout or canonical failure, no whole-gate
repeat. Doctest filter exclusions are not prerequisite skips. Raw lines follow.

`cache/wildlife-kontorid-1/debug-final.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=20 executed=20 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

`cache/wildlife-kontorid-1/asan-final.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=20 executed=20 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

`cache/wildlife-kontorid-1/original-final.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

Both affected TUs pass Apple Clang 21 and GCC 15.3 O3 strict Werror. GCC negative
shadow control rejects; format 23.1.3, sim lint, links and diff check pass. The
inherited Apple duplicate-library linker warning remains disclosed; no whole-build
zero-warning claim. All 276 tracked fixtures byte-equal base, including pins;
not a full pins.py run. M5 Max/18 logical CPUs/48 GiB/macOS 27.0.1, 1.5 TiB free,
contention checked; jobs 1, 2 test threads, 2048 MiB/thread, admission enabled.
ASAN detect_leaks=0:halt_on_error=1; UBSAN halt_on_error=1:print_stacktrace=1.
No owner's app, awake assertion, coordinator or other job stopped.

Reuse unchanged resource/stone/cliff/rock/fertility/BERGWERK/OCCUR/ANBAU/DISCOVER
algorithms and long campaigns: their production logic and metadata readers are
unchanged. Affected prefix integration is rerun. No rendering or new public
measurement tool; automatic picture contract omitted for this headless scope.
NOT_RUN: full suite/pins.py, Windows/WSL/original runtime/History Edition,
post-island/global callbacks, World/native/save integration, founding/market id
allocation, UI/visibility, derived-cache consumers or later wildlife. No state,
tag, registry, PRNG identity or default changes. Exact publication CI pending.

## Decisions and reasoning

- Widen the original word into owned optional u32 without new engine capacity.
- Keep initialization/default 1 distinct from an observed import and explicit zero.
- Copy, never allocate/increment/OR; do not treat a market KONTID as KONTORID.
- Refuse malformed lengths defensively without claiming safe original I/O.
- Stop at actual PROD2 55; reuse unaffected campaigns, no silent path to HIRSCH.
- Correct P3 prose while retaining historical frozen source/log identities.

QUESTIONs: none in this finite arm. Independent orchestrator review pending.

## SHA-256 receipts (private raw artifacts stay in cache)

| Artifact | SHA-256 |
|---|---|
| `cache/wildlife-kontorid-1/debug-final.log` | `1cbce6c5487b92fa1171fbca39fd23c848fba8b9b010293e6b968f1b22eaafb1` |
| `cache/wildlife-kontorid-1/asan-final.log` | `91b7ae5231a7028f8f1ebe6f989ba4448136a51848f9e7500ef5a81260e71389` |
| `cache/wildlife-kontorid-1/original-final.log` | `839d329ee9deb585b858c2e33ce6195b1f50d9c00a14842440fd29b828f5d3ac` |
| `cache/wildlife-kontorid-1/debug-final.log.inventory.json` | `1dc25d45dcafb80b603b1328695d5a7d69e744d4b0df6120809a71fddb8657ce` |
| `cache/wildlife-kontorid-1/asan-final.log.inventory.json` | `dce9033f6a046fccbda8cbc067a10ef6b071b1b6bc6c7cacf8117e90c3b66a6a` |
| `cache/wildlife-kontorid-1/original-final.log.inventory.json` | `3111c066b2ed5bc8ba78cec67f0107b8b9c6b87374e3abb38ecf9aa56ce5a46e` |
| `cache/wildlife-kontorid-1/fixture-audit.json` | `20980147d41298db71df46f163df00cd75c39583199ad2cfdf7bfccf084d7994` |
| `cache/wildlife-kontorid-1/independent-header.py` | `9cb508c892832dbaa1dfbdd16b2a77200ccd96801dcf37c5611e55ee9e52725f` |
| `cache/wildlife-kontorid-1/independent-header.json` | `28b0c1de4445c80994a055afbee26d2bbb4d92f79047643c818c44b92383ab66` |
| `cache/wildlife-kontorid-1/independent-header.log` | `d5bfc567e17656b59dd05210d469f970f684c1be6565a8d995f4158c2742b220` |
| `cache/wildlife-kontorid-1/arm.private` | `b796d3a6def2df56ee232edfec56a90b482d472fe4474beb00a763b6a64bf2d9` |
| `cache/wildlife-kontorid-1/save.private` | `5ee802887cea8936e934c786f80d419352b7c97cd450c7b976a6b6ae4db26592` |
| `cache/wildlife-kontorid-1/reset.private` | `da3e02c8b93e010fe2b5e37584bb22f15415043e828100d33a6e28b6cc24ffd7` |
| `cache/wildlife-kontorid-1/constructor-store.private` | `01cf8577f5984907e4638a0c4c0b9fe520939f9a5548cee4662178d52c92e36a` |
| `cache/wildlife-kontorid-1/reset-identities.json` | `98dbe164cec255b775e55c0906fca90468ae7dc5d813f087750fcacb1e2c022e` |
| `cache/wildlife-kontorid-1/reset-legal.log` | `6e02d647d18f5ddce74c8b8401cd22c2a27460b0eb96bbdf1d1d5668a7cb8594` |
| `cache/wildlife-kontorid-1/reset-mutant.log` | `d6d84cd8a402dca6625c06cc445684bad66b1d77f2889d44f2fdd1e44b11633d` |
| `cache/wildlife-kontorid-1/mutation-proof.py` | `48834c0b6292827f4b44896094294abe139f1f2f8b28035a7dda0db92e148b2f` |
| `cache/wildlife-kontorid-1/mutation.log` | `79f14f42f8c20446e5fd5030b51ffecf526ca5ab182b93b47b7099cb879b4657` |
| `cache/wildlife-kontorid-1/build-first.log` | `777a9046029e138cace7600b3b56ac033e68c193fae47d92b5f8b9f5db233294` |
| `cache/wildlife-kontorid-1/build-canonical.log` | `c2ed93ffd93c840755c0e91392ae593b1b52de9b7aaf6c208123feaceb1dde2e` |
| `cache/wildlife-kontorid-1/build-asan.log` | `777a9046029e138cace7600b3b56ac033e68c193fae47d92b5f8b9f5db233294` |
| `cache/wildlife-kontorid-1/apple-warnings.py` | `d9b09dcc199e6bb08385513ac708b36b54988349d477653f6211f845f8dabfca` |
| `cache/wildlife-kontorid-1/clang-warnings.json` | `60580ed8b79094714f004214bde44bf35d63665075484d335278f4bd3b421f29` |
| `cache/wildlife-kontorid-1/apple-warnings.log` | `923b631138bf1be975b9f8d2727e5d12afe236a6a0c6b8bf48a990f4e02382ac` |
| `cache/wildlife-kontorid-1/target-warnings.sh` | `434c63bb5f96f4549b9cc6644522810f8c4dd6e58603179f63b494a25d7ffe0d` |
| `cache/wildlife-kontorid-1/gcc-warnings.log` | `e029815c43244dcd9c829e399c8cfe3b664363751f161c9b3263ec1ee9cb330b` |
| `cache/wildlife-kontorid-1/controls-first.log` | `2ff860d150fd70fd9eb8e270cec62a48f4be87a123ea965f768d2478c685d1c0` |
| `cache/wildlife-kontorid-1/lint.log` | `c90ca6eba541545a82beb573bfcbcc47531df735edd4ff68c814ab50296e6948` |
| `cache/wildlife-kontorid-1/format.log` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `build/dev/src/sim/anno_sim_tests` | `3f9a3ceb9bdd04badffb62fa9bae69b75658cbb192c563bf3755aa931ea8f0dd` |
| `build/asan/src/sim/anno_sim_tests` | `c9c325d25fc0e6c926649c751a2b30cd2a71bc977f833602ea9924986635869b` |
| `docs/orchestration/evidence/wildlife-discover-1.md` | `648a889e576bb2a0fc63dec59ca91a993ad7032cd4504b20ba8c0caca7e78f9f` |
| `docs/sim/wildlife-discover.md` | `122f25f377d605a131da7adcacfa8cc28b547d5e943549e09e9393f82e52b725` |
| `docs/sim/wildlife-kontorid.md` | `f579ca351a589fd86112124b773dd7e9588463de02ea84c9fdf02787c23a3ce6` |
| `src/sim/tests/wildlife_prefix_test.cpp` | `003771c2928126b68278ac939a0e6f9c6e8bb374b81a4cfa0ae23f6ed5debd52` |
| `src/sim_load/include/anno/sim_load/wildlife_prefix.hpp` | `fcd0b649a4c9406898525a4139489204128e6ce70a9c36a6e3c2f877530adc24` |
| `src/sim_load/src/wildlife_prefix.cpp` | `a7bb6b22ebc9b24cd9cca4cff7dc2ed766b9e7a1a2e3829daaf3364b84c9b174` |
| `game/app/AnnoWelt.dll` | `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2` |
| `game/app/AnnoGame.dll` | `ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc` |
| `game/app/Scenes/Tutorial/01Discover01.sze` | `225ed50b005743e00d8d3ffe18667d4565dc781ef45bd6d9b767fd2fe056c0f6` |
