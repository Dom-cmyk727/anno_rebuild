# TOWN-START-SETUP-1 — code and local evidence reviewed; CI pending

**Disposition: no blocking code/local-evidence finding; acceptance awaits exact-head CI.**
Reviewer: Mac orchestrator (Orchestrator II Mac), independent read-only review.
Reviewed head `ad360af024543a71e083dc3de714b7f48f354e63`, tree
`ea620a8889e2bc8e0512370fde6de6db8210f462`; tested code
`e71f2dc0294d1c578e044bc0310972d83144af9b`, base
`b5c0ada209d94a0082bc88d793c917c7222e6a21`.
The final descendant adds report/evidence docs only. CI push run
[44209875238](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44209875238)
was still in progress when checked; no hosted success is claimed yet.

## Code and discriminating tests

Both ship-start entry points append an optional ExtensionContext with empty default. The installer
passes it to the loader and the loader to World::create. Existing arguments/callers retain their
meaning. World validates the context before constructing state and copies it into the new world;
there is no stored reference lifetime problem, setter or duplicate validation policy. Install still
replaces the old target only after the whole load succeeds. No scheduler/default/tag/version change.

Four original-candidate tests exercise both public entry points, omitted-versus-empty saved-byte/hash
identity, enabled fisher and native reload, and invalid enabled=2 refusal. The invalid-install test
first advances the old target, then verifies its exact bytes/hash survive refusal. The reviewer read
both native mutant outputs: dropping either forwarding edge fails the two intended enabled checks
while the default control executes and passes. This is an assertion-level kill, not a crash/build error.
The source was restored and rebuilt before gates; the reader whitespace correction reused the first
native result and did not erase a failure or justify an extra run.

## Verified receipts

| Gate | Selected | Executed passes | Failed / empty / incomplete | Skips | Sanitizer lines |
|---|---:|---:|---|---:|---:|
| Focused Debug | 30 | 30 | 0 / 0 / 0 | 0 | 0 |
| Focused ASan/UBSan | 30 | 30 | 0 / 0 / 0 | 0 | 0 |
| Normal full pins check, Debug | 1709 | 1691 | 0 / 0 / 0 | 18 opt-in | 0 |

Required contracts were separate 1/1 passes for each gate, not added to the table's counts.
The canonical reader independently reconciled all three saved logs with their inventories.
All 18 indexed raw-artifact hashes/sizes verify. Pins output says zero moved pins and zero fixture
byte changes, full scope, no regeneration. No refusal or whole-gate rerun was used.

The available warning check exited 0: independently reconciled 1,006 project compile invocations
and two rejected shadow controls. Actual compiler Apple Clang 21 for both script labels, with disclosed
Mac launch adaptations; native GCC 13/Clang 18 remain NOT_RUN. Formatting used 23.1.3. Duplicate-library
link warnings remain disclosed. Unsupported LeakSanitizer and target-host original-file lanes were not run.

Evidence: `docs/orchestration/evidence/town-start-setup-1/{summary.md,sha256.json}`;
raw receipts in the executor's `cache/town-start-setup-1/`. This review reran readers/hashes only,
not builds, tests or the historical 5,184.520-second pins campaign. The checkout was clean at inspection.

## Release boundary and efficient next packet

When exact-head CI passes, record acceptance and release TOWN-START-1 on this accepted head (or a
landed equivalent). It may be a local stack: no main landing or owner relay is required to begin the
measurement after review. The Windows orchestrator still owns final stacking and target-host landing.

For the next measurement-only packet, reuse this full pins result if the delta is confined to test
harness/registration and docs and leaves production, rules, fixtures and pins byte-identical. Verify
that condition explicitly and run the new/affected tests through the canonical gate. Do not repeat
the full unchanged pins campaign just to demonstrate that adding an opt-in measurement changes no
gameplay. A real production or identity change instead stops for scope/gate reassessment.
