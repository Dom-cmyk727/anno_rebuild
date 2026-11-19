# WILDLIFE-IMPORT-PLAN-1 — frozen focused evidence

2026-11-19. READY FOR REVIEW, independent orchestrator review pending.
Base ee67516642de3a9ff6ac1e0986f1a17c1be2ad41; exact dispatch 7f448187f336129320817601a016391a96ef612c.
Tested source `39b49ddca0759715cc38b1e7e7e5b2393350605c`, tree `c390e0ca96444e46ebdabed1c5d618606801aab5`; publication adds report/evidence only.
[Contract](../../sim/wildlife-import-plan.md). No main/board merge or rebase.

## Source and literal proof

Original GOG AnnoWelt.dll SHA256 f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2.
Private addressed reads: HIRSCH 0x10094744–0x10094797; prepend 0x1007bd50–0x1007bd60;
BERGWERK 0x10077f74–0x10077f87/0x1007c290–0x1007c2cc;
OCCUR 0x10095242–0x10095255. All use chunk+0xc head; proven presence bits 1/2/0x40.
No original listing, bytes or assets in git. Cache-only private reads retained.

Authored raw source actually passes through first-call preparation. Headers interleave
HIRSCH, multi-chunk BERGWERK, OCCUR with duplicate ignored load, then HIRSCH.
Global tokens 0/1 spawn, 2–5 footprint nodes, 6 occurrence, 7 later spawn.
Target mixed head [7,6,2,1,0]; final maintenance [1001,1000,7,6,2,1,0].
Explicit non-presence 0x8000 yields 0x8043; explicit zero valid; overlap 0x43 refuses.
Typed sidecars retain resource masks/offsets/rotation/ore state and both load provenances.
Known loaded zero is distinct from unknown saved ID/high bit/byte 11.
Final three authored cases execute 318 assertions; 17 corruption variants plus flag cases
follow legal controls. Ownership after source destruction, unchanged inputs and repeat tested.

Original Tutorial/01Discover01 chunk (10,2): one actual imported spawn, token 21,
header 71/record 0, local position 47, loaded time 8/goods 0. Source has 177 represented
insertions: 156 spawn, 20 BERG, 1 occurrence. Independent raw parser constructs chronology,
then reverses/filter selects the target; no C++ output used to generate expectation.
Oracle SHA256 338361128f659ba8031b327814481959349e458c7d03cf87c9806f55d4e31e2d.
Original target lacks interleaving, so the authored companion remains necessary.
Authored runtime time 10000/flags 0x8000, no fresh allocation. Named draw counts:
imported-prefix 4745, maintenance 0, selector-only 2; these are not a global total.
Independent species rows 0/1 yield 137, row 2 yields 0/type 528: saved goods 0 matches
row 2 after 3 inspections. Raw species oracle SHA256
fda833fca3e31245f1b9ce6088015ac6b0379a37fde6687ed2602b914617b0cc.
Original import case executes 44 assertions, including oracle equality (environment set).

## Causal controls and preserved first failures

Private actual source mutants compile/link and execute the same authored cases.
Legal control: 3 cases/318 assertions pass. Wrong controls reject:
- group concatenation: 2 cases/13 assertions fail;
- forward chronology: 2/15;
- duplicate-load insertion: 3/3;
- unknown saved-ID zero: 1/4;
- maintenance draw replay into selector: 1/1 (selector refusal).
Replacement application asserted exactly once; canonical files/binary unchanged.
`causal-assertions.json` has first assertions, log hashes; private sources/binaries retained.
First replay mutant compilation failed on unused selector parameter, NOT semantic evidence.
`causal-resume.py` explicitly marks it unused and reruns only that control; exit 1 above.

Earlier compile failures preserved: narrowing uint16 division before widening (`build-first.log`),
unused helper before its intended pipeline test (`build-authored.log`), and range-loop copy
(`build-all-controls.log`). Corrected without suppressions. First original run had three
incorrect test expectations: saved zero was treated as unmatched. Independent raw rows settled
slot 2/type 528/3 inspections; production unchanged. `all-first.log`, actual failed binary,
original test source, and `selection-derivation.json` retained. A failed exact-anchor editing
attempt made no edit and ran the old expectations (`all-no-edit-attempt.log`), also retained.
A later negative-control edit assertion failed before writing; not counted as a run.
The raw fixture Git-blob comparison initially failed on pre-existing CRLF in growth/comments.dat.
Exact predecessor frozen WORKING bytes match all 292 fixtures; LF-normalized blob also matches.
No fixture modified, and no such comparison failure is represented as a passing gate.

## Canonical focused gates

Host Mac17,6, Apple M5 Max, 18 logical CPUs, 48 GiB, macOS 27.0.1 (26A434), Apple Clang 21.
Build jobs 2, gate jobs 1, ANNO_TEST_THREADS=2, timeout 180; admission enabled via host socket.
Original files + import/species/final/region/habitat independent oracle paths explicitly set.
ASAN_OPTIONS=detect_leaks=0; LeakSanitizer NOT_RUN. No gate rerun or refused test.
Selector and required names in cache `gate.py`/`selected-names.json`; source inventory retained.
29 unique/profile = 4 import + 4 habitat + 7 chunk + 7 materialization + 4 final + 3 prefix.
Four original cases each required/run once; remaining 25 form main selection.
No renderer contracts required for this headless source-planning seam; auto-picture contract skipped.

Debug `cache/wildlife-import-plan-1/debug.log`, SHA256 `9b3fd168a5c9e97943da4a20e3a18512fa430f94ba83683cee5032fece5fd3ea`:
```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=25 executed=25 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```
ASan/UBSan `cache/wildlife-import-plan-1/asan.log`, SHA256 `d1b567f8f1921664b76f75e2c9244aac2da14da055b41a227633f8f9313b4f23`:
```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=25 executed=25 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```
Both first gates PASS, no skip/empty/incomplete/timeout/sanitizer diagnostic.
Apple Clang 21 and Docker GCC 15.3 strict O3/Werror compile new source and affected test TU;
shadow negative control rejects. 0 compiler warnings; inherited duplicate-library linker warning
retained in build logs. clang-format 23.1.3, lint_sim and diff checks pass.

## Identity, preservation and limits

1027 frozen source/fixture/tool/build/binary identities reverified after both gates.
Debug executable SHA256 be335fed10440ba51cb4e092625927e80b54f564fe91e78394aff9deb17ac61a.
ASan executable SHA256 483b5fa2a8b17084d16d490df7d14b163a1e4445ea4a2c68528bd596c73a60c0.
Actual binaries/source copies retained, not just hash claims. All 292 fixture working bytes unchanged.
Prior habitat113/final128/CI-close50/materialize114/chunk110 owned receipts still match.
Closeout manifest: 116 artifacts, SHA256 `1f27ae66f95ca48b4feb8460185440b72e84b05cdd0ac38c6519e8c3ca69787d`; logs stay cache-only.
Hosted exact-head CI read after ordinary own-branch push; orchestrator owns pending queue closeout.
No previous failed CI/gate is cancelled, relabelled or erased.

NOT_RUN: native manager/allocation/unknown cache flags, omitted runtime categories, arbitrary
post-import mutation, whole sweep/global history, native creation/coordinates/first actions,
World/save/default/state/identity, cave provider/geometry, navigation/FP, original/HE runtime,
Windows/WSL, broad/full/heavy/opening/pins, LeakSanitizer. Existing stage algorithms unchanged.
Next seam: native runtime context and creation/geometry remain separately unresolved, not implicit
zeroes or successful spawn effects. No QUESTION within this bounded conditional profile.
