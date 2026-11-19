# WILDLIFE-POSTLOAD-INPUTS-1 — bounded input-contract evidence

READY FOR REVIEW. Research source `625db996a2c320327476180ecd19f829f4c6460b`, tree
`fd7a20cf8b9e09bd23cacd39083330a46426e199`, from locally reviewed GFX `ad2a1a23bbc4fe877c18f0e7bf8f4ce0180270b5`.
Final publication adds evidence/report only; exact head/tree/CI accompany direct handoff.
Dispatch `392c0c007b7b793dc1f061df68febc26e87ee7dc` and GFX review read, not merged.
No src/tools/fixtures/CMake/CI/identity/default/World change or new public reference.

## Scope and primary findings

[Producer/type/lifetime table](../../re/wildlife-postload-inputs.md) and
[capability/acceptance](../../sim/wildlife-postload.md#10-first-call-input-capability--wildlife-postload-inputs-1)
are the result. Original modules are the packet's GOG reference installation;
SHA-256 and available PE resource versions are recorded in modules.json.
Missing Game/Scene/Welt version resources are null, not assigned an invented value.
profiles.json's module label denotes that reference installation, not a measured
PE version resource. The initial optional version probe's AttributeError note is
preserved; corrected inventory uses hashes. Misaligned/short exploratory windows
remain private and are not relied on instead of the aligned/full spans below.

D: Frame 2024e ->20108/20114 creates kind0x101 through Game13760 ->07650/065c0/03830,
installs catalogue215a4, checks init, then passes global9c268 as Scene factory
arg3 at207a6/291af. Scene1150 ->6ec0/7a00 ->2fc0 ->Scene+154 ->World+110
->island+1c is explicit. Factory argument positions are not conflated with
World factory's external fourth argument or base Init's third argument.

D/P: Game020a0 opens Anlagen.dat; 01d10–0206b reads ENTRY keys, resolves/creates
kind0x700 descriptors through063a0 ->c240, actual table21a88. Derived insertion
05f00 ->03260 stores pointer at214+4*key; id settera670 stores lowu16. Lookup37e0
has no bounds/null check. Supported keys2/11 select offsets21c/240, not adjacent
entries. Original4000 array/0x294 object sizes are not modern content caps.
HEAD flags and walk producers plus actual runtime getter targets are re-read;
standard flags29 means four unsigned corner samples, not descriptor float height.
Both raw HEADs first/shape2/3 ->code2; fish still preserves existing aux code/mask.

D: pointers are borrowed. Type parent attach stores catalogue at+c; catalogue
teardown03760 removes/detaches/releases owned descriptors after Frame releases
its scene. Reuse/injected suppliers are identified but not generalized into the
fresh proof. A modern immutable fact snapshot or retained shared const registry,
retained Terrain backing and owned output are required. Parsed HEAD/registry
cross-validation and successful FRUCHTBAR completion are new contract obligations,
not already implemented just because the existing registry can provide fields.
Raw fertility payload and effective ground/variation are distinct, low nibbles
must agree; all-zero completed FRUCHTBAR is valid. walk_class is not walk_code.

## Executed inspection versus unexecuted rules

Before inspection, hand-cases.json records asymmetric81 and23 expectations plus
15/16/31/32 boundaries. Existing anlagen_roads.walk_code and placement_ref.height_analysis
are used directly; no C++ output, new engine test, echo oracle or new public tool.

```text
PROFILE_CHECK profiles=2 corner_predictions=12 rotation_predictions=8 wrong_profile_controls=2 authored_walk_controls=2 status=PASS
Original runtime NOT_RUN; full grass/fish fill NOT_EXECUTED; existing Python helpers only
BINDING_CHECK module=AnnoGame.dll concrete_slots=8 disagreements=0 status=PASS
Original runtime NOT_RUN; PE table inspection only
```

Two actual wrong-profile controls change the helper's mean flag to0 and produce0,
not expected81, after valid inputs. Authored first3/shape0 ->3 and allzero ->default4
avoid a vacuous shipped-code2 comparison. Rotation predictions are8 values, not
8 CTest passes. PE binding checks are static, not original execution. No COUNTS or
VERDICT test-gate line is fabricated for this docs-only packet.

V1 capability rejection, V2 whole-word skip/full writes, V3 unsigned grass and
V4/V5 exact aux/phase effects remain H/NOT_EXECUTED here. Their getter dependencies
are now bound; this packet does not implement the composed callback. The proposed
next files/controls are explicit in §10, including an authored grass code3 and
nonzero old fish code/mask to distinguish wrong common-writer reuse.
First-call draw-free closure is limited to actual37e0/a660/08fae–08fdf/07dc0–07ea2
leaves and928a0 body under this validated profile. No catalogue-init, c2-zero,
second-call raster/navigation, global cursor or transitive session draw total.

## Verification and boundaries

Docs/links/anchors/diff/receipt checks only; no C++ campaign repeated. All277
predecessor synthetic files and runtime/tools/CMake/CI remain byte-identical.
GFX declaration/definition/tests remain its only callers. Other branches, jobs,
coordinator and owner's awake assertion are untouched. Exact-head CI read after
ordinary push; pending is not accepted/green.

Decisions and reasoning:
- Follow the concrete factory/array producer rather than equating a pointer store with identity.
- Reuse parsed owned metadata, add only the immutable validation capability the fill needs.
- Distinguish raw file widths/unsafe original indexing from widened defensive engine inputs.
- Keep present-ground capability separate from nonzero fertility and effective/raw planes.
- Preserve read/static/Python/H statuses and stop before any apply/wiring or unrelated port.

NOT_RUN: C++/reference CTest gates, original runtime/HE, complete grass/fish fill,
c2-zero preprocessing, navigation/V9 repair, raster consumers, global RNG/session,
World/native/state/lifecycle/defaults, corpus/performance, full pins/warnings.
No new implementation or pin move. QUESTIONs: none; remaining static/subsystem
edges are recorded as separate dependencies, not invented owner policy choices.

## SHA-256 receipts

Private source listings/raw data are not committed. The named hash rows pin the
frozen docs, unchanged consumers, private primary spans and executable inspections.

| Artifact | SHA-256 |
|---|---|
| `docs/re/wildlife-postload-inputs.md` | `3a1e5e48dce0168235c7c60f193f71789d549253e542f8fc37723bbf12accc1d` |
| `docs/sim/wildlife-postload.md` | `6bed72756aab1f90d89a19201462010a28a3bee084e93388c1fbbf1385d8612b` |
| `docs/re/fisher.md` | `ec486767cfe92ca4d5ba6c01f149a634450243a1afbc92afe9a5ff7c50835e0e` |
| `docs/open-questions.md` | `19e2ccccfb0e467bd6561766252c56e73a89f2e8477e1e16f4f3540fccab3cea` |
| `src/sim_load/src/original_registry.cpp` | `076278ebc3825fc4176986f788d43a0c1840aca49314f8421ed8ee06c0ce2d5b` |
| `src/sim_load/src/wildlife_prefix.cpp` | `7512970c1d1675a816b910c7a77ce779834d3e242bab278c1ec919599322d468` |
| `src/sim/src/types.cpp` | `a55309de55bd74fce7fe0382c97a45e1dc71ade27d7243c3334977263a6f88a0` |
| `src/sim/src/walk_cells.cpp` | `82ff22ee0daceefd8ead1e29efcf2ba9036bf8c47ff8352fdd0064dbcb947c47` |
| `src/sim/include/anno/sim/types.hpp` | `ee9f19a7b973185041068723db128d9e964b46e264d505b47b94f67b22c8cbc0` |
| `src/sim/include/anno/sim/resource_import.hpp` | `0f2055f910ab609db0505430246fdd3981f4b00a3d44b68bd058ffb904e8dab5` |
| `src/sim_load/src/wildlife_graphics.cpp` | `b585749f33b31fd5da6215cb8a0618851ec382d0a7d70b1e5e8744310d4322fb` |
| `tools/anlagen_roads.py` | `b44c16fb7fa8d1d4882025718438792acb5ccfc53a0822f4b74d703a0a3ba608` |
| `tools/placement_ref.py` | `15fcf70c91f95702af6f3a0769ece0ec8cd1af38a01e4667394a7207fd7f42f3` |
| `cache/wildlife-postload-inputs-1/scene-6ec0.private` | `5e2e4480b7fdbe7c298e3519326d6fe3104b083121d021969293827b2a002d4e` |
| `cache/wildlife-postload-inputs-1/scene-7a00.private` | `b8ae7feb7bc71d259cf37ed2e255af65bd8786bee79dc31119cbbbccd097fa0a` |
| `cache/wildlife-postload-inputs-1/scene-factory-full.private` | `971c14da99d8f54f0deea188433e7ce432e12edea3eb89abc15c317deac21b38` |
| `cache/wildlife-postload-inputs-1/frame-scene-callers.private` | `0d7bc16a7305c88fbfa29374c84bc85b3f93fe95a26c9b3a1630f2e71e1382b8` |
| `cache/wildlife-postload-inputs-1/view-scene-callers.private` | `7e6f840253c9cc92ab5a69e71f5a27030d04217b959bf6d8cebd49e262a5f8b0` |
| `cache/wildlife-postload-inputs-1/frame-thunk-callers.private` | `145a1931764f9b987ee4232ae14425a334fb0544d730056da5ba512462b462f4` |
| `cache/wildlife-postload-inputs-1/view-thunk-callers.private` | `d8a637ce3aff811c1002fed0668a46d2e3675ef3cce269242b1f629970c80f2c` |
| `cache/wildlife-postload-inputs-1/frame-scene-primary.private` | `acd85d66b4606527a67c83bdf32ff8863b1c36a7b7723209dc89dcb1321ecda8` |
| `cache/wildlife-postload-inputs-1/frame-scene-second.private` | `7f03ee9ef46a9f78ee920b8a866be31c6db49f0a5d5184c84042e2a56b25c1aa` |
| `cache/wildlife-postload-inputs-1/frame-scene-reuse-primary.private` | `4c00660f248f5412293cc535c53da114bf2700b141227c38aae44a5bd2332919` |
| `cache/wildlife-postload-inputs-1/view-scene-primary.private` | `40b2b7e254228ef546dbc054c9d6a5297a673043f97a1ac1de76522bff2129b9` |
| `cache/wildlife-postload-inputs-1/frame-create-catalogue2.private` | `ed75b6a12b62de9c37d0623e853c063d4f55f01118c0fefeecabf13b928b4b86` |
| `cache/wildlife-postload-inputs-1/frame-init-caller.private` | `6f723ee9ad4fed9f0c94a8a727794e58da88314caadafa497433285a0b03af56` |
| `cache/wildlife-postload-inputs-1/frame-release-primary.private` | `412fb9ce30ecf84780d25fc583372e58c5182dfb1c5d6bd1d0d8d263fa06ea62` |
| `cache/wildlife-postload-inputs-1/frame-reuse-store.private` | `93c952f7abee0dcb2319c68bfe06a7d601b2842b82f2e983ff115c4e95f55b5d` |
| `cache/wildlife-postload-inputs-1/game-factory.private` | `64b52315df59ef6701c2ed6f989b1a91a509fe06b7919bb21287123059d7ab3b` |
| `cache/wildlife-postload-inputs-1/game-derived-constructor.private` | `df57251b3dccce2a6cf087755ddbb84e47157f802d74ae00731398c6d663e870` |
| `cache/wildlife-postload-inputs-1/game-base-constructor.private` | `735bc9a079c60026de1ed98e659eeff53114b064ce35e93fcc0a88d4484ff9cc` |
| `cache/wildlife-postload-inputs-1/game-array-insert.private` | `e192a4ebd007592178ed2f0a379875fe086c463eca9411b79abe5e270ddbc938` |
| `cache/wildlife-postload-inputs-1/game-derived-insert.private` | `1a7dfd3023191735ab5e3bc94c8ad403fb11167215d0c0f945079dee014ac2af` |
| `cache/wildlife-postload-inputs-1/game-insert-tail.private` | `34dc2fbe9ddacccc4f27692cc5108772ecdabb3d4b453ef4bc38917d5e31a32f` |
| `cache/wildlife-postload-inputs-1/game-real-type-constructor.private` | `ac83052b160ff7e89c7d2251ec06af129f222d2715e95bcad7614c1afa424e3d` |
| `cache/wildlife-postload-inputs-1/game-type-constructor-full.private` | `835285b2fcbcc145bb38196ac3c223912b6469edf50b26333b246bafbb1b33dd` |
| `cache/wildlife-postload-inputs-1/game-anlagen-open.private` | `a548658a58159388068df401e4221cd68bb356675af69fa5098a040933d6b155` |
| `cache/wildlife-postload-inputs-1/game-chunk-load.private` | `ba24c3142c2a2a565bf65a918de05ff4f39d59bca94f61b94654041ba659f130` |
| `cache/wildlife-postload-inputs-1/game-derived-load.private` | `867a9c0107dd2167a0e626452e5b242eea8d4cb8b053efe884f95d8fc4333502` |
| `cache/wildlife-postload-inputs-1/game-init.private` | `5e02333554095d9c2d51a720d5fdc6c58a586d36a78ed2193953ab68da4f7b6e` |
| `cache/wildlife-postload-inputs-1/game-init-base.private` | `e28760bcc7ac9fa29575c5b4bb439dbf4ae275641db125eb52bf9f3244f59db7` |
| `cache/wildlife-postload-inputs-1/game-load.private` | `fda73721abf08cd80116fedcff3d66e61ab7ee1587ac252d83845882845b9752` |
| `cache/wildlife-postload-inputs-1/game-id-read.private` | `57d4a749173633369fe94fda9e106e86d74b6fe629140061cc0a4079b38c950f` |
| `cache/wildlife-postload-inputs-1/game-id-write.private` | `041a1846e011ef6914f5ff070361bfdc840f8390d1885c314ae0fec17f855ee1` |
| `cache/wildlife-postload-inputs-1/game-type-parser-fallback.private` | `dc6366c50a5a1fe59a4581bf9bd21a43b07d6cef59c103ad15bfc3e21e01a4f8` |
| `cache/wildlife-postload-inputs-1/game-record-reader-full.private` | `6fe5a5db1b7fe8266c41ef9c0cba67506db6a96c7378c1973f7f3b4ffe20dd48` |
| `cache/wildlife-postload-inputs-1/game-mean-getter.private` | `e6a9a6ae740a7693a2d6bf9a36cafb6a720a085da9c0366bd90c5eb9f459aee4` |
| `cache/wildlife-postload-inputs-1/game-walk-encoder.private` | `cca315f54d568cdbd524dcea412c995370dc83243648edbd2ee21e961ae54778` |
| `cache/wildlife-postload-inputs-1/game-walk-getter.private` | `0fa67531614a4fafc72f352aea1b3e707ccd0ee813c2661cc34c7c5f80f9eeb8` |
| `cache/wildlife-postload-inputs-1/game-type-parent.private` | `19ae7effef3f17f97b10711df7838ebe31b642f3deac4cb494a6655ba35be372` |
| `cache/wildlife-postload-inputs-1/game-base-release.private` | `1cf99de2ea26dcfdd339daadc53194b7287ff89a2cc07b5bd9709e624966ec39` |
| `cache/wildlife-postload-inputs-1/game-middle-destroy.private` | `30d515fe9f21d2ad9a24b7f07ae714a0a75f1e3541d6f79fad1ef8e9e3de250c` |
| `cache/wildlife-postload-inputs-1/game-delete-thunk.private` | `3c78c20737e26c859ff40f40e5a8851e486baed714aed1d7bf2a15991643516a` |
| `cache/wildlife-postload-inputs-1/game-derived-destroy-primary.private` | `465c09d2af6a13dc01eb6f2d8522312e873c204375d39bcff71fffc1e6d0c1a8` |
| `cache/wildlife-postload-inputs-1/game-property35.private` | `240234d2e1a573a859db3541d16d6adf2ba85eb107830cd62db662ee64c03137` |
| `cache/wildlife-postload-inputs-1/game-catalogue-postprocess-full.private` | `22574cfa7c73057432e6452344762eb08fbf9639f3ee247eeda1e09bd3598beb` |
| `cache/wildlife-postload-inputs-1/packet.md` | `9f90a4b03aa67be25e485aae982d3c3c804b5ca54ddeb52fa8625fddcd50f3fc` |
| `cache/wildlife-postload-inputs-1/gfx-review.md` | `5f54247ca47b80b811f57a7698ef934031457ab3eeb1dbbaedb908ecdc18a58e` |
| `cache/wildlife-postload-inputs-1/intake-sha256.json` | `a29787578f628dcc0dee8e325dedd395338f63a94deb4660fa74ab3e77ad72b9` |
| `cache/wildlife-postload-inputs-1/hand-cases.json` | `e900e83df045f63f57c862f3998000e30cfc311990e9055f0aaf1c43ad0c9290` |
| `cache/wildlife-postload-inputs-1/inspect-profiles.py` | `5435e77dddcaf379aa88e99057b93c8429d2314d35936c4855017be05920a8b6` |
| `cache/wildlife-postload-inputs-1/profiles.json` | `48877df8d8639cc989954db269af041a79f2bb8c749279768d104d076a92153b` |
| `cache/wildlife-postload-inputs-1/profiles.log` | `4e0306cd1d1ca29a4b63fe9fe081b4775dcba4db305cb8f87e961d1ec1ab085a` |
| `cache/wildlife-postload-inputs-1/bindings.json` | `4514444e28c0c70f033d12d09c732d3805b91e8ef50205033cb3a90f0ac5ba4a` |
| `cache/wildlife-postload-inputs-1/bindings.log` | `053396dd3554e92b3052c2c7157b6504b51dd4bd42947cbbb9805c1cc4fbd11b` |
| `cache/wildlife-postload-inputs-1/modules.json` | `78301d89d644fc90b35c647e9a54646e56ccf3cadf462d5581bc52e68338fe8d` |
| `cache/wildlife-postload-inputs-1/module-version-first.log` | `fcd01888c0156440ae3412ec1f608ff29479b1f7ee43920e5a329a4b622685c7` |
| `cache/wildlife-postload-inputs-1/scope.json` | `b6e3482f424c4dfc947720a0d45023bf9c8af682ff4ec1ae0ea07545ef67d5e4` |
| `cache/wildlife-postload-1/fill.private` | `e6d449b3013d7d629fbf0fee33e79527bc485bb89e316ec5ca15739ec5df6a43` |
| `cache/wildlife-postload-1/scene-catalogue-input.private` | `c652c7342ed7db00da7a572e170a4f67dec0506bb501b92a35c6f8f13a62934b` |
| `cache/wildlife-postload-1/scene-catalogue-setter.private` | `f791a3125f482071b34773fa3c3d4ce8d1e08bd8df216a6f9f46aefedfbd324a` |
| `cache/wildlife-postload-inputs-1/docs-check.log` | `5f0b9f83295e27cd63d0d6ac068a8848fe776eb374eed0ec21de319976066926` |

Final docs check, after the 71 artifact receipts above:

```text
DOC_CHECK files_or_changed_sections=6 links=22 hashes=71 missing=0 status=PASS
SCOPE: docs-only; 277 synthetic fixtures, src/tools/CMake/CI unchanged; GFX remains unwired
EXECUTION: raw-file/PE and existing Python inspection only; C++/CTest NOT_RUN; full fill NOT_EXECUTED
```
