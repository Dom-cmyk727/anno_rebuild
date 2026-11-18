# WILDLIFE-LOAD-1 — frozen review evidence, 2026-11-18

**READY FOR REVIEW**, not accepted. Accepted independent base
`2f9e4c0bf84cf09998a43c7ff8098fa1522f3a4e`; packet `c292a3c8`, factual amendment
`93eae8d7`. KBEWOHN/DOCS/pure wildlife implementations were not merged.
Full pushed head/tree and exact-head CI are supplied with the frozen handoff.

## Acceptance table

| Requirement | Positive proof | Distinguishing negative | Evidence | Review |
|---|---|---|---|---|
| All eight file bytes | Exact independent x/y/flags/raw5/goods assertions; independently authored encoder expectation; extrema/ownership/order | Swap x/y, discard raw5, mask flags | Seven C++ cases; four counted overlays with legal zero control | pending |
| Absence and duplicates | Missing optional differs from present-empty; first valid/empty/malformed header; all chunks retained | Collapse absence to empty fails presence assertion | Accessor tests, original callback read | pending |
| Explicit strictness | Only decoder/accessor rejects nonmultiples of 8; outer island parse/opaque bytes unchanged | Lengths 1–15 except 8 refuse; malformed first duplicate does not fall through | Synthetic C++ cases and public Python contract | pending |
| Independent reference/corpus | Nine hand-derived vectors, 28 CLI checks; C++/Python corpus counts and decoded-field digest agree | Changed expectation exits 1; unknown/zero/malformed input exits 2; malformed corpus exits 1 | Required reference pair, two real corpus cases | pending |
| No premature integration | No production caller, engine/state/registry/draw/default edit; 119 tracked fixtures identical | Explicit call-site/scope/hash audit | reuse-proof.json; integration boundary | pending |

## API, raw evidence and unresolved next scope

[Raw format and addressed policy](../../formats/insel.md#hirsch-raw-spawn-records-2026-11-18-wildlife-load-1).
[Integration boundary and proposed section/tag needs](../../sim/wildlife-load-boundary.md).
`HirschRecord` holds u16 x/y, u8 flags/raw5, u16 goods. Public API:

- `decode_hirsch(Bytes) -> Result<vector<HirschRecord>, string>`;
- `encode_hirsch(span<const HirschRecord>) -> ByteBuffer`;
- `Island::hirsch_records() -> Result<optional<vector<HirschRecord>>, string>`.

The original loader `0x100946c0`–`0x10094873` was re-read in AnnoWelt.dll SHA256
`f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`.
No original bytes, instruction listings or decoded per-record data are committed.
The original counts whole entries (`payload_size >> 3`); our explicit accessor
refuses a remainder. This is parser safety policy, not a claim that an outer
original reader accepts trailing bytes. Existing island loading remains unchanged.

The apparent lookup `0x100946dc` calls a supplied-header string comparator,
`0x100b4b20`–`0x100b4b6c`. The INSEL5 child loop `0x100a2f8c`–`0x100a2fbc`
dispatches each header through island slot +0xdc, with wildlife table entry
`0x100cad08` selecting this loader. Do not call our first-record convenience
policy original duplicate emulation; all chunks remain available to a later loader.
Original records become prepended chunk-list objects (`0x10094792`, `0x1007bd50`),
so same-chunk A,B file order becomes B,A traversal, without reversing file/jitter order.

**Confirmed primary correction:** `0x100946e9`–`0x10094705` computes
`floor(u16 island[0x9a] / 500)`, range 0–131, maximum plus jitter 31 = 162.
Inherited `/1000` and 0–65 are withdrawn in the new boundary note. The smallest
distinguishing input 500 gives 1, not 0; exhaustive integer comparison covers all
65,536 inputs and was independently confirmed by the orchestrator and another
read-only original-byte reviewer. Physical clock units remain unproved.
No timestamp code exists in this packet; the preserved wildlife read/helper/oracle
correction is a separate unit before persistent integration.

Still U: global order across fresh/reused islands, templates/links, actor/building
and player/city draws, script INIT, world-mode provenance/upstream duplicate
admissibility and later materialization. These are bounded handoff gaps, not guessed
zero draws. Proposed `header.wildlife_rules` / `islands.wildlife_spawn_records`
need coordinated tag values and a reviewed canonical payload; none is allocated.

## Original corpus

| Source | Files | Islands | Absent | Present-empty | Entries | Malformed | Duplicate islands |
|---|---:|---:|---:|---:|---:|---:|---:|
| SCP | 465 | 465 | 465 | 0 | 0 | 0 | 0 |
| SZE | 50 | 668 | 0 | 530 | 13,919 | 0 | 0 |
| Total | 515 | 1,133 | 465 | 530 | 13,919 | 0 | 0 |

668 HIRSCH headers; selected and all-header entry counts coincide here. All
13,919 observed entries have byte 4 bit 0 clear. This predicts the loader's
conditional branch, not successful allocation or a whole-runtime draw total.
C++ and Python canonical SHA256:
`f4078805e58d909226279c20b7a2f2d38b6bc3d1aeb7578906511214780f9aa9`.
Canonical input: sorted Scenes-relative UTF-8 filename (u32 length), u32 island
count, then each u32 index/u8 presence/u32 entry count and the explicit eight raw
field bytes. C++ builds these from decoded fields independently of encode_hirsch,
so a mutually wrong reader/writer cannot hide a field swap behind round-trip equality.
Private `python-corpus-manifest.json` retains every input file SHA256 and counts.
Original bytes/record listings remain outside git. CI asset skips are not this coverage.

## Canonical gates and reuse

Logs/inventories below are under ignored `cache/wildlife-load-1/`. Required
`wildlife_load_reference_agrees`, `wildlife_load_reference_contract` and
`picture_matrix_contract` run separately, once each per original focused gate;
the remaining selection is 21 CTest entries (not 13,919 or 9 vector records).

```text
focused-dev.log
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=21 executed=21 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
focused-asan.log
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=21 executed=21 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
raw-bytes-dev.log
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
raw-bytes-asan.log
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

Exact main selector:

```text
^(anno_formats_tests\.(HIRSCH .*|insel corpus|nested exact fit|garbage after NUL is kept|opaque payload has no children|trailing bytes after root|rejects bad names and sizes|twelve-character name without NUL is read|rebuilds container from children|name field validates|depth limit matches reference|tagged records: .*|unit prefix round trip)|wildlife_load_reference_(agrees|contract|corpus))$
```

Both profiles use `tools/test/run_selected.py --build-dir build/<profile>
--target anno_formats_tests --select <above> --require-test wildlife_load_reference_agrees
--require-test wildlife_load_reference_contract --jobs 2 --timeout 300
--asset-policy required --log cache/wildlife-load-1/focused-<profile>.log`.
ANNO1503_GAME_DIR is set to the read-only original installation; threads 2 and
normal admission enabled. ASan uses detect_leaks=0:halt_on_error=1 and UBSan uses
print_stacktrace=1:halt_on_error=1. LeakSanitizer is NOT_RUN on this host.

The targeted warning check initially passes production insel.cpp but rejects the
raw-byte test's unchecked copied-vector writes with GCC -Wnull-dereference at
hirsch_test.cpp:79/80. That first failure remains in its own logs. The authored
8-byte buffer now uses a fixed array (the on-file record width, no engine cap).
Only that case was rebuilt/rechecked, jobs 1, in both profiles: raw-bytes-*.log.
All other production/reference inputs and tests are unchanged. Those two recovery
calls use --skip-auto-contract, reusing the already passed unchanged references/
picture contract and the other 20 cases, rather than repeating the entire gate.
The final GCC 15.3 Release -O3 -Werror test compile is clean; the successful initial
production compile is retained. No warning flag/tolerance/assertion was suppressed.
Later edits only clarify unused addressed documentation; no decoder gate rerun.

Four private production overlays each match one replacement, compile/link, then
exit 1 at the intended assertion: swap coordinates, discard raw5, mask flags,
collapse absence. Every run retains PASS for the legal all-zero record. Positive
control passes all 8 checks. Actual production source hash remains unchanged.
Normal restored builds precede the final gates. Controls are a bounded private
probe, not a new mutation framework or claimed CTest suite.

No existing parser method body or existing Island data member changes; the new
accessor has no production caller. No src/sim, sim_load, app, render, test_support
or fixture path changes. All 119 tracked sim fixture blobs remain byte-identical,
including pins; no recording/update mode or pin/oracle writer is invoked.
This justifies reuse of the accepted base's unrelated sim/render/sailing/town
campaigns. It does not claim World integration, fresh game progression or GPU parity.

## Frozen identities and raw receipt hashes

Final source SHA256s (the first-gate and warning-recovery differences are separately retained):

| Repo-relative path | SHA256 |
|---|---|
| `CMakeLists.txt` | `576d659d62c7db91a6360d979884bac57759fa123ffacadd973fccc4e0383f7d` |
| `src/formats/CMakeLists.txt` | `bebe3d78a82c120f8bca3371228bd1957517bd70e7a93656f15a4d0be8900e82` |
| `src/formats/include/anno/formats/insel.hpp` | `d8280ee2fd94e93fef155da443d56f67bd21c468fa940d933ec63275c1da74cb` |
| `src/formats/src/insel.cpp` | `7921b34fbc0d80608ce3b05701f0eae31b46de99fcfea34afacf3b3db2b2b648` |
| `src/formats/tests/hirsch_test.cpp` | `954a6919b30bb1a5997925064890c0322ab0964deac5d80312abd01e7d6cfb42` |
| `tools/wildlife_load_ref.py` | `573ebc64bbec97a48a68589fe1b52aad0184d16d9453e18763706bf0882f3171` |
| `tools/wildlife_load_ref_contract_test.py` | `c6cf0481e1a7aa76f6e1354d6910ff07f1864323d17430f370a8e45ce7d9c634` |
| `docs/formats/insel.md` | `150ffdbb43ea0e74a90f8ae22e49d154109312cef46b36904d06af5e498552f7` |
| `docs/sim/wildlife-load-boundary.md` | `065b9f6b8b6db5d421eaffe9d9af0bb5152cd984392c400a363f1543c3b5a6dc` |

Private evidence SHA256s:

| Path under cache/wildlife-load-1 | SHA256 |
|---|---|
| `focused-dev.log` | `dd883d95c0d8a8915723a39344c302fb766b89f9f02234d24ee6266ead60c483` |
| `focused-asan.log` | `9a71934072af10d09cb14f5ca66b17630dd169ee93aafa9d8575a2785ccbbefe` |
| `raw-bytes-dev.log` | `c0c4022dc991d4a7aac388bc6af056765087239f166a344b31c7dc053983b7ee` |
| `raw-bytes-asan.log` | `8779010e56c8e1bb543554b4234208f0fa3740bd87943cd4ea11ca8c56b7772e` |
| `primary.json` | `9cbf15027628a12ac826523d26927b7e5feb2598b54a658cfe9d2a8dd7f713f0` |
| `time-arithmetic.json` | `b569bf261fd2d991207567960d1450a80b8d819b83c8dd4b6eb04fa649f4e9da` |
| `controls.json` | `fce0de68332b20e2f8ca0c66f9743a88c27521401d39d8867588153087760fe6` |
| `python-corpus-manifest.json` | `69138910e648b0182b868cb6dee3cfa3e980a487057fa93cd02201e7f9b3a1fa` |
| `fixtures-before.json` | `37451f2dea0f3854205ba4bc4498930beff2be10f0eb7c71f233b357ca7d47a4` |
| `frozen-inputs.json` | `a3a4d9f7be5a711e53acd3d183451d09180648061b338112c7fad83845047c3b` |
| `final-inputs.json` | `d03eb58721dbd878f4a2b2058e1c5781b80020e8f6029c8fc50a654291ad1c64` |
| `test-only-warning-delta.patch` | `ae245e8518628348dd05da4b24043bb62154b620ac4b304fc081fcfa2f99f1d5` |
| `reuse-proof.json` | `5f20cd409b8fd1c418a33bb3b218b9adb8485cafb7f1fac927e086fad897a40a` |
| `gcc-release-results.log` | `4a621a90a6a6f7d48cc624e89e08ad69f64a2edf966e53bf24e6ad1f86d4b145` |
| `src_formats_tests_hirsch_test.cpp.gcc.log` | `3119f15aaa971d9b2118d7837d6de7e4d6f455f7abc4301333becc3526159f0d` |
| `gcc-corrected/gcc-release-results.log` | `99cd7e7fcde29a3e7a9ea38dade12d3cbcefca201c5d4672fe00cc61dfe3edf8` |
| `build-dev-first.log` | `bde30d5d1bc32614cc6c19637f4cb80bd32b7d695bf1ac61ab2c824344ce2dea` |
| `build-dev-final.log` | `760f8b465d33ebaae7a4acbf876f28d35577e0b909519df330a060fcdaa31d98` |
| `build-asan.log` | `e41c285eca58e4f15e3d5bb30a77fc7f0a48e1eb92bab33993c5cd0ddabfa852` |

Final executed test binaries:

- `build/dev/src/formats/anno_formats_tests`: `a02cd57f2c24d6844ce57d66760ab31b1e9a5a2c1e03b62b582129d9fe086566`.
- `build/asan/src/formats/anno_formats_tests`: `4df861ceb0dc742f97f767dd4912c83718d91bddd19f86b4b0717a1120cce9f6`.

Host: Apple M5 Max / 18 logical CPUs / 48 GiB RAM / macOS 27.0.1 / Apple Clang 21;
CMake 3.31.10, Ninja 1.13.2, clang-format 23.1.3. Targeted GCC 15.3 x86 container
has source read-only, no network, CPU/memory limits. Its compile is not Linux runtime.
Builds have 0 compiler/linker warnings after correction; diff/format/file links pass.
Original source/tool/binary/fixture identities and full commands remain in canonical logs.

Decisions and reasoning:
- Own raw primitive values; preserve byte 5/unknown flags instead of guessing species.
- Keep absence/present-empty and file order explicit; never silently reverse jitter order.
- Opt-in strict length validation preserves all current unrelated island loading behavior.
- Bundle hand-derived reference vectors in the tool, leaving fixture/pin files unchanged.
- Correct the primary arithmetic in the handoff; do not smuggle persistent code/helper intake.
- Reuse unaffected gates after a one-case test portability fix and documentation-only clarification.
- NOT_RUN: original/HE runtime, native Windows/WSL original-file suites, full warning sweep,
  live wildlife/materialization, persistent state, complete load-draw ordering and LeakSanitizer.
- QUESTIONs: none blocking raw decoder review. Next persistence packet requires the separately
  corrected helper domain, global ordering/world-mode contract and coordinated extension tags.
