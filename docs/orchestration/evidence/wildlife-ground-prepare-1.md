# WILDLIFE-GROUND-PREPARE-1 — frozen leaf evidence

2026-11-19, READY FOR REVIEW. Exact dispatch
`97f65165178d6f8ca26105b4fc9dd9d38edf4c10`; base
`54798a4b5f5bc8a2c65874ea375441b7cca9e21f`.
Reference-first commit `559f22ef`; frozen implementation/test source
`92c17db16f235e5d0db213093de9a25e3c61a2d3`, tree
`c20b4b5819c3e6baa46b855dfd074ab99a6597e8`.
Final publication adds this evidence and REPORTS only; no implementation changes.
[Contract/acceptance table](../../sim/wildlife-ground-prepare.md),
[addressed original rules](../../sim/wildlife-preprocess.md).

## Scope and representations

- Pure synchronous UNWIRED supplied-fact computation, full island only; no partial
  update parameter, scene load, callbacks, city lookup or live fill call.
- This mathematical no-city-prewalk contract does not certify city absence or
  original provenance. No misleading Boolean, seal, completed header or c2 field.
- u16 GFX comes from Terrain's column type; whole u32 object words match TileLayer,
  including 0 and high ids. Missing facts refuse; N/A/F/V-zero retain lookup p.
  24 authored rows cover all states, successful object-zero descriptor, distinct
  word 16384 versus 0, modern ids 70000/90001, unsigned bytes and both kind bounds.
- Facts sorted once, deterministic lower_bound binding; no full registry scan per
  tile, global state, content cap, new registry field or pointer-order behavior.
- Positive geometry and last TileXY checked with i64; u32*u32 in u64; exact lengths
  and vector representability before scratch allocation. All fact states/coverage
  validated before numerical work. Result owns storage, borrowed views do not escape.
- Scratch and output distinct; two complete passes; products/sums u32; no FP/PRNG.
- Refusals are explicit defensive API behavior, not original malloc-failure parity.

## Reference and numerical controls

Hand derivations and reference were committed before C++. Public command:
`python3 -I tools/wildlife_ground_ref.py fixtures/synthetic/sim/wildlife_ground_vectors.txt`.
Exit 0 with `24 vectors, 0 disagreements`; contract reports
`25 CLI controls passed; 24 hand-derived vectors`.
Wrong expectation exits 1; empty/malformed/missing input exits 2. A private
source variant replaced `got = compute(p)` once with `got = expected`;
the unchanged contract fails at wrong.txt (exit 1), proving it rejects echo-only
behavior. It is not a substitute for the independent hand expectations.

The new C++ test runs every row through the public helper and checks translated
origin/reversed fact order. The asymmetric 3×3 profile at (17,-11) has b rows
1,2,3 / 4,1,2 / 2,3,1; centre+2east+south gives 9,9,5 / 8,8,3 / 8,5,1.
All corner/edge/interior values avoid saturation. Three further tests cover
one-precondition refusals/unchanged inputs, cheap enormous-geometry rejection and
returned ownership after source destruction. Direct cheap run: 4/4 cases,
4,706 assertions pass; filtered-out doctest cases are not prerequisite skips.

Private actual production-TU variants are compiled/linked into owned binaries,
using existing clean objects/libraries. Canonical source and binary hashes are
checked before/after. Only the vector testcase runs, so every numerical case has
valid inputs and succeeds past validation. Each variant fails the intended
`result.value() == column(...)` assertion; no validation failure counts as proof.

| Implementation | Executed assertions | Passed | Intended failures |
|---|---:|---:|---:|
| Legal | 4596 | 4596 | 0 |
| Aggregate division | 4596 | 4595 | 1 |
| Missing zero-centre guard | 4596 | 4593 | 3 |
| Byte-width product | 4596 | 4594 | 2 |
| Whole-byte write | 4596 | 4574 | 22 |
| Output-as-scratch | 4596 | 4595 | 1 |
| Signed lookup byte | 4596 | 4593 | 3 |
| Transposed indexing | 4596 | 4595 | 1 |

S5 remains the nondistinguishing saturated width example; the historical failed
probe is only linked through the prior research, not rerun. S10 distinguishes
260 from wrapped 4. For a future growth parser, real short-row retained values
are all zero; use authored NONZERO staging to discriminate against zero-fill.
No parser is implemented here.

Initial new causal harness stopped at a link-command shape assertion before any
compile or testcase. Ninja appends test-discovery commands, not a final `&& :`.
Corrected harness takes only the compiler/link segment and excludes post-link
canonical discovery entirely. Initial causal.log remains; causal-02.log has the
actual successful legal and seven rejected numerical implementations. No product
failure, test-discovery rewrite or hidden rerun of a historical failure occurred.

## Canonical gates

Both use tools/test/run_selected.py, jobs 1, ANNO_TEST_THREADS=2, timeout 180,
asset-policy required (no selected case needs original assets), admission enabled.
Debug includes target build. ASan target built with jobs 2 beforehand;
ASAN_OPTIONS=detect_leaks=0 (unsupported here), UBSAN_OPTIONS=halt_on_error=1.
Required contracts are wildlife_ground_reference_contract and
wildlife_postload_reference_contract, distinct from the main selection.
The unchanged picture contract is outside this non-rendering leaf's scope
(`--skip-auto-contract`); no renderer evidence claimed.

Selection: wildlife ground/fill/graphics, vertex grid, prefix fertility ownership,
and ground/postload reference agreement. Exclude original Tutorial fill and
shipped vertex corpus explicitly, not by prerequisites or a fake asset setting.
Both logs freeze exact commands, inventories, clean source identity and binaries.

`cache/wildlife-ground-prepare-1/debug-final.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=21 executed=21 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

`cache/wildlife-ground-prepare-1/asan-final.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=21 executed=21 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

No memory refusal, timeout, failed/empty/incomplete case, skip or sanitizer
report. No whole gate repeated. Two required contracts per profile are each 1/1;
main selection is 21/21 in each, not 23/21 or nested assertion counts.

## Warning, scope and host checks

Two changed TUs compile at O3/Werror under Apple Clang 21 and existing container
GCC 15.3.0, all strict conversion/shadow/etc flags. GCC shadow negative control
refuses as expected. Compiler warnings 0; inherited Apple link diagnostic
`ignoring duplicate libraries: src/sim/libanno_sim.a` remains visible.
Format 23.1.3 dry-run and lint_sim pass. No full warning campaign claim.

Caller audit finds only declaration, definition and focused tests. Existing
fill implementation/validator byte-identical to base; all old fixtures/pins
byte-identical, only a new authored vectors file. No World/native/default/PRNG/
command/registry or source adapter changed. Only two test registrations and the
new library/test source registrations. No original table or asset committed.
Host Apple M5 Max, 18 logical CPUs, 48 GiB, macOS 27.0.1, 1.5 TiB free at intake.
Shared coordinator and owner awake assertion preserved. No reviewer agents.

NOT_RUN: original/HE/Windows runtime, LeakSanitizer, full corpus/full suite/full
pins/full warning campaign, original source/city/climate binding, prep-to-fill
composition, World/live path. Hosted CI is read after push and sent directly;
no green claim while pending. Independent orchestrator review is pending.

## Frozen receipts

| Local path | SHA256 |
|---|---|
| `cache/wildlife-ground-prepare-1/reference-contract.log` | `fe6c73f2419805355ce2dcc1739097857136956412b7e71771b9cdb3a6e7a529` |
| `cache/wildlife-ground-prepare-1/cheap-cpp-01.log` | `a6b37968332b10bfd91ddf7a0cf47fbd50c450c1b531eca345a2252cd9454c2b` |
| `cache/wildlife-ground-prepare-1/debug-final.log` | `ea408ef455226c3aaf01c62620eb91d1294d82950bc121bf58371ddb3d1b36e6` |
| `cache/wildlife-ground-prepare-1/debug-final.log.inventory.json` | `0331b47ebd335f4a3ccfd590af3477f775fdf5f90d2e18823dd9a0b58eb21482` |
| `cache/wildlife-ground-prepare-1/asan-final.log` | `356d1c557959685829d4a0b7f8ed45c64d9f5d5b83dc8ef9ff9a8399ae7ebfb0` |
| `cache/wildlife-ground-prepare-1/asan-final.log.inventory.json` | `1ef025de4853444873bd67761593f56c7382827f1dee6fb7b69f6e3c0252a018` |
| `cache/wildlife-ground-prepare-1/build-dev-01.log` | `540ecfc98c00071af711bc68b34c22fd5b1f6ac74401a580d884406dae452bd6` |
| `cache/wildlife-ground-prepare-1/build-asan-01.log` | `7c9fb878bc267fa7d62f2fcc4181b2ce50bff4b7456242d707a9711db51b043b` |
| `cache/wildlife-ground-prepare-1/causal.py` | `6594ccbdfe02b2c15251d5d431aa3db57918c8a46d87ff492bec3edd10d7a87a` |
| `cache/wildlife-ground-prepare-1/causal.log` | `6f42214ba10e5c585b9a987b91b585a0dce5f6c4733215ac33d0eac60e8a8e7c` |
| `cache/wildlife-ground-prepare-1/causal-02.log` | `35807d60a64665d49727641bb1cd43343a404eb11c194b0c5ad8126435e74999` |
| `cache/wildlife-ground-prepare-1/causal-results.json` | `0ffd227955a54a0c2068d913d8aae9ad4f2a05aebf9c792f5aa24e013284ce27` |
| `cache/wildlife-ground-prepare-1/causal-counts.json` | `037d58e57ad94b575cbb5c7be982bc9f81a820a05f8dadf72c2424e4f585be6d` |
| `cache/wildlife-ground-prepare-1/canonical-before.json` | `779a6b48a3faf26ac3e3e195083eee443170f37e45bb3d15d4bd678d27c15f9c` |
| `cache/wildlife-ground-prepare-1/echo-control.log` | `604f53501239bb01aa8e3a87aa46416775a3234a6d3acff440b6a52b6c09d7b7` |
| `cache/wildlife-ground-prepare-1/warning-results.json` | `27f4ff77a926c487c40159f4c4d9a012c4dd0ca2f4b4ea40f35d531f23553ea9` |
| `cache/wildlife-ground-prepare-1/gcc-warnings.log` | `bea111602631e7ffbf877148a3e2a8bf35acc282a4ee48a200e477c1fe943607` |
| `cache/wildlife-ground-prepare-1/lint.log` | `c90ca6eba541545a82beb573bfcbcc47531df735edd4ff68c814ab50296e6948` |
| `cache/wildlife-ground-prepare-1/callers.log` | `6f70a18a65783058da9f4eaa017536255fba875c28c7d71c929a4f6a0c910101` |
| `cache/wildlife-ground-prepare-1/sha256.json` | `3c6b3625ebbc9ce37d1bd6bf1761de1727d6d3de76ae2bb75032250efc79d0a1` |
| `build/dev/src/sim/anno_sim_tests` | `41f0926e8a1529ef1e4f8faa5087da6bf132d5f4a73d81b143656493fcd262f6` |
| `build/asan/src/sim/anno_sim_tests` | `9cbd4c149ea0b20b3420cc337bd28d52513d073945f7c2b9dd739bfe641bf598` |

Docs closeout log `cache/wildlife-ground-prepare-1/docs-check.log`:

```text
CLOSEOUT files=12 local_links=5 manifest_hashes=73 published_hashes=22 scope=UNWIRED existing_fixtures_pins_unchanged=1 status=PASS
```

Log SHA256 `4f9866b3b353457585dfc812179e3917c5e489edd4281572d19bf3beadb46fc8`.
Excluded from the manifest to avoid a circular self-hash.
