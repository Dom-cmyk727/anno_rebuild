# WILDLIFE-CREATE-CLOSURE-1 evidence

2026-11-19, READY FOR REVIEW. [Addressed contract/delta/field and draw ledger](../../sim/wildlife-create.md).
Branch task/WILDLIFE-CREATE-CLOSURE-1 from reviewed
7d2df0fc5c94786372960000b1c7fe8d3a28dee2, exact dispatch
2c41b46d1baf50ec8c4fb93c4ff0699938fb29ef. No board/main merge or engine code.
Independent orchestrator review pending; predecessor hosted acceptance is separate.

## Methods and source scope

Read-only PE/Capstone, Python 3.14.6, Capstone 5.0.7, pefile 2024.8.26.
Ghidra/HE/original runtime NOT_RUN. Existing wildlife §§1–7/corrections, hunter,
load/decisions/prefix/first-call and source-provenance receipts reused. The older
wildlife-re-1b worktree has no cache directory available here; addressed accepted
docs remain the reused research, not an invented retained older raw log.
New private read.py records exact module/address/range invocations as filenames;
only aligned function/call targets in the public ledger support findings. Some
exploratory range starts cut instructions (e.g. 0x10011100/0x100aa950/0x10001df0);
those first fragments are not evidence. Concrete entries/targets were separately
read (0x100aa910, 0x10011dd0 etc.). No original listing/table/data is committed.

| Original input | SHA256 |
|---|---|
| `AnnoWelt.dll` | `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2` |
| `AnnoGame.dll` | `ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc` |
| `AnnoFrame.dll` | `dd3f36ff564d2685d10795272c4ed3c0b2da32a946c2883fb1158ff439249b91` |
| `AnnoScene.dll` | `5b521f9541332846d32b57c93eaaee9c9d8de2ac2404cd75da0fe93e0e36fd38` |
| `FigurenHeader.dat` | `29fa1f0528fb337d9a61e8b73169281fd05bb27efa6df65638580a4c36c468a8` |

Concrete new primary closure: manager slot 0 0x10097250, free-id 0x10099130/
0x10099290, growth 0x10099020/0x10099870, type factory 0x1009a110 kind tables,
New ctor 0x10052c10, attach 0x10052a30, cargo 0x1002ff40/0x1002ff50,
property 0x2f 0x10052aa0, spatial construction 0x10099400/0x10099850/
0x10097060 ->table 0x100cb09c ->append 0x10096f40 and relocation 0x10096fb0.
Game actual descriptor factory 0x10006400 ->0x10011dd0/table 0x10021e8c;
class/kind/HP/id/property-0x20/goods concrete leaves resolved. Sweep through
0x10093875 reads those leaves and stores success fields; no first-action call.

The catalogue source chain from Frame/Scene/world initialization is reused from
accepted resource initialization, not inferred from equal vtable offsets. Manager
base/M+4, cell/C+8, figure/F+0x6c and separate module descriptor bases are explicit.
Normal successful allocation/unhooked concrete receivers only: failure retry can
call arbitrary handler at 0x100e3cc4 through 0x100bafc1. Its actual runtime value
is U. No full allocation-failure draw/rollback claim or FP adaptation follows.

## Actual small checks

Commands (private, uncommitted Python):

- `python3 cache/wildlife-create-closure-1/descriptor_probe.py`, redirected to
  descriptor-probe.log: `descriptor entries 368 selected 29 kind49 29 kind51 0 special [128, 129, 1206]`.
  Original BEWOHNTYPE label mapping/LOGIC/WARE parse; raw rows stay private.
  This is the current file's inventory, not an allowed-species cap.
- Hand table was written before checker. authored_rows.py constructs literal
  expected rows without importing/executing checker.py. No result re-recording.
- `python3 -I cache/wildlife-create-closure-1/checker.py cache/wildlife-create-closure-1/authored-rows.json`:
  `PRIVATE_MODEL selected=11 executed=11 failed=0 original_runtime=NOT_RUN unresolved_callee_execution=NOT_RUN`.
  Eight effect cases (including null continuation), three explicit refusals.
  Calls take resolved outcomes and supplied binary32 words; they do not execute
  original callbacks or validate arbitrary input FP projections.
- `python3 -I cache/wildlife-create-closure-1/contract.py`:
  `PRIVATE_CONTROLS legal=1 passed=1 wrong_models=6 rejected=6 wrong_expectations=1 rejected=1 empty=1 rejected=1 original_runtime=NOT_RUN`.
  Actual source replacements checked before execution; six mutants identify the
  intended first mismatching row, not crashes/empty selections. Empty input and
  changed expectation each return 1. No canonical suite COUNTS/VERDICT invented.

| Counterfactual | Intended rejecting row |
|---|---|
| Constructor amount treated as capacity 1024 | ordinary quantity 512 |
| Omit type addition | type 128 quantity 2048 |
| Cargo goods copied from animal yield | carried 69 versus yield 137 |
| Cave uses entrance as saved home | tile anchor 5/6 versus entrance projection 1/3 |
| Null result marked occupied | all four supplied prior spawn fields unchanged |
| First action starts before return | current 0/requested 27, first_action false |

Initial authored ordinary home 10/20 was inconsistent with supplied binary32
x=1.25/z=3.75; a resolved-outcome checker cannot establish that conversion.
Initial script/rows/log retained, with its successful result labeled modeled only.
Corrected ordinary home 1/3 and valid original id 65535 were used in final controls;
cave home 5/6 explicitly differs. No original execution or production expectation
changed. Source-listing exploratory fragments likewise remain private and labeled.

## Preservation and receipts

998 preceding code/fixture/binary hashes match the first-call freeze, including
292 synthetic working files. Current Debug binary
312755f380ed0fce261b873ec14758e5af0007e28b9a499ce4993a7272518968;
ASan 96a85d9c82602c914bafd289c2654368e9e72f1f5f55850f9a1b8f708706d0a0.
No builds or C++ tests rerun for this docs/private-probe packet. No tags/state/
defaults/pins/production caller; original files/coordinator/awake assertion preserved.

Private 109-entry manifest closeout-sha256.json SHA256
7274a96936148d7070ee1f9919568bcbbf8b321c18c957f5d9bb2c31b0d5c548.
Paths below are relative to the owned checkout. Manifest freezes all primary
listings, source readers, model scripts/inputs/results and controls; raw files
remain private. Publication-only link/diff and CI reads occur afterward.

| Selected receipt | SHA256 |
|---|---|
| `cache/wildlife-create-closure-1/allocator-imports.json` | `57075b29557f9e880c9657b8039e99e30e5bdabcc1d4e54eab362b7a62fea2f8` |
| `cache/wildlife-create-closure-1/authored-rows-final.log` | `e9655e8118b0bc1335cc7ac6064411d8857ab6ac328e0655cd89ff018b9153c1` |
| `cache/wildlife-create-closure-1/authored-rows-final2.log` | `e9655e8118b0bc1335cc7ac6064411d8857ab6ac328e0655cd89ff018b9153c1` |
| `cache/wildlife-create-closure-1/authored-rows-initial.json` | `eef3089e16b064ce7c23df6a797d7a8c5c3caf74318fea8ae6d362be3fccf79e` |
| `cache/wildlife-create-closure-1/authored-rows.json` | `41d7c9951a78e1c4d8f0a2061063c824040e433a29464fa4d65f8d39ff387772` |
| `cache/wildlife-create-closure-1/authored-rows.log` | `e9655e8118b0bc1335cc7ac6064411d8857ab6ac328e0655cd89ff018b9153c1` |
| `cache/wildlife-create-closure-1/authored_rows-initial.py` | `3ce4b9a7b20e07dede5ad8de9c6dc43e8fa8d30d2ddf944102db6a71214aafd6` |
| `cache/wildlife-create-closure-1/authored_rows.py` | `93534042e4cb95437563cbdc066f8a19ac40412652cc987fa6a86b7206516a43` |
| `cache/wildlife-create-closure-1/binding-tables.json` | `70582636cd96246ea8708fdf2aa0ef0d05fdb0e32bfc27b67b51062d870088c9` |
| `cache/wildlife-create-closure-1/checker-legal-initial.log` | `71abcbe552ef90c3603375d2773fe725bfe662ed50fc32d08c3835ee15f62edb` |
| `cache/wildlife-create-closure-1/checker-legal.log` | `71abcbe552ef90c3603375d2773fe725bfe662ed50fc32d08c3835ee15f62edb` |
| `cache/wildlife-create-closure-1/checker.py` | `1287041841c1b217a3b999683aa6f8044ae18d71e29d24fc9b8d709b25c0da05` |
| `cache/wildlife-create-closure-1/constructor-as-capacity-control.log` | `7fa58569cdd39f964102845fb89eb7b4f81f126f56b083da0548e8e238362bf1` |
| `cache/wildlife-create-closure-1/constructor-as-capacity.py` | `873e6946ee8ef30f89b8f2df7534cb70e3b144c7084a4accbfd7f2ff88d81ea5` |
| `cache/wildlife-create-closure-1/contract.py` | `d7a766f45e13d20d114b10da7c94c557fc1688d1ec9b8cd0389ac0bc2e576eb7` |
| `cache/wildlife-create-closure-1/controls.json` | `a0245e6dd476852f217979393b7f91fe054086ded92ae7250cef5bc66292d4b6` |
| `cache/wildlife-create-closure-1/controls.log` | `d35d51319b6a94ffaadee66e8794f17b70fc95721946c2abe16dc5e2310e7558` |
| `cache/wildlife-create-closure-1/descriptor-probe.json` | `04580e90432a86a52d21407ac815ea6187f4dd470f3ef78a0ae3c81202f07746` |
| `cache/wildlife-create-closure-1/descriptor-probe.log` | `536b9578a73982e55b6ca33e4efda6409859c975a4fdfe098d45e977ee2a9b63` |
| `cache/wildlife-create-closure-1/descriptor_probe.py` | `03c3d2000f6fb34fc481e760f3e7bdba52cd36cc4e243e619d06304dcbbea956` |
| `cache/wildlife-create-closure-1/drop-type-adjustment-control.log` | `2a0c35e35f2a77a68d5c083b2c828c5cd72fa5fc98f7ad2c68ea2627794932aa` |
| `cache/wildlife-create-closure-1/drop-type-adjustment.py` | `83b77817e960288007c39c9801bf4794008f2508b73adf6cc39e3ec364937383` |
| `cache/wildlife-create-closure-1/early-first-action-control.log` | `7fa58569cdd39f964102845fb89eb7b4f81f126f56b083da0548e8e238362bf1` |
| `cache/wildlife-create-closure-1/early-first-action.py` | `46705ebee7cfc701cb0fcbe26f73b65f4613cf8d1923e274d515827ffa947bf3` |
| `cache/wildlife-create-closure-1/empty-control.log` | `199335ba53cee7e8e7b1ada3f1c103abdcc962833a29b2be234e90c051f2a2fc` |
| `cache/wildlife-create-closure-1/empty.json` | `37517e5f3dc66819f61f5a7bb8ace1921282415f10551d2defa5c3eb0985b570` |
| `cache/wildlife-create-closure-1/entrance-as-home-control.log` | `80a9b858e8df25e30bf818f6a3be70060cffefe12b9617c20bac5a61e9e4f38d` |
| `cache/wildlife-create-closure-1/entrance-as-home.py` | `ede8f5843ccd905beabbc2371dbdfaf02e079cd4ae4edaf1a2fc34f6dd3331c8` |
| `cache/wildlife-create-closure-1/legal-control.log` | `71abcbe552ef90c3603375d2773fe725bfe662ed50fc32d08c3835ee15f62edb` |
| `cache/wildlife-create-closure-1/legal.py` | `1287041841c1b217a3b999683aa6f8044ae18d71e29d24fc9b8d709b25c0da05` |
| `cache/wildlife-create-closure-1/module-pins.json` | `cc1678fcdb0e54e3e4ac7ddd9c201a35c6985d6c15feebce548d88655a241403` |
| `cache/wildlife-create-closure-1/null-occupied-control.log` | `995e990885553ee449eeaee7be25f2d9539d21c194243597d4a081d22ed328bd` |
| `cache/wildlife-create-closure-1/null-occupied.py` | `16857975c98c2b3dfccdab369ea2200cc74a561d1d34c9c60732bf7ba0ff38f5` |
| `cache/wildlife-create-closure-1/preservation.json` | `3374f61907a42d57511ea11dc4234b1433ea5712173fb7ef6b6925661c6bc489` |
| `cache/wildlife-create-closure-1/read.py` | `78c883b7e393941c3533e01f7a947a7b90b18f3fb2140e46dc30b135cc28198b` |
| `cache/wildlife-create-closure-1/wrong-carried-goods-control.log` | `7fa58569cdd39f964102845fb89eb7b4f81f126f56b083da0548e8e238362bf1` |
| `cache/wildlife-create-closure-1/wrong-carried-goods.py` | `54ed6a61cce1367863e7031fa9b1e1a143191cd6de1d49fc49e79876d498452e` |
| `cache/wildlife-create-closure-1/wrong-expected-control.log` | `7fa58569cdd39f964102845fb89eb7b4f81f126f56b083da0548e8e238362bf1` |
| `cache/wildlife-create-closure-1/wrong-expected.json` | `5704fdb3071fd10ae3d4d7dcbd94da91240894ea5d6d74888ac79370af222296` |

## Review disposition and remaining work

All addressed claims/hand rows are READY FOR REVIEW, not self-approved. OQ-578
has only a dated selected normal-success narrowing; allocator retry handler,
later animation/search/storage/death/Old/predator callbacks remain open.
Prospective owned creation-effect kernel can proceed with validated facts and
explicit resolved outcomes, UNWIRED. Persistent manager/World state, id namespace,
scheduling/global PRNG and allocation-failure policy need separate coordination.

NOT_RUN: original/HE runtime, cross-host FP and cave entrance producer, live
allocator/retry-handler observations, complete animal tick/routes/hunting/death,
World/native/tag/identity integration, navigation/raster/graphics, Windows/WSL,
full/heavy/repeated C++/pins/warnings gates. No blanket lifecycle/draw certificate.
Hosted CI after ordinary push is reported directly; terminal status belongs to
orchestrator. No reviewer agents or unrelated tasks were started.
