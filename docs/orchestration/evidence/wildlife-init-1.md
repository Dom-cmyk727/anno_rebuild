# WILDLIFE-INIT-1 — local research/reference receipt

READY FOR REVIEW, 2026-11-18. Branch `task/WILDLIFE-INIT-1`, accepted base
`2f9e4c0bf84cf09998a43c7ff8098fa1522f3a4e`; source `0efffe589fd1b63530dff62cab4ce1189ab5b652`,
tree `29232be0f242c7f393c0a24a4b628bb047a3a2a7`. Final report head and exact hosted CI are supplied in the direct
handoff. Independent review is the Mac orchestrator's, not an executor subagent.

**Whole-profile original RNG alignment: NOT_ESTABLISHED.** This packet identifies
one concrete faithful prerequisite, not an impossibility proof for every profile.
No alternative RNG phase or resource implementation has been adopted.

## I0–I3 scope and addressed result

The [contract](../../sim/wildlife-initialization.md) distinguishes the fresh
embedded-island candidate, explicit original inputs, the independent local model,
proposed rebuild persistence and unclosed caller effects. The packet and an
acceptance table were written/read before tracing; raw primary reads are private.
Decoder f1e245d2 and corrected helpers 533ee3fc were read by pin, never merged.

| Primary edge (module) | Result / limit |
|---|---|
| AnnoScene `0x10002e40`–`0x10002edd` | Fresh scene flag 0 constructs; reused flag calls Init; explicit nonzero seed sets world property 8 |
| AnnoWelt `0x100a3790`–`0x100a37bb` | -1 queries bit 0; other arguments set only that bit. Constructor clears bits 0/1/2; MAIN restores bit 1, not bit 0 |
| AnnoWelt `0x100a33c5`, `0x1008c2dc`–`0x1008c2e6` | Property 4 is u32 tick; island samples its low u16 once; / 500 conversion remains separate from clock-unit claims |
| AnnoScene `0x100026db`–`0x10002720`, AnnoWelt `0x100a2f8c`–`0x100a2fbc` | Scene/header stream order, not numeric slots or first-HIRSCH lookup |
| AnnoWelt `0x10094770`–`0x10094822` | Successful allocations only; file-order jitter assignment, per-chunk prepend traversal; no allocation-failure guess |
| AnnoWelt `0x10089301`–`0x1008949d` | Admitted GELAENDE group 225 resource draws at `0x10089468`; catalogue `+0x88` receives the draw; result updates separate island `+0x18` byte plane |

GOG module SHA256s and exact receiver/table boundaries are in the contract.
Catalogue receiver at island `+0x1c` is distinct from world at `+0x20`.
Four candidate AnnoGame tables point `+0x88` to `0x10001b30`; proving its concrete
dynamic receiver and reading its selector/closure is the next bounded edge.
The variation plane is neither growth phase nor spare base-tile auxiliary bits.
BuildCmd `placement_rule` rejects scenery intentionally; import must not widen it.

Representative: Tutorial/01Discover01, one embedded island slot 11, 156 HIRSCH
records (all jitter-enabled), no links/cities, player header kinds 8/2. First
nonempty GELAENDE header 34 precedes HIRSCH header 71. Type 70 / kind 226 at local
(59,29), 1 × 1 / flag 0x29 / four corners 48 passes the existing addressed
placement predicate on the constructor-empty cell. This proves a concrete
precursor draw omitted by current import. 4,589 is the number of parsed resource
**candidates**, not an actual original draw total. Native/HE runtime was not run.

## I2–I4 executable evidence

14 hand-derived JSONL vectors (12 load, 2 proposed rebuild restore), with a
derivation comment per vector. The authoring receipt never imports/executes the
reference to calculate expected outputs. All outputs, including timestamp/draw
assignment, effective goods and per-chunk lists, are compared. Arrays preserve
order; unknown effects refuse rather than implicitly consume zero draws.

26 CLI controls include malformed/missing/unknown/empty/exhausted cases, explicit
restore callback refusal and two exactly-once source replacements. Reversing
records assigns the first draw to x 2, yielding timestamp 2 instead of that record's
hand-derived 32. Replacing / 500 by / 1000 produces 0 instead of 1 at raw 500.
Both child invocations fail with exit 1, while raw 499 legal controls stay at exit 0.
These are local supplied-stream decisions, not a world generator or full-map oracle.

Configured dev registration only; no C++ build or sanitizer campaign. Canonical
command, jobs 1 / timeout 120 / required asset policy:
`tools/test/run_selected.py --build-dir build/dev --select '^wildlife_initialization_reference_' --require-test wildlife_initialization_reference_contract`.
The required reference contract, required picture_matrix_contract and main
reference each execute once, with no skip/empty/incomplete/sanitizer outcome:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

Direct reference: `14 vectors, 0 disagreements`.
Direct contract: `wildlife initialization reference contract: 26 checks passed; 14 vectors; 2 applied source mutants killed; legal controls passed`.

Python AST, source/file-link/host-path checks, lint and 70 lint self-tests pass.
Documentation-only clarifications followed the gate; executed Python/vector
SHA256s still equal canonical provenance (`focused-reuse.json`). No valid gate
was repeated for logging. All 119 existing sim fixtures and every production src/
file are unchanged; only the new authored JSONL is added. No pins regenerated.

Host: same Apple M5 Max / 18 logical / 48 GiB / macOS 27.0.1 as the immediately
preceding packet. Python 3.14; static reads use installed pefile/Capstone through
system Python (the isolated mac-tools environment lacks pefile; no install was made).
Game files were read-only. No original bytes/listings/decoded records are published.

## Implementation boundary and disposition

Faithful prerequisite OPEN: resolve the resource selector/admission, actual
separate-plane effect and native representation, plus bounded surrounding load
callbacks before an original cursor alignment claim. This is authorized faithful
work, not a request for the owner to reapprove the port. Only a proposed divergent
phase would require an owner decision; none is selected or implemented here.
Header/islands payload proposals have no numeric tags. Serial initialization
must precede parallel island building; feature off preserves current bytes/cursor,
and native reload restores persisted effective state without rerolling. The
contract's exact next-code checklist preserves the resource-plane/import boundary.

NOT_RUN: original/HE execution; native Windows/WSL suites; C++ build/ASan/whole
suite (unaffected); World import/native implementation; complete original load
cursor; resource selector/admission transitive closure and all-map profile proof.

Decisions and reasoning:
- Prefer fresh embedded islands and verify a real representative; do not infer zero draws from missing code.
- Keep bit-0 mode separate from MAIN bit 1 and raw stamp sampling separate from the / 500 conversion.
- Preserve source order while reconstructing chunk prepend order; distinguish original evidence from rebuild restore rules.
- Carry resolved intervening draws explicitly; do not hide the missing resource precursor or implement a dummy cursor burn.
- Stop at the contract/reference; let the orchestrator size the next faithful resource prerequisite without an unnecessary owner relay.

## Private receipt hashes

Paths are relative to `cache/wildlife-init-1/`; logs/raw reads remain ignored.

| Receipt | SHA256 |
|---|---|
| `focused.log` | `0561c56b0baac56fa1c81c7c0ba58c482290dc1c744e8c30069e079adc92966a` |
| `focused.log.inventory.json` | `5a3e617b63cd9e109931f686eb2ec3564710a5aafde913e997c2aaf932034fc7` |
| `focused-reuse.json` | `902b6aaa0068bdf52e5c84970daad3c55a854d46b1424e38e432a4725029ec91` |
| `reference.log` | `49f79016317e93bfef3dbe449c7df23c2788d2da2d44a428f057d3744158fa2c` |
| `reference-contract.log` | `b2bd52fcaa0cba341e586a3309179aaacea5f9010359be5cf45b0821f789d1dc` |
| `fixture-audit.json` | `179a62febf4e33c10f63982909b0aa8cf7b4467a4a1c7c988a014c1ffea5be29` |
| `acceptance.md` | `c74e972329bddc56da36bdfd54a50c18b893a5a245d2d41776170c6fbd819554` |
| `modules.json` | `6d86c1a3e0bbc1e5cf84b29c844e299c63d7db989283df2b83b90a3c0340d806` |
| `first-load-draw.json` | `a19c8ca8e102636c597ca00ddfc32624f600272cc7e59afede5ec96695958997` |
| `catalogue-next-edge.json` | `025a821dcf39e9ee76679827e8f21a09b12e2f12c7ba0a6a51905367c632f338` |
| `corpus-inventory.private.json` | `1189bf9863c0efa0264b7447d36b896ca0d60f1d4da9e422ec9d4e1c05a003a3` |
| `inventory-summary.log` | `0dcc55c7d0a9e1bc8b70c5354ba5fab0aeb13ae017150a4d2b01b04bbc85dbbf` |
| `configure.log` | `48bb26a361ec53de76f44dd6f578c2a7c68743526aa71dad3fbf208db4bb6ffb` |
| `lint.log` | `c90ca6eba541545a82beb573bfcbcc47531df735edd4ff68c814ab50296e6948` |
| `lint-self-test.log` | `25d5dbd23f182403d85606da5bdd4ceb5f592c1fa1516ac9754f8c8c9a303903` |
