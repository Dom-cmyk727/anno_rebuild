# WILDLIFE-ROCK-FIX-1 — focused writer-callback correction

READY FOR FOCUSED RE-REVIEW, 2026-11-19. Documentation only; no production defect was established by the review.

Branch task/WILDLIFE-ROCK-1 from 81e8f03f3bf882afe00272c27a8f1e0989f56283. Tested runtime remains 26899502717c4d6b2c61ed799bde0db8a7fe4c3e. Dispatch/read-only review read from exact board commit `8df31d68677d8c2e7e4afa1d26e20a3030387822`; no board/main merge or rebase. Exact correction head/tree/new CI supplied in direct handoff.

## Verified mapping and disposition

| Call / slot | Concrete target and finite effect | Disposition |
|---|---|---|
| Writer 0x10082040 | Continues through ret at 0x10082b0a, not 0x10082637 | Current contract extents corrected; dated old-read explanation preserved |
| Call 0x10082ab9 / island +0x19c | Vtable 0x100cac2c, pointer 0x100cadc8 -> 0x10084150–0x10084360; derived chunk-height bounds, island +0x94 bytes 2/3 | Executes inside writer, omitted from transient projection |
| Type +0x58 in bounds refresh | Vtable 0x1002185c, pointer 0x100218b4 -> AnnoGame 0x10007d50–0x10007d53; pure float height getter | Terrain extrema/anchored item-height input, pure addressed conversion at Welt 0x100b5f90 |
| Call 0x10082ad8 / island +0x188 | Pointer 0x100cadb4 -> 0x1008f2d0; event 0xcb minus 0xc9 -> index 2 -> 0x1008f2f0; dirty chunk byte 5 increment at 0x1008f41e | Executes; no represented tile/variation write or RNG draw |
| Event 0xcc / index 3 | Target 0x1008f31c contains World +0xbc branch | Separate event, not this writer invocation |
| Island +0x1a0 | Pointer 0x100cadcc -> 0x100928a0 empty-tile fill | Transport +0x19c attribution corrected; no implementation/caller expansion |

Direct matching PE reads verify targets; finite disassembly verifies reached effects and writer ret. Primary listings remain private. The initial linear sweep passed the ret into following data/code because the disassembler padding heuristic did not stop; writer-full.private is explicitly bounded at 0x10082b0a and no following function is analyzed. Event excerpt likewise bounded at its ret 0x1008f433. No broad survey or new original-runtime experiment.

The wrapper +0xc8-zero / import observer bypass does not suppress internal lower-writer calls. Its separate class-bit-conditional World +0xf8 call at 0x10082afd is not reached with this profile’s caller flags 0x04000000. Reached +0x19c/+0x188 effects update derived caches, neither represented tiles/variation nor RNG. The old projection and numerical results remain valid, but complete original writer state and complete callback bypass are not claimed.

Current prepared output projects tile/variation/draw effects and omits original chunk-height-bound and dirty-byte updates. TileLayer’s own dirty tracking is not asserted equivalent. Before future World/render consumers rely on those caches, correct initialization/rebuild must be established under a separate reviewed integration contract. Empty-tile fill and FRUCHTBAR remain separate/unreleased; neither is assumed no-op/draw-free.

Matching module identities:

- AnnoWelt.dll: SHA256 `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`.
- AnnoGame.dll: SHA256 `ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc`.

## Changed prose and frozen receipts

- Narrow transport slot/call attribution corrected with dated note.
- Stone/cliff/rock contracts identify full writer endpoint, executed callbacks, projected-state limit and future rebuild obligation.
- Three frozen evidence files receive dated banners/addenda; their original bodies and all 40/52/49 SHA rows are preserved. Documentation-source hash rows name old frozen prose, not the later addenda.
- REPORTS gets one focused-fix addendum; original results/failures remain history. No generalized cleanup, FRUCHTBAR work or policy change.

## Checks and explicit reuse

Byte-derived Git blob audit against tested 26899502: 1270 runtime/test/tool/fixture/data/CMake/CI files checked, 276 synthetic fixture files included, 0 differences. Both frozen Debug/ASan executables and 27 raw build/gate/mutation/inventory receipts also match their old SHA256s. Old numerical evidence remains unchanged.

Reuse cache/wildlife-rock-1/debug-final.log 21/21, asan-final.log 21/21, three separately required 1/1 contracts in each, and real-final.log 1/1. Those gates support the tile/variation/draw projection, not original derived caches. No new C++ build/test/sanitizer/canary/mutation/full gate ran, as explicitly required by the fix packet.

Cheap checks: documentation relative paths/fragments resolve, git diff --check clean, docs-only allowlist clean and unchanged-code/receipt/binary audits pass. The link checker validates the added exact heading fragments, not just file existence. Command outputs and hashes retained privately. Hosted CI is read on the exact correction push, not called green before terminal success.

## Private verification identities

| Artifact | SHA256 |
|---|---|
| `cache/wildlife-rock-fix-1/packet.md` | `0e19d3b9732a9abc22836d8078ea941229a2c0865117a089506272d85e7bbb31` |
| `cache/wildlife-rock-fix-1/review.md` | `d2a8f2ba84e7232ab2385e7d7d7123a5920eb781bdee322cca91de87834b3da3` |
| `cache/wildlife-rock-fix-1/dispatch-pin.txt` | `21dac8a3c0e1d7f2e59fd2007e1b17a5ddc26d1432d31504e11f9cbec0a3cbca` |
| `cache/wildlife-rock-fix-1/targets.json` | `432d97076df6a218d2f194a94453ca6893a346fbb2750e69d8821f116672cdc1` |
| `cache/wildlife-rock-fix-1/event-targets.json` | `77c8c326021e07ac61f44255f029c6a1048a4bb74b86f6ed26943e8365bc00f7` |
| `cache/wildlife-rock-fix-1/writer-full.private` | `ad86dcef344972f60385af75dae1dc24ef552bb060635cb181f36d1a4697c601` |
| `cache/wildlife-rock-fix-1/chunk-height.private` | `7856d17ca9f2862376e97799c6765b6bbc511515a0c5044feaed8a63c599947c` |
| `cache/wildlife-rock-fix-1/chunk-event.private` | `74543658231d0c45e061503df3a152f5f70049447f34ad14328c60bbd1b9d56a` |
| `cache/wildlife-rock-fix-1/type-height.private` | `7da8c2b3fc9ca32a7f9ad77ca2e2882711c40d50cf7517fcf9cfaf9d622b8344` |
| `cache/wildlife-rock-fix-1/unchanged-code.json` | `8bc194add2e4a4d714d52984a9320e9eb0990a1411ef2fff55a8811eb3a31146` |
| `cache/wildlife-rock-fix-1/frozen-receipt-audit.json` | `1aa07a61b08e493cfb576b0f1ccc3d2c7213d87569e0f28332b7bfb91f03e6de` |
| `cache/wildlife-rock-fix-1/raw-receipt-audit.json` | `430297692a1bf6603d5dc34eb8e9120b5d8185fd9522f8580c162aabc9571f47` |
| `cache/wildlife-rock-fix-1/preserved-binaries.json` | `0f4dd36ce225e14d3533884995ef86defaf2a8fdd96196bbdf141fd45572995a` |
| `cache/wildlife-rock-fix-1/check-docs.py` | `0e1461b4aa965f7e91fed6464829939345fbe42d4ca518d442657d58e09ab30f` |
| `cache/wildlife-rock-fix-1/doc-checks.json` | `c5bb53c7d43b9e5ca9daf5941bb6f3f28ab9fae3b5ee269a4fe60ddc6b72c82d` |

## Decisions and reasoning

- Correct the primary span/target evidence gap without altering valid projected code or widening the packet.
- Distinguish wrapper bypass from executed writer callbacks; acknowledge omitted derived caches and require their future initialization/rebuild.
- Preserve historical source/hash receipts and all first failures rather than silently replacing them with new prose identities.
- Reuse frozen valid gates and run documentation/source checks only; no owner decision or expensive rerun is needed.

NOT_RUN: any new C++ build/test/sanitizer/canary/mutation/full gate, original/HE runtime, FRUCHTBAR/empty-tile-fill implementation, World/native/cache consumer integration. Independent focused re-review pending; no self-approval or reviewer subagents.

QUESTIONs: none. Mapping matches review. Main/other lanes/coordinators untouched; no new state/default/tag/storage decision.
