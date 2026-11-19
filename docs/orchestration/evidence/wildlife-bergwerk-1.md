# WILDLIFE-BERGWERK-1 — frozen review evidence

READY FOR REVIEW; independent orchestrator review pending.
Tested source `7e4ac88f68a9cec14100a41e6c82b6bf9ed338a3`, tree `3e408d06e51102169cee6d1de4e19071e31a3da4`.
Base corrected FERTILITY `81026d1b451955d9167649cb3b8b2e3f5648d120`;
dispatch `f30239dcaf7374b709e8e829243922f5ce834aa3` read, never merged.
Final publication adds only report/evidence. Pushed head/tree/CI accompany the
direct handoff. No extra intake, main merge/rebase or other-lane changes.
Original files/listings/records and absolute host paths remain private.

## Primary contract and acceptance

[Contract written before code](../../sim/wildlife-bergwerk.md),
[ordered prefix](../../sim/wildlife-prefix.md),
[preceding fertility](../../sim/wildlife-fertility.md).
D = matching GOG AnnoWelt instructions; P = original parsed input; C = executed
public adapter controls. Research ANNOSCENE-ORE-2 was read at accepted pin
`b12d22b39caee90429aef77b52659aa2bfedb38e` without importing its branch.

| Requirement | Primary / independently authored result | Independent disposition |
|---|---|---|
| Correct container | Dispatcher 0x10089d37–0x10089d7b -> island +0x24 / table 0x100c9b10 +0x18 -> 0x10078960 | pending |
| Packed fields, not ore | 0x100789ef–0x10078a2e: low 12-bit x, next 12-bit y, next 2-bit rotation, upper 6 ignored; u16-origin addition wraps then widens | pending |
| Tagged records | Existing 6-name/u16/24-bit escape parser reused; short/long ENTRY and unknown tags; authored parse/write byte round-trip passes | pending |
| Footprint / chunks | 0x10077e63–0x10077e93: 3 × 4 or 4 × 3. Authored (23,30), rotation 1 -> chunks 0,1,2,3 and offsets 0x67,0x6f,0xe7,0xef | pending |
| Signed matrix indexing | 0x10077ede–0x10077f21: truncating division by 8. Origin x 7 puts two world chunks into matrix slot 0; test preserves it | pending |
| Typed node and prepend | 0x1007c290–0x1007c2cc / 0x10077f74–0x10077f87: kind 1, rotation, mask 0x10, packed signed offsets, next/head and presence bit 2; repeated header appends, not deduplicates | pending |
| Defensive malformed input | Wrong ENTRY lengths, impossible extended lengths, trailing fragment and out-of-matrix cells refuse without completion; prior accepted entries remain diagnostic | pending |
| Ownership / surrounding effects | Resource–empty header–ENTRY–fertility–duplicate ENTRY–HIRSCH, 2 draws, raw output owns all fields after source destruction | pending |
| Actual-code negative | Legal 128 assertions; wrong x-from-y adapter TU mutant fails 12 intended checks / 116 pass, canonical source unchanged | pending |
| Original continuation | Independently parsed 9 entries -> 20 nodes / 8 chunks; all typed fields/offsets/links/flags checked, no draw or tile/variation change; next OCCUR 51 | pending |

The reader spans 0x10078960–0x10078aa3. It uses the existing small tagged header
helper 0x100b4f50 through ret 0x100b4fc3 (the contract's earlier endpoint identifies
the read-body span). Unknown tagged names skip through 0x10078a36; ENTRY payload
is one u32. No header count or fixed array capacity is inferred from 108 bytes.
Fresh +0x24 factory 0x1008be60 / constructor 0x10078c80–0x10078cf1 copies origin
and dimensions. Matrix allocation 0x1008c2ed–0x1008c39e proves ceil dimensions/8;
chunk initialization 0x1007c730 / 0x1007c710 clears list head and flag word.

Per-entry insertion is 0x10077e30–0x10077fc9. Actual resource node allocation
0x1007c290 only packs offsets, clears next/mask and writes kind/rotation; the
caller overwrites mask to 0x10. List prepend 0x1007bd50 -> concrete +8 setter
0x10040230 writes only next +4. Presence bit 2 is ORed into chunk byte +8.
No resource selector, world draw, tile writer, global factory or further observer
is reached. Caller bookkeeping returns without another callback. Post-island
callbacks remain outside this local adapter, not claimed draw-free globally.

This is separate from OCCUR table 0x100caf34 and its ore occurrences/quantities,
reserve/discovery, marker types and later mine binding. No tonnage or goods is
manufactured from the name, INFO or packed word. Nodes are typed owned transient
state, not raw bytes labelled semantic completion. Matrix geometry uses the
original 8-cell rule independently of the spawn-head chunk setting. Full original
chunk caches/other flag categories remain outside this model under the existing
corrected ROCK consumer initialization/rebuild boundary; no new cache omission
is hidden. Normal scenario/deposit imports, mining and World remain untouched.

Defensive exact ENTRY length, full tagged framing and matrix-index checks are
explicit adaptations to malformed input; original unchecked I/O is not claimed
transactional or safe. Later invalid ENTRY leaves earlier accepted entries as
partial diagnosis. Whole malformed framing refuses before that header effects.
Allocation-success domain is retained, no original allocator-failure emulation.

## Same original input, one continuation

Private independent packed-u32 parse of Tutorial/01Discover01 header 50 finds
9 entries, 20 physical nodes across 8 chunks. Original-derived lists stay private;
only aggregates are published. The test independently reads installed words and
checks every local/world coordinate, rotation, source identity, node/offset,
kind/mask, prepend link/head and presence flag. Authored asymmetric cases are the
causal numeric oracle; the original comparison alone is not independent proof
of the decoded formula.

All earlier tile/variation/fertility checks remain: 4,760 terrain records completed
and accepted / 0 refused; 4,589 resource-class records/draws; 84 stones / 63 cliffs /
24 rocks; 16 terrain epilogues, 1 fertility completion (8,160 low-nibble changes),
1 BERGWERK2 completion / 9 entries / 20 nodes / 8 chunks; other draws 0, spawns 0.
Supplied stamp 500 / mode 0 / draw 7 unchanged, total cursor 4,589. It stops before
OCCUR header 51, island ordinal 0 / slot 11 / record 0 (header convention).
HIRSCH 71 remains unreached; complete world/scene cursor alignment is not claimed.

## Canonical gates

Each synthetic profile runs three separately required controls: typed-node causal
chronology, BERGWERK malformed-entry adapter contract, existing malformed-prefix
contract. They are excluded from main selection and not double-counted. No new
reference CLI or format mechanism is introduced. Picture contract is explicitly
skipped: no rendering/picture input changes, only a headless transient adapter.

### `cache/wildlife-bergwerk-1/debug-final.log`

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=8 executed=8 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

### `cache/wildlife-bergwerk-1/asan-final.log`

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=8 executed=8 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

### `cache/wildlife-bergwerk-1/original-final.log`

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

Totals: Debug 11 executed passes including 3 required controls; ASan/UBSan 11
including the same controls; separate original-file Debug 1. No failures, empty,
skips by any class, incomplete, sanitizer line, timeout or memory refusal. No
whole gate repeated. Commands, inventories, source/build/binary identities and
terminal outcomes retained in logs; test-name/verdict preflights run per gate.
Doctest filter exclusions are not prerequisite skips. Direct authored controls
are 3 cases / 400 assertions and causal legal 128 assertions, not extra gate passes.

Two affected TUs pass O3/Werror Apple Clang 21 and cached GCC 15.3; shadow negative
control is rejected. Compiler warnings 0; inherited Apple duplicate-library linker
warning retained. Format 23.1.3/lint_sim/diff/links pass; targeted warnings are not
a full sweep. All 276 tracked fixtures, pins and recordings byte-identical to
81026d1b; no full pins.py run claimed. Unchanged tagged helper, selectors/PE/HHbb,
HIRSCH numeric/reference CLI and resource modes reuse reviewed dependency evidence;
long render/sailing/corpus suites unaffected. No canonical/build failure occurred.

The private cloned mutation harness footer says whole-byte-copy: that inherited
label is stale. The first line, actual one-expression production-TU replacement,
named world-x/offset failures and hashes establish the sole wrong-axis mutation.
Canonical source and legal binary remain unchanged; post-mutation build checked.

Host Apple M5 Max / 18 logical / 48 GiB / macOS 27.0.1, Apple Clang 21,
CMake 3.31.10/Ninja 1.13.2; jobs 1, threads 2, 2,048 MiB/thread with normal
admission. Original game files read-only at `game/app`; existing awake assertion
and coordinator untouched, no competing gate at preflight. ASAN
`detect_leaks=0:halt_on_error=1`, UBSAN `halt_on_error=1:print_stacktrace=1`.
GPU unused. Public paths are repository-relative; raw absolute paths stay in cache.

## SHA256 receipts

Changed-source rows identify frozen source; raw private receipt files are not
published. A later report-only commit does not modify their source identity.

| Path | SHA256 |
|---|---|
| `cache/wildlife-bergwerk-1/debug-final.log` | `25af4db0a0462481c2c42d476c3f56c82837a5e64aac8decec6903b40c42ad6b` |
| `cache/wildlife-bergwerk-1/asan-final.log` | `5bc87ff6376247724838b31ab099ffe30e7be02f98164444de85029b84433088` |
| `cache/wildlife-bergwerk-1/original-final.log` | `5395e73bca8e322fbf9ced63bb950fcdca06b890e05cbfd99c4c2db2b0b03d5c` |
| `cache/wildlife-bergwerk-1/debug-final.log.inventory.json` | `f8da047401b8a364e8383ece5442376cbbbf75a91778ebf8e2a25647c579ced0` |
| `cache/wildlife-bergwerk-1/asan-final.log.inventory.json` | `8aad2954796f47b27618e9aaf0d39ee8350e7e7b00564d5902f6139f8d86b8d7` |
| `cache/wildlife-bergwerk-1/original-final.log.inventory.json` | `3111c066b2ed5bc8ba78cec67f0107b8b9c6b87374e3abb38ecf9aa56ce5a46e` |
| `cache/wildlife-bergwerk-1/fixture-audit.json` | `d63e9def7513e26de0590df1301443917f2036f467dcadffaf69d8427c693280` |
| `cache/wildlife-bergwerk-1/independent-header.private.json` | `a9c77b3649a14cd83a483678a85dfa11323b202b774d192f2acee0596472069a` |
| `cache/wildlife-bergwerk-1/arm.private` | `9198b3c4a04f92095880792416c6d0bc28717833d9da7f11bf23e3fb418e542f` |
| `cache/wildlife-bergwerk-1/container-reader.private` | `e0dcd91a98578ec0afb5c3e2c384dee9fa6f6f189f9684af50cb06d15acec5b6` |
| `cache/wildlife-bergwerk-1/container-constructor.private` | `a88a92d637fc3631c446e8b2c585ef3e48b2423a1011aaa71aaa4e7954d84118` |
| `cache/wildlife-bergwerk-1/insert.private` | `46213eb6ec0d8478aa88deee83c007a3e888c1617a0cd749ff2cd2eae1339250` |
| `cache/wildlife-bergwerk-1/record-constructor.private` | `2a7b17c84c9b5c9c27246c51e6544c930315919c897799dc7a6a4abdd5d21501` |
| `cache/wildlife-bergwerk-1/list-insert.private` | `0f6273599fc69f08b7f9b14551d3bb6d91c6d9651ec279eae0629c3d9e5e6675` |
| `cache/wildlife-bergwerk-1/next-setter.private` | `3bcaf01b17e798284aa1823b373fb97e407d85e9603b95b0b45285c406ca5a65` |
| `cache/wildlife-bergwerk-1/island-setup-full.private` | `efaad5a304e33e793b10f23753ca731f882d5607bb1971ce7950f0275d906709` |
| `cache/wildlife-bergwerk-1/chunk-constructor.private` | `88ee4f91c910bd931f52c1e41607bfc5ecd682a9d3de614943bb0d57e3e4bacc` |
| `cache/wildlife-bergwerk-1/chunk-base.private` | `329256cde492df0ad9f8a1ed6cc4d04a9d7872858d65173bdffed8679e32688b` |
| `cache/wildlife-bergwerk-1/mini-reader.private` | `94f80d342e9fc42a2a2747c61eff8164188bf361ba1413b52047da5537067fd1` |
| `cache/wildlife-bergwerk-1/mini-reader-tail-complete.private` | `fd29b5d1100712d611f0df9db4b1e05ab93c8360a464bfb313b198ed6bc59e6e` |
| `cache/wildlife-bergwerk-1/mini-payload.private` | `15a532c9f20be89fa516b51426986f760787a52347a76d756a2bc47009f433d9` |
| `cache/wildlife-bergwerk-1/bergwerk-identities.json` | `4349503734540b15adeb05cad03a3c8e6bb824ef86235003e67b96c2ca71785b` |
| `cache/wildlife-bergwerk-1/bergwerk-legal.log` | `03767bb3720bf9b51d553c0b08ab03e16947395c4a4cb11b0e95f36bb6e1c051` |
| `cache/wildlife-bergwerk-1/bergwerk-mutant.log` | `c862058e47501c016831d56d0cb4c83db13bb250329abc41ae9fd6d1e4219d9c` |
| `cache/wildlife-bergwerk-1/mutation-proof.log` | `bb912fa45003ed290a4c6a212c3b95452ba345ba2b5eb5672974041519db516e` |
| `cache/wildlife-bergwerk-1/build-first.log` | `777a9046029e138cace7600b3b56ac033e68c193fae47d92b5f8b9f5db233294` |
| `cache/wildlife-bergwerk-1/build-asan.log` | `777a9046029e138cace7600b3b56ac033e68c193fae47d92b5f8b9f5db233294` |
| `cache/wildlife-bergwerk-1/post-mutation-canonical-build.log` | `c2ed93ffd93c840755c0e91392ae593b1b52de9b7aaf6c208123feaceb1dde2e` |
| `cache/wildlife-bergwerk-1/apple-warnings.console` | `923b631138bf1be975b9f8d2727e5d12afe236a6a0c6b8bf48a990f4e02382ac` |
| `cache/wildlife-bergwerk-1/clang-warnings.json` | `54dfc525ef2cda6a77394f8287d1a77b35bfa54d8060f3577909e8adc70d4663` |
| `cache/wildlife-bergwerk-1/gcc-warnings.console` | `e029815c43244dcd9c829e399c8cfe3b664363751f161c9b3263ec1ee9cb330b` |
| `cache/wildlife-bergwerk-1/synthetic-first.log` | `33839c23acfe3a173900af50fc0cc05718f6adc52e618ff5e70a4b2036683664` |
| `cache/wildlife-bergwerk-1/source-identities.json` | `2bf98ac0eff7f8e89322c5fa2cea02e9eb49ee921cc1644e79d864ccd5342e6f` |
| `build/dev/src/sim/anno_sim_tests` | `0c085b5f881b7b86f5f9c1cda1292252e86a06fef9d1ac7f831b2f226b8477d1` |
| `build/asan/src/sim/anno_sim_tests` | `498c2b48ae378bbb6f4b80cb5654d5373d2c27a84126d8aaf55d324b18b34b74` |
| `docs/sim/wildlife-bergwerk.md` | `ea0ffe58363b5a20e6e33db8caace695391f9052de154af1c4f6939ba7e5f50d` |
| `docs/sim/wildlife-prefix.md` | `352540e2df6af37dacc88906f115d93bec57fdda6b54dc9b82fa5065acafb786` |
| `src/sim/tests/wildlife_prefix_test.cpp` | `61c9cabddc46c88cdc11197929d571eb1c1fff39b90e4d2d0f7c91b5cb15fdb1` |
| `src/sim_load/include/anno/sim_load/wildlife_prefix.hpp` | `af16b6f47e0ba12512de6004ec9709bc08ba5cd77a53be781142796c0e5eed42` |
| `src/sim_load/src/wildlife_prefix.cpp` | `73e69981850912e9f433b012816c68bec3a9a0c945959e7f76983fd29fa33ff6` |
| `AnnoWelt.dll` (read-only original input) | `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2` |
| `AnnoGame.dll` (read-only original input) | `ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc` |
| `Anlagen.dat` (read-only original input) | `b2317a5c24b8c9fe796cf40f3fedaefcfa30ad3f185df9e69b2dca6a03d8dd75` |
| `Scenes/Tutorial/01Discover01.sze` (read-only original input) | `225ed50b005743e00d8d3ffe18667d4565dc781ef45bd6d9b767fd2fe056c0f6` |

| `cache/wildlife-bergwerk-1/independent-header.py` | `93a5de89d6829ec6a42a63301f74ba9af250d3dd8fbc1d608862dfd32b2a5e35` |
| `cache/wildlife-bergwerk-1/independent-header.log` | `a3cd23dde9cdf12dd18d849bbeb48c8b3041567a6f44d5acfdcbf92dc6e2ca6b` |

## Decisions and reasoning

- Trace the actual typed container and insert/list setter through return; never
  treat BERGWERK2 as ore tonnage, a raw no-op or a fixed capacity from one map.
- Reuse existing tagged framing and own typed entries/nodes/order/presence bits;
  retain signed index and nibble behavior with defensive checked geometry.
- Prove a wrong axis on the legal public adapter, then focused lifetime/error
  gates and one original continuation; reuse unchanged numeric/long campaigns.
- Stop before OCCUR; no unsupported factory, ore/extraction or global callback
  is skipped or guessed to manufacture completed-world success.

NOT_RUN: original/HE runtime, Windows/WSL game-file runs, whole suites/full pins/
full warning sweep, LeakSanitizer, allocator-failure emulation, World/native/cache
consumer integration, OCCUR/ore binding/production, post-island initialization,
wildlife lifecycle. No reviewer agents; orchestrator review is independent.
QUESTIONs: none blocking bounded review. Next finite task is the OCCUR header 51
handler and directly reached occurrence factory/state/draw contract, not a whole
ore/extraction or wildlife port. Exact-head CI is read after push and submitted
separately without waiting; pending CI is never described as acceptance.
