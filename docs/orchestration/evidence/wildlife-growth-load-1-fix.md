# WILDLIFE-GROWTH-LOAD-1 — focused P2 correction

2026-11-19, READY FOR FOCUSED RE-REVIEW. Dispatch
`8861a9e245b757700e2be0599e17e0823634cd60`, existing task branch/base
`e89409e17b6173c442fddfa959f70de781689678`. Frozen correction source
`464e49350a8a6208469d34bd03630c515e7e5335`, tree
`0b96d804eb799e4d543b7e00c47a9d71744a3d46`.
Final publication is docs/report only. [Bounded contract](../../sim/wildlife-growth-load.md).

## Finding and retained public-command evidence

The reference's TAIL regex checked syntax only. C++ already rejects out-of-int32
discarded tokens under the documented modern bounded grammar. This correction
aligns the oracle; it is not an original-runtime observation or production change.

Before editing, executed the actual public `--source ... --write-lookup ...`
command on tiny authored sources: Size 1×1, Id 1, Proz prefix 20, final END.
No-tail control has a full zero map except index 64=20. Actual stdout/status
and source/output SHA receipts are preserved, not reconstructed from later code.

| Discarded tail | Before exit | Corrected exit | Complete output effect |
|---|---:|---:|---|
| none | 0 | 0 | Same full map |
| INT32_MAX | 0 | 0 | Same full map, ignored |
| INT32_MIN | 0 | 0 | Same full map, ignored |
| 1,INT32_MIN,INT32_MAX | 0 | 0 | Same full map, ignored |
| 2147483648 | 0 (wrong) | 2 | Output sentinel preserved after fix |
| -2147483649 | 0 (wrong) | 2 | Output sentinel preserved after fix |
| 1,2147483648 | 0 (wrong) | 2 | Later token checked; sentinel preserved |

All seven source byte identities remain unchanged. Legal tails execute real
computation and compare all 16,384 bytes, not just a digest or refusal.

## Minimal correction and decisive controls

Exactly one semantic reference line adds `numbers(text[pos:])` after syntax
validation. It checks every tail token using the same bounded parser, then
throws away the result. It does not append tail values to row/staging.
The captured old reference is byte-identical to e89409e1; removing exactly that
new line from current code reproduces it. Run the NEW public contract against
that old copy: exit 1 at tail_over.dat, after legal max/min/multi-tail controls
execute. This is the requested wrong-check control, not a repeated historical gate.

Public contract grows from 32 to 39 CLI controls. Adds three legal tails and four
bad tails (single over/under and invalid later over/under); every source-mode
case now checks source preservation, and refusals check output sentinel unchanged.
C++ adds one boundary testcase with full legal-map expectations and the same
four refused cases. Existing production diagnostic is unsupported numeric tail;
no error message or parser implementation is changed to satisfy the new test.

All 13 existing authored maps agree. Corrected reference re-reads the unchanged
original file: 146 records, 1 section, 30 Bridge, 92 Klima, 1 short row.
Source SHA256 `0b8c4416dd43fb76ea2dca6e82bfd822b0b861f15440e83479a8aa56b8d92e24`;
lookup SHA256 `760314eb744a0f64eff6bd827be8876153f7cf69e39739e6d4ef8f53642608a7`.
New output also compares byte-for-byte with the prior private reference output.
Expected fixtures/oracle inputs are not regenerated or replaced from C++.

## Focused canonical execution and reuse

Both profiles select only existing refusal case, new discarded-tail case and
13-vector reference agreement. Required changed reference contract runs separately.
Jobs 1, threads 2, timeout 180, admission on; no selected original-asset prerequisite.
ASan/UBSan uses detect_leaks=0 / halt_on_error=1 as the reviewed Mac profile;
LeakSanitizer NOT_RUN. Unchanged renderer contract is outside this numerical scope.

`cache/wildlife-growth-load-1-fix/debug-final.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=3 executed=3 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

`cache/wildlife-growth-load-1-fix/asan-final.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=3 executed=3 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

No failed/empty/incomplete/skipped case, memory refusal, timeout or sanitizer
report; no whole gate repeats. Contract 39 controls is one CTest entry per
profile; main count 3 is two C++ cases plus agreement, not nested assertions.

Reuse: every production sim_load/formats/sim source/header, generic text.py,
all fixtures/expected maps and old C++ test bodies are unchanged from e89409e1.
Prior reviewed original-file byte comparison, ownership/ground/text cases and
five numerical causal variants remain applicable. Only the reference rejected
an extra unsupported-input domain; accepted maps were freshly checked unchanged.
Original C++ case and broad Debug/ASan 13-case selections are NOT rerun.
Earlier logs/CI/refs are preserved. Original CI 44475449514 still had 9 successes,
2 in-progress jobs at this closeout read; no green/acceptance waiver inferred.

Changed C++ test TU strict O3/Werror Apple Clang 21 and GCC 15.3 clean; shadow
negative control refuses, format 23.1.3/lint/diff checks pass. Dev/ASan rebuild
only changed test; inherited duplicate-library linker diagnostic retained.
Compiler warnings 0. No production/data/generic parser/state/default/pin change,
new callback/World/fill/kernel binding or reviewer agent. Owner awake and other
jobs/coordinator untouched. Independent focused review and exact-head CI pending.

NOT_RUN: broad/full suites, old numerical mutants, original C++ corpus rerun,
original executable/HE/Windows runtime, kernel/source composition/World wiring,
full pins/warning sweep, LeakSanitizer. Fresh original DATA read is a Python
producer-result comparison, not original executable execution.

## Frozen receipts

| Local path | SHA256 |
|---|---|
| `cache/wildlife-growth-load-1-fix/reference-before.py` | `72d56a75398990edcd870cdc3d1b0d149f6b1cfba9f229778eb954d52be10bb6` |
| `cache/wildlife-growth-load-1-fix/before.json` | `0425e13e5d0a252ce83f2b750c378faaafd1d9e4a27b699f5647dedab5b78933` |
| `cache/wildlife-growth-load-1-fix/after.json` | `b5e06319e37ab148a5a5b78cbc1885a8bffb3b34dcc727d05c0b0e3923be0364` |
| `cache/wildlife-growth-load-1-fix/over.before.log` | `d15a730a7dd467483ccc2bebdc5f6965bc4f0d1942cb0bb1fdae52199a46c9ee` |
| `cache/wildlife-growth-load-1-fix/under.before.log` | `43307395c45de9d133a4fd8ff0e106ba772470623e52ee581bc821ec4e972e53` |
| `cache/wildlife-growth-load-1-fix/later_bad.before.log` | `ac39c6df1f9755b10b7ea26a0bd10590102800a65d585b7eeb8f124a270c3aba` |
| `cache/wildlife-growth-load-1-fix/max.after.log` | `096b70cb32df601dbeeb66eafa38d7d93da9ee4b54a9e812949ca1429da4e835` |
| `cache/wildlife-growth-load-1-fix/min.after.log` | `5773b154be7ee0242c851be95fad7ce166df146c0a263ff0db07251944ce8869` |
| `cache/wildlife-growth-load-1-fix/over.after.log` | `beae974dd1fe36c15fb2c2bba6b48059c1c700221a205a474e1242039acc8fa9` |
| `cache/wildlife-growth-load-1-fix/under.after.log` | `beae974dd1fe36c15fb2c2bba6b48059c1c700221a205a474e1242039acc8fa9` |
| `cache/wildlife-growth-load-1-fix/later_bad.after.log` | `beae974dd1fe36c15fb2c2bba6b48059c1c700221a205a474e1242039acc8fa9` |
| `cache/wildlife-growth-load-1-fix/old-reference-contract.log` | `b54ba8abdc782ea561614f7deb14a5882eb5ce765f57ebb53ffaac9c983b8d98` |
| `cache/wildlife-growth-load-1-fix/contract-final.log` | `797dd203c3fc7dad8f25d6e6a7f209c8b385a68dcaa44566e7f5f1b7d2bb0260` |
| `cache/wildlife-growth-load-1-fix/agreement.log` | `2990da0f9c3c634b5ba9b2a5025e7de2b205558067f20f39b64d44386f987225` |
| `cache/wildlife-growth-load-1-fix/original-corrected.log` | `03210d5392f56575d4c8a39b2b531b9b955cdcc09b12bbd346bac95fa19ef5f6` |
| `cache/wildlife-growth-load-1-fix/original-corrected.private` | `760314eb744a0f64eff6bd827be8876153f7cf69e39739e6d4ef8f53642608a7` |
| `cache/wildlife-growth-load-1-fix/debug-final.log` | `2045754dda6db716343ceced429b4882478d652faf1d273abf8d8c2a84543621` |
| `cache/wildlife-growth-load-1-fix/debug-final.log.inventory.json` | `24b5e61bd6ef3774ff9ce22434486bd4080c77a4479fe135addb0347986e4722` |
| `cache/wildlife-growth-load-1-fix/asan-final.log` | `4e08a6c760fecc98fe0331e4a6e07a652b5869acbc1aae41ba11a5b8ed256f2d` |
| `cache/wildlife-growth-load-1-fix/asan-final.log.inventory.json` | `f59f4dada8624d413fbbbf04d97b706e7dee6c4dad0b65ae889e9acbd037e8f8` |
| `cache/wildlife-growth-load-1-fix/apple-warnings.log` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `cache/wildlife-growth-load-1-fix/gcc-warnings.log` | `22b31ed1b66fa9c833878a15e2e0e7930d0df1ddeb84f0bae0c9348fa5e5c5d8` |
| `cache/wildlife-growth-load-1-fix/lint.log` | `c90ca6eba541545a82beb573bfcbcc47531df735edd4ff68c814ab50296e6948` |
| `cache/wildlife-growth-load-1-fix/old-ci-status.json` | `77799f299f001459434ef0bd4399cd64c9f665c59327c74f85a820cd2b2c173b` |
| `cache/wildlife-growth-load-1-fix/sha256.json` | `0fc0203d891ebcee9dc1cdfbcf4a6f8d6fd87590ea2d826dead15ffa4395afd9` |
| `build/dev/src/sim/anno_sim_tests` | `b677768f8b918dd612a7fbcf173cd18972b059fe0036f6e9a697666bb8c18a14` |
| `build/asan/src/sim/anno_sim_tests` | `cd2e6ec01c6f9964fb82b74c9330b9ced98fe3567d8fb46dc0efb7722988bd93` |

Closeout `cache/wildlife-growth-load-1-fix/docs-check.log`:

```text
CLOSEOUT files=6 local_links=6 manifest_hashes=62 published_hashes=27 production_parser_data_pins_unchanged=1 status=PASS
```

Log SHA256 `ac7a6d50ae9d9b11051ff90d53f618167d14857052ef1e945a7d5fc31fa68add`.
The log is excluded from the manifest to avoid circular hashing.
