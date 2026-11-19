# WILDLIFE-NAV-INPUTS-1 — private reproduction receipts

READY FOR REVIEW; independent orchestrator review pending. Base/tested production
source `889143ba1485a2fed72c03bd648c9c39878c4f06`, tree
`37c1194ba88d3017893246e00d680d54427972c8`. Final head/tree and exact-head CI
are supplied in the direct handoff. All committed changes are documentation.

Read exact dispatch `09fc0c6afda4b6ced5106b99f746fae6a2abcfae` from orch/mac-lane,
including FILL review; no board/main merge. See the addressed acceptance table,
legal setup, original contract and repair options in
[wildlife-navigation-inputs.md](../../sim/wildlife-navigation-inputs.md).
All primary instructions, authored private programs, executables and raw logs
stay under ignored `cache/wildlife-nav-inputs-1/`; no original bytes in git.

## Original proof and setup

Primary Welt producer 0x10084150–0x10084360: 9 × 9 vertices; 8 × 8 anchors;
component-only byte mask, widened sum, full maximum halved into a byte.
Game common slot +0x58 ->0x10007d50 reads descriptor +8, populated from HEAD
at 0x10009372–0x10009384. Division/truncation helper and divisor rechecked.
Builder 0x100ab0b8–0x100ab0e7 and shore 0x100ab94b–0x100ab966 read those halves.
Fine query 0x100904e7 is a distinct byte-add rule, not a repair target.

Hand-derived expectations were written before execution. Author-owned entries
go through TypeRegistry::build, then real resource_import::evaluate and its
placement writer; six rows accept without draws and verify exact anchor/base.
No setup field is injected to bypass admission; no item_base override. The
island/terrain/registry owners remain alive. The explicit adapter seam is tested,
not World::create or full original runtime. Flat-18 V9's literal base 250 remains
unproved; its legal mode-2 companion uses base 16 + component 254 = 270.

## Actual direct executions, not CTest acceptance

| Program / log | Child exit | Exact terminal output | Interpretation |
|---|---:|---|---|
| probe-current / probe-current.log | 1 | INVESTIGATION executed=13 mismatches=3 setup_refused=0 | Initial unmodified archive-linked result; preserved failed investigation |
| probe-canonical / canonical-run.log | 1 | INVESTIGATION executed=13 mismatches=3 setup_refused=0 | Fresh canonical route_world TU; same failed investigation |
| probe-sum_only / sum_only-run.log | 1 | INVESTIGATION executed=13 mismatches=3 setup_refused=0 | Correct sum masking alone leaves all three exact-byte failures |
| probe-corrected / corrected-run.log | 0 | INVESTIGATION executed=13 mismatches=0 setup_refused=0 | Both arithmetic sites corrected privately; all expected results agree |
| probe-no_component_mask / no_component_mask-run.log | 1 | INVESTIGATION executed=13 mismatches=1 setup_refused=0 | Overcorrection loses required component wrap, caught by component 256 |
| geometry / geometry.log | 0 | GEOMETRY executed=4 failed=0 | Final border zeros, interior, misaligned origin, nonaligned width |

Each 13-row run covers six imported object profiles plus seven ordinary/equality/
far-corner controls. Sum 256 and 270 are wrongly water instead of mixed, with
shore absent instead of present; sum 510 has the same class but the wrong byte.
The seven controls do not fail. No failed investigative output is relabelled a
green gate, and no permanent test records the wrong arithmetic as expected.
Private variants replace exact source occurrences once, compile to private
objects and link BEFORE the unchanged dependency archive. Canonical source never
changes. Exact command arrays and binary/source hashes are in causal.json.

Build-only first probe used a 16 × 16 rig, never executed. Before first execution
it was enlarged to 24 × 24 so a high-terrain neighbour's forced-zero border
would not confound the shore predicate. Both build receipts remain. Fresh
canonical route_world.cpp compilation removes uncertainty about its archived
object. A dry Ninja invocation only found pending CMake regeneration; no rebuild
or canonical build gate is claimed. Dependency archives are frozen by hash.
The initial shore start 0x100ab920 was mid-instruction; that exploration is
explicitly named NOT-EVIDENCE. The aligned read starts at 0x100ab923 and agrees
with the earlier complete-function listing. No claim relies on the misaligned read.

## Documentation correction and unchanged scope

First docs edit withdraws FILL's controls-first.log retention claim. Search found
no retained failure stderr/console receipt. Only the historical note survives;
the empty log and SHA e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
remain unchanged. Failure not recreated, successful gates not repeated. Of FILL's
51 published receipts, 50 current files still hash identically; the only changed
one is wildlife-postload.md's authorized NAV research append, whose old git blob
still verifies its original receipt. Detailed audit is retained.

No committed src/tools/fixtures/CMake/CI/state/tag/default/registry changes.
First-call fill and graphics remain unwired, unchanged. No pin or recording
changed; this is a byte/scope audit, not a full pins.py execution.

N4 checks: git diff --check; changed-document relative links and SHA receipts;
scope audit versus exact base. No public evidence code was added, so no new CLI
contract or CTest suite was required. Exact local docs-check output is appended
to the report; hosted CI is read after push, initially pending, terminal outcome
reported separately. No fabricated COUNTS/VERDICT lines for private tools.

Host Apple M5 Max, 18 logical CPUs, 48 GiB, macOS 27.0.1; Apple Clang 21.0.0.
Private probes run one at a time, small 24 × 24/16 × 16 islands, no suite load,
thread/admission/coordinator changes or GPU. Host contention and command record
retained; owner's awake assertion untouched. No memory refusal or timeout observed.

## Compatibility disposition

RouteCache/CoastIndex are derived, but production ShipMover planning consumes
their topology/costs and writes saved/hashed NavMotion. Identical layout/digest
does not prove replay equivalence. The contract proposes a coordinated faithful
base correction (recommended), optional gated correction, or temporary isolated
postload-only correction. Owner/Windows semantic-identity decision required
before production repair; no version/tag allocation here. World +0x120 secondary
raster and its consumers remain separate unresolved composition prerequisites.

NOT_RUN: CTest suites/dev/ASan/full pins/warnings (docs-only packet), original/HE
runtime/emulation, shipped-content overflow census, Windows/WSL, full World/save/
replay trajectory proof, performance, secondary raster, complete postload wiring.

## SHA-256 receipts

The provenance manifest records 43 source/input/program/log/archive hashes; the
closeout manifest adds command/host/audit identities. Original files remain private.

| Private receipt | SHA-256 |
|---|---|
| `cache/wildlife-nav-inputs-1/provenance.json` | `ff4b96db53624380fbfbe1cd6f917dc21f1a86def7114f3528cca64ecf56f17a` |
| `cache/wildlife-nav-inputs-1/closeout-sha256.json` | `66a71de13c435a095f6fc33f769af6840f456f3a293984fdda28b26840e45112` |
| `cache/wildlife-nav-inputs-1/commands.txt` | `176b5fbd07b45c44af9a155619922fd9859fcba7f4b17b7159f23e028a7aa331` |
| `cache/wildlife-nav-inputs-1/host.txt` | `f05ff48fe22adac10a560c46b03719801ca6b4155b725dd0e29d95621a2ed5f7` |
| `cache/wildlife-nav-inputs-1/fill-receipt-audit.json` | `71bf9857e6284b96630e3978315ef006f44e7ee722c824025eb8e8a719b7e037` |
| `cache/wildlife-nav-inputs-1/probe.cpp` | `00a2566250155267e8859bfd435c1d1c2dd700d1dc46d04b9ba6d9edbf97077a` |
| `cache/wildlife-nav-inputs-1/geometry.cpp` | `1a0bd81765fc8e18211c072189427b78f4ad76476d17cfa5826f75debe91ea1d` |
| `cache/wildlife-nav-inputs-1/hand-f32.json` | `7d9419c7e4d5ccb549d71fbdd97a8bb4f66ba78fc02e146eb72832f2097bc568` |
| `cache/wildlife-nav-inputs-1/probe-canonical` | `fb19cf8a47d4507b847d50cfe0abada8bef3fa18879c52d3bc59b572abef73c5` |
| `cache/wildlife-nav-inputs-1/canonical-run.log` | `8fcf10d5bc71d7c3cf90d5c26ca89bb12fc7068305e6d716f8cea0b169456a04` |
| `cache/wildlife-nav-inputs-1/sum_only-run.log` | `d0ce525d2624fd03249367f6b9243f8e772f077a1c03525db520b0567bbfd160` |
| `cache/wildlife-nav-inputs-1/corrected-run.log` | `8fc9fefa9c862a67c6321cd27b18a0f318fc31e2dcd3a342934610d3d41a22c6` |
| `cache/wildlife-nav-inputs-1/no_component_mask-run.log` | `cc21c2e0d85a785f30b5ad4caf145535cb77969b1b4bc0afde4d59acc28956a6` |
| `cache/wildlife-nav-inputs-1/geometry.log` | `a18e6c92e1e0f7573a154f2454a7769644ef70d6ad31ff0c065105a91860dba3` |
| `cache/wildlife-nav-inputs-1/causal.json` | `d5a1110d9240fc42a909efdbe18688ea143f6318877bfc0d551021b68ff5ea98` |
| `cache/wildlife-nav-inputs-1/chunk-extrema.private` | `075616a44aed62830b0a5d6a508434b07cde7fbf1619d678a0a377232821e9ba` |
| `cache/wildlife-nav-inputs-1/head-reader.private` | `c73356bc0bdfe22c341e5ebd478a9f22d986bae3bd80bfe99156ef32b344df6c` |
| `cache/wildlife-nav-inputs-1/height-getter.private` | `7da8c2b3fc9ca32a7f9ad77ca2e2882711c40d50cf7517fcf9cfaf9d622b8344` |
| `cache/wildlife-nav-inputs-1/ftol.private` | `937c3299487fc112963a7810897dc1f52ed42b4749f1f23ae3967907fe8c6e0d` |
| `cache/wildlife-nav-inputs-1/island-init.private` | `efaad5a304e33e793b10f23753ca731f882d5607bb1971ce7950f0275d906709` |
| `cache/wildlife-nav-inputs-1/classification.private` | `e5d3d306ebc34618cc20349e526efb970fc0ac29ee7e8cdf9d57518a1e8377af` |
| `cache/wildlife-nav-inputs-1/shore-aligned.private` | `6d7b23ed03d55e477858bf98d3ae34299bcd3710e46237a230be01b07897d115` |
| `cache/wildlife-nav-inputs-1/fine-query.private` | `9cab4419a273a335a4780df5c2cc1501d81769498aeaef2b050a5d7f9aa1f8c0` |
| `cache/wildlife-nav-inputs-1/docs-check.log` | `1a9387f0c324eed198f5b4ff50f60cac12f2dd8408af26468f0a40587eaeb8a8` |
