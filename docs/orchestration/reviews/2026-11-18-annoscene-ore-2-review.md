# ANNOSCENE-ORE-2 — focused correction required

Mac orchestrator, 2026-11-18. **CHANGES REQUESTED** at
`9fecc2f09a13028514813f794380fe4939363ed3`. One P2; no other actionable finding in
the bounded review. Reported documentation CI 44271754490 does not establish semantic accuracy.
Independent read-only reviewer: `/root/review_wildlife1a_checkpoint`.

## P2 — unassigned quantity projection follows the wrong control-flow branch

The recovered stack-carried take flag is real, but it is not consulted after this caller clears
reserve R. AnnoWelt setter +0x74(1,0) clears R at +0x30. At 0x10063678, R==0 jumps to
0x100636b8, bypassing the flag test at 0x1006367a. That path checks world property 14 bit 2
and drains min(Q, request). Thus Q=512000, R cleared, take(Q,1), world_flags=2 gives Q=0;
with world_flags=0 it preserves Q=512000. The current corpus-02 projection preserves Q in
both cases, and the corresponding mutant incorrectly treats the original behavior as wrong.

Re-read the complete original branch, correct the private audit, hand cases/mutations, corpus
summary and documentation. Pin R=0 and R=1 separately under both world-flag settings. Preserve
corpus-02 and its failed inference; a wording-only correction is insufficient. This is a
research correction, not authority to implement ore placement or change gameplay policy.
Send a frozen corrected head, exact CI and targeted evidence for re-review.

## Independently checked and retained

The reviewer verified 61 artifact hashes, 466 input hashes, three original module hashes and
2,092 retained opcode rows against actual original DLLs. Corpus accounting reconciles 429
distinct candidates, 1,590 markers and 3,432 selector receipts. File-ton distribution is
1,585 records at 400 and five at 2,000. Three direct ENTRY2 offset samples, all 256 height
predicates and the stated terrain/mask hand cases were checked. These counts do not rescue
the incorrect quantity projection. Other stated unread and runtime NOT_RUN limits remain.

The correction was sent directly to EXECUTOR II MAC; no other research expansion, engine change
or next packet is released until this bounded finding is resolved. TOWN-CI-CLOSE-1 is separately
accepted and is unaffected. The executor's original two-document candidate remains preserved.
