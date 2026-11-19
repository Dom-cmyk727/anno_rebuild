# Mac lane dispatch and review ledger

Owned by the Mac orchestrator on `orch/mac-lane`, created from `origin/orch/board-next`
`8eae12d1` on 2026-11-17. Authority: [lane handover](mac-lane-2026-11-17.md).
All execution goes to **EXECUTOR II MAC**, one packet at a time. The Windows orchestrator
owns STATUS.md, TASKS.md, candidate stacking and landing; this branch never pushes main.

Owner instruction, 2026-11-17: the executor sends results directly to the Mac orchestrator
for review; keep the overnight loop moving toward gameplay rather than prolonged detail work.
After each frozen report the orchestrator reviews it, records a verdict and releases the next
bounded packet or a focused correction. No owner relay is needed for ordinary results.
Owner policy decisions still go to the owner; silence is not approval. After the gate and short
evidence cleanup, prioritize TOWN-START-1 and the listed wildlife critical path.

| Wave / packet | Status | Head / tree | CI | Review |
|---|---|---|---|---|
| 0.1 [GATE-RUN ui-info-2d-buildcat](tasks/gate-run-ui-info-2d-buildcat.md) | CLOSED with FAIL: two no-frame cases; warning check passed; candidate NOT ACCEPTED | Candidate `95897a55bc939a185766b7207e69d098997cdb1d`; tree `d65099256a598f7ce82d37ea303b81ff87088542` | Exact-head lookup: no run found | [Final failure review](reviews/2026-11-17-gate-ui-info-2d-buildcat-review.md); ten named-adapter GPU skips now reconciled; two window failures remain |
| 0.2 [MAC-HEAVY-1b](tasks/mac-heavy-1b.md) | ACCEPTED: publication correction only; Windows landing pending | `8c7155080fbe3e4544f4d4908518859b8e80c4fe` | [44193521811](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44193521811), exact-head docs gate green | [Acceptance](reviews/2026-11-17-mac-heavy-1b-review.md) |
| 1.1 prerequisite [TOWN-START-SETUP-1](tasks/town-start-setup-1.md) | ACCEPTED; Windows landing pending | Head `ad360af024543a71e083dc3de714b7f48f354e63`; tested code `e71f2dc0` | [44209875238](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44209875238), all 12 jobs success at exact head | [Acceptance](reviews/2026-11-18-town-start-setup-1-review.md) |
| 1.1 [TOWN-START-1](tasks/town-start-1.md) | ACCEPTED bounded measurement on repaired stack; landing pending | Accepted `9edb151c414246d306194a703db71475affbdad4`; preserved report `156badab`, measured `f36c22fa` | [44266114323](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44266114323), all 12 jobs pass | [Acceptance and preserved failures](reviews/2026-11-18-town-start-1-review.md) |
| 1.2 [WILDLIFE-1a](tasks/wildlife-1a.md) | LOCAL REVIEW REOPENED for load-base bound; inherited CI closure pending; no World integration | `756e008b8027cf9186b4e79e3ae2e76b1dab4c29`, pure helpers only | [44262049576](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44262049576); not claimed green | [Conditional local review](reviews/2026-11-18-wildlife-1a-review.md) |
| 1.3 [NET-ADR-1](tasks/net-adr-1.md) | ACCEPTED documentation; owner decisions remain open | `cf7109fe14fa729cbb0f1c775fd243cd06b34c26` | [44264630010](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44264630010), exact-head docs CI green | [Acceptance](reviews/2026-11-18-net-adr-1-review.md) |
| 1.4 [MIL-RE-1](tasks/mil-re-1.md) | ACCEPTED bounded static research; no combat implementation | `20edc775e12a4e8cd73d703d80b8ea0e4bb4e02b` | [44268712248](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44268712248), exact-head docs CI green | [Acceptance](reviews/2026-11-18-mil-re-1-review.md) |
| 1.5 [ANNOSCENE-ORE-2](tasks/annoscene-ore-2.md) | ACCEPTED bounded research after P2 correction; no engine implementation | `b12d22b39caee90429aef77b52659aa2bfedb38e` | [44273143955](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44273143955), exact-head docs CI green | [Acceptance and preserved finding](reviews/2026-11-18-annoscene-ore-2-review.md) |
| 1.6 [OQ-562-1](tasks/oq-562-1.md) | ACCEPTED bounded headless causal diagnosis after focused CI correction | `b7046e5e279fa979d83079feb56dc83a660f32ef` | [44282873819](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44282873819), all 12 jobs success at exact head | [Acceptance and preserved failure](reviews/2026-11-18-oq-562-1-review.md) |
| 1.7 [SIM-LIMITS-1](tasks/sim-limits-1.md) | ACCEPTED compatibility maintenance; behavior and bytes unchanged | `2f9e4c0bf84cf09998a43c7ff8098fa1522f3a4e`; code `33871080` | [44298551123](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44298551123), all 12 jobs success at exact head | [Acceptance](reviews/2026-11-18-sim-limits-1-review.md) |
| 1.8 [DOCS-SIM-1](tasks/docs-sim-1.md) | ACCEPTED bounded inventory/tools scope after focused corrections | `1fab3ca9ec80cb69a119ea7201e4a7332bd1ddf0` | [44321152729](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44321152729), all 12 jobs success; first failure preserved | [Independent review](reviews/2026-11-18-docs-sim-1-review.md) |
| 2.6 [KBEWOHN-1](tasks/kbewohn-1.md) | LOCAL REVIEW APPROVED; final CI macOS runner unavailable, retry pending | `6fb7dbb359288289c70ac10161579b5f7666ae11`; tested source `3c20430b` | [44336688872](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44336688872), terminal FAIL (no macOS runner); source44335070081 PASS | [Independent review](reviews/2026-11-18-kbewohn-1-review.md) |
| Wildlife import [WILDLIFE-LOAD-1](tasks/wildlife-load-1.md) | LOCAL REVIEW APPROVED; macOS C++ CI never obtained runner | `f1e245d2769306451d4c2079aadf6a64be93defd` | [44344197686](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44344197686), FAIL: 10 successful jobs, 1 cancelled runner, failed aggregate gate | [Independent review](reviews/2026-11-18-wildlife-load-1-review.md) |
| Wildlife correction [WILDLIFE-LOAD-FIX-1](tasks/wildlife-load-fix-1.md) | LOCAL REVIEW APPROVED; CI blocked by no hosted macOS runner | `533ee3fc96a560f1c3189f3a4d32ce113abff65a` | [44354548420](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44354548420), 10 success, build-macos cancelled, aggregate failed | [Review](reviews/2026-11-18-wildlife-load-fix-1-review.md) |
| Wildlife initialization [WILDLIFE-INIT-1](tasks/wildlife-init-1.md) | ACCEPTED bounded research/reference; landing pending | `97d893af9d78e6662dd8859acc3a905f71c80402` | [44359989371](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44359989371), all12 successful | [Review](reviews/2026-11-18-wildlife-init-1-review.md) |
| Wildlife resource [WILDLIFE-RESOURCE-1](tasks/wildlife-resource-1.md) | ACCEPTED after FIX-1; landing pending | `0627532f330dc898b80941ea31d9dafe87980aa4` | [44372186862](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44372186862), all12 successful | [Review](reviews/2026-11-18-wildlife-resource-1-review.md) |
| Wildlife composition [WILDLIFE-PREFIX-1](tasks/wildlife-prefix-1.md) | LOCAL REVIEW APPROVED; two macOS CI jobs cancelled without runner | `1441183a74be4745415c23c399f80648bf886314` | [44385455810](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44385455810), terminal FAIL:9 success,2 no-runner cancellations, aggregate failure | [Review](reviews/2026-11-18-wildlife-prefix-1-review.md) |
| Wildlife stone [WILDLIFE-STONE-1](tasks/wildlife-stone-1.md) | ACCEPTED bounded import; Windows landing pending | `18473cf4b1d44cadfa564636f56b889bdcabe146`; source `f3e18bdf` | [44390015763](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44390015763), all12 jobs successful | [Review](reviews/2026-11-18-wildlife-stone-1-review.md) |
| Wildlife cliff [WILDLIFE-CLIFF-1](tasks/wildlife-cliff-1.md) | ACCEPTED bounded projection; Windows landing pending | `11bc3be60acc71a4c9fbf03dff077e908b4c8204`; source `eb3ea560` | [44417567937](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44417567937), all12 jobs successful | [Review](reviews/2026-11-19-wildlife-cliff-1-review.md) |
| Wildlife rock [WILDLIFE-ROCK-1](tasks/wildlife-rock-1.md) | ACCEPTED after FIX-1; bounded projection | `6c6554a23a7e637f2c88fb360cf050aff643c613`; source `26899502` | [44422288006](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44422288006), all12 jobs successful | [Review](reviews/2026-11-19-wildlife-rock-1-review.md) |
| Wildlife rock correction [WILDLIFE-ROCK-FIX-1](tasks/wildlife-rock-fix-1.md) | ACCEPTED; P2 closed, prose-only | `6c6554a2` | Frozen-source gates reused; exact CI all12 successful | [Focused review](reviews/2026-11-19-wildlife-rock-1-review.md) |
| Wildlife fertility header [WILDLIFE-FERTILITY-1](tasks/wildlife-fertility-1.md) | ACCEPTED; bounded transient header | `81026d1b451955d9167649cb3b8b2e3f5648d120`; source `12586595` | [44425779949](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44425779949), all12 jobs successful | [Review](reviews/2026-11-19-wildlife-fertility-1-review.md) |
| Wildlife BERGWERK2 [WILDLIFE-BERGWERK-1](tasks/wildlife-bergwerk-1.md) | ACCEPTED; bounded transient container | `03fff3c6ffc2e3c0bbd52f5e8f24c0fc083afb94`; source `7e4ac88f` | [44428107745](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44428107745), all12 jobs successful | [Review](reviews/2026-11-19-wildlife-bergwerk-1-review.md) |
| Wildlife occurrences [WILDLIFE-OCCUR-1](tasks/wildlife-occur-1.md) | ACCEPTED; bounded occurrence slice | `5bcd50c5b22be0d134fb63473680f321a7870ba4`; source `8c2de16c` | [44432068823](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44432068823), all12 jobs successful | [Review](reviews/2026-11-19-wildlife-occur-1-review.md) |
| Wildlife ANBAU header [WILDLIFE-ANBAU-1](tasks/wildlife-anbau-1.md) | ACCEPTED; bounded transient header | `02111ea2139cfddf1e4018bcc229fd55d918056e`; source `d645071a` | [44436355753](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44436355753), all12 jobs successful | [Review](reviews/2026-11-19-wildlife-anbau-1-review.md) |
| Wildlife DISCOVER header [WILDLIFE-DISCOVER-1](tasks/wildlife-discover-1.md) | ACCEPTED; bounded scalar import | `8d97b6956189faf9483f6d9944b42676362d1c7d`; source `ddab0993` | [44438472504](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44438472504), all12 jobs successful | [Review](reviews/2026-11-19-wildlife-discover-1-review.md) |
| Wildlife KONTORID header [WILDLIFE-KONTORID-1](tasks/wildlife-kontorid-1.md) | ACCEPTED; bounded counter import | `eaa61980162b1552eb7acee1553ce11fb7a8869c`; source `c199cf1f` | [44439685272](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44439685272), all12 jobs successful | [Review](reviews/2026-11-19-wildlife-kontorid-1-review.md) |
| Wildlife PROD2 boundary [WILDLIFE-PROD2-1](tasks/wildlife-prod2-1.md) | ACCEPTED; bounded empty-block path | `d7a21ac4a41975e5dd3a2afba8af84a430ed4e17`; source `8d65f385` | [44441246351](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44441246351), all12 jobs successful | [Review](reviews/2026-11-19-wildlife-prod2-1-review.md) |
| Wildlife EVENTINSEL boundary [WILDLIFE-EVENTINSEL-1](tasks/wildlife-eventinsel-1.md) | ACCEPTED; bounded empty-header/local-record scope | `218b838eb543782c60629f51288005fdd699c492`; source `c6c106a4` | [44443163952](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44443163952), all12 jobs successful | [Review](reviews/2026-11-19-wildlife-eventinsel-1-review.md) |
| Wildlife post-island contract [WILDLIFE-POSTLOAD-1](tasks/wildlife-postload-1.md) | ACCEPTED; corrected bounded research | `1628ba18a5b6eefb55684afc3091ba3be9fa10c8`; research source `09614995` | [44447642251](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44447642251), all12 jobs successful | [Focused review](reviews/2026-11-19-wildlife-postload-1-review.md) |
| Wildlife graphics leaf [WILDLIFE-GFX-FINALIZE-1](tasks/wildlife-gfx-finalize-1.md) | ACCEPTED; isolated UNWIRED graphics leaf | `ad2a1a23bbc4fe877c18f0e7bf8f4ce0180270b5`; source `e9256f22` | [44449632964](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44449632964), all12 jobs successful | [Review](reviews/2026-11-19-wildlife-gfx-finalize-1-review.md) |
| Wildlife catalogue inputs [WILDLIFE-POSTLOAD-INPUTS-1](tasks/wildlife-postload-inputs-1.md) | ACCEPTED; bounded first-call input contract | `263958edb83daa290aeb674f5b2b30c0655c5bb4`; research source `625db996` | [44452465008](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44452465008), all12 jobs successful | [Review](reviews/2026-11-19-wildlife-postload-inputs-1-review.md) |
| Wildlife first-call fill [WILDLIFE-POSTLOAD-FILL-1](tasks/wildlife-postload-fill-1.md) | ACCEPTED; isolated UNWIRED fill; evidence prose corrected in NAV-INPUTS | `889143ba1485a2fed72c03bd648c9c39878c4f06`; source `db2240a6` | [44456284511](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44456284511), all 12 jobs successful | [Review](reviews/2026-11-19-wildlife-postload-fill-1-review.md) |
| Wildlife navigation inputs [WILDLIFE-NAV-INPUTS-1](tasks/wildlife-nav-inputs-1.md) | ACCEPTED research; production compatibility decision pending | `74d50bfeedf1a48ab7e140cc66e36859e0d0a9c3`; production base `889143ba` | [44458725953](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44458725953), all 12 jobs successful | [Review](reviews/2026-11-19-wildlife-nav-inputs-1-review.md) |
| Wildlife secondary raster [WILDLIFE-RASTER-CONTRACT-1](tasks/wildlife-raster-contract-1.md) | ACCEPTED conditional research; runtime numeric qualification remains | `8217e1879aead7fa3b451c155b060f0a05405874`; production base `889143ba` | [44461780207](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44461780207), all 12 jobs successful | [Review](reviews/2026-11-19-wildlife-raster-contract-1-review.md) |
| Wildlife load-thread numerics [WILDLIFE-RASTER-FP-1](tasks/wildlife-raster-fp-1.md) | ACCEPTED research/capture card; Windows observation pending | `aec8bcc8fc1a35230dffba91c8e8b34c35f31bf3`; production base `889143ba` | [44464134541](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44464134541), all 12 jobs successful | [Review and Windows intake](reviews/2026-11-19-wildlife-raster-fp-1-review.md) |
| Wildlife ground preparation [WILDLIFE-PREPROCESS-CONTRACT-1](tasks/wildlife-preprocess-contract-1.md) | ACCEPTED bounded preprocessing contract | `54798a4b5f5bc8a2c65874ea375441b7cca9e21f`; production base `889143ba` | [44467047682](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44467047682), all 12 jobs successful | [Review](reviews/2026-11-19-wildlife-preprocess-contract-1-review.md) |
| Wildlife numerical preparation [WILDLIFE-GROUND-PREPARE-1](tasks/wildlife-ground-prepare-1.md) | ACCEPTED; isolated UNWIRED numerical leaf | `5bdfd92ac01a70656a718cea4f7c7405445b41d5`; tested `92c17db` | [44469519552](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44469519552), all 12 jobs successful | [Review](reviews/2026-11-19-wildlife-ground-prepare-1-review.md) |
| Wildlife growth source [WILDLIFE-GROWTH-LOAD-1](tasks/wildlife-growth-load-1.md) | ACCEPTED; focused P2 closed | `fe4355fd719fad0ecafbe9322788f4b7d885a7af`; tested `464e4935` | [44478146499](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44478146499), all 12 successful; earlier failed run preserved | [Review](reviews/2026-11-19-wildlife-growth-load-1-review.md) |
| Wildlife climate kernel [WILDLIFE-KERNEL-LOAD-1](tasks/wildlife-kernel-load-1.md) | ACCEPTED; UNWIRED owned reader | `c9259c1612863def493dcb47f455a018b8e110c0`; tested `fb6c7f52` | [44483388631](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44483388631), all 12 successful | [Review](reviews/2026-11-19-wildlife-kernel-load-1-review.md) |
| Candidate 10 display [MAC-GPU-WINDOW-1](tasks/mac-gpu-window-1.md) | CLOSED, diagnostic approved; candidate remains FAIL | Unchanged `95897a55bc939a185766b7207e69d098997cdb1d` | 4/4 post-script failures; 7287 successful acquire/present returns; original no-frame cause unresolved | [Review / Windows handoff](reviews/2026-11-19-mac-gpu-window-1-review.md) |
| Wildlife object facts [WILDLIFE-GROUND-FACTS-1](tasks/wildlife-ground-facts-1.md) | ACCEPTED; bounded owned metadata binding | `4054d93aa42f777c49c9c4dd035bd3cdce23f64b`; tested `f443ed12` | [44490451270](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44490451270), all12 jobs successful | [Review](reviews/2026-11-19-wildlife-ground-facts-1-review.md) |
| Wildlife source provenance [WILDLIFE-PREP-PROVENANCE-1](tasks/wildlife-prep-provenance-1.md) | ACCEPTED bounded contract; P3 closed | `0a30d44a6214b2c3e1d6859fb5a65b7c1c3b4719`; engine `4054d93a` | [44494353183](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44494353183), all12 jobs successful | [Review](reviews/2026-11-19-wildlife-prep-provenance-1-review.md) |
| Wildlife first-call composition [WILDLIFE-FIRST-CALL-1](tasks/wildlife-first-call-1.md) | ACCEPTED; isolated source-to-fill | `7d2df0fc5c94786372960000b1c7fe8d3a28dee2`; tested `a6f7527d` | [44496760869](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44496760869), all12 jobs successful | [Review](reviews/2026-11-19-wildlife-first-call-1-review.md) |
| Animal construction callbacks [WILDLIFE-CREATE-CLOSURE-1](tasks/wildlife-create-closure-1.md) | ACCEPTED; bounded creation contract | `252ed66cdb5e61818f459039822d2dd8167cbd04` | [44500172507](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44500172507), all12 jobs successful | [Review](reviews/2026-11-19-wildlife-create-closure-1-review.md) |
| Animal creation effects [WILDLIFE-CREATE-EFFECTS-1](tasks/wildlife-create-effects-1.md) | ACCEPTED; bounded owned creation effects | `25bab774208c9e610b43447e247d99cc1aba0195`; tested `37c144b1` | [44508278589](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44508278589), all 12 jobs successful | [Review](reviews/2026-11-19-wildlife-create-effects-1-review.md) |
| Animal end/release effects [WILDLIFE-RELEASE-EFFECTS-1](tasks/wildlife-release-effects-1.md) | ACCEPTED; P3 closed, bounded release effects | `901a8467f766825ae9683cb41dee33522a9aff39`; tested `33fe219c` | [44511432981](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44511432981), all 12 jobs successful | [Review](reviews/2026-11-19-wildlife-release-effects-1-review.md) |
| Animal species source [WILDLIFE-SPECIES-LOAD-1](tasks/wildlife-species-load-1.md) | ACCEPTED; bounded source-backed species plan | `f5f1c9fa7f0eca4d6263147797022ff1c5e6d771`; tested `4acf5545` | [44515114872](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44515114872), all 12 jobs successful | [Review](reviews/2026-11-19-wildlife-species-load-1-review.md) |
| Wildlife chunk planning [WILDLIFE-CHUNK-PLAN-1](tasks/wildlife-chunk-plan-1.md) | LOCAL SEMANTICS APPROVED; CI FAIL, correction released | `95d2eb0f4bf09c559413ef380fba3fd4281939ff`; tested `ef8fcb48` | [44522148816](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44522148816), MSVC build and Linux dependency setup fail | [Review](reviews/2026-11-19-wildlife-chunk-plan-1-review.md) |
| Wildlife materialization planning [WILDLIFE-MATERIALIZE-PLAN-1](tasks/wildlife-materialize-plan-1.md) | LOCAL SEMANTICS APPROVED; portability blocked | `1c027079fd0d431fae1613df839f1bde9c38d2bc`; tested `49c9ddb3` | [44528637255](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44528637255), pending | [Review](reviews/2026-11-19-wildlife-materialize-plan-1-review.md) |
| Wildlife planning portability [WILDLIFE-PLAN-CI-CLOSE-1](tasks/wildlife-plan-ci-close-1.md) | RELEASED; narrow typed-conversion repair | Base `1c027079fd0d431fae1613df839f1bde9c38d2bc` | Preserve old failures; exact-head MSVC proof required | Direct reportback to Mac orchestrator |
| Windows dual-edition navigation [WIN-NAV-EDITIONS-1](tasks/windows-nav-editions-1.md) | Owner-requested HANDOFF READY; Windows intake pending | Accepted diagnosis `74d50bfe`, production base `889143ba` | GOG and HE verification requested; no repair released | Owner forwards to Windows orchestrator |
| Parallel [TOWN-CI-CLOSE-1](tasks/town-ci-close-1.md) | ACCEPTED; isolated repair, main untouched | `9edb151c414246d306194a703db71475affbdad4`; tree `6dd647aac8dbe9e1335acd0659daa9c287ee99c1` | [44266114323](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44266114323), all 12 jobs pass | [Final repair review](reviews/2026-11-18-town-ci-close-1-review.md) |

## Review and reporting contract

An executor reports READY FOR REVIEW, not accepted. Acceptance requires a frozen reviewed head,
green hosted CI at that exact head, and a Mac orchestrator review file under `reviews/`.
Report accepted packets to the owner as **packet, full head, CI run/link, review path**, with
scope/NOT_RUN qualifications. The Windows orchestrator handles the landing gates and land.py.
For a read-only GATE-RUN, the candidate stays untouched and the receipt review names its exact tree.
Nothing in a queued packet selects a production map, enables living defaults or approves an owner policy.

## Open questions / owner decisions

OQ-800 through OQ-899 are reserved for this lane; none allocated yet. Next: OQ-800.
D-1 map choice and D-2 living defaults remain with the owner. TOWN-START-1 measures candidate A
with explicit test rules only. Any new policy question stops the dependent step and is sent to the owner.
Deleting or rewriting the old public MAC-HEAVY-1 branch is also an owner decision; this dispatch does neither.

Navigation arithmetic: independent NAV-INPUTS review confirms premature wrapping
and capping against the inspected GOG producer. The owner now requests Windows
verification against BOTH GOG and History Edition before deciding the correction.
[Detailed Windows handoff](tasks/windows-nav-editions-1.md) records that scope.
This reply is not repair or identity authorization. Saved NavMotion/replay may
be affected even without a layout change. No identity is allocated and no
production repair is released; the Mac source-binding work proceeds independently.

## Dispatch history (pending CI statements below are historical)

Wave 0 and the setup prerequisite are closed as recorded above. Town's bounded local measurement
is reviewed: 16 capped Pioneer houses / 128 residents, no Settler; exact duplicate traces and a
distinguishing food omission. CI acceptance remains blocked, including two unchanged base build
paths. The town-owned helper linkage correction awaits final-head GCC confirmation. Preserve that
branch and its evidence; no third opening or production repair is authorized in the town packet.

The historical ui-tabs-b PASS receipt is already closed; receiving it again does not interrupt or
replace the town task. Do not rerun either old gate. A newly assigned GATE-RUN still has priority.
Results go directly to this orchestrator for review. WILDLIFE-1a is released without waiting for
town landing: it is independent and starts from the named main pin. No owner relay is required.
This is a scheduling decision, not a CI waiver or acceptance. The landing orchestrator must receive
the freight/GCC Release and ciso646/Clang-ASan failures for separate triage. Never fold an unrelated
repair into wildlife or silently convert a red job into a pass.

Owner instruction, 2026-11-18: take care of those CI blockers in a separate package without
interrupting wildlife. The Mac orchestrator owns TOWN-CI-CLOSE-1 in a separate managed worktree;
the executor continues WILDLIFE-1a. Scope is the reproduced GCC vector-copy warning and the
upstream doctest C++20 header fix, plus validation and independent review. Further diagnostics
exposed a guarded statistics helper, two test-fixture access warnings, and a genuine Mine native
reader bug: an incoming presence flag was used to dereference a still-empty optional. The repair
keeps native bytes/identity unchanged. Hardened recovery passes 28/28; affected Debug and ASan
pass 56/56 at `283e8f9b`; the subsequent minimap test and CI diagnostic changes are separately
checked. GCC Release now gathers independent compiler errors in one pass and still fails on any
error; full build logs are retained. No main push, gameplay
change or automatic intake by wildlife. Town's own linkage correction now passes the final-head
GCC Debug build; its original failure remains preserved.

WILDLIFE-1a is now frozen and independently locally reviewed without blocking findings.
NET-ADR-1 is released as the next bounded, independent documentation packet; the executor
preserves wildlife and does not take pending CI repairs or wire World. This is not an inherited
CI waiver. The Mac orchestrator retains CI closure; network policy remains the owner's decision.

NET-ADR-1 is accepted as documentation at exact green head cf7109fe. Its eight owner answers
remain blank and network implementation stays gated. MIL-RE-1 is released next, independently
of pending CI closure. The CI keep-going run exposed seven further test-only compiler failures;
the reviewed correction compiles all seven under x86 GCC Release warning flags. Focused real-file
and sanitizer gates plus exact-head CI are running; no green result is assumed.

MIL-RE-1 is accepted at 20edc775, bounded static research only. ANNOSCENE-ORE-2 is released
next under wave 1.5, with the accepted predecessor read from its pinned ref rather than merged.
TOWN-CI-CLOSE-1 now has all focused local receipts and independent review; GCC Debug/Release
and macOS hosted lanes pass. Windows and Clang-ASan remain running, not accepted early.

## Current closeout — 2026-11-18

TOWN-CI-CLOSE-1 and the bounded TOWN-START-1 measurement are ACCEPTED on exact green stack
9edb151c (CI 44266114323, all 12 jobs success). Earlier pending/failure entries above remain
history, not current blockers. No main push; Windows owns stacking and combined-candidate gates.
Wildlife remains locally reviewed with inherited-base CI pending, preserved and untouched.
NET-ADR-1 and MIL-RE-1 are accepted documentation packets. ANNOSCENE-ORE-2 has reported a
frozen docs candidate and is under independent review; its row is updated when review completes.
The owner's keep-awake process remains active. No owner policies or defaults were changed.

ANNOSCENE-ORE-2 review found a P2 in the unassigned-marker quantity projection: after R is
cleared, the original jumps past the take-flag check and uses world property14 bit2. The current
audit wrongly preserves quantity for both settings. A focused read/projection/corpus/docs fix
is released directly; preserve its failed evidence. No next implementation or OQ-562 release yet.

## Latest dispatch — ore correction accepted, OQ-562-1 released

ANNOSCENE-ORE-2 is ACCEPTED at b12d22b3, exact docs CI 44273143955 green. The focused
re-review closes its one P2; the previous correction status above is historical. OQ-562-1
is released directly to EXECUTOR II MAC from accepted repair stack 9edb151c, avoiding the
older baseline's known CI failures. This is an explicitly named new task-branch base, not
an automatic merge/rebase of preserved work and not a main landing. Scope is the three
opening-recipe discrepancies and their causal regressions; no living/default/map change.

### OQ-562-1 recovery dispatch

The executor is idle with a clean task/OQ-562-1 at its 9edb151c base. Its latest turn created
that branch and read the relevant callers, but after compaction returned the old ui-tabs-b /
MAC-HEAVY-1 receipts. No OQ-562 implementation or acceptance exists. Resume the existing
packet and keep a private current-task checkpoint note; do not rerun closed gates. The next
planned packet remains SIM-LIMITS-1, to be refreshed and released only after this checkpoint
is reviewed. No additional lane, merge or source mutation is introduced by this correction.

OQ-562-1 now has a frozen pushed tests/docs checkpoint at1ea32f5c. Independent review runs
in parallel with hosted CI; no premature acceptance. SIM-LIMITS-1 is refreshed against the
accepted139 baseline and remains queued. Its old premise is corrected: NoDepositInReach is
reserved, so preserving valid-domain semantics matters more than taking the last enum value.

OQ-562 local causal review passes at1ea32f5c, but own GCC Release warnings in the new test
block acceptance. A narrow lookup/access correction is authorized, with affected D6-only
Debug/ASan reruns and focused re-review. Preserve all assertions and the failed source run.
SIM-LIMITS remains queued; no blanket gate repeat or unrelated repair is released.

## Latest dispatch — OQ-562-1 accepted, SIM-LIMITS-1 released

OQ-562-1 is ACCEPTED at `b7046e5e279fa979d83079feb56dc83a660f32ef`, exact-head CI
44282873819 with all 12 jobs successful. The independent focused re-review confirms the
lookup correction preserves all causal assertions and all 750 trace rows. The old GCC
Release failure remains recorded; earlier pending statuses above are historical.

SIM-LIMITS-1 is released directly to EXECUTOR II MAC on its independent accepted 9edb151c
base. Preserve all OQ/wildlife/ore branches and evidence. Centralize the four compatibility
bounds with unchanged bytes/identities and focused consumer checks, then ordinary-push the
task branch and send READY FOR REVIEW directly to this orchestrator with full head, exact-head
CI and evidence. No main merge/push, owner policy change or DOCS-SIM-1 start is authorized.


## Next handoff — owner-authorized 2026-11-18

SIM-LIMITS-1 is active. Once its frozen result is independently reviewed and exact-head
CI succeeds, accept it and release DOCS-SIM-1 directly, without owner relay. The executor
reports both checkpoints to the Mac orchestrator. DOCS-SIM-1's prepared packet has had
independent dispatch review: its finite set includes every Seams member, and mechanical
declaration/default/mapping checks are explicitly separated from reviewed behavior claims.
No premature SIM acceptance, source integration or gameplay-policy decision is implied.


SIM-LIMITS-1 exact-head CI exposed seven owned constexpr_count lint findings: five wire
framing definitions and two HarvestState layout counts. Its focused local selection omitted
lint; this is not an inherited waiver. Narrow reasoned entries using the existing allowlist
mechanism, canonical lint/self-test and an unrelated-gameplay-cap negative control are
authorized. No production change or repetition of the 55-case gates is needed if source stays
identical. Preserve the failed CI artifact; focused re-review and corrected exact-head CI are
required. DOCS-SIM-1 remains prepared, not released.

The focused lint correction is independently approved at `2f9e4c0bf84cf09998a43c7ff8098fa1522f3a4e`.
Runtime source and the two 55-case gates remain unchanged; canonical lint2/2 and the
unrelated-cap negative control pass. Exact-head CI44298551123 is the only remaining
acceptance condition. Preserve prior failed run44294408513; no premature next-task release.


## Latest dispatch — SIM-LIMITS-1 accepted, DOCS-SIM-1 released

SIM-LIMITS-1 is ACCEPTED at `2f9e4c0bf84cf09998a43c7ff8098fa1522f3a4e`, code33871080,
exact-head CI44298551123 with all 12 jobs successful. Independent review covered source,
consumer tests, all six distinguishing controls, both 55-case local gates and final evidence.
Earlier pending statuses are historical; no further local campaign is required.

DOCS-SIM-1 is released directly from that accepted candidate. The task is a current rules,
commands, runtime-class and hook inventory with bounded source/document drift checks, not
new game behavior. All Seams members are included; mechanical checks and reviewed semantics
are distinguished. The executor reports its frozen pushed head, exact CI and evidence directly
to this orchestrator. Earlier branches and evidence remain intact; Windows owns main landing.

## Current checkpoint — DOCS-SIM reviewed, next gameplay packet prepared

The owner authorizes direct release of the next gameplay task after DOCS-SIM-1
review. Its corrected frozen head is independently locally approved; exact-head
CI44307300031 is still pending, so acceptance and next-task release have not been
claimed. The initial enum-initializer blind spot, two prose corrections and
receipt-pointer fix are documented in the review; unaffected tests were reused.

KBEWOHN-1 is next: complete original figure classification before wildlife/hunting
integration. Independent dispatch review `/root/review_docs_sim_dispatch` approved
the bounded packet. Direct original-data reading corrected the historical premise:
57 names, 20 existing real mappings plus one empty sentinel, hence 37 omitted;
WUESTE legitimately equals0. Exact-value tests replace the old nonzero criterion.
No new lifecycle, extension-tag allocation, load-draw assumption or owner policy
is needed. The candidate must measure registry-digest and affected pin changes;
this is not permission to claim animals or hunting already run.

The earlier WILDLIFE-1a remains locally reviewed on its preserved branch with
inherited CI closure pending. Its later state/load ordering and materialization
contract still need a separate packet. KBEWOHN-1 neither silently takes that
candidate nor blocks on it. No main/STATUS/TASKS changes by this lane.

## Focused Windows correction and independent gameplay intake

CI44307300031 finished with one owned test failure: sim_capabilities_contract
could not apply its real-row omission on Windows (replacement count0). The
MSVC build passed; counts2647 selected,2195 executed,1 failed,451 classified
skips. The other four C++ lanes passed. build-macos was separately cancelled
before any runner/step because GitHub reported macOS arm64 capacity constraints.
Neither condition is an accepted pass. The verified Windows artifact14430724547
ZIP SHA256 is f9e530b3b7d4512924429c79198cbd944bfe71fe48a6d8b583f8dfda49140e18.

The executor receives only the test portability correction with LF/CRLF controls
and an affected checker/contract gate, then focused re-review. No engine or
unaffected local campaign. DOCS-SIM acceptance still requires corrected exact-head
CI. To keep independent gameplay moving after that local correction review,
KBEWOHN starts from already accepted SIM-LIMITS2f9e4c0b, not the pending DOCS
candidate. This scheduling change is not a CI waiver: the inventory can be read
from its pinned ref; none of its pending tools enters gameplay source. Preserve
both branches. Next task is still PREPARED until explicit direct release.

## Latest dispatch — KBEWOHN-1 released on an accepted independent base

The Windows CRLF contract correction is independently reviewed and committed at
DOCS-SIM head1fab3ca9ec80cb69a119ea7201e4a7332bd1ddf0, tree
30fc22789c65f6f855a83715d87b4f0a9624f557. All eight committed inputs match the
reviewed before-gate manifest. Exact-head CI44321152729 is running and remains
owned by the Mac orchestrator; DOCS-SIM is not yet accepted. The historical
Windows test failure and no-runner cancellation remain retained.

KBEWOHN-1 is released directly from accepted SIM-LIMITS head
2f9e4c0bf84cf09998a43c7ff8098fa1522f3a4e. Independent packet review also checked
this scheduling split: DOCS adds no gameplay/importer dependency. Read its
capability document via the named ref only; do not merge its pending tools.
The combined inventory check belongs to eventual stacking. EXECUTOR II MAC
reports its frozen candidate and any essential questions directly here. No
main push, policy/default change or pending-candidate acceptance is implied.

## Current acceptance — DOCS-SIM-1 closed; gameplay continues

DOCS-SIM-1 is accepted at1fab3ca9ec80cb69a119ea7201e4a7332bd1ddf0 after
independent re-review and exact-head CI44321152729, all12 jobs green. Five
hash-verified C++ artifacts were re-parsed with their inventories; the corrected
contract executes on Windows as well as Unix. Full counts and preserved first
failures are in [the acceptance review](reviews/2026-11-18-docs-sim-1-review.md).
No main push or landing is claimed.

KBEWOHN-1 is already executing from accepted2f9e4c0b under dispatch39a04206;
its active work and gates are not interrupted by this acceptance. The executor
reports directly here for independent review. The factual classification audit
is underway; this is progress, not acceptance of that new implementation.

## Latest dispatch — reviewed classification; original spawn decoding released

KBEWOHN-1 is locally approved at6fb7dbb after primary-byte/source/test/evidence
review and two documentation corrections. Exact-head CI44336688872 is still
pending; acceptance is not claimed. The executor retains its CI watch and reports
terminal receipts here. No further local gate is needed for the documentation delta.

WILDLIFE-LOAD-1 is released on the accepted independent2f9e4c0b base, after
read-only dispatch review. It adds the currently missing HIRSCH formats decoder,
synthetic byte proofs and original-file inventory, then a bounded handoff for
future load ordering and persistence. It does not consume pending KBEWOHN or
wildlife helpers, create animals, choose state tags or change draws/defaults.
This keeps the wildlife/hunting path progressing while CI runs, without a waiver.
The executor ordinary-pushes its branch and sends READY FOR REVIEW directly here;
the orchestrator reviews before World integration. Windows owns main landing.

### KBEWOHN CI infrastructure receipt

Exact final run44336688872 at6fb7dbb remains in progress, but macOS job134915336938
is cancelled before execution. Its public annotation says the hosted runner never
acquired the job after multiple attempts; macOS arm64 capacity notices accompany it.
This is NOT_RUN, not a test pass or a source defect. Source run44335070081 remains
separate. An authenticated GitHub operator should retry only the no-runner job once
the run permits it; the Mac orchestrator currently has no authenticated CI session.
No empty commit, full gate repeat or gameplay repair is warranted. WILDLIFE-LOAD-1
continues independently; KBEWOHN acceptance remains pending.

### Wildlife load divisor — verified correction required

During WILDLIFE-LOAD-1, the executor found an inherited primary-reading error.
The orchestrator and independent reviewer each checked the original DLL bytes:
AnnoWelt SHA256 f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2,
0x100946e9–0x10094705. The zero-extended u16 island field is divided by500,
not1000. Exhaustive arithmetic over65536 inputs agrees with floor(n/500);
base range is0..131 and maximum jittered result162. The input's gameplay units
are not inferred from this calculation.

WILDLIFE-1a's previous load-domain approval is reopened: its helper/reference/
adapter cap65 wrongly rejects66..131. The base65+jitter31=>96 example remains
correct arithmetic but is not a maximum. Other pure-helper findings are not
invalidated. A focused correction and inherited-CI closure are required before
World integration; no silent edit to the preserved branch or current decoder
packet. WILDLIFE-LOAD-1 continues because it preserves raw fields and computes
no timestamps or draws. Its handoff explicitly supersedes the old divisor.

### KBEWOHN terminal receipts

Source3c20430b CI44335070081 completed Success. Final documentation head6fb7dbb
CI44336688872 completed Failure: macOS134915336938 cancelled without acquiring a
runner, aggregate gate134935041389 failed. Exact SHA links and saved HTML hashes
were independently checked. The public graph collapses matrix jobs, so this
receipt does not invent twelve individual job verdicts or executed test counts.
Local review remains approved; exact-final-head acceptance remains blocked.
An authenticated operator can retry the cancelled job and dependent gate without
repeating successful jobs. WILDLIFE-LOAD handoff preparation continues.

## Latest dispatch — raw wildlife decoding reviewed; helper correction released

WILDLIFE-LOAD-1 is locally approved without findings atf1e245d2. Independent primary
bytes, corpus recount/digest, parser/reference tests, mutants, raw gates and focused
GCC-test recovery were checked. Exact-head CI44344197686 remains pending, so no
final acceptance is claimed. Executor reports its terminal receipt directly here.

WILDLIFE-LOAD-FIX-1 is released from accepted2f9e4c0b after independent dispatch
review. The packet explicitly authorizes five preserved helper commits on a new
task branch, resolves only mechanical intake conflicts, and repairs the confirmed
load-divisor/domain error with an executable raw-stamp boundary. No pending decoder
or KBEWOHN implementation is consumed. No World wiring, tags, gameplay defaults,
main push or owner-policy choice is authorized. Frozen results go directly to this
orchestrator for review before persistence work.

## Owner-authorized next handoff — 2026-11-18

The owner directs: review WILDLIFE-LOAD-FIX-1 when ready, then release the next
gameplay task directly and keep reportback to the Mac orchestrator. The executor
is active on the correct branch after a stale chat completion repeated old gate
receipts. Actual intake and F0 failing tests were preserved; no files were reset
or old gates rerun. Its current-task checkpoint remains in private cache.

WILDLIFE-INIT-1 is prepared and independently dispatch-reviewed, not released
before the fix review. Scope is one evidenced fresh-import profile, necessary
mode/time/order callers and an executable ordered-load model. It must distinguish
original caller evidence from supplied synthetic ordering and successful allocation
from mere decoded counts. Missing original load draws may require a concrete owner
choice; no silent adaptation or unlimited lifecycle investigation is authorized.
The packet reserves no tags and consumes no pending implementation.

Decoder CI44344197686 is terminal Failure: ten jobs succeeded, macos-clang
134942185691 was cancelled without any steps, and the aggregate gate failed.
The original annotation says no hosted runner acquired the job; capacity/internal
server notices accompany it. This is infrastructure NOT_RUN, not a decoder defect.
A signed-in operator must retry the cancelled lane and dependent gate; successful
lanes and local corpus evidence remain retained. Main remains untouched.

## Latest dispatch — load correction reviewed; initialization released

WILDLIFE-LOAD-FIX-1 passes independent local review at533ee3fc, tested source
73edb244. The exact intake, original arithmetic, failing-before boundary, three
distinguishing mutants, Debug/ASan receipts, partial pins and unchanged-family
evidence were checked with no actionable findings. The historical load-domain
finding is closed for this candidate. Exact-head CI44354548420 remains pending;
local approval is not acceptance for landing.

WILDLIFE-INIT-1 is now released under the owner's 2026-11-18 instruction. It uses
the independently accepted2f9e4c0b base and reads corrected helper/decoder notes
by exact ref. It must establish one evidenced fresh-load profile and executable
ordering model before persistent implementation, or return a precise owner choice
if omitted original draws make that profile impossible. No silent adaptation,
World integration, tag allocation or defaults change. The executor reports its
frozen pushed result and CI directly to this orchestrator; Windows owns landing.

### WILDLIFE-INIT checkpoint — finite original-load prerequisite

Independent primary review confirms an omitted resource-variant draw before
HIRSCH in the proposed no-link/no-city Tutorial profile. The first nonempty
GELAENDE placement succeeds on an empty flat tile; the original commits it, checks
kind225–256 and draws through world+0xb4 before catalogue variant selection.
The current importer copies fields without this draw. This is one established
witness, not a claim that all4589 resource candidates draw or that every possible
profile is impossible. INIT's whole-profile equivalence remains unestablished.

The faithful-port objective is already authorized. A bounded original GELAENDE
load prerequisite can be dispatched after INIT review without asking the owner
to approve faithful development again. A different random-order policy would
require owner approval; none is selected. Executor continues only INIT examples,
contract and handoff, not resource implementation. The packet's automatic owner-
question wording is clarified accordingly. No defaults or source changed.

### LOAD-FIX CI infrastructure receipt; INIT remains active

At exact head533ee3fc, CI44354548420 build-macos job134979143062 was
cancelled before any steps. The executor preserved the jobs JSON and public
annotation: the hosted runner never acquired the job after multiple attempts.
Other C++ lanes were still running at this receipt; this is not a terminal
aggregate verdict or a product failure. Independent local review remains
approved, final acceptance pending. No source change or empty commit is warranted.
If the aggregate ends blocked, an authenticated operator should retry only the
unexecuted lane and dependent gate, retaining successful lanes.

EXECUTOR II MAC continues WILDLIFE-INIT-1 and reports its frozen pushed head,
focused gate and CI directly to this Mac orchestrator for review. The next
faithful prerequisite is the ordered GELAENDE resource prefix, not a new owner
policy choice. Its future packet must preserve original admission and variant
effects; the separate variant plane cannot be stored in growth phase or spare
aux bits, and BuildCmd eligibility must not be widened for scenery import.
The prerequisite is not released before the INIT review.

## Latest dispatch — INIT locally approved; RESOURCE released

WILDLIFE-INIT-1 at97d893af (source0efffe58) is independently APPROVED for its
bounded research/reference scope, no blocking findings. Primary DLL bytes,
14 authored examples,26 CLI checks/two distinguishing mutants,15 receipt hashes,
executed-input identities and119 unchanged existing fixtures were checked.
Review: [2026-11-18-wildlife-init-1-review.md](reviews/2026-11-18-wildlife-init-1-review.md).
Exact-head CI44359989371 is still pending; no landing acceptance is claimed.
Whole-profile original RNG alignment remains NOT_ESTABLISHED. The grouped225–256
values are kind values, not content type ids; that minor wording is clarified
in the review and next packet.

[WILDLIFE-RESOURCE-1](tasks/wildlife-resource-1.md) is RELEASED to EXECUTOR II MAC,
with independent dispatch review. Base remains accepted2f9e4c0b; INIT is read by
exact ref, no pending implementation intake. Scope: resolve the finite original
resource selector/admission and implement an ordered C++ import-prefix evaluator
with owned scratch tiles, separate transient variation plane and exact draw
accounting. Prove the bounded real Tutorial prefix and distinguishing controls.
No live World wiring, tag allocation, default/state/digest change or unrelated
load lifecycle research. Frozen pushed results, CI and evidence go directly to
this Mac orchestrator for review before the next unit.

LOAD-FIX CI44354548420 at533ee3fc is terminal FAILURE:10 jobs succeeded,
build-macos134979143062 was cancelled with no steps because no hosted runner
acquired it, and the aggregate gate failed. Root verified the terminal JSON,
12-job inventory and annotation against all3 reported SHA256s. This is an
infrastructure NOT_RUN, with no product-test failure in that run. Local review
stands; final acceptance is blocked on an authenticated retry of that unavailable
lane and dependent gate. Successful lanes are retained, and no fake source change,
empty commit or full rerun is authorized. Main remains untouched.

## Acceptance receipt — WILDLIFE-INIT-1

ACCEPTED:97d893af9d78e6662dd8859acc3a905f71c80402,
CI44359989371 SUCCESS with all12 jobs independently reconciled against the
complete public-API inventory. Review:
[2026-11-18-wildlife-init-1-review.md](reviews/2026-11-18-wildlife-init-1-review.md).
The head and both receipt hashes were verified; no tests repeated or source
changed. Ready for the Windows orchestrator's normal stacking/landing procedure,
not merged here. Scope remains research/reference, not full RNG alignment.
EXECUTOR II MAC continues RESOURCE-1 closeout and reports its separate frozen
candidate directly here. LOAD-FIX's no-runner CI block is unchanged.

## Latest dispatch — RESOURCE focused corrections

RESOURCE3aab2a93 has two independent P2 findings: repeatedO(R×F²) metadata
validation, and a zero-id regression that succeeds through missing-type refusal
instead of exercising the new guard. Original selector/import semantics otherwise
passed the bounded primary review; tests/evidence review found no additional
blocker. All37 receipt hashes and119 old fixtures were verified.

[WILDLIFE-RESOURCE-FIX-1](tasks/wildlife-resource-fix-1.md) is RELEASED on the
executor's existing task branch. Prepare/index facts once per invocation; pair
a registered-zero refusal with a successful nonzero control and a single causal
mutation. Focused Debug/ASan plus one real-prefix Debug verification only. No
broad repeat, World/persistence work or default change. Executor pushes its own
branch and reports exact frozen results directly to the Mac orchestrator for
focused re-review. The next ordered raw-input adapter is not released yet.

## Latest dispatch — RESOURCE corrections approved; PREFIX released

Independent focused re-review closes bothRESOURCE findings at0627532f/source
754ef6a0. Sorted metadata is prepared once; registered-zero control/mutant reaches
the intended guard.25 receipts and120 unchanged parent fixtures verified; Debug
6/6 plus1/1 andASan5/5 plus1/1 pass. Exact-head CI44372186862 remains pending.
No new original-rule or long-gate campaign was run.

[WILDLIFE-PREFIX-1](tasks/wildlife-prefix-1.md) is RELEASED after independent
dispatch review. The task explicitly authorizes a locally reviewed dependency
stack, preserving original branches and their pending/no-runner CI dispositions.
It adapts actual raw metadata and ordered records into the existing resource and
HIRSCH helpers, with an owned transient result and explicit supplied stamp/mode/
draws. Unknown effects stop at an exact source boundary. No new duplicate helper,
World publication, persistence/tag/default change or claim of full-map alignment.
The executor pushes only its task branch and sends the frozen result directly
to this Mac orchestrator for independent review.

Cross-lane coordination request for the Windows board owner: before later native
integration, coordinate ownership/tags for wildlife enable/mode, resource variation
plane, effective spawn records and their distinct load draw sites. No numeric tag
is allocated or claimed by this dispatch; current transient work does not need one.

### PREFIX checkpoint and next handoff instruction

Owner reaffirmed on2026-11-18: review PREFIX when ready and directly release the
next gameplay packet, updating this board and keeping executor reportback here.
PREFIX is active; its dependency intake reached7366c8fa with only mechanical
report/CMake conflicts. No ready-for-review or acceptance claim yet. The owned
incremental-session clarification is recorded in the packet; it avoids repeated
map copies while preserving interleaved resource/HIRSCH draws. The next task's
exact scope will follow the frozen result and first unsupported boundary, not
an assumption that the entire original load schedule has already been closed.

## Latest dispatch — PREFIX reviewed; STONE released

PREFIX1441183a/source1e061493 is independently locally APPROVED, no blocking
findings. Primary callback/decoder semantics and exact original boundary were
re-derived;43 receipt hashes,11 intake blobs and276 fixture identities checked.
Debug32/32 andASan30/30, each plus four required1/1 contracts, match retained logs.
The actual result is diagnostic partial:37 accepted resource records/draws,0spawns,
stopping before header35/record22/type1558. Full scene/global cursor remains
unestablished; exact-head CI44385455810 is pending at this review.

[WILDLIFE-STONE-1](tasks/wildlife-stone-1.md) is RELEASED after independent dispatch
review. Establish the flags0x3a height/coverage/overlay contract, implement only
that evidenced import class, and continue the same raw prefix to its next boundary.
No type-id hack, skipped effects, arbitrary class expansion, World/native/tag or
default changes. Executor pushes its task branch and sends frozen results directly
here for review and the next release.

RESOURCE0627532f is now ACCEPTED: exact CI44372186862 and all12 jobs verified green.
Its [review](reviews/2026-11-18-wildlife-resource-1-review.md) records both closed
findings and terminal receipt hashes. Ready for Windows-owned stacking/landing;
main remains untouched. Decoder/helper original no-runner failures remain separate.

## Latest dispatch — STONE reviewed; CLIFF released

STONE18473cf4/sourcef3e18bdf is independently locally APPROVED by primary and
causal-test reviewers. All40 receipt hashes and276 baseline fixture files checked.
The corrected raw HHbb parse independently reproduces258 accepted records,
255 resource draws,3 stones and4 completed epilogues. First Debug15 passes/1failure
is preserved; corrected2/2 plus1 required contract and ASan15/15 plus3 required
contracts support the corrected scope. No runtime change hid the expectation error.
CI44390015763 is still running; acceptance is not claimed.

[WILDLIFE-CLIFF-1](tasks/wildlife-cliff-1.md) is RELEASED after independent dispatch
review. Establish the flags0x12/group0x141 cliff behavior, especially later placement
over a non-replaceable cliff, and continue the same original prefix only to the
next unsupported boundary. No World/native/tag/default changes or global-RNG claim.
The executor ordinary-pushes its task branch and reports frozen results directly
to this Mac orchestrator for review and the next release.

PREFIX CI44385455810 currently has8 successful jobs,2 cancelled macOS jobs and
Windows still running. Both cancellations have runner_id0 and no steps:
build-macos135088771726 and cpp(macos-clang)135088771902. These establish no
build/test result. Windows/owner authenticated narrow retry remains required;
no fake source commit or broad rerun is authorized. STONE's newer successful
build-macos does not retroactively accept PREFIX's exact head.


## CI closeout — 2026-11-19

STONE18473cf4 is ACCEPTED: independently reviewed and exact CI44390015763/all12
jobs successful. [Acceptance review](reviews/2026-11-18-wildlife-stone-1-review.md)
records terminal receipt hashes. Ready for Windows-owned stacking; main untouched.
PREFIX1441183a remains locally approved but not accepted: terminal CI44385455810
has9 successful jobs,2 no-runner cancellations and aggregate failure. The review
records complete receipts; no rerun or source-trigger commit was made.
CLIFF is finishing its frozen handoff; its reported focused gates are not yet an
orchestrator approval. Do not interrupt that closeout or expand its assigned scope.


## Latest dispatch — CLIFF reviewed; ROCK released, 2026-11-19

CLIFF11bc3be6/sourceeb3ea560 is independently locally APPROVED: primary and
causal-test reviews found no blocking defects.52 receipt/source/input hashes and
276 baseline fixtures checked; nine reference rows append, no old fixture changes.
Debug19/19 and ASan18/18 each pass three additional separate required contracts.
The first cached-Python mutant failure remains retained; fresh-directory bytecode
isolation and the actual production predicate mutant provide causal proof.
Exact CI44417567937 is still in progress, so acceptance remains pending.

Independent original-input continuation reproduces2,387 accepted records,2,333
resource draws,29 stones,25 cliffs and nine completed epilogues. No spawns yet.
Next boundary42/39/type1512/flags0x8018 is explicit, not skipped or called refusal.

[WILDLIFE-ROCK-1](tasks/wildlife-rock-1.md) is RELEASED after independent dispatch
review. Establish mode0 height and upper-bit effects, extend only that evidenced
fact-defined class, then continue once to the next unsupported boundary. Preserve
shared writer/ownership and existing classes; no World/native/tag/default changes.
The executor ordinary-pushes its task branch and reports frozen results directly
to this Mac orchestrator for review and the next release. Windows retains landing
and cross-lane identity ownership; main remains untouched.


## Latest dispatch — ROCK evidence correction, 2026-11-19

ROCK81e8f03f/source26899502 has no identified production defect in its bounded
projection; tests/evidence reviewer approved, primary review requests one P2
correction. Original writer extends through82b0a and executes derived chunk-bound
and dirty-counter callbacks. Fresh wrapper observer bypass does not skip those.
The transport attribution of empty-tile fill928a0 to slot19c is stale: it is1a0;
actual19c is84150. These callbacks do not change current tile/variation/draw
projection, but omitted derived cache effects must be explicit before integration.

49 hashes and276 baseline fixtures verified. Independent raw continuation agrees
on4760 accepts/4589 resource draws/16 completed epilogues, stopping before
FRUCHTBAR49. Debug21/21, ASan21/21, three required contracts each and real1/1
remain valid. No broad rerun or engine change is requested for this correction.

[WILDLIFE-ROCK-FIX-1](tasks/wildlife-rock-fix-1.md) is RELEASED after independent
scope review. Executor corrects the named documentation and stale concrete slot,
preserves frozen evidence/runtime bytes, ordinary-pushes its existing task branch
and reports directly here for focused re-review. Fertility-header implementation
is not released until the correction closes; its next packet will explicitly
trace loader effects and callbacks rather than treating metadata as a no-op.
CLIFF and ROCK exact-head CI remain pending at this review. Main untouched.


## Latest dispatch — ROCK correction closed; fertility header released

Focused independent re-review APPROVES corrected ROCK6c6554a2, no blocking finding.
The writer extent/concrete callbacks and omitted derived-cache scope are corrected;
all15 new receipts verified, runtime delta from26899502 empty. Valid Debug/ASan/
real-prefix gates reused; no repeat. Exact CI44422288006 remains pending.

CLIFF11bc3be6 is now ACCEPTED for its bounded projection: exact CI44417567937/all12
jobs successful and independent review complete. Carry the later cache-scope
clarification when Windows stacks it; no original-world completeness claim.

[WILDLIFE-FERTILITY-1](tasks/wildlife-fertility-1.md) is RELEASED after independent
dispatch review. Establish FRUCHTBAR49's actual ordered loader effects/callbacks,
then implement only the proven transient-header slice and continue to the next
unsupported boundary. Explicitly check empty-tile-fill1a0, immutable terrain/view
lifetimes and duplicate source order. It does not overlap FERTILITY-1 crop/UI policy.
No World/native/tag/default changes. Executor ordinary-pushes its task branch and
reports frozen results directly here; no owner relay or self-approval. Main untouched.


## Latest dispatch — FERTILITY reviewed; BERGWERK2 released

FERTILITY81026d1b/source12586595 is independently locally APPROVED. Actual original
nibble merge, source order, finite callbacks and owned terrain lifetime verified;
37 hashes and276 unchanged fixtures checked. Debug8+3required, ASan8+3required,
and original1 pass. One public host path was corrected in a single prose line;
no runtime change, test repeat or extra substantive review. Exact-head CI44425779949
remains pending. Header49 is complete; next unsupported header50 BERGWERK2 is
before effects. No spawns/full-world/global-cursor claim.

[WILDLIFE-BERGWERK-1](tasks/wildlife-bergwerk-1.md) is RELEASED after independent
dispatch review on corrected81026d1b. Establish that container's actual handler,
typed values/order and direct effects; distinguish OCCUR ore objects and mining
from this transient header. Continue once to the next unsupported boundary.
No raw-stash/no-op surrogate, identity/default change or extra header expansion.
Executor ordinary-pushes its branch and reports directly here for independent
review and next release. Main/other lanes remain untouched.


## Latest dispatch — BERGWERK2 reviewed; OCCUR released

BERGWERK03fff3c6/source7e4ac88f is independently locally APPROVED, no blocking
findings. Primary instructions and raw9entries/20nodes/8chunks verified; tests
pin wrapped coordinates, signed chunk division, duplicates and prepend links.
42 hashes and276 unchanged fixtures checked. Debug8+3required, ASan8+3required
and original1 pass. No corrections or gate repeat needed. CI44428107745 remains
pending; corrected FERTILITY/ROCK exact-head CI also remains nonterminal.

[WILDLIFE-OCCUR-1](tasks/wildlife-occur-1.md) is RELEASED after independent dispatch
review. Establish the actual reached occurrence reader/factory/attach/load/callback
chain using prior ore research; preserve distinction from BERGWERK lists and later
scene assignment. Implement only the proved transient slice and stop at unsupported
classes/effects or next header. No whole factory, World/native/tag/default changes.
Executor ordinary-pushes its task branch and reports frozen results directly here
for independent review and next release. Main/other lanes untouched.


## Latest dispatch — OCCUR reviewed; ANBAU released

OCCUR5bcd50c5/source8c2de16c is independently locally APPROVED, no blocking
findings. Primary and tests/evidence reviews verify the bounded class145/goods1–8
constructor/attach/load, shared lists, duplicate selection and reached mask callbacks.
Original one ore/Q512000/R1/node21/all20 masks16→17 independently reproduced.
57 hashes and276 unchanged fixtures checked. Debug12+3required, ASan12+3required
and original1 pass; no corrective work or repeated gates. ExactCI44432068823 and
predecessor BERGWERK CI remain pending, not acceptance.

[WILDLIFE-ANBAU-1](tasks/wildlife-anbau-1.md) is RELEASED after independent dispatch
review. Establish actual header52 fields, ordering and reached effects/callbacks,
then extend only that transient slice and continue once to the next boundary.
No assumption that crop-named metadata is inert, no other-lane crop policy,
World/native/tag/default changes or extra header expansion. Executor ordinary-pushes
its branch and reports frozen results directly here. Main/other lanes untouched.


## Latest dispatch — ANBAU reviewed; DISCOVER released

ANBAU02111ea2/source d645071a is independently locally APPROVED with no blocking
findings.42 evidence hashes and unchanged fixtures verified. Both reviewers checked
primary instructions, name lookup and bounds, chronology and retained gates:
Debug16+3required, ASan16+3required, original1. A citation endpoint correction goes
in the next documentation edit, without runtime change or repeated gates. Exact
CI44436355753 remains running; local approval is not acceptance.

ROCK corrected6c6554a2, FERTILITY81026d1b, BERGWERK03fff3c6 and OCCUR5bcd50c5 now
have all12 successful jobs at their respective exact reviewed heads. They are
ACCEPTED within their independently reviewed transient scopes. Terminal receipts
and limitations are appended to their individual reviews; no tests repeated.

[WILDLIFE-DISCOVER-1](tasks/wildlife-discover-1.md) is RELEASED after independent
scope review. Establish the actual header53 load and complete finite effects,
separate scalar decoding from player-slot/visibility policy, implement only that
transient slice and continue once. No World/native/tag/default or other-lane
changes. Executor ordinary-pushes its task branch and reports directly here for
review and next release. Main untouched.

## Latest dispatch — DISCOVER reviewed; KONTORID released

DISCOVER8d97b695/source ddab0993 is independently locally APPROVED, no blocking
findings.44 receipt hashes and unchanged fixtures verified. Original low-u16
replacement and raw continuation independently derived. Retained Debug18+3required,
ASan18+3required and original1 pass; no gates repeated. One P3 identifies payload
scratch rather than the read-count local; next task corrects only that prose.
Exact CI44438472504 and predecessor ANBAU CI remain pending, not accepted.

[WILDLIFE-KONTORID-1](tasks/wildlife-kontorid-1.md) is RELEASED after independent
scope review. Establish actual header54 counter import and finite effects,
distinguish raw values from normal engine nonzero/default policy, and continue
once to the next unsupported boundary. No World/native/tag/default, counter
allocation, founding or other-lane changes. Executor ordinary-pushes its task
branch and reports directly here for review and next release. Main untouched.

## Latest dispatch — KONTORID reviewed; empty PROD2 path released

KONTORIDeaa61980/source c199cf1f is independently locally APPROVED with no findings.
41 receipt hashes and unchanged fixtures verified. Original raw counter transfer,
saver/init and55-header continuation independently confirmed. Debug20+3required,
ASan20+3required and original1 retained green; no gate repeated. DISCOVER's P3
scratch correction is verified. Exact CI44439685272 remains pending, as do the
ANBAU/DISCOVER runs at this check; none is labelled accepted prematurely.

[WILDLIFE-PROD2-1](tasks/wildlife-prod2-1.md) is RELEASED after independent scope
review. Prove the reached empty-block path and its full finite effects, implement
only that path, explicitly stop at unsupported nonempty records and continue once.
Do not reuse GELAENDE admission/epilogue or claim full building import. No World/
native/tag/default/other-lane changes. Executor ordinary-pushes the task branch
and reports directly here for review/next release. Main untouched.

## Latest dispatch — PROD2 reviewed; EVENTINSEL released

PROD2d7a21ac4/source8d65f385 is independently locally APPROVED, no findings.
42 hashes and unchanged fixtures verified. Retained Debug22+4required,
ASan22+4required and original1 pass. No gates repeated. Sixteen empty blocks
now reach HIRSCH71:156 owned spawn records, checked fields/draws/chunk links,
4745supplied draws total; NOT live animals.72headers complete, then EVENTINSEL72.
Exact CI44441246351 and ANBAU/DISCOVER/KONTORID remain pending at this check.

[WILDLIFE-EVENTINSEL-1](tasks/wildlife-eventinsel-1.md) is RELEASED after independent
scope review. Trace actual container delegation and empty-path effects; do not
assume empty means inert. Project only proved local behavior and continue once to
next boundary or local-island end. No global/script/World/native/tag/default or
live-wildlife integration. Executor ordinary-pushes its task branch and reports
directly here for review and next release. Main untouched.

## Latest dispatch — local records reviewed; post-island contract released

EVENTINSEL218b838e/source c6c106a4 is independently locally APPROVED.51 hashes and
unchanged fixtures verified. Debug25+4required, ASan25+4required and original1
retained green; no gate repeated. Both initial invalid exploratory attempts remain
preserved. All73local headers complete;156owned spawn records/4745supplied draws
are not live wildlife or completed original island initialization.

ANBAU02111ea2 is now ACCEPTED: independent review and exact CI44436355753/all12
successful jobs. DISCOVER/KONTORID/PROD2/EVENTINSEL remain pending CI at this check.

[WILDLIFE-POSTLOAD-1](tasks/wildlife-postload-1.md) is RELEASED after independent
scope review. Resolve the three post-island calls, their ground/fill/navigation
and other direct effects, using existing research and minimal independent vectors.
Deliver the dependency contract and bounded implementation steps toward World
integration. No src/native/tag/default/World changes in this research packet.
Executor ordinary-pushes its task branch and reports directly here for review
and next implementation release. Main untouched.

## Latest review — POSTLOAD focused corrections

POSTLOADe05d9504 is reviewed as docs-only research. Two P2 contract fixes are
required: V12 cannot distinguish equivalent clamping from zero padding, and the
raster write's rounded byte can set bit4 without a proven upstream bound. Both
are assigned directly to the executor as prose corrections, no engine edits or
repeated campaigns.47 receipts verified; all12 vectors remain NOT_EXECUTED.
V9 is a supported static discrepancy only; later repair must inspect both sum
mask and final clamp. The UNWIRED graphics helper packet is prepared, not released
until corrected contract receives focused re-review.

DISCOVER8d97b695/CI44438472504, KONTORIDeaa61980/CI44439685272 and
PROD2d7a21ac4/CI44441246351 now have all12 jobs successful at their independently
reviewed exact heads and are ACCEPTED within their bounded scopes. Terminal
receipts are appended to their reviews. Main untouched.

## Latest dispatch — POSTLOAD corrections approved; graphics leaf released

Corrected POSTLOAD1628ba18 is independently locally APPROVED by both reviewers.
The raster arithmetic and nondiscriminating padding-test claims are corrected;
8 new receipts verified and original evidence preserved. All15 vector rows remain
NOT_EXECUTED. No engine changes, compatibility disposition or repeated campaign.
Exact CI44447642251 and EVENTINSEL CI remain pending at this check.

[WILDLIFE-GFX-FINALIZE-1](tasks/wildlife-gfx-finalize-1.md) is RELEASED after
independent scope review. Implement the complete third-call leaf as a pure owned,
UNWIRED graphics transformation with independent vectors/reference, digest and
causal checks. Preserve fill→navigation/raster→cleanup publication order. No
navigation repair, World/native/tag/default or other-lane changes. Executor
ordinary-pushes its task branch and reports directly here for review/next release.
Main untouched.

## Latest dispatch — graphics leaf reviewed; catalogue inputs released

GFXad2a1a23/source e9256f22 is independently locally APPROVED, no findings.
41 receipts verified;12 independent vectors, reference CLI, ownership/digest and
actual bit/quad mutants support the isolated leaf. Debug9+1required and
ASan9+1required retained green; no gate repeated. Caller search confirms UNWIRED.
Exact CI44449632964 and corrected POSTLOAD CI remain pending at this check.
EVENTINSEL218b838e is now ACCEPTED with exact CI44443163952/all12 jobs green;
this accepts local records, not post-island initialization or live animals.

[WILDLIFE-POSTLOAD-INPUTS-1](tasks/wildlife-postload-inputs-1.md) is RELEASED after
independent scope review. Close direct catalogue producer/type2/type11 facts and
lifetime/validation requirements for first-call fill. No src/global/preprocessing/
navigation/World changes. Executor ordinary-pushes the task branch and reports
directly here for review and next implementation release. Main untouched.

## Latest dispatch — catalogue inputs reviewed; first-call fill released

INPUTS263958ed/source625db996 is independently locally APPROVED, no findings.
72 receipts verified; docs-only result. Fresh concrete catalogue/type-array,
HEAD/getter profile and lifetime facts bind first-call grass/fish semantics.
Python/PE checks stay distinct from original-runtime or full-fill execution;
no C++ campaign repeated. Exact CI44452465008 and GFX/corrected POSTLOAD runs
remain pending at this check.

[WILDLIFE-POSTLOAD-FILL-1](tasks/wildlife-postload-fill-1.md) is RELEASED after
independent scope review. Implement the isolated owned first-call fill under
validated precomputed-ground/catalogue capability, with independent reference,
full-field controls, ownership/digest checks and bounded real-input leaf evidence.
Remain UNWIRED; stop before navigation/raster/graphics. No c2-zero preprocessing,
V9 repair, global/World/native/tag/default or other-lane change. Executor ordinary-
pushes its task branch and reports directly here for review/next release.
Main untouched.

## Latest dispatch — first-call fill reviewed; navigation-input reproduction released

FILL889143ba/source db2240a6 is independently locally APPROVED, no P1/P2.
51 receipts verified; Debug13+1required, ASan13+1required and original1 retained
green. Isolated real leaf:8486fish/1559grass/6339unchanged,0new draws. No ordered
postload or World publication. P3: controls-first.log is empty, so its historical
failure-text retention claim is withdrawn and correction assigned as next first
doc edit. No gate repeat. Exact FILL CI44456284511 remains pending.

Corrected POSTLOAD1628ba18/CI44447642251, GFXad2a1a23/CI44449632964 and
INPUTS263958ed/CI44452465008 now have all12 jobs green at exact reviewed heads
and are ACCEPTED within their bounded scopes. Receipts appended to reviews.

[WILDLIFE-NAV-INPUTS-1](tasks/wildlife-nav-inputs-1.md) is RELEASED after independent
scope review: prove exact extrema/halving and execute a valid V9 reproduction,
then assess callers/replay compatibility before any production route correction.
No runtime routing/identity change, secondary-raster port or World integration.
Executor ordinary-pushes its task branch and reports directly here for review
and next release. Main untouched.


## Latest dispatch — navigation evidence reviewed; secondary raster released

NAV-INPUTS `74d50bfe` is independently locally APPROVED, no actionable findings.
47 manifest receipts and all 24 published rows verified. Canonical and sum-only
probes each have 3 mismatches in 13 rows; both corrections give 13 agreements;
the component-mask overcorrection is caught. Seven ordinary controls and four
geometry checks execute. The legal import domain is proved, not a shipped-content
trajectory divergence or World publication. FILL's empty-log disclosure is
corrected. Both exact NAV-INPUTS and FILL CI runs remain in progress.

[WILDLIFE-RASTER-CONTRACT-1](tasks/wildlife-raster-contract-1.md) is RELEASED
after independent scope review. Close the initialized height-table producer,
flags-2 sampling/rounding/merge, generation and actual first-reader contract.
No production routing repair, compatibility allocation, raster implementation or
World integration. Executor ordinary-pushes its task branch and reports directly
here for review and next release. Main untouched.


## Latest dispatch — raster contract reviewed; numeric caller closure released

RASTER-CONTRACT `8217e187` is independently locally APPROVED within its
conditional research scope. 72 manifest receipts and 25 published rows verified.
31 modeled checks and 6 separate float transcriptions agree; the wrong nibble
mask fails four controls and skipped empty increment fails one. These are not
original execution or general integer-equivalence proof. Numeric producer and
actual first minimap readers are resolved; load-thread rounding/precision remains
a precisely named prerequisite. NAV and RASTER exact CI remain in progress.

FILL `889143ba` is now ACCEPTED: independently reviewed and all 12 exact-head
CI44456284511 jobs successful. Its isolated helper remains UNWIRED. No local
gate repeated or main change. Terminal receipt is appended to its review.

[WILDLIFE-RASTER-FP-1](tasks/wildlife-raster-fp-1.md) is RELEASED after independent
scope review. Trace only the two numeric checkpoints and concrete intervening
control-word changes. If static closure reaches a genuine external/thread edge,
stop with a precise Windows capture card rather than guessing or repeating the
same research. No production port, route correction, compatibility allocation
or World publication. Executor reports directly here for review and next release.


## Latest dispatch — numeric research reviewed; ground preparation released

RASTER-FP `aec8bcc8` is independently locally APPROVED. 31 manifest receipts
and 18 published rows verified. Prior 31 controls and 19 sensitivity checks
pass, with independently derived differences. Actual table-init and callback
profiles remain unobserved; the precise Windows capture card is ready at
`docs/testing/wildlife-raster-fp-capture.md` on that task head. It needs Windows
orchestrator scheduling, not another Mac static campaign. No Windows execution
is claimed. RASTER-CONTRACT and RASTER-FP exact CI remain in progress.

NAV-INPUTS `74d50bfe` is now ACCEPTED within its diagnosis scope, with all 12
CI44458725953 jobs green at the reviewed head. The pending owner production
correction/compatibility decision is unchanged; acceptance does not allocate an
identity or release that repair. Terminal receipt appended to its review.

[WILDLIFE-PREPROCESS-CONTRACT-1](tasks/wildlife-preprocess-contract-1.md) is
RELEASED after independent scope review. Resolve c2-zero metadata/kernel inputs,
exact writes and scratch initialization/lifetime. This broadens initialization
support while capture is pending; the selected Tutorial has completed FRUCHTBAR
and already skips this branch. Do not make it a new Tutorial prerequisite.
Executor ordinary-pushes its task branch and reports directly here for review
and the next bounded task. Main, runtime behavior and other lanes untouched.


## Latest dispatch — preparation contract reviewed; numerical implementation released

PREPROCESS-CONTRACT `54798a4b` is independently locally APPROVED. 67 manifest
receipts and 25 published rows verified. The 23 authored model checks and two
finite producer probes support the bounded integer contract; original execution
is not claimed. The preserved nondistinguishing width control is corrected by
S10. Actual growth-file retained zeros do not prove retention versus zero-fill;
a later parser needs an authored nonzero-staging control. Partial rectangles
can read uninitialized scratch and remain outside the supported helper domain.

RASTER-CONTRACT `8217e187` is now ACCEPTED conditional research, with all 12
CI44461780207 jobs green at the reviewed head. RASTER-FP and PREPROCESS exact
CI remain pending. Windows numeric capture and owner navigation compatibility
decision remain separate; no runtime policy or identity is allocated.

[WILDLIFE-GROUND-PREPARE-1](tasks/wildlife-ground-prepare-1.md) is RELEASED after
independent scope review and correction of its missing-fact/null-descriptor
distinction. Implement the two-pass integer numerical leaf from explicit
validated supplied facts, with owned output, independent reference and focused
Debug/ASan gates. Remain UNWIRED: no scene provenance certificate, growth parser,
fake FRUCHTBAR, relaxed old fill validator, World/default/state change or original
load claim. The selected Tutorial remains unchanged. Executor ordinary-pushes
its task branch and reports directly here for review and the next release.


## Latest dispatch — integer leaf reviewed; growth source loader released

GROUND-PREPARE `5bdfd92a` / tested `92c17db` is independently locally APPROVED.
73 manifest hashes and 22 published rows verified. Reference precedes C++; 24
full-output vectors, independent 2D arithmetic, ownership/refusal controls and
seven actual wrong-source variants support the conditional leaf. Debug and ASan
each pass 21 main cases plus two required 1/1 contracts. No failures, skips or
sanitizer reports; LeakSanitizer remains NOT_RUN. No gate repeated, no original
scene/city provenance or live integration claim. PREPROCESS and GROUND exact CI
remain pending.

RASTER-FP `aec8bcc8` is now ACCEPTED research/capture card with all 12
CI44464134541 jobs green. Original Windows observations and the owner navigation
correction/compatibility decision remain pending; no numeric policy is selected.

[WILDLIFE-GROWTH-LOAD-1](tasks/wildlife-growth-load-1.md) is RELEASED after
independent scope review. Implement one owning original growth-lookup loader from
source bytes, after closing its bounded parser contract. Keep generic text::ints
unchanged, use nonzero retained-staging controls and a real-source/authored-leaf
integration test. No kernel extraction, city proof, FRUCHTBAR capability, fill
composition, World/state/default or other-lane change. Executor ordinary-pushes
its task branch and reports directly here for review and next release.


## Terminal acceptance — preparation contract and integer leaf

PREPROCESS-CONTRACT `54798a4b` / CI44467047682 and GROUND-PREPARE `5bdfd92a` /
CI44469519552 are now ACCEPTED within their independently reviewed scopes. Root
verified all 12 successful jobs at each full head. Receipt hashes and limitations
are appended to both reviews. No retries, source changes or repeated gates.

GROWTH-LOAD remains the active released task at accepted base `5bdfd92a`. The
executor reports focused Debug complete and its sanitizer gate running. That
progress is not reviewed acceptance. Continue unchanged; no interruption, new
assignment, board merge or main push is requested.


## Latest review — growth loader needs one reference correction

GROWTH-LOAD `e89409e1` has approved bounded production review, but independent
evidence review found one P2: the Python reference accepts out-of-int32 values
in discarded missing-comma tails, while C++ correctly refuses them. The existing
32 CLI controls miss that branch. Root verified 105 manifest hashes and 30
published rows; other reference-first, numerical, original-file and scoped gate
reuse evidence stands. Current CI is pending and cannot waive the discrepancy.

The [focused correction](tasks/wildlife-growth-load-1-fix.md) is RELEASED on
the existing task branch. Fix reference tail bounds and add paired legal/invalid
boundary controls in both languages; rerun only affected tests/contracts and
reuse unaffected evidence explicitly. No production, generic-parser, identity
or original-data change. The next kernel-binding packet is prepared and scope-
reviewed, but NOT RELEASED until this correction passes independent re-review.
Executor reports directly here; main and earlier evidence remain untouched.


## Latest release — kernel source binding; Windows edition comparison requested

GROWTH-LOAD correction fe4355fd passed independent focused re-review; P2 is closed.
Exact-head CI remains pending and old e89409e1 has an unresolved hosted ASan test
failure. Root verified all correction receipts. Acceptance is NOT claimed.
KERNEL-LOAD is released on that locally reviewed base for the bounded original
PE kernel reader and authored numerical connection, without World/state changes.
Executor ordinary-pushes its task branch and reports directly here for review;
no repeated full campaign or unrelated CI repair is requested.

The owner answered the navigation decision by asking for GOG AND HE verification.
WIN-NAV-EDITIONS-1 is a detailed forwardable Windows handoff with separate edition
inventories, original-instruction checks, exact arithmetic controls, bounded
runtime/data work, compatibility assessment and independent review. It stops
before a repair or identity change. Windows chat intake is pending; this Mac
publication does not assign other Windows work or claim a message was delivered.
Main, shared TASKS/STATUS and original files remain untouched.


## Latest priority — candidate 10 display diagnosis

Owner/Windows request releases MAC-GPU-WINDOW-1 at the completed kernel checkpoint.
Run the two magnifier cases alone once with an explicitly visible, unlocked/awake,
terminal-frontmost setup and once in the recovered original launch context. The
owner confirms the ORIGINAL display was unlocked and awake. Do not substitute
locked/asleep assumptions; record unrecoverable coverage/focus facts as unknown.
Four case runs only, retained stderr/traces, no source changes or whole gate.
Report the proved draw blocker or insufficient observation; Windows owns follow-up.

Mac GPU skips for candidate 10 are the TEN named-adapter cases (two play, eight
atlas/RENDER-DIRECT), with DXGI platform and texture-ceiling opt-in. This resolves
the old skip-policy mismatch, not the two Metal first-frame failures. Exact names
remain in the preserved skip inventory/review; no blanket new skip allowance.

Growth fe4355fd is ACCEPTED with all 12 exact-head CI jobs green (44478146499).
Kernel c9259c16 has both independent reviews APPROVED, 45+45 hash receipts verified,
and focused Debug/ASan 17+1+1 each; exact-head CI 44483388631 pending. No gate
repeated. Common-HEAD gameplay binding remains queued after the small GPU packet.
Executor reports directly here for review and continuation. Main untouched.


## GPU diagnostic closed; gameplay source binding resumed

MAC-GPU-WINDOW-1's four authorized investigations are complete and independently
reviewed. All four drew frames and reached script, then failed their zoom checks;
no original no-frame failure reproduced. All 7287 recorded acquire/present returns
succeeded. Root verified 85 retained hashes. Candidate 95897a55 remains FAIL;
original hidden-versus-back-off attribution stays unresolved. Owner unlocked/awake
facts and unknown focus/coverage remain separate. The review is a forwardable
Windows handoff for two distinct investigations: first-frame observation and
high-DPI click-to-hit/zoom behavior. No cause, repair or display policy is invented.

WILDLIFE-GROUND-FACTS-1 is RELEASED on locally reviewed c9259c16 after independent
scope review: bind whole-object/common-HEAD properties to owned facts, then check
the existing numerical leaf. No INFO/city/FRUCHTBAR/fill/World or navigation change.
Kernel exact-head CI remains pending independently. Executor pushes only its task
branch and reports directly here for review/continuation. Main and Windows-owned
boards/source remain untouched; the short UI investigation does not grow into
another Mac repair campaign.


## Latest review — metadata binding approved; source contract released

GROUND-FACTS 4054d93a passed both independent reviews with no blocking findings.
Original factory/getter proof and all 921 source properties were independently
re-derived; zero-success, full IDs and source/registry ownership hold. Root verified
106 manifest and 38 published hashes. Debug/ASan each18+1+1 passes; its exact CI
remains pending. Old reconstructed kernel binary is labelled accurately and old
receipts unchanged. No repeated gate or runtime integration claim.

Kernel c9259c16 is now ACCEPTED: all12 jobs in CI44483388631 successful at exact
head. PREP-PROVENANCE is RELEASED on locally reviewed4054d93a to establish final
INFO selection and the prewalk city state for the supported fresh-island path,
then specify the next owned composition boundary. No fabricated FRUCHTBAR or
city-absence certificate. Windows UI and navigation/FP work remain separate.
Executor ordinary-pushes its task branch and reports directly here for review and
next release. Main and other lanes remain untouched.


## Latest release — completed-ground first call

PREP-PROVENANCE corrected0a30d44a is locally independently approved. Actual
instructions establish geometry adjustment and completed-FRUCHTBAR skipping of
all preparation/city prewalk. Root verified59 manifest and83 published receipts;
24-file sample and12 arithmetic probes remain bounded. P3 retention wording is
corrected: preserved first-selection script/empty stdout are not a retained runtime
diagnostic. No rerun or invented evidence. Exact-head CI remains pending, as does
GROUND-FACTS CI; neither is silently accepted by this continuation.

FIRST-CALL-1 is RELEASED: internally source-build one supported fresh-island
prefix, validate full aligned geometry/genuine completed fertility and compose the
existing owned fill operations. Reuse exact retained Tutorial inputs and add
independent authored field-level/refusal/ownership controls. No absent-data prep,
World, navigation, secondary raster or graphics wiring. Unresolved city eligibility
belongs to the absent-data path and does not block this selected completed path.
Executor ordinary-pushes its task branch and reports directly here. Main untouched.

## First-call review and next gameplay dependency

FIRST-CALL 7d2df0fc is independently locally APPROVED by both reviewers, with no
actionable findings. Root verified70 manifest and54 published local hashes;
Debug/ASan each14 executed passes, authored field/draw/ownership controls and
Tutorial all-cell expectations hold. First cheap expectation failures remain
retained. CI44496760869 is pending, as is provenance CI44494353183; these are
not accepted by association. GROUND-FACTS4054d93a is now ACCEPTED with all12
exact-head jobs green in CI44490451270. No gates repeated.

Next postload composition waits on Windows navigation/identity and secondary-raster
numeric qualification. No silent substitution or early graphics call is authorized.
WILDLIFE-CREATE-CLOSURE-1 is independently scope-approved and RELEASED on the locally
reviewed FIRST-CALL head: close the specific original kind49/51 manager creation,
attach and sweep-tail callbacks needed for hunting. Reuse established reads; stop
before first tick, World/state/default/tag changes or global draw-order claims.
Executor ordinary-pushes its task branch and reports directly here for independent
review. Main, shared TASKS/STATUS, Windows/UI source and original assets untouched.

## Creation callbacks reviewed; isolated implementation released

CREATE-CLOSURE252ed66c is independently locally APPROVED. Primary review resolved
the selected descriptor/manager/cell receivers and repeated the368/29/0 inventory;
evidence review verified109 manifest and998 preserved prior identities. Root
verified109 manifest and39 published local hashes. Eleven modeled rows and six
counterfactuals remain models, not original runtime. Null creation does not prove
manager rollback; arbitrary allocator retry and later actions remain outside scope.

PREP-PROVENANCE0a30d44a is now ACCEPTED with all12 jobs green in CI44494353183.
FIRST-CALL7d2df0fc and CREATE-CLOSURE252ed66c still have CI in progress; both are
locally reviewed only. No gate was repeated and no inherited acceptance invented.

WILDLIFE-CREATE-EFFECTS-1 is independently scope-approved and RELEASED on252ed66c:
read validated original descriptor facts, compute isolated owned creation effects,
validate ordinary position/home consistency, prove null/success/quantity/flags and
source lifetime with focused tests. No live animal or new World/state/identity/
default/PRNG integration. Actual manager and upstream float/scheduler policies stay
separate. Executor pushes its own task branch and reports directly here for review.
Main, Windows/UI work and shared TASKS/STATUS remain untouched.

## Creation effects approved; end/release continuation

CREATE-EFFECTS25bab774 is locally independently APPROVED by primary and evidence
reviewers. Original segmented goods lookup and all29 descriptor rows were
independently reproduced. Root verified209 manifest and35 published local hashes;
evidence reviewer verified1004 frozen identities and coverage reuse across three
sources. Debug/ASan each18 unique cases covered by18+6+5 executions, not a fresh
full final-head gate. Initial failures/missing historical artifacts remain disclosed.
CI44508278589 is still running; exact-head acceptance is not claimed.

FIRST-CALL7d2df0fc and CREATE-CLOSURE252ed66c are now ACCEPTED: all12 jobs green
in44496760869 and44500172507 respectively, with their existing independent reviews.
No passing local gate repeated.

WILDLIFE-RELEASE-EFFECTS-1 is independently scope-approved and RELEASED on25bab774:
close only remaining selected New cleanup callbacks, then implement owned normal
end/release effects. Home lookup, preserved ID/time/goods, no saved-ID guard,
repeated detach and distinct direct deletion receive discriminating checks.
Unsupported cleanup state refuses; no invented safer ownership policy, live
manager or World/state/default/identity change. Windows navigation/FP work stays
separate. Executor pushes own task branch and reports directly here for review.
Main, shared TASKS/STATUS and other-lane work remain untouched.

## End/release reviewed; source species plan released

RELEASE-EFFECTS corrected901a8467 is independently locally APPROVED. Original
empty-cleanup callbacks and category/home lookup agree with code; focused
Debug/ASan each14 unique passes, four causal mutants and83 receipt hashes hold.
The sole P3 getter-address typo is corrected and re-reviewed; code/gates unchanged.
The293 physical fixture count includes ignored Python bytecode, not a new tracked
fixture. Full World/end/hunt/runtime claims remain excluded.

Corrected CI44511432981 is queued; prior release5116ba92 CI44511028378 remains
preserved. CREATE-EFFECTS25bab774 still awaits its last hosted lane/aggregate in
44508278589. Neither is silently marked accepted. No local tests repeated.

WILDLIFE-SPECIES-LOAD-1 is independently scope-approved and RELEASED on901a8467.
Bind the original climate's32-entry u16 ring and numeric goods/kind facts, preserve
ring order/duplicates/unsupported selections, then call the unchanged pure selector.
Source-profile bounds must be established independently of unrelated kernel rows.
Use supported synthetic60001 rather than impossible90001 in a u16 source ring.
Stop at owned selection/creation plan; no fractional positions, live manager,
World, default/tag/identity/PRNG change. Executor pushes its own branch and reports
directly here for review. Main, shared boards and Windows/UI work untouched.

## Species source clarification during implementation

WILDLIFE-SPECIES-LOAD-1 remains in progress. A focused independent original-byte
review approves safe unmatched WARE retaining constructor yield0 only for fresh
generic selector descriptors, with explicit absent/matched/unmatched provenance.
Reused/overlay catalogues are excluded; the New creation binder's conservative
unknown refusal and full ring order/draws remain unchanged. No new owner policy,
packet acceptance or code review is implied.
[Focused primary review](reviews/2026-11-19-wildlife-species-ware-review.md).
Executor continues its existing task and returns the frozen result here.

## Species approved; ordered chunk planning released

SPECIES-LOAD f5f1c9fa is independently locally APPROVED by both reviewers. They
re-derived the original 32-slot row and five selection outcomes, including the
unsupported predator entry. Root verified all 178 closeout hashes; Debug and
ASan/UBSan each have 29 unique passes, with five actual mutations caught. Climate-2
selection is proved; other climates and live spawning are not claimed.

CREATE-EFFECTS 25bab774 is now ACCEPTED: all 12 exact-head jobs in CI44508278589
are successful. Corrected RELEASE-EFFECTS 901a8467 and SPECIES f5f1c9fa still await
hosted CI (44511432981 and 44515114872). Their local approvals are not silently
promoted. No local gate was repeated.

WILDLIFE-CHUNK-PLAN-1 is independently scope-approved and RELEASED on locally
reviewed f5f1c9fa. Implement one owned chunk's ordered retention, recycling,
candidate collision/replenishment and cleanup before materialization. Supplied
habitat and normal-success allocation are explicit assumptions. Full-list controls
must distinguish collision on an unoccupied record, reverse recycling, prepend
order and timestamp writes. Exhaustion remains noncommittable, not rollback or
success. No World/default/state/identity/global-PRNG change and no bypass of
Windows navigation or raster decisions. Executor pushes only its own branch and
reports directly here for review and continuation. Main and shared boards remain
untouched.

## Chunk C0 — exhaustion test boundary clarified

At contract4253dfbc, independent focused review approves the consistent-input
reachability proof: counted colliders and D(N) <= N leave enough distinct
noncolliding samples for every positive deficit. The packet no longer requires
an impossible full-chunk exhaustion example. The defensive guard and lower-level
exhaustion test remain, with their scopes explicit; reachable draw exhaustion
still requires precise partial-state checks. Original whole-game OQ-572 remains
open. Attempt writes precede lookup; timestamp waits for successful insertion.
[Focused disposition](reviews/2026-11-19-wildlife-chunk-plan-c0-review.md).
Executor continues C1–C5 and reports directly here; no policy decision, source
change, World integration or repeated gate is authorized by this clarification.

## Chunk reviewed; ordered species batch released

CHUNK-PLAN 95d2eb0f is independently locally APPROVED by both reviewers, with no
actionable findings. Root verified 110 retained hashes; Debug and ASan/UBSan each
have 16 unique passing cases. Complete ordered effects, partial diagnostics and
eight actual mutants support the bounded claim. CI44522148816 remains pending.

RELEASE-EFFECTS 901a8467 and SPECIES-LOAD f5f1c9fa are now ACCEPTED with all 12
exact-head CI jobs successful in 44511432981 and 44515114872. Their independent
reviews are updated. No passing local gate repeated.

WILDLIFE-MATERIALIZE-PLAN-1 is independently scope-approved and RELEASED on locally
reviewed 95d2eb0f: process a completed chunk in list order, apply exact eligibility,
bind source facts once per batch and plan species with cumulative draw accounting.
Supplied tile/cave facts and planning-only scope stay explicit. Unsupported selection
or missing required facts stop with diagnostics; no invented live spawn, coordinates,
manager, World, state/default/identity or global-PRNG change. Windows navigation/FP
work remains separate. Executor ordinary-pushes only its task branch and reports
here for independent review and next release. Main and shared boards untouched.

## Materialization review; finite portability correction

Materialization1c027079 preserves the bounded planning scope; evidence review
verified114 receipts and1023 frozen identities. Debug covers29 unique cases across
a preserved workflow failure and corrected remainder, ASan29 passes. No first-run
clean whole gate is claimed. Seven actual mutants distinguish intended semantics.

Predecessor CHUNK95d2 hosted CI44522148816 is FAIL: MSVC reports a narrowing
std::optional conversion; Linux Release failed apt setup before compiling source.
These are distinct failures, not permission to weaken gates. Both packets await
portability closure; neither is accepted merely on local reviews.

WILDLIFE-PLAN-CI-CLOSE-1 is RELEASED on1c027079 for correctly typed constants and
any same-cause changed-packet sites, focused checks and exact-head hosted proof.
No warning suppression, semantic change, old-history rewrite or broad repair.
Executor pushes only its task branch and reports here promptly; gameplay planning
resumes after the small correction is reviewed. Other lanes and main untouched.
