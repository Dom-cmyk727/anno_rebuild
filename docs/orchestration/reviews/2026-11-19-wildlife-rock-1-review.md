# WILDLIFE-ROCK-1 — independent review, 2026-11-19

Verdict: **CHANGES REQUIRED** for one P2 documentation/evidence finding. No
production-code defect was found in the bounded tile/variation/draw projection.
Do not mark the packet accepted or advance into FRUCHTBAR until this correction
has received focused re-review. No repeated C++ gate is requested.

- Head: `81e8f03f3bf882afe00272c27a8f1e0989f56283`.
- Tree: `5da2fdae92b74880c021527b1d7b55cd96ea6b86`.
- Tested source: `26899502717c4d6b2c61ed799bde0db8a7fe4c3e`.
- Source tree: `388a378ef6a733f43ec9f52660af45024a5a7a25`.
- CI: [44420622010](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44420622010),
  in progress at review; no terminal acceptance claim.
- Evidence: `docs/orchestration/evidence/wildlife-rock-1.md`,
  `docs/sim/wildlife-rock.md`; raw artifacts in `cache/wildlife-rock-1/`.

## P2 — the claimed complete writer span omits executed callbacks

The current ROCK contract/evidence and inherited wildlife notes describe the
writer as ending at `0x10082637` and use the fresh-profile observer bypass to
support a broader no-additional-effects claim. Actual matching AnnoWelt bytes
show `0x10082040` ends at `ret` `0x10082b0a`. It calls island `+0x19c` at
`0x10082ab9` and `+0x188` at `0x10082ad8`.

The independent primary reviewer read concrete vtable `0x100cac2c`:

- `+0x19c` at `0x100cadc8` points to `0x10084150`, recomputing derived per-chunk
  height bounds (island `+0x94` record bytes2/3) from terrain and anchored objects.
  The type height getter `+0x58` is pure AnnoGame `0x10007d50`.
- `+0x188` points to `0x1008f2d0`. Reached event `0xcb` takes table index2 to
  `0x1008f2f0`, incrementing the derived chunk dirty byte5. The World `+0xbc`
  branch is event `0xcc`, not this invocation.
- Empty-tile fill routine `0x100928a0` is slot `+0x1a0`, pointer `0x100cadcc`.
  `docs/re/transport.md` wrongly attributes it to the writer's `+0x19c` call.

These executed callbacks do not change represented tiles/variation or consume
random draws in the checked profile. Therefore this is a narrow evidence/scope
correction, not authority to change the engine, current numbers or tests. State
that derived chunk-cache updates are omitted by the transient projection and must
be rebuilt/provided before future consumers depend on them. The wrapper bypass
is not a bypass of these internal writer calls. Correct the stale transport slot
and exact writer extent. Keep the unrelated empty-tile fill separate, particularly
for the forthcoming FRUCHTBAR investigation. Preserve frozen-source hash receipts
and historical results; add dated corrections where needed.

## Independently validated work

Primary reviewer `/root/review_docs_sim_dispatch` confirmed actual mode0 highest
quantized cell minimum versus mode2 lowest, unsigned flag getter and stored
computed height. Tests/evidence reviewer `/root/review_town_ci_close` approved
the causal controls and receipts. Neither reviewer edited code or repeated long
gates. The orchestrator verified49 published hashes and276 baseline fixture files;
only nine authored rows append, with old rows and other fixtures unchanged.

The shared loop preserves ordinary mode1 and earlier mode2 classes. The actual
mode0-high-to-low mutant passes77 assertions and fails exactly three intended
height checks, versus legal80/80. Tests distinguish computed48 from raw207 while
preserving phase59, and do not invent a one-cell mode difference.44 reference
vectors and23 public controls retain fresh-directory bytecode isolation.

Independent raw HHbb reconstruction reproduces4,760 accepts,4,589 resource draws,
84 stones,63 cliffs,24 rocks and16 completed GELAENDE epilogues. Former42/39 is
height48 with two writes and no draw; both final anchors become1 after subsequent
partial placement. Next boundary is FRUCHTBAR header49 before any effect, not a
placed-object record. No wildlife spawns or full-world alignment is established.

Frozen gates stand: Debug21/21 and ASan21/21, each plus three separately required
contracts1/1, and one separate real-file Debug1/1. No failure/skip/empty/incomplete/
sanitizer outcomes. Strict affected warnings and unchanged dependency reuse are
valid within their recorded scope. NOT_RUN stays as disclosed in executor evidence.

## Disposition

Release [WILDLIFE-ROCK-FIX-1](../tasks/wildlife-rock-fix-1.md) on the same branch:
correct only these evidence claims, verify concrete targets, freeze/ordinary-push
and report back for focused re-review. Reuse unchanged runtime gates. A later
fertility-header packet must establish its actual loader/callback effects, not
skip metadata or treat it as draw-free without evidence. Main remains untouched;
Windows retains stacking/landing ownership.
