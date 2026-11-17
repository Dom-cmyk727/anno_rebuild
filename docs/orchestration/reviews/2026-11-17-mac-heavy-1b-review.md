# MAC-HEAVY-1b — ACCEPTED, publication correction only

**Reviewer:** Mac orchestrator, Orchestrator II Mac; independent read-only handover review.
**Reviewed head:** `8c7155080fbe3e4544f4d4908518859b8e80c4fe`, tree
`f3336f4b35b86f870b916da18c87e5d044a6bb5c`, branch `task/MAC-HEAVY-1b`.
**Base and historical tested source:** `4dfdd9b7ce51b9ad04970da0b247294345f05ac7`.
**Hosted CI:** [44193521811](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44193521811),
push event, exact reviewed head, completed/success. The changes/gate jobs passed; native, format,
lint and Rust build/test jobs were docs-skipped. This CI establishes no new native test coverage.

## Independent checks

- Clean checkout, one new commit from the stated base. Only one REPORTS entry and evidence docs
  changed; no engine, test, tool, data, fixture or identity change. The old evidence commit is not
  an ancestor introduced by this packet.
- Fourteen committed evidence files total **60,752 bytes**, well below 1 MiB. Atlas TSV, verbose
  outcomes/matrices and generated registries are absent from the sole new commit.
- Scanned changed evidence and the added report text for absolute host paths. The public summaries
  use relative locations and documented external-root placeholders; no host path found.
- Recomputed hashes and sizes of **all 1,719 preserved files**. Compared all **95 original publication
  files** directly with their Git blobs at `ce35bb7b`; all copies are byte-identical.
- Verified **23 raw-artifact** and **13 publication-file** entries in the SHA index. Raw bytes and
  transformed summary identities remain distinct. The index SHA-256 is
  `433389ca5cf26a787a0ae043777e820bc7a95281a4f0ad14bedc1e0eaabeb4a3`.
- Compared all **81 historical COUNTS/VERDICT/GATE_END receipt lines** against the old publication:
  identical. Read README, methods, findings, acceptance, numerical summaries and report for scope.

## Verdict and limits

No blocking finding. This accepts the two requested publication corrections: path sanitization and
small committed evidence with verifiable private raw artifacts. It does not turn historical failures
into passes, validate a new executable, or independently repeat the original heavy campaign.

MH-001 remains an assertion without sanitizer diagnostics; the isolated pass does not erase it.
MH-002 through MH-005 retain their order observations without claiming a proven cause. MH-006 still
stops before transport measurement. The smaller-than-requested leather workloads remain explicit.
Only 11 maps per mode/build reach invariant checkpoints; 39 loader refusals do not count as coverage.
The 100 matching native output tuples include refusals and do not establish complete-state equality.
Windows corpus/atlas comparison and unsupported LeakSanitizer remain NOT_RUN.

The old public branch and local evidence are preserved. Deletion/history cleanup is the owner's
decision, not part of acceptance. Raw logs stay local; the Windows orchestrator must arrange any
needed transfer without publishing original content or host paths.

**Owner/landing handoff:** MAC-HEAVY-1b; head `8c7155080fbe3e4544f4d4908518859b8e80c4fe`;
CI 44193521811; this review. The Windows orchestrator owns stacking/landing; no main push here.

**Next release:** TOWN-START-1, `docs/orchestration/tasks/town-start-1.md` on `orch/mac-lane`.
Use freshly fetched main as its recorded base, not the failed UI candidate and not this evidence
branch. Measurement through public commands only; no owner default/map decision is implied.
