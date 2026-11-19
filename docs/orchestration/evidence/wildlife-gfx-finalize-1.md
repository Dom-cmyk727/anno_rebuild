# WILDLIFE-GFX-FINALIZE-1 — isolated leaf evidence

READY FOR REVIEW. Tested source `e9256f2234535d12917cc8cc4a39523d94dec358`,
tree `09d6fc88608cfec3cacbfeb41b99986414a6c1f0`, from POSTLOAD
`1628ba18a5b6eefb55684afc3091ba3be9fa10c8`. Final publication changes docs only;
head/tree and exact CI identity accompany direct handoff. No main/board merge.
Dispatch `7b64a314739a8e9a7d86e3f2ee58853020b0a972` was read, not merged.

## Primary predicate and supported geometry

D: AnnoWelt 0x10085290–0x100853a9 freshly re-read (private span/module hashes below).
Exact high byte 0x20 at 852d1–852e0; 4x4 x/y-1..+2, inclusive W/H bounds
85303–8532d; max zero clears full u16 at 8534f–85353. No PRNG or other write.
[Contract/acceptance](../../sim/wildlife-postload.md#9-isolated-graphics-finalizer--wildlife-gfx-finalize-1)
and [vectors](../../../fixtures/synthetic/sim/wildlife_postload_vectors.txt) retain
hand derivations. Reference/vectors commit `68516060` precedes any C++ commit.
Only positive validated TileLayer geometry/raw Terrain is supported; exact HEIGHT
and GFX lengths, fertility absent or exact, matching source digest. Product uses
u64. VertexGrid supplies forced-zero logical W/H borders, outside zero. Arbitrary
logical borders and physical H+1 sentinels are NOT_EXPRESSIBLE, not fake controls.
Nearest-edge clamping only repeats legal nonnegative samples: no distinct-mutant claim.

## Ownership, reference and causal checks

`finalize_wildlife_graphics` is pure and UNWIRED; only declaration/definition/tests
appear in callers.txt. No prefix, scenario loader or World call. One output graphics
copy; other columns copied once into new immutable Terrain; digest computed once
at completion. O(W*H), at most 16 vertex reads per qualifying tile. Input snapshot
success/refusal equality, output after source destruction, other channels,
changed/no-change digest cases, repeated invocation and idempotence are asserted.

12 hand-derived vectors execute in Python and C++, including 0x6001/0x2101,
full-word clear, height 1, (+2,+2), (-1,-1), x/y+3 exclusion, asymmetric 5x2,
1x1, tall 2x3 and whole-plane scan. 23 CLI controls check actual child statuses,
nonzero execution, wrong expectation exit 1, malformed/missing/zero exit 2.
First contract control incorrectly put the second arrow on a new line: wrapper
failed its expected diagnostic; the failure note is retained, fixture corrected.
First C++ build had three test-only TileLayer API errors; full compiler log retained.
Corrected before source freeze; no canonical gate failed.

Actual production-TU private mutants are recompiled/linked with the unchanged tests.
Legal control: exit 0, 3 cases / 1639 assertions pass. Bit-test mutant: exit 1,
2 failed cases, 6 intended assertions fail / 1633 pass. Truncated 2x2 mutant:
exit 1, 1 failed case, 8 intended assertions fail / 1631 pass. Both include exact
full-plane disagreement after valid setup. Replacement counts verified first;
no private mutant entered the branch. Canonical source hash equals legal source;
canonical rebuild precedes gates. Outputs/commands/hashes in causal.json.

## Canonical gates

Debug: cache/wildlife-gfx-finalize-1/debug-final.log
```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=9 executed=9 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```
ASan/UBSan: cache/wildlife-gfx-finalize-1/asan-final.log
```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=9 executed=9 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```
Each profile has 9 main passes (3 leaf cases, 5 reused VertexGrid dependency cases,
1 reference agreement) plus 1 separately required CLI contract, not double-counted.
No failures, empty, skipped, incomplete or sanitizer diagnostics; no memory refusals,
timeouts or whole-gate repeat. Jobs 1, test threads 2, 2048 MiB/thread, admission
unchanged; host access used as authorized. No assets needed by this selection.

Changed-file format 23.1.3/lint/diff check pass. Two new TUs compile -O3/-Werror
under Apple Clang 21 and GCC 15.3; GCC's shadow-warning negative control is rejected.
Compiler warnings 0. Inherited Apple duplicate-library LINKER warning retained;
no full warning sweep claim. Host Apple M5 Max, 18 logical CPUs, 48 GiB,
macOS 27.0.1, 1.5 TiB free at intake. Other jobs/coordinator/awake assertion untouched.
All 276 predecessor synthetic fixtures (120 sim) byte-identical; only new authored
vector file added. Runtime sim core/public headers and prefix adapter byte-identical.
No state/tag/default/identity/pin change; no full pins.py claim.

Decisions and reasoning:
- Use the existing verified VertexGrid/terrain_digest rather than a second grid/cache.
- Support raw Terrain only; narrow boundary claims instead of test-only interface widening.
- Return owned data and reject malformed/stale inputs before output writes.
- Prove distinct actual-code predicates, without a false clamp-equivalence mutant.
- Keep this leaf unwired; original fill -> navigation/raster -> cleanup order remains binding.

NOT_RUN: original-file leaf/runtime/HE, full postload/aggregate draws, World/native/
lifecycle, full pins/warnings, unrelated prefix/corpus/sailing/render/long suites,
Windows/WSL, LeakSanitizer. Unchanged evidence is reused by dependency: no production
caller, no existing sim/core/prefix change. Prior EVENTINSEL/POSTLOAD evidence stays frozen.
QUESTIONs: none blocking this isolated leaf; first/second-call inputs remain separate work.
Exact-head CI pending at publication, read immediately after push; no green claim yet.

## SHA-256 receipts

Private listings/raw game bytes are not committed. Hashes refer to frozen source,
synthetic inputs, raw logs/commands and binaries, not later prose changes.

| Artifact | SHA-256 |
|---|---|
| `docs/sim/wildlife-postload.md` | `240d18e1bcde6eec1a416af60403b519f0c518ee0255811269ea5134d93d2581` |
| `src/sim_load/include/anno/sim_load/wildlife_graphics.hpp` | `1acd636fc89af390256ed13de26161a6728e5cf143ee1d3cd98b384281f7bc8a` |
| `src/sim_load/src/wildlife_graphics.cpp` | `b585749f33b31fd5da6215cb8a0618851ec382d0a7d70b1e5e8744310d4322fb` |
| `src/sim/tests/wildlife_graphics_test.cpp` | `74405f387a6a54936867cbfae41c817df5bb75c3262c891722f72307dff7e6ac` |
| `tools/wildlife_postload_ref.py` | `78d85d2b258935d5c3d432bce04ccf2589517029cda236bb7e01e6c1c7a5ce22` |
| `tools/wildlife_postload_ref_contract_test.py` | `b337da7c54565384ccdcc1c933d8ffd3e5224c5ef91c28305644f81dcfdb0c14` |
| `fixtures/synthetic/sim/wildlife_postload_vectors.txt` | `54e47431c698b3f1220e13367a31b6a9e20298ea426b9fc461eb107fa1e68a58` |
| `CMakeLists.txt` | `16ca32ce894fd4e8347438d3dc423091c257af7f3152c0d84a722979ca64c3d1` |
| `src/sim/CMakeLists.txt` | `8596f9a6004f1fccceeeaeaa049673b57cbe28472b632c8eaff610294ad02c07` |
| `src/sim_load/CMakeLists.txt` | `b81b23d13af49773c6046dee2c08dfda20dd5eb229015f7bc3b19cce0740a03b` |
| `build/dev/src/sim/anno_sim_tests` | `f27170ec1f14079e8c619bf7f886f7f8347fcebd13ace04529b64d4be3119110` |
| `build/asan/src/sim/anno_sim_tests` | `609d97690fccdf5ca8bcad0e659fe45f1584a66826ca7cf09f21a235f25d9f41` |
| `cache/wildlife-gfx-finalize-1/leaf.private` | `ceeb8446b2b92f0bf11b785704eafacce9176f64ccf7fe6f361b6def4a0d837e` |
| `cache/wildlife-gfx-finalize-1/primary.sha256` | `e810b57ed8c5643be814f7909eff9d1b9852dc93209002f6d72c268c353b5c3a` |
| `cache/wildlife-gfx-finalize-1/reference-before-cpp.log` | `b1a8da798eab914274a5838099cbe6c54b51724b793de4f5e58ffa10eb9e9f90` |
| `cache/wildlife-gfx-finalize-1/controls-before-cpp.log` | `77fbcd77ca37ab7fc9750e86b3b22fbf088893a8f774940d559c5340535e8bf1` |
| `cache/wildlife-gfx-finalize-1/controls-first.log` | `e2d37a356e5cb3f328f127b640d5f4aa36440e36f55c1f2a8b4a0608f9c87362` |
| `cache/wildlife-gfx-finalize-1/build-first.log` | `ab6fed2549695aafa7a456ce89407d004df789c91f4831f46cb719e290a4be92` |
| `cache/wildlife-gfx-finalize-1/build-corrected.log` | `fdfd8af94eced33c34d13251317a0433a7b2db63ed01683e166f8d57ad966eb2` |
| `cache/wildlife-gfx-finalize-1/build-canonical.log` | `c2ed93ffd93c840755c0e91392ae593b1b52de9b7aaf6c208123feaceb1dde2e` |
| `cache/wildlife-gfx-finalize-1/build-asan.log` | `d87a291103ce5e89530b409ace942f0a84ecac3f90798a795e9608cf6bb301cd` |
| `cache/wildlife-gfx-finalize-1/legal.cpp` | `b585749f33b31fd5da6215cb8a0618851ec382d0a7d70b1e5e8744310d4322fb` |
| `cache/wildlife-gfx-finalize-1/bit_test.cpp` | `19cc3369d941f8d2ee87d21f24cc0d7a216b7fbe8ff9c6f6018b17ddecbbd92a` |
| `cache/wildlife-gfx-finalize-1/quad.cpp` | `e66cd9970210e019307a9d068249fa1c15f45a4eee4093b2863ab2bef7d1fe18` |
| `cache/wildlife-gfx-finalize-1/legal-run.log` | `99b5e45b5ece146fdba7db779ebcf0cc9c33c981184d169ac71e2af84f9fb041` |
| `cache/wildlife-gfx-finalize-1/bit_test-run.log` | `3fcd3672d7833ec7b43a52d5f049fd732fc6983d865a6c284f2e9713a06983f0` |
| `cache/wildlife-gfx-finalize-1/quad-run.log` | `d3228355246a69accc83cab323d0ceee72d16a88716502d1df1ed6bc966790c2` |
| `cache/wildlife-gfx-finalize-1/causal.json` | `1efa608ae552bf9701089830b2616e498bfd823ee91b5bbc5bbc9de890986177` |
| `cache/wildlife-gfx-finalize-1/causal-console.log` | `0485dd502c574c76d69f2125604947670675dc68e189b44af59fc002b1289d69` |
| `cache/wildlife-gfx-finalize-1/apple-warnings.json` | `7db596c942d5d4b0e8db126aef89c1c308f167fd853cfd23996f04eba61623ed` |
| `cache/wildlife-gfx-finalize-1/apple-warnings.log` | `f98d778a04eca401808fa172b70925acd3833694a4a040800036c48a71cfdee9` |
| `cache/wildlife-gfx-finalize-1/gcc-warnings.log` | `c0ea4602e82055b336265b758bdaa5f9f7d371efa252f56686881ad7b1412ae1` |
| `cache/wildlife-gfx-finalize-1/target-warnings.sh` | `f58ad95628c9aecf81159fbbd080f4cd8bd98744df7454545bcace20a0a9ddd2` |
| `cache/wildlife-gfx-finalize-1/scope.json` | `5cbb3f39678106741261d09a8005e20d6bc067237ac077ffe681fcb1004dc4d0` |
| `cache/wildlife-gfx-finalize-1/callers.txt` | `80fd729f86593657cbf95307c443787f90d98286eea1838adbf2a5ebd3b9132b` |
| `cache/wildlife-gfx-finalize-1/debug-final.log` | `08f1481da5ce8e5774b2eae0c7dc52734e3a3091aed3be36992b2e355c39b192` |
| `cache/wildlife-gfx-finalize-1/debug-final.log.inventory.json` | `18a21b8938951f8b9ceaa4da4ee0e5b0bfb95cd8f5ea66c7b609dfda97bc1cbd` |
| `cache/wildlife-gfx-finalize-1/asan-final.log` | `78950c4505a526d8f6484e23f6c4bc1abf1cd99c3a74459a61c909363dd564ce` |
| `cache/wildlife-gfx-finalize-1/asan-final.log.inventory.json` | `ea310685b2c424c1bf58e091a5ffe16b86f757f6e08c09aecb2cbb6b64857c54` |
| `cache/wildlife-gfx-finalize-1/lint.log` | `c90ca6eba541545a82beb573bfcbcc47531df735edd4ff68c814ab50296e6948` |
| `cache/wildlife-gfx-finalize-1/docs-check.log` | `dfaf66650b579836cec578cfb0df59d5c0f5e3e3b7034c9402b5ac75c71958ec` |

Final docs check (40 pre-existing receipts checked, followed by this check-log receipt):

```text
DOC_CHECK files=2 links=13 hashes=40 missing=0 status=PASS
SCOPE: frozen runtime/tools/fixtures/CMake unchanged after gates; only report/contract/evidence updated
```
