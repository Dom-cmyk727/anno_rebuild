# WILDLIFE-LOAD-1 — decode original wildlife spawn records

**RELEASED by the Mac orchestrator after independent dispatch review.**
One bounded gameplay prerequisite, about 2–4 hours. EXECUTOR II MAC owns it and
reports directly back to the Mac orchestrator. This is the first import slice
before WILDLIFE-1b's persistent state and materialization, not live wildlife.

## Intake and ownership

Create `task/WILDLIFE-LOAD-1` from accepted
`2f9e4c0bf84cf09998a43c7ff8098fa1522f3a4e` in an owned clean checkout.
Preserve KBEWOHN, DOCS-SIM and WILDLIFE-1a branches and evidence. This independent
formats task needs none of their implementation: read the named notes by ref,
without merging or rebasing pending candidates. KBEWOHN CI continues separately;
its pending status does not authorize calling it accepted. Ordinary-push only
your task branch at checkpoints. Never push main or force-push.

Read CLAUDE.md, PLAN.md sections 0.1–0.3, TASKS.md owner rules/section1,
`docs/testing/evidence-contract.md`, `docs/testing/README.md`, and this packet.
Then read:

- `docs/re/wildlife.md` section3 and the section11 corrections; its source
  addresses must be checked against the original bytes, not just repeated.
- `docs/formats/insel.md`, `src/formats/include/anno/formats/insel.hpp`,
  `src/formats/src/insel.cpp`, `src/formats/tests/insel_corpus_test.cpp`,
  the existing chunk/sze tests and `src/formats/CMakeLists.txt`.
- `tools/insel.py`, `tools/chunks.py`, `tools/wildlife_ref.py` and its contract.
- ADR0006 and `docs/sim/state.md` sections9.7–9.8; inspect current scenario import and
  draw-site interfaces read-only to identify the next integration boundary.
- `origin/task/WILDLIFE-1a:docs/orchestration/evidence/wildlife-1a.md` if present;
  otherwise locate its evidence from the pinned review at756e008b. Do not import
  the helpers or treat their inherited CI as closed.

## L0 — finite contract before code

Write an acceptance table and verify the HIRSCH loader
`AnnoWelt.dll 0x100946c0–0x10094873`, with the module SHA. Its records are eight
bytes: local x/y u16, raw bytes4/5, goods u16. Byte4 bit0 controls later jitter;
byte5 is not an animal kind. Keep unknown bits/bytes exact. The original consumes
`payload_size >>3` records; distinguish that behavior from any stricter decoder
validation. Missing HIRSCH delegates elsewhere; present-empty is a different
input. File order differs from the later prepended per-chunk traversal.

Choose a small explicit **formats-layer** API consistent with existing parsers:
absence distinguishable from present-empty; records in file order; all eight
bytes representable and round-trippable. Do not make parsed raw records into
engine figures. Do not infer species, initialized figure ids, coordinates beyond
the stored values, world mode or draw results. A strict malformed-length error
in the new accessor is a documented parser safety policy, not original gameplay;
it must not change unrelated island loading which does not call that accessor.
Check duplicate HIRSCH handling against the existing first-record accessor and
original lookup; preserve bytes/order and document unproven original behavior.

## L1 — typed decoder and synthetic evidence

Implement the smallest typed decoder/encoder and island accessor in formats,
following Result<T,E>, explicit little-endian fields and existing ownership/lifetime
patterns. Keep raw flags and byte5, rather than losing information through a bool.
File widths belong here; add no engine population, species or map-size limit.
No changes under sim, sim_load, app, renderer, test_support or tools/test.

Before or alongside implementation, hand-write synthetic bytes with distinct
x/y/goods bytes and nonzero unknown bits. Cover absent, empty, one and multiple
records, duplicate lookup behavior, truncated/trailing payload and coordinate
extremes. Round-trip must reproduce every byte; parsing must also assert each
field independently so two mutually wrong reader/writer functions cannot pass.
Add a small independent Python transcription/reference using the existing tool
style; reject zero executed vectors and unknown records. Do not generate expected
values from C++. Keep original data and per-record decoded listings out of git.

## L2 — real corpus and negative controls

Exercise the accessor on the available original `.sze` and `.scp` corpus. Record
actual file/island/absent/empty/record/malformed counts, not assumed totals. Compare
C++ and Python results by aggregate digest or private per-record audit. Preserve
original file hashes in private evidence; public summaries contain counts and
hashes only. If malformed payloads exist, report them without weakening the
contract or silently discarding bytes. An asset skip is not corpus coverage.

Before final gates, show legal controls pass and targeted broken decoding fails
at the intended assertion: swap x/y, discard raw byte5 or flag bits, and collapse
absent into empty. Distinguish an assertion kill from compilation failure/crash.
Use owned scratch builds and restore/rebuild before the frozen gate. No new
mutation framework or broad campaign.

## L3 — integration handoff, not premature World wiring

Document the smallest next persistent-state unit: which loader owns the raw
records, when world mode and original island time become available, where load
jitter would consume world draws, and how file order becomes chunk-list order.
Trace enough callers to distinguish established ordering from unknowns; unresolved global order is a handoff item, not a new transitive lifecycle investigation. List
missing/empty, world-mode override, load-time draws and materialization as separate
requirements; do not defer jitter until first tick and claim equivalence.

No state/tag allocation, Rules switch, PRNG site/timeline change, native payload,
fauna sweep, animal creation, hunter/trapper behavior or guessed float route is
authorized here. Propose the exact section/tag needs in the handoff for coordination
before the next implementation. This packet does not require resolving the entire
animal callback graph. Stop a dependent step on a real policy question and report
options to the orchestrator; continue independent decoding work.

## L4 — gates, review and stop

Run canonical focused Debug and ASan/UBSan formats/accessor tests, required new
reference contract, and affected original corpus cases with assets set. Format
changed C++, run available focused warnings, and check the final diff. No long
sailing, rendering, full-sim or unrelated corpus gate is needed: justify reuse by
showing the new accessor has no production caller and changes no existing parsed
result, registry, serialized state or random draw. If that premise changes, stop
before expanding scope. Fixture/pin/state/default changes must be zero.

Freeze source for gates; preserve first failures and truthful selected/executed/
failed/empty/skipped/incomplete counts. Record source/tool/log hashes and exact
commands, using repo-relative public paths. Report NOT_RUN platforms plainly.
Committed evidence stays compact; raw original records/disassembly remain ignored.

Ordinary-push the clean task branch, inspect exact-head hosted CI and send
READY FOR REVIEW directly to the Mac orchestrator: full head/tree, CI link/status,
acceptance table, Decisions and reasoning, evidence paths, any unresolved ordering
and the next integration boundary. The orchestrator supplies independent read-only
review of source, primary evidence, tests and receipts; no executor reviewer agent.
Fix scoped findings and return for focused re-review, without repeating unaffected
gates. Do not self-accept or begin World integration. At six hours, return a clean
bounded checkpoint and precise remaining work rather than widening the packet.
