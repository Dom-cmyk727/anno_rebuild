# WILDLIFE-PROD2-1 — frozen review evidence

READY FOR REVIEW; independent orchestrator review pending.
Tested source `8d65f38583b3180fe960caba79f25bc3ab657446`, tree `240588c63a3dc06dfcd23b6931daafd801cdbeb1`.
Base `eaa61980162b1552eb7acee1553ce11fb7a8869c`;
dispatch `af83b290ddd8ebe00ab93d7f70e23b4ca64d62e2` read without merging.
Publication adds this evidence and REPORTS only; full head/tree/exact CI in direct
handoff. Previous branches/evidence, main, other lanes and running jobs preserved.

## Primary contract and acceptance

[Prewritten contract](../../sim/wildlife-prod2.md),
[prefix and GELAENDE epilogue](../../sim/wildlife-prefix.md),
[corrected writer callback limits](../../sim/wildlife-rock.md#writer-callback-correction-2026-11-19).
D = GOG instructions/data, P = original parsed file, C = authored execution.

| Requirement / primary | Executed distinguishing evidence | Independent disposition |
|---|---|---|
| D full zero-item path / 0x100895d9 -> 0x10089d6c | Six-byte block success, records 0 with engaged marker, nonzero geometry preserved | pending |
| D no terrain epilogue | Water-only rig with no GELAENDE keeps masks 0; empty PROD2 never calls water OR 511 | pending |
| Ordered owned completion | Two empty blocks retain copied geometry/source after inputs destroyed, including unused outside-map X/Y and zero height | pending |
| Explicit unsupported item | Known, unknown, rotated high bits and nonzero state stop item 0, preserving previous empty completion/resource state | pending |
| Defensive framing | Header lengths 0–5 refuse; partial-only tails 1–5 refuse; whole item plus tail stops at first unsupported item | pending |
| C actual-TU semantic cause | Legal 36/36; exactly one removal of nonempty guard reaches later MYSTERY and fails precise PROD2/source/reason/completion assertion, 35 others pass | pending |
| P same original once | Sixteen raw six-byte PROD2 blocks complete with 0 items/draws/epilogues; already-supported HIRSCH loads 156 records, then EVENTINSEL 72 boundary | pending |

## Finite original path

AnnoWelt arm 0x1008957d–0x10089692, tag PROD2 at 0x100dcef4. Declared size
comes from header +0xc through 0x100b4b10; read six bytes at 0x100895a7–0x100895b1.
Block header = u16 X/Y, u8 width/height. Island origin is added to scratch X/Y
with word arithmetic at 0x100895bb–0x100895c8. Only scratch/byte accounting
change. Successful six-byte read leaves unsigned length minus 6; multiply/shift
0x100895cd–0x100895d6 divides by 6. Zero branches at 0x100895d9–0x100895db
straight to shared return 0x10089d6c–0x10089d7b. No width/height cell visit,
writer, +0x198 epilogue, other callback or draw. This is unlike the proved
GELAENDE epilogue even for zero items. No empty-tile fill or cache update omitted
from this empty path. Owned per-header geometry is completion provenance only.

Nonempty reads six-byte items at 0x100895e5–0x100895f5. Type uses 14 bits,
rotation high two, signed item X/Y bytes added into word coordinates, phase low
six state bits and height upper byte. Null descriptor skips the original item;
known descriptor calls island +0x78 with flag 0x04000000 at 0x10089638–0x10089676.
Concrete wildlife table 0x100cac2c slot +0x78 targets wrapper 0x1008ec90, base
0x100ca19c slot +0x78 targets 0x10082040. Wrapper calls lower writer unless bit
0x80000000, then follows the fresh +0xc8 observer path. Prior corrected lower
writer/caches remain dependencies, not a new claim of full PROD2 equivalence.
City PROD/ENTRY creation and resource admission are separate. This implementation
stops ALL nonempty PROD2 before lookup/effects, including unknown ids; it does
not claim to reproduce that original skip or any nonempty placement family.

Partial-tail nuance: local floor arithmetic does not prove safe source-order
continuation. Caller 0x100a2f8c–0x100a2fbc subtracts actual reported bytes, then
header reader 0x100b4c20–0x100b4c35 immediately reads 16 more bytes, with no skip
of partial tails. For 7–11-byte payloads the original local empty return leaves
tail bytes unread. Defensive projection refuses those tails rather than claiming
whole-stream equivalence. Short headers and unsafe short reads/underflow are
also not emulated. Whole item plus tail stops before that unsupported item.

## Controls, causal proof and one original continuation

Three authored cases pass 764 assertions. Exact empty branch owns geometry after
input destruction, preserves input bytes and resource/OCCUR/DISCOVER/ANBAU/
KONTORID/fertility/HIRSCH state. A water-only rig proves no accidental terrain
water-mask epilogue. Distinct nonempty boundary case appends MYSTERY after its
unsupported item, ensuring a mutant still returns an inspectable diagnostic:
legal 36/36; private actual adapter-TU guard removal wrongly marks PROD2 complete
and reaches MYSTERY, failing exactly the PROD2/source/reason/completion assertion.
35 controls pass. Source unchanged; canonical rebuilt binary matches legal run.
GCC shadow negative rejects. No new evidence framework or generated oracle.

Same raw Tutorial/01Discover01, ordinal 0/slot 11, origin (24,24), 128x128.
Supplied stamp 500/mode 0/raw draw 7 remains controlled, not original global
cursor. Independent Python reads PROD2 headers 55–70, all length 6, count 0;
C++ independently checks each raw header geometry and completion marker.
No extra resource/water/cache pass or draw occurs. Already-supported HIRSCH 71
has 1248 bytes / 156 records, all flags 0, so 156 load draws. C++ checks every
raw field, mode-0 goods, source index, draw cursor/timestamp, chunk and prepend
head independently from raw bytes. Each timestamp is 500/500 + 7 = 8.
Now 72 complete local headers, 73 evidence rows including boundary EVENTINSEL 72.
Total supplied draws 4745 = 4589 resource + 156 load; 156 OWNED SPAWN RECORDS,
not living animals/figure creation. No further header or post-island work executed.

Prior state remains asserted: 4760 accepts, 0 rejected, 84 stones/63 cliffs/
24 rocks, 16 GELAENDE epilogues, fertility 16384 bytes/8160 changes, BERGWERK
9 entries/20 kind-1 nodes, OCCUR 1 ore/1 kind-6 node/Q512000/R1/20 masks17,
21 shared nodes/8 chunks, ANBAU 135 bytes/13 tokens/mask65535, DISCOVER low0,
KONTORID1. HIRSCH reachability now established locally, not global initialization.

## Canonical gates, host and limits

Debug 22 main + 4 separately required = 26 executed passes; ASan/UBSan same 26.
Original Debug 1/1 once. No failures, skips, empty, incomplete, sanitizer reports,
memory refusals or timeouts. Doctest filter exclusions are not skipped tests.
No whole-gate repeat; exact raw lines follow.

`cache/wildlife-prod2-1/debug-final.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=22 executed=22 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

`cache/wildlife-prod2-1/asan-final.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=22 executed=22 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

`cache/wildlife-prod2-1/original-final.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

Affected test/adapter TUs pass Apple Clang 21 and GCC 15.3 O3 strict Werror;
negative shadow rejects. clang-format 23.1.3, sim lint, links and diff check pass.
Inherited Apple duplicate-library linker warning remains disclosed; no whole-build
zero-warning claim. All 276 tracked fixture files byte-equal base, including pins;
this is a byte audit, not full pins.py. Host M5 Max/18 logical CPUs/48 GiB RAM,
macOS 27.0.1, 1.5 TiB free; contention checked. Jobs 1, 2 threads, 2048 MiB/thread,
host admission on. ASAN detect_leaks=0:halt_on_error=1, UBSAN
halt_on_error=1:print_stacktrace=1. Owner's awake assertion/coordinators preserved.

Unchanged algorithms and long campaigns reused: resource/stone/cliff/rock,
fertility/BERGWERK/OCCUR/ANBAU/DISCOVER/KONTORID and wildlife_load production
logic, formats framing and metadata unchanged. Affected prefix integration rerun.
No new public measurement tool or rendering; automatic picture contract omitted
for this headless import. NOT_RUN: full suite/pins.py, Windows/WSL/original
runtime/HE, global initialization/cursor, post-island callbacks, World/native/
save/default/state/tag/registry/PRNG changes, nonempty PROD2/city factory/production,
UI and derived-cache consumers, living wildlife creation/tick/removal. Exact
publication CI pending, not represented as accepted.

## Decisions and reasoning

- Implement the empty arm's proven return; never route it through resource admission.
- Engaged empty completion is distinct from absent/failed/unsupported provenance.
- Refuse partial-only tails defensively: local floor count is not stream framing.
- Stop nonempty item 0 before effects, even for a type the original would skip.
- Reuse supported HIRSCH naturally reached during this one continuation; stop EVENTINSEL.
- Keep loaded spawn records distinct from created figures and global PRNG alignment.
- Reuse unchanged long evidence with current affected integration and strict checks.

QUESTIONs: none for this finite empty path. Next boundary needs a separate release.
Independent orchestrator review pending; no executor self-approval.

## SHA-256 receipts (raw original records/listings stay private in cache)

| Artifact | SHA-256 |
|---|---|
| `cache/wildlife-prod2-1/debug-final.log` | `4fa741a9eb742469de0fcab7586a95a219a0859e0fbd6ad04a485ca151e3ffa8` |
| `cache/wildlife-prod2-1/asan-final.log` | `dd4dcb56c93f4f655e7b0961c4e60cbd1c39bb3470fcc95002f3bedaf8a36254` |
| `cache/wildlife-prod2-1/original-final.log` | `c7903a7f37b33c5bb088f98cc14033e756fc9a643f0d1bbb7cf1ff16a8a5075d` |
| `cache/wildlife-prod2-1/debug-final.log.inventory.json` | `19f7d8ef6a4f4dbd257122aa9c7e40ac024c92b4985ced15fc79a798754d825b` |
| `cache/wildlife-prod2-1/asan-final.log.inventory.json` | `eb5560c9b76e95bc146447c215164b6e4ef13856d2ccae8f14045e55fe628b72` |
| `cache/wildlife-prod2-1/original-final.log.inventory.json` | `3111c066b2ed5bc8ba78cec67f0107b8b9c6b87374e3abb38ecf9aa56ce5a46e` |
| `cache/wildlife-prod2-1/fixture-audit.json` | `931942c15c5523eba7328569c0c133bb673e451baf5185ac730d86e52596acf7` |
| `cache/wildlife-prod2-1/independent-header.py` | `d1d422a97abb1f07315fbfe3320dbb3dddb79ae1659333871c158b79b8e9da03` |
| `cache/wildlife-prod2-1/independent-header.json` | `20f724cd5efb5d31dd561495babd285df114cbd54e2105a8b37238f7640eb18d` |
| `cache/wildlife-prod2-1/independent-header.log` | `1b56b4b15f3e7a3e4af7542fb00acebe9264ea7faaafe9e47ee3ccbe8a0a36b9` |
| `cache/wildlife-prod2-1/arm.private` | `becef362abea07878669b0175713bff71f5f707487b6678e76632c2ecca69583` |
| `cache/wildlife-prod2-1/return.private` | `4290238c93964984798cc16bbe33edd4f6b0e89e670a7fd4da6a219815c5b482` |
| `cache/wildlife-prod2-1/nonempty-return.private` | `078a23c7fbae65fcfae003db8ada0087c12517ce202b98aef4e1a7266e2b2999` |
| `cache/wildlife-prod2-1/caller.private` | `75da93c76cd20b1a9c5ab433a08779c225ca2a5112e9fbca34dc6cfb42f46f39` |
| `cache/wildlife-prod2-1/read-header.private` | `f07d9cee6ec85ae135f4f5890fe53259735b68e7d770c1847910626649d78400` |
| `cache/wildlife-prod2-1/wrapper.private` | `cded08c702875b4d26fffd381a294821eb567b78bee352f8a4c7f4e3f3f595f1` |
| `cache/wildlife-prod2-1/writer-start.private` | `0cbd95eb7aed66c3e504b2fd0486d24c1eedc99c2901ddc3a79fb0cb38aab1c7` |
| `cache/wildlife-prod2-1/reset-identities.json` | `dbd03bb32cfe793305d8a3c94d64f3cb6c450a9169628b64ed540d492a71a296` |
| `cache/wildlife-prod2-1/reset-legal.log` | `c703fbc1a2c92b48f98f6558373359a3cba6daf84daceef83dd1a3a9856c1833` |
| `cache/wildlife-prod2-1/reset-mutant.log` | `8d433eea53686e0c5258675d5e23328724e69eb0d0c26f1656ec0187b970339b` |
| `cache/wildlife-prod2-1/mutation-proof.py` | `5244213d626f69d6c7937dd3014e9b33ca6d8fe2d5d0dd6629475cc92ea7fbba` |
| `cache/wildlife-prod2-1/mutation.log` | `bd65a0ab7b0c138d1189846073c35f3ce253cd1b5a029d769654bcf04d0aab30` |
| `cache/wildlife-prod2-1/build-first.log` | `777a9046029e138cace7600b3b56ac033e68c193fae47d92b5f8b9f5db233294` |
| `cache/wildlife-prod2-1/build-canonical.log` | `c2ed93ffd93c840755c0e91392ae593b1b52de9b7aaf6c208123feaceb1dde2e` |
| `cache/wildlife-prod2-1/build-asan.log` | `777a9046029e138cace7600b3b56ac033e68c193fae47d92b5f8b9f5db233294` |
| `cache/wildlife-prod2-1/apple-warnings.py` | `5e3df2e134e7fc546673a8e1bd6cdda5e69245a2b5c80b6e1726ac36c103b74a` |
| `cache/wildlife-prod2-1/clang-warnings.json` | `f5bc496034699f75f6473849fc717a58e89dcc09e45b1983cf7453207f8aa532` |
| `cache/wildlife-prod2-1/apple-warnings.log` | `923b631138bf1be975b9f8d2727e5d12afe236a6a0c6b8bf48a990f4e02382ac` |
| `cache/wildlife-prod2-1/target-warnings.sh` | `434c63bb5f96f4549b9cc6644522810f8c4dd6e58603179f63b494a25d7ffe0d` |
| `cache/wildlife-prod2-1/gcc-warnings.log` | `e029815c43244dcd9c829e399c8cfe3b664363751f161c9b3263ec1ee9cb330b` |
| `cache/wildlife-prod2-1/controls-first.log` | `2ae19e64f2841845d58c96d640ff73ffc77a775748b976894e2b743812dc8427` |
| `cache/wildlife-prod2-1/lint.log` | `c90ca6eba541545a82beb573bfcbcc47531df735edd4ff68c814ab50296e6948` |
| `cache/wildlife-prod2-1/format.log` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `build/dev/src/sim/anno_sim_tests` | `8e92ad7c434a895a69db118d381e872e09b722f4dd23ee4d4b62c86a9419c8e1` |
| `build/asan/src/sim/anno_sim_tests` | `51b0881b7727eb42e5bd6cc22ad42e5165fe38fe039e061c1da417508c557a8b` |
| `docs/sim/wildlife-prod2.md` | `0de2f1bf06e6fbfb41955bd4795c4509871f741ff980becdb8dffa95be6df901` |
| `src/sim/tests/wildlife_prefix_test.cpp` | `60c59540f5b836408068997339ff1d704014929fa65e012d448ff54801052b6a` |
| `src/sim_load/include/anno/sim_load/wildlife_prefix.hpp` | `3232da80e82fcf659642b367ded3464c2165a1c1e7f2ec630ed38cc107db569a` |
| `src/sim_load/src/wildlife_prefix.cpp` | `00bce2a62b57a4cd1bfadba51e8a2d786d70a8e1ee1371c6865c07b2ee0ab379` |
| `game/app/AnnoWelt.dll` | `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2` |
| `game/app/AnnoGame.dll` | `ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc` |
| `game/app/Scenes/Tutorial/01Discover01.sze` | `225ed50b005743e00d8d3ffe18667d4565dc781ef45bd6d9b767fd2fe056c0f6` |
