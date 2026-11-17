# GATE-RUN ui-info-2d-buildcat — Mac wave 0.1

**Released to EXECUTOR II MAC, 2026-11-17.** Run before any other Mac packet.
Execution only; no implementation or evidence repair. Read CLAUDE.md, the evidence contract,
testing/README.md and mac-lane-2026-11-17.md. No reviewer subagents.

1. Fetch `refs/candidates/ui-info-2d-buildcat`; verify exact commit
   `95897a55bc939a185766b7207e69d098997cdb1d` and tree
   `d65099256a598f7ce82d37ea303b81ff87088542`. Use an owned detached gate worktree.
   A moved ref is a QUESTION; do not silently substitute it. Earlier gate runs do not cover this candidate.
2. Record CPU, RAM, disk, load, tools, GPU, admission/coordinator identity and all relevant environment
   settings. Wait for existing Mac work to finish. Keep admission enabled; pick jobs from current resources.
3. Configure/build Release with viewer/GPU and original files available. Run canonical preflight/contracts
   and the full game-file selection through `tools/test/run_selected.py`, asset policy required.
   Use the preceding gate's explicit GPU/other skip policy, but audit every skip by name and reason.
   Only these two GPU skips are expected:
   - `anno_render_gpu_tests.play: GPU matches the CPU path on one scene`
   - `anno_render_gpu_tests.play: GPU matches the CPU path on one scene, on the Radeon iGPU`
   Other platform/opt-in prerequisites remain separately reported skips. Compare with ui-tabs-b's
   inventory; an unexplained new skip is not waived by a generic allow-skip option.
4. Run the warning check under the documented Mac method; name the actual compiler and all host
   adaptations. Native GCC/Clang versions that are unavailable stay NOT_RUN. Preserve first failures;
   rerun only refused tests alone per README, not a passing whole suite. Fix nothing in this packet.
5. Report exact head/tree, clean status before/after, commands, contracts separately from main counts,
   skips, failed assertions, refused/incomplete outcomes, sanitizer lines, times and SHA-256 receipts.
   Keep raw logs in ignored cache and hand off repo-relative evidence references. No host paths or
   original assets enter git. Stop with the report; review/landing belongs to the orchestrators.

The Mac orchestrator records the review in mac-lane.md. A failed candidate remains failed and goes
back to the Windows orchestrator; it does not authorize a repair campaign on the Mac.
