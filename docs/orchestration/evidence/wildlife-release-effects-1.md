# WILDLIFE-RELEASE-EFFECTS-1 evidence

2026-11-19, READY FOR REVIEW; independent review pending orchestrator.
[Contract and addressed closure](../../sim/wildlife-release-effects.md).
Pinned dispatch 979aac55818502741b110673ef4bbe3e4e378eb9, base
25bab774208c9e610b43447e247d99cc1aba0195; no board/main merge.
Tested implementation 33fe219cae79bdde3cf309a71a8385aa01f5159e,
tree e146caae2787840c5557435f932f5b25652df591. Publication adds documentation only.

## Primary evidence and causal boundary

D: GOG New table 0x100c66fc resolves +0x110/+0x168/+0x48/+0x8c to
0x10052b40/0x10051a90/0x10052b00/0x10052570. Query 12's jump table selects
0x100525a2 (configuration dword); island 0x1008bd21 tests AL with mask 2,
so default 4 skips optional follower-manager calls even with an empty list.
New's earlier follower loop uses mask 1; empty list skips all its callbacks.
Current 0 skips action cleanup 0x100330b0; motion count 0 skips manager storage
release through 0x10030030; absent route skips deleting destructor. Nonempty
profiles refuse, not modeled as zero-draw destruction. Direct selected bodies and
the resolved category/next field getters add no world draw in this empty profile.

D: saved home unsigned words via 0x10052b00; island origin unsigned words;
signed /8 of differences, absolute-home %8, category presence bit 0, first
category-0 packed coordinate match. New callback 0x1008bdd9 clears only 0x40,
with no saved-ID ownership guard. Home8/origin9 tests truncation, not floor;
current8/8 versus home3/0 tests moving position separation. Original runtime lists
are not executed: sparse ordered owned snapshots supply the same lookup facts.
No invented token certifies those snapshots. Geometry beyond the original-projection
or valid supplied grid domain refuses, not a native invalid-pointer emulation.

H/C: four new synthetic cases, 80 literal assertions, passed first authored run.
The composed creation/move/end case is projection composition only. Full returned
state comparison checks preserved type/id/time/HP/quantity/cargo/home/source facts;
spawn comparison retains id/time/goods/other bits. Category, ordering, missing/clear
record, mismatching ID, detached repeat/replacement, direct-delete contrast and
unsupported state siblings discriminate meanings. Owned result survives all input
owners; repeated invocation deterministic; no actual free, manager or motion tick.

Actual private production-TU mutants compile/link before running the four cases:
legal 4 passed / 80 assertions; current-position 3 failed / 4 failed assertions;
extra-field clearing 3 failed / 8 assertions; saved-ID guard 3 failed / 4 assertions;
release-again 1 failed / 2 assertions. Source replacements assert exact counts
(two coordinate reads, one each other mutation); intended assertions checked.
Canonical source/binary hashes unchanged after causal run. `causal.py` and
`causal/causal-results.json` retain commands, actual source/binary hashes/results.
No source-mutant failure is reported as a canonical gate failure.

## Canonical gates

Cache root `cache/wildlife-release-effects-1/`. Same frozen source for both profiles.
Commands recorded in logs: canonical run_selected, jobs 1, timeout 180, required
assets, two test-support threads, original descriptor oracle path supplied;
no render auto-contract because no render/evidence-tool change. ASan leak detection
explicitly disabled on Mac; ASan/UBSan remain enabled. Admission stayed enabled
under host permissions; no coordinator/other process changed.

Selection: four new end cases, six existing creation cases, two unchanged prefix
(raw fields / serial interleaving) cases and two unchanged leaf legal/refusal
controls. Required real-descriptor comparison executes separately once, excluded
from main selection: 13 + 1 = 14 unique entries per profile. Real comparison still
checks 29 original descriptor rows against the earlier independent raw oracle;
it is creation regression evidence, never original release/runtime corpus proof.
Existing prefix implementations, original-image readers and fixtures are unchanged;
prior broader format/prefix evidence is reused with that dependency limit.

`debug.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=13 executed=13 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

`asan.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=13 executed=13 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

Both first canonical attempts passed; no memory refusals, timeouts or reruns.
Apple Clang 21 and GCC 15.3 compile both affected TUs at -O3 -Werror with strict
conversion/sign/shadow flags. GCC deliberate shadow control rejected. clang-format
23.1.3 dry-run, lint_sim and diff check pass. Debug/ASan builds have 0 compiler
warnings; inherited duplicate anno_sim linker warning remains, not silently zeroed.
Host: M5 Max / 18 logical / 48 GiB, macOS 27.0.1 (26A434), large free disk;
Spotlight active, jobs 1 chosen conservatively; no timing/performance claim.

## Frozen receipts

83-entry recursive private manifest `closeout-sha256.json`, SHA256
`0c9dc8947fbba8bd215f1af30dc675d0c99ee4692810c36f057e79a56c6e3b8a`.
It pins raw private ranges, receiver table facts, logs/inventories, scripts,
mutant source/executables, strict objects, frozen canonical source/binaries,
module and fixture identities. No originals/listings enter git.

| Receipt | SHA256 |
|---|---|
| Debug gate | 0644b0629995325804cd0fb8b57b606f1417ba4d42197d415df63ab8a3bd88af |
| ASan gate | 6f0329848b67eea8ded468277a10332fa113421c103de2a6cec44541187d9e17 |
| Debug actual retained sim binary | ef941431984d22bb5b4aa43f93cdcb7362f39fe5b8bdedc5b8673df5e3642c67 |
| ASan actual retained sim binary | 54fbf0fe254d92342a949846a1125447e7c008f2f2d4f9c53827a6bc13f5d9b7 |
| 1015-entry frozen source/binary receipt | 7969c0a89f37235e2bb504aa68349d29cc877965eb052ddc6d385474451f6bb8 |
| Actual causal source/results | d6ce640defb93f11b5c319da781ed5190debed03239bf5cc4bb267cf37daba46 |
| Original module identities | 999f3ede2176d0866c85c24655a08b96e8732acecd64ce8dd249b0daa7a117dc |
| Concrete receiver table facts | cc979e5551e9316082585db1dc458ff962ecf163c1a6c45f9bbdbfcaba070711 |
| Apple strict commands/status | da5623f4430ed9c860f3427f5ad1298ccad8bee581b8c94965e055ab34d8fa6a |
| GCC strict results | c0f428bf0c692691cc4b246df64ed2fe39aa77d0cde3042950ab21b8eb6310ab |
| Unchanged 293 physical synthetic fixture files | 869e0b1bcb6e7903ecea062b7d45639cebe286be72a2f14c06b23798f822352b |

All 1015 frozen entries rechecked after gates. Prior creation 209-entry manifest
rechecked intact; all earlier evidence remains on its branch/cache. New binaries
copied, never reconstructed. No hash/pin/fixture bytes/default/tag/state changed.

## Limits and disposition

NOT_RUN: original/HE runtime, live World/manager/spatial/HP/motion/destruction,
nonempty cleanup callbacks, original reachability of unended direct deletion,
full OQ-578/579/584 closure, LeakSanitizer, Windows/WSL local, broad/full/heavy/pins.
No new allocator policy, owner/generation check, FP policy or PRNG input/advance.
Hosted exact-head CI is orchestrator-owned; read after ordinary publication push,
then direct handoff without waiting for CI terminal closeout. Orchestrator review
is pending; local gates are not cross-host acceptance.
