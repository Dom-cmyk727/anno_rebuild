# MAP-POP-IMPL-1 — accepted after bounded test/reference corrections

Accepted for the **headless population input and pure-calculation scope**. It
does not enable town growth in play. Two P2 findings below are corrected; no
blocking engine defect was found. The next assignment is the larger internally
reviewed MAP-POP-LIVE-1 bundle, not another readiness-only round.

## Findings and disposition

**[P2, closed] Fail when requested canary evidence cannot be written.** At
executor head `7939b958`, `population_canary_test.cpp:293–300` opened an ofstream
and returned success without checking it. Root ran the real case with a valid
output and with a file as its parent directory: both exited 0, but the latter
produced no artifact. The correction checks open, write/flush and explicit close.
The real invalid-path case now exits 1 at the intended open assertion; the legal
control exits 0 and produces independently verified vectors. Cheap stream controls
also cover an unopened stream and a flush failure.

**[P2, closed] Remove the original goods count from the numeric oracle.**
`house_kernel_ref.py:66,124` fixed the domain at 143. A valid input with goods 200
and infrastructure roles 200..240 returned exit 2 with an IndexError, although
the engine supports that domain. The reference now derives a sufficient domain
from referenced ids. The C++ vector harness also expands its test goods table.
The added R-wide case is hand-derived: E 128, stocks 396/198, lowest 148, mood 2,
residents 80. Both implementations pass all **71** vectors, including that case.

**Evidence attribution corrected.** `final/cheap-checks.txt` retains the failed
test-name preflight, not its later pass. Corrected preflights pass in both final
gate logs. The REPORTS wording now points to those logs; no failed evidence was
deleted and no gate rerun was needed to correct attribution.

## Exact identities and review scope

| Item | Pin |
|---|---|
| Comparison baseline | `2f876af5a44fc3395819134e79a9595a13d9f7d9` |
| Executor's full gate source, clean | `a518637549b30ef58425b26d9a1b0bbe86369883` |
| Executor's reported docs head | `7939b9583b6f8255a39011098802fe82bf4183c3` |
| Root's test/reference correction and focused gate source | `4438d7f2a9c31e4690da7623025595dbdba29d2e` |
| Accepted documentation/closeout head | `73e7ab1a129c4712747333d9f34a225be03f063f` |

Reviewer `/root` directly reviewed the change, surrounding importer/registry/
configuration/persistence paths, kernel, tests, references and decisive original
instruction excerpts. Full masks reach created players; optional zero is kept;
metadata inputs affect identity; group tables follow their index/order rules and
are bound to config roles; the kernel does not mutate World or dispatch effects.
The checked arithmetic, deferred effects and domain results match the released
scope. The original tick/mood excerpts independently support the loop ranges,
caps, Force checks, tavern contribution, flag precedence and removal bypass.

The original review was read-only and completed before root authored the bounded
correction. Independent reviewer `/root/review_evidence_workflow` then reviewed
only that four-file correction, derived R-wide separately and approved with no
findings. It did not source-review the executor's candidate on root's behalf.
Its separate bookkeeping work reconciled saved gate inventories and identities.
Earlier executor-side package approvals remain author-reported: no actual reviewer
ids or raw receipts were supplied. This direct review is the acceptance basis;
do not reconstruct an archive or restart broad reviews to fill that metadata gap.

## Validation and practical limits

The saved canonical full gates reconcile as reported: Debug **1121/1121**
(1108 C++ and 13 Python entries), affected ASan/UBSan **128/128** (125 C++ and
3 Python), with zero failures, empty tests, skips, incomplete outcomes or
sanitizer lines. The postgate executor commit is prose only.

At clean correction source `4438d7f2`, using canonical workflow `3b3c5555`, root
ran focused Debug and ASan/UBSan: **7 selected/7 executed in each**, plus the
separately required `house_kernel_reference_contract`. All outcome/error counts
are zero. Each real canary again evaluates 541 houses; Debug/ASan vector files
are byte-identical and each independently checks **541/541**. The bundled
reference/CLI contracts and stream oracle pass. Formatting uses clang-format
23.1.3. Linking retains the existing duplicate-libanno_sim.a warning; no new
compilation diagnostic appeared. Logs, inventories and copied before/after probes
are under `cache/map2-pop-impl1/orchestrator-close/`, ignored and Mac-local.

The 1121/128 gates are retained rather than repeated: the correction changes only
tests, the reference and one synthetic vector, not engine code or identities.
Stream **127**, PRNG timeline **4**, command wire unchanged. Corpus creation
hashes and recorded sessions are self-baselined; the independently encoded bytes,
mask/metadata/group comparisons and semantic assertions provide separate checks.
The original 70-vector result remains historical; the corrected suite has 71.

The bounded Create/import research establishes original import last=0 and no
zero-resident house among the 3283 loaded records. Its shipped below-64 argument
does not define behavior for arbitrary mods or crafted state. Player-built
timestamps and live effects remain open. Widened arithmetic is an approved
adaptation; equality with the original also requires defined original
intermediates, not merely inputs fitting their old storage fields.

Windows/MSVC, Linux/WSL warnings and sanitizers, original-runtime observations,
GPU and multiplayer scale remain NOT_RUN. Long opening and sailing canaries were
not repeated. No live House tick, resident walker, tax, unlock or population
transition is accepted by this packet. Windows' integration input remains exact
`0c401c8d084cfd9feb8aec846595291f87f22cce`; no newer Mac merge is assigned.

## Next delivery boundary

MAP-POP-LIVE-1 combines supplier/resident transactions, actual job 0x35,
House lifecycle effects and their activation/progression checks. Independent
package reviews unlock the next component automatically; the orchestrator reviews
the combined result once. Evidence-only work is front-loaded and parallelized
where useful, not substituted for the assigned implementation. Activation is
conditional on all reachable effects and existing-policy decisions being resolved;
otherwise retain the no-op and report that activation as incomplete while
delivering independent components. Do not install numeric-only live ticking.

Independent document reviewer `/root/review_windows_dispatch` approved this
root-authored release record and the L0–L5 dispatch with no findings. Its scope
was task coherence, decisions/proof boundaries and preservation of Windows work,
not source or raw-gate review. It checked all seven W4 sections, the Windows
dispatch row and owner-playtest updates against the saved `3b3c5555` board;
those instructions are unchanged.
