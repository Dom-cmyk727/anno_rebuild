# LAYER-CHECK-1: the orchestrator's review (2026-11-17)

`task/LAYER-CHECK-1` `8bb58166`, Windows executor, base `main` `ac02d4a2`. Files: `tools/layer_check.py` (new),
`CMakeLists.txt`, `docs/testing/README.md`, `docs/orchestration/REPORTS.md`. CI running at review time (run
44008782965).

## What holds

- **The link list comes from CMake.** `file(GENERATE)` writes `$<TARGET_PROPERTY:...,LINK_LIBRARIES>` for the three
  targets after every `target_link_libraries` call; the script reads that file. Only targets strictly below a
  library are allowed.
- **The `anno/...` rule is one table** in the script's docstring. It covers each library's own prefix, the layers
  below it, `anno/test/` for tests, and `anno/sim_load/` for the sim tests, which matches the existing link of
  `anno_sim_tests`.
- **Two CTest tests, label `workflow`:** the real tree and the self-test. Both synthetic failures the packet named
  fail with file, line and rule. The real tree passes; no violation today.

## Findings (changes asked)

**F1. Headers outside `anno/` are checked against a deny list. The packet asks for an allow list.**
- `classify_include` refuses a few named patterns (SDL, webgpu/wgpu) and then lets every other non-`anno/` header
  through ("a third-party header this rule does not name"). So `<vulkan/vulkan.h>`, `<windows.h>`,
  `<GLFW/glfw3.h>` or `<d3d12.h>` in a sim source would pass.
- The packet: the standard library, the library's own and lower `anno/...`; "Everything else is refused".
- What the three libraries include today outside `anno/`, all on `main`:
  - standard headers;
  - quoted helpers in the same directory as the including file (`world_access.hpp`, `golden.hpp`, ...);
  - `doctest/doctest.h` in tests;
  - `<sys/resource.h>` and the generated `"freight_original.inc"`, both in `src/sim/tests/founding_assets_test.cpp`.
- Asked:
  - allow the C++20 standard library headers (one list in the rule file, with the standard as its source);
  - allow a quoted header only when it resolves next to the including file or in the library's own include
    directory;
  - allow doctest in tests;
  - name every other header the tree uses in the rule, with its reason and the file that needs it
    (`<sys/resource.h>` for that test's memory probe, the generated `.inc`). Anything not listed is refused.
- Add two self-test cases: `<vulkan/vulkan.h>` and `<windows.h>` in a sim source must fail. The real tree must
  still pass.

**F2. The docs section sits inside another section.**
- `## The layer check` was inserted after the first paragraph of the golden-writer text. The paragraph "The
  existing `golden.hpp` writer ..." now stands under the layer check's heading. Move the new section so that it
  ends before the next `##` heading of its own, or put it at the end of the file.
- "This page and CLAUDE.md cite it" is not true; `CLAUDE.md` does not, and an executor does not edit it. Drop the
  `CLAUDE.md` part.

## Note, no change needed

- Only the three library targets' link lists are checked, not their test targets'. That is enough for the rule:
  a test executable that links more does not change what the library needs.

## Verdict

**CHANGES REQUESTED**, about 45 minutes, on the same branch. ACCEPTED once F1 and F2 are done and CI is green on
that head.
