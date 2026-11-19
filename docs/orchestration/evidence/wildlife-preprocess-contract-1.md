# WILDLIFE-PREPROCESS-CONTRACT-1 evidence — 2026-11-19

Research only; READY FOR REVIEW. Base `aec8bcc8fc1a35230dffba91c8e8b34c35f31bf3`;
exact dispatch `2da44e19b6602aaebc0c328cdaddbc0d71103b03`.
[Contract and acceptance table](../../sim/wildlife-preprocess.md).
No code/build/test fixture/default/identity change. No original bytes or listings
are published. The table below identifies private local receipts for review.

## Primary scope

- AnnoWelt SHA256 `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`.
- AnnoGame SHA256 `ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc`.
- Both freshly hash-verified against read-only game files; original reference GOG 2.0.0.5.
- Full aligned preparation span 0x10084370–0x10084818; wrapper, load/save
  rectangles, getter/table bindings and growth/INFO producers retained privately.
- `actual-slot-callers.json` filters executable CALL memory operands, not all
  displacements. Stack matches in the exploratory broad search are excluded.
- `exploration-limits.txt` identifies misaligned exploratory prefixes, an adjacent
  table consumer and the first stale jump-index JSON; none is primary evidence.
  Correct property 0x1c dispatch index 2 is in `property1c-target.json`.
- Six production building tables share the addressed pointer getter. Other
  dynamic profiles remain unbound, not implicitly pure because a result is dead.
- Full fresh/matching load and save rectangles are supported. Arbitrary partial
  callers are not certified; a proper partial can read uninitialized scratch.
- Kernel seven-profile extraction is private, image .rdata; total table extent
  remains unproved. No public original coefficient array or derived map.

## Hand derivation, checks and initial failure

The contract's authored expectations and hand-expectations.json existed before
model.py. The model is a conditional integer transcription, not an oracle
executed in the original. 17 positive checks = 4 quantization + 3 blend + 10
smoothing/boundary cases. Six actual wrong interpretations distinguish aggregate
division, missing centre guard, byte product, whole-byte merge, using the output
plane as scratch, and signed growth bytes. Outputs are retained in the log.

```text
INTEGER_MODEL executed=23 failed=0 original_execution=NOT_RUN
GROWTH_FILE ids=146 short_rows=1 line=765 parsed=3 retained_staging_cells=3 original_execution=NOT_RUN
FILE_PROBE checks=2 failed=0 original_execution=NOT_RUN
```

Initial `model.log.first` has 16 positive passes and two passing wrong-rule
controls, then `WRONG_byte_product FAIL`. The expected distinction was wrong:
26*255=6630, but wrapping the product to byte gives 230; both results clamp to 10.
No terminal PASS summary was produced. AssertionError was observed in the tool's
stderr; the retained file contains stdout only. Corrected the written example
before adding S10: 26*10=260 versus wrapped 4 gives aa versus a0. S5 remains a
legal saturation control; S10 is the width counterexample. Final 23 checks pass.
The first source/expectation/log are preserved rather than replaced with a
recreated failure. No production mutation or product failure is claimed.

The separate finite producer probe parses only the supplied Wachstum numeric
fields and preserves staging across Id records. It confirms one short Proz row,
three entries parsed and three retained. Its generated 16 KiB lookup is explicitly
a source-derived MODEL, not original memory. The other check verifies kernel
bytes are in the image's non-writable .rdata section, extracting seven profiles
without asserting that no others exist. OQ-029(g) is narrowed, not the entire OQ.

## Commands, host and boundaries

- Read-only extraction: `python3 tools/disasm.py <module> <address> <length>`;
  Capstone executable-call/field scans and PE relocation/import/table reads.
- Private model: `python3 -I cache/wildlife-preprocess-contract-1/model.py`.
- Finite file probe: `python3 cache/wildlife-preprocess-contract-1/file-probe.py`.
- Docs gate: diff whitespace; resolve new local links; verify all private manifest
  hashes and published receipt rows; restrict changed paths to the six docs.
- Apple M5 Max, 18 logical CPUs, 48 GiB, macOS 27.0.1 build 26A434; 1.5 TiB free.
  CPU consumers were desktop/browser/virtualization; no heavy gate launched.
- Existing caffeinate PID 27411 remains running. No coordinator/job changes.
- NOT_RUN: CTest/build/sanitizers/full corpus/full pins, original/HE execution,
  arbitrary partial runtime calls, unknown provider/profile paths, Windows FP
  capture. These docs-only checks are not selected/executed CTest counts.
- C: existing sim_load/wildlife_postload_fill.cpp lines 59–70 require completed
  FRUCHTBAR. Proposed absent-column capability therefore needs separate typed
  validation/composition; it cannot be passed through the current validator.
- Independent review is the orchestrator's at handoff; no executor agents.
- Proposed transactional refusal differs explicitly from original allocation
  failure followed by unconditional fill. No live wiring is authorized.

## Docs closeout

`cache/wildlife-preprocess-contract-1/docs-check.log`:

```text
DOC_CHECK files=6 new_local_links=13 manifest_hashes=67 published_hashes=25 missing=0 scope=docs-only status=PASS
```

Log SHA256: `13826d2dbec493bc15d3d00abc44054f3ca5a42094ce76fa13b937f13a18cdff`.
The log is excluded from the private manifest to avoid a circular self-hash.
No CTest run or fabricated test-count reconciliation is implied.

## Private receipt hashes

| Local path | SHA256 |
|---|---|
| `cache/wildlife-preprocess-contract-1/preparation.private` | `aa87bd55e4049189b912dc6b9171d46dc14ccfd707477a3db4bf4df15e687880` |
| `cache/wildlife-preprocess-contract-1/wrapper.private` | `81d43e5c88b9ad0e6c481e958fc2916afd3f7463e35f8175dcb0445dc690adeb` |
| `cache/wildlife-preprocess-contract-1/actual-slot-callers.json` | `3eb87194f9cba2a4712420db34de4b137d46474a0e4358a46cbad2f48f3dfc68` |
| `cache/wildlife-preprocess-contract-1/actual-slot-context.private` | `f8f8ded3cbb183b1826c67050b609bbdd8ca28c1ec042dcba796ac139522935d` |
| `cache/wildlife-preprocess-contract-1/save-full-prep.private` | `947e5a145d135b0f070619b2746c00b837d0038bd1ad845427cef1489eb3d03c` |
| `cache/wildlife-preprocess-contract-1/building-type-slot-bindings.json` | `f16a12f7a975e1ad56b21f4c3fcb5e100a80e6ac80b61be4f89a128b794132db` |
| `cache/wildlife-preprocess-contract-1/building-type-getter.private` | `88797c7a698dbf79e9cbbc4eda9d27f950af29b919e66963818e177c40affc6d` |
| `cache/wildlife-preprocess-contract-1/catalogue-property.private` | `4188e23fef8bd10c6d7294560fe5e8a0ff9e327b881aa570ccf2cd05fe7dfab0` |
| `cache/wildlife-preprocess-contract-1/type-property-2e.private` | `a182d77fc20d028d11c1c69fcac96258b715a705af39a1a94d1c0c82365b4343` |
| `cache/wildlife-preprocess-contract-1/growth-binding.private` | `e1beb59a0efb5daf445f464f29cb73b87e60a2bc1d5504df5069d471edcfe3be` |
| `cache/wildlife-preprocess-contract-1/growth-loader.private` | `69eb309119cc35234732b2c96117575cc09e1ff3267a50c3c6568ba80c0eafd5` |
| `cache/wildlife-preprocess-contract-1/number-list-parser.private` | `530d1d5775d4a3a74f4f30dd4ce1f0d54bc96b89a5e1c745cd924a3088df47fe` |
| `cache/wildlife-preprocess-contract-1/climate-load-lead.private` | `bd1eb1d158c2027eb690f269898a913da2b09f1260e834019be3587ba6395132` |
| `cache/wildlife-preprocess-contract-1/constructor-zero.private` | `56650f526a70b79d6f68c458b2a38ab323bc04c1f7c153035d23598191cc5917` |
| `cache/wildlife-preprocess-contract-1/allocator-heap.private` | `f568c67f9ce16f47f36f7c5bcd3048e396fd59125ffe4d00e08eea32e6aabe00` |
| `cache/wildlife-preprocess-contract-1/hand-expectations.json` | `dd79323f86e2d8f51d313451e243ea903a2cd8d421944e9b841e363abcd673b0` |
| `cache/wildlife-preprocess-contract-1/model.py` | `5dd757844e624a752e43126cbeb2be445c3ddfd5c8894ec91f99d7192249f694` |
| `cache/wildlife-preprocess-contract-1/model-final.log` | `b4b5a1d653bfdb6788d8a6fd4b75602b9337cd0e95d0eaf9f84957f0936c8085` |
| `cache/wildlife-preprocess-contract-1/model.log.first` | `ccaf2151320e0f7d0f67572d066620cf6a95ee8479b9d642a951e0989b723ffe` |
| `cache/wildlife-preprocess-contract-1/model.py.first` | `2f1358c3b0ff2600f735c35e3490ba95c2b85d585523a57a2a228854bcdf2e36` |
| `cache/wildlife-preprocess-contract-1/file-probe.py` | `c8eb648d074af54a13849863a2176896f63b16d1fe5ba027225aa63c476579c1` |
| `cache/wildlife-preprocess-contract-1/file-probe.log` | `5f078bc5b908f402285d345866cf59d532468a1d0f11f4d0f78a2059e0996893` |
| `cache/wildlife-preprocess-contract-1/exploration-limits.txt` | `e8f0f190c57413d2daf5fc9339ebb1b25fef2bb2e046c43cb65a6c05970de225` |
| `cache/wildlife-preprocess-contract-1/sha256.json` | `118e414206f8adb88da610c518d615bd95abb4ba0d13b27154ec9e8c6a93f686` |
| `cache/wildlife-preprocess-contract-1/reset-zero.private` | `71bf1c2268391403c4b2254d52be6dbec05befe7bdbc8df13322f130956d2d50` |
