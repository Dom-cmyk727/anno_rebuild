# WILDLIFE-GROWTH-LOAD-1 — frozen source and evidence

2026-11-19, READY FOR REVIEW. Exact dispatch
`746b0e191d26f7e6b7dbce98483b54bb2accd64a`; base
`5bdfd92ac01a70656a718cea4f7c7405445b41d5`. Reference-first commit
`076170fa20b76609ac49823208b44b7d7e538dd9` precedes C++.
First implementation/test source `6e980f7931c40869fb38c3fd35d7c38dd83517db`,
tree `1050a5dfce0ec13717bd0ff9443f25e5f06601a6`. Final tested source
`0dc52331c39dd63be33eee691e4adb00b56acfda`, tree
`458a60f4c2b5c82e7747cf6c572da4b59ed0492b`.
Final publication changes docs/report/evidence only.
[Contract/acceptance](../../sim/wildlife-growth-load.md).

## Primary producer facts and validation boundary

Original reference AnnoGame SHA256
`ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc`.
Full original producer in earlier private growth-loader.private; new finite
integer/token/line-helper spans under cache/wildlife-growth-load-1.
The prefix of 100072b0.private is misaligned exploratory output; only its
aligned 100072c1 onward section-loop span is used. No listing/array is published.

D: 6b4b–6b6b clears 256 DWORDs per array, default 6×6. Id resets counters,
not staging. Completed height-row batches write low bytes into the packed map,
later writes overwrite. Repeated WACHSTUM clears staging, retains already
allocated primary map. Primary/Bridge/Klima map outputs are distinct; only primary
is returned. Signed parser branch and final negation independently read through
175b9; supported canonical decimal int32, not a global change to text::ints.

Explicit subset refusals: malformed/incomplete or extra records/rows, unsafe
staging dimensions, overflow/hex/octal, unknown keys/sections, ambiguous quoted
code/bracket comments/EOF without terminal newline/END. These are documented
modern transactional validation, not emulated original unsafe memory effects.
One fresh owned source result, not reload into an existing catalogue, and not
an original scene/city/kernel/FRUCHTBAR capability. No full postload wiring.

## Independent expectations and original source

13 authored source cases and a sparse-byte manifest construct FULL expected
16 KiB maps including all zero regions. The reference and expectation commit
precedes implementation. Cases cover nonzero retention across Id, comma stop
versus whitespace, initial zero stage, default shape, section reset, duplicates,
signed limits/low-byte storage, packed id/column aliases and independent sidecars.
The public CLI contract passes 32 controls, including zero/malformed input,
changed expected byte, parse-failure output preservation and source/output alias
refusal. An echo-only agreement variant fails the changed-expectation control.

No original byte sequences are used in public fixtures; source snippets are
hand-authored. comments.dat has CRLF locally and Git may normalize text files;
the C++ ownership test also constructs CRLF explicitly on every host.

One unchanged installed Wachstum.dat source, SHA256
`0b8c4416dd43fb76ea2dca6e82bfd822b0b861f15440e83479a8aa56b8d92e24`,
accepted without editing. 1 WACHSTUM plus final END, 146 Id / 876 Proz /
30 Bridge / 92 Klima / 1 Size 6×6; one short row at line 765 parses 3 of 6.
The three retained values in that real row are ZERO; it alone cannot prove
retention rather than zero-fill. Authored prior values 20/30 make that distinction.

The NEW independently written reference uses an actual 256-word stage and
produces full-map SHA256
`760314eb744a0f64eff6bd827be8876153f7cf69e39739e6d4ef8f53642608a7`,
matching the earlier finite probe without importing its oversized convenience
array. It was run before C++ implementation and its full raw output stays private.
The C++ asset case compares all 16,384 bytes with that private output when supplied
via ANNO_WILDLIFE_GROWTH_ORACLE. Both canonical profiles and the final targeted
rerun did so. Ordinary asset runs without that optional artifact check the known
whole-map SHA and report additional byte comparison unavailable; no hidden
new prerequisite. CI without game files skips this asset case explicitly.

Authored leaf integration: Id 1, rotation 3, p 20 -> b 2 -> low 2. Real-source
integration: Id 11, rotation 3, p 100 -> b 10 -> low 10. Other object/null fact,
position, kernel and ground are AUTHOR-DEFINED, not original map/city/climate.
No source-binding claim for these other inputs or any fill call follows.

## Causal and reliability controls

Four initial direct cases pass 1,206 assertions with the original assets/oracle.
Five private ACTUAL production-TU variants are compiled/linked into owned scratch
binaries. The public vector testcase parses all 13 legal sources; numerical
mismatch, not parser refusal, is required for each wrong implementation.

| Variant | Passed assertions | Intended full-map failures |
|---|---:|---:|
| Legal | 972 | 0 |
| Whitespace continuation | 971 | 1 |
| Clear stage per Id | 970 | 2 |
| Zero-fill short rows | 970 | 2 |
| Wrong packing | 965 | 7 |
| Clamp instead of low byte | 971 | 1 |

Canonical producer source and Debug binary are checked unchanged immediately
before/after these probes. Later binary rebuilds for the separate test-report
correction have their own gate hashes; no claim the build path stays immutable.
No historical failed experiment was repeated or reconstructed.

After the first valid Debug pass, inspection found that nonfatal CHECK could
print an agreement line after a mismatch. One-line correction at 0dc52331 makes
it REQUIRE. New corrupted-private-oracle control fails the intended full-byte
comparison (exit 1), prints NO agreement line; legal oracle exits 0 and prints
agreement. The old success marker was never observed on a failed old run; this
was a static finding, not a fabricated historical failure. Production loader
and all other tests/fixtures/tools are byte-identical between these two pins.
Only the affected Debug asset case reruns; 12 unaffected first outcomes reused.
ASan runs the whole focused selection once on the corrected head.

## Canonical gate receipts

Jobs 1, threads 2, timeout 180, admission enabled, asset-policy required, original
assets set and private oracle supplied. Required growth/ground reference
contracts are separate from main count. Picture contract is outside this numerical
non-rendering scope (`--skip-auto-contract`); no renderer evidence claimed.
Selection is four loader cases, four ground cases, three existing text-format
controls (including their small text-file corpus), and two reference agreements.
No sailing/opening/renderer/full-map corpus or full-suite campaign.

`cache/wildlife-growth-load-1/debug-final.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=13 executed=13 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

`cache/wildlife-growth-load-1/debug-oracle-final.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

`cache/wildlife-growth-load-1/asan-final.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=13 executed=13 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

Debug initial source 6e980f79: 13/13 plus two required 1/1. Targeted Debug source
0dc52331: 1/1 supersedes only original asset case; unique covered main tests
remain 13, not 14. ASan/UBSan source 0dc52331: 13/13 plus two required 1/1.
No canonical failed/empty/incomplete/skip, memory refusal, timeout or sanitizer
report. No whole gate repeated. ASAN_OPTIONS=detect_leaks=0 on this unsupported
Mac profile, UBSAN_OPTIONS=halt_on_error=1; LeakSanitizer explicitly NOT_RUN.

## Final scope and warning audit

Both dev/ASan targets build. Compiler warnings 0; inherited duplicate-library
Apple linker diagnostic retained. Changed TUs strict O3/Werror Apple Clang 21
and GCC 15.3 clean; asset test TU rechecked after its reporting correction.
GCC shadow negative control rejected; format 23.1.3, lint_sim and diff checks pass.
Generic text header/source/tests/tools/text.py, numerical leaf and old fill
implementation/validator unchanged. Only new loader/tests and nearby registrations,
new authored fixture files/tools and documentation. No old fixture/pin bytes moved,
World/registry/PRNG/command/native/Rules/default change or production caller.

Host Apple M5 Max, 18 logical CPUs, 48 GiB, macOS 27.0.1, 1.5 TiB free. Intake
contention desktop/browser/virtualization; no other job/coordinator changes.
Owner caffeinate PID 27411 left running. No reviewer agents.

NOT_RUN: original GOG executable/HE/Windows runtime, original city/kernel/header
or scene binding, existing-buffer reload/unsupported grammar profiles, sidecar
outputs, prep-to-fill/World/live execution, LeakSanitizer, broad/full gates or
full pins/warning sweep. Independent orchestrator review pending. Exact pushed
head/tree/CI sent directly before CI completion; terminal receipt separately.

## Frozen local receipts

| Local path | SHA256 |
|---|---|
| `cache/wildlife-growth-load-1/original-reference.log` | `03210d5392f56575d4c8a39b2b531b9b955cdcc09b12bbd346bac95fa19ef5f6` |
| `cache/wildlife-growth-load-1/original-reference.private` | `760314eb744a0f64eff6bd827be8876153f7cf69e39739e6d4ef8f53642608a7` |
| `cache/wildlife-growth-load-1/reference-contract-01.log` | `5bc946f83ce6b7148b18ec489f3ba8bb98d3718bef5644008957c473eafa0089` |
| `cache/wildlife-growth-load-1/cheap-cpp-01.log` | `ba0f75b984a57bc33423cbcb22319085b952de5bc51f108ae55b533e455ed9aa` |
| `cache/wildlife-growth-load-1/debug-final.log` | `382705c367fbf7538d717e170dab4408a151f3d7e536191cbd137ff9d94d3c4f` |
| `cache/wildlife-growth-load-1/debug-final.log.inventory.json` | `eb3bfb56404ae60144b0661b50ca7dc99488fbc840b50cf3cedc9ce527bd0138` |
| `cache/wildlife-growth-load-1/debug-oracle-final.log` | `143c6110870ac7ceb81fa4c534fa1098690ef5c8a70bad30660f5122c00b116d` |
| `cache/wildlife-growth-load-1/debug-oracle-final.log.inventory.json` | `01413761b128c3533aa85ea6fdef348cd8af6f2f5fa95bca5a5540cf3f95c4bd` |
| `cache/wildlife-growth-load-1/asan-final.log` | `d6ad71a8372cff83c5876b035005ba8890d5733a6501f70ea10c943d530fa55a` |
| `cache/wildlife-growth-load-1/asan-final.log.inventory.json` | `c1519cc3a8102b3f8bc69243cdf36e76da1fe2934ea3a6aa137c9f89bf413979` |
| `cache/wildlife-growth-load-1/causal.py` | `8722fe034e07edd1aa0b589e3a9d20109c271da4beda3206ad3876e78c91544e` |
| `cache/wildlife-growth-load-1/causal-01.log` | `0c4870494b1154ecf9b3b91bb71c454ab7e9ffc9be94dc43fb1a760cefc7ef3d` |
| `cache/wildlife-growth-load-1/causal-results.json` | `6815e42509a69c2085b82e24768499999aad3e49575c595c4b9103234841b520` |
| `cache/wildlife-growth-load-1/causal-counts.json` | `8d0b94aa9287e5ecb5475b5ab4ac9d06a4e0b54f57af16b8f3a4ea16769c2488` |
| `cache/wildlife-growth-load-1/canonical-before.json` | `bc0bc4d98e448dc920405ee0e75524224b0eb8a522cf612522d7c8abf2ac2667` |
| `cache/wildlife-growth-load-1/echo-control.log` | `c780e6d20aeaa1a45f91b2ea7ded3312cb3ac01d6d7e47e2a9c0047012571a10` |
| `cache/wildlife-growth-load-1/oracle-controls.json` | `6f51dcdba3bc8aa8c573be69a72cc8a2fe68adf937afc5dadb1fc36c3e69d946` |
| `cache/wildlife-growth-load-1/oracle-wrong-control.log` | `f123be3441cdc3ac4ad6783f1c64d0bee7a3056c385962364dfe4fc5bd461c3d` |
| `cache/wildlife-growth-load-1/oracle-legal-control.log` | `7f3797e3789c2a033b400f2b9ac1bc95131d95095644818ea8c2fbcbbc088cda` |
| `cache/wildlife-growth-load-1/warning-results.json` | `8da5c2f8d09624c38e13afabce05d5cf3f9e5babbe84367dd19569144fa981ad` |
| `cache/wildlife-growth-load-1/gcc-warnings.log` | `8885cbf5aff167be98301613ad986daa8adeebdae39a900079666c40b88bbf9b` |
| `cache/wildlife-growth-load-1/final-test.gcc-warnings.log` | `22b31ed1b66fa9c833878a15e2e0e7930d0df1ddeb84f0bae0c9348fa5e5c5d8` |
| `cache/wildlife-growth-load-1/host.log` | `5c84f09322f0aea3cc466d8423bedb0715b0eaf99f81ce9ed30f276ec131c6b3` |
| `cache/wildlife-growth-load-1/callers.log` | `8e5a62080f9744e38575bf3a9cab61e11c7874da9c8f78b4d4f55dee2b08b04c` |
| `cache/wildlife-growth-load-1/integer-core.private` | `7c8dd13309197ba29e9d19df319195abcbcd9cf3a6fc6a4ecad3565bf13ca4a9` |
| `cache/wildlife-growth-load-1/sha256.json` | `c1e8ed43d7e08fc104296a355c376d3a288fe05ef2a37cef9516a9cc40265d28` |
| `build/dev/src/sim/anno_sim_tests` | `054b8fd7869acfe6c63915b362d9a3c575053af5e476b43777ce59413870f8a3` |
| `build/asan/src/sim/anno_sim_tests` | `63615dbb817584902977cf03f3ed0cbf9f034f2c81e7f3891e222806207416dc` |
| `build/dev/src/formats/anno_formats_tests` | `77e3ff78f18d28343517fee83846a40a3c91eb01bcedfde976d99fdede86ca98` |
| `build/asan/src/formats/anno_formats_tests` | `f9e3cd7a8b8b0f0e3cde904c8b0668b0c1adc742f37e9d23648cf3daaaf3bab1` |

Docs/scope closeout `cache/wildlife-growth-load-1/docs-check.log`:

```text
CLOSEOUT files=26 local_links=8 manifest_hashes=105 published_hashes=30 old_parser_leaf_fill_fixtures_pins_unchanged=1 scope=UNWIRED status=PASS
```

Log SHA256 `f5d0622ce1af8fe10fafe78213ef8a39c407c7531f6d4a4ee29383f94055740c`.
The closeout log is excluded from the manifest to avoid circular hashing.
