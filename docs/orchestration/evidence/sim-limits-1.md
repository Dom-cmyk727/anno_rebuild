# SIM-LIMITS-1 — compatibility bounds evidence

READY FOR REVIEW, 2026-11-18. Independent accepted base:
`9edb151c414246d306194a703db71475affbdad4`; source/spec/tests commit `3387108059ed05562555d90145275580c150cd7e`.
The branch is independent of accepted OQ-562-1 and preserved wildlife/ore work.
No main merge, force push, rebase, state/default change or pins regeneration.
Exact dispatch: `3bce55cc749aba41699733e2bc2b863c8ebcbc79`.

Identity stays state 139, all ten section versions 1, PRNG timeline 5,
command version 1, replay version 4 and native wrapper 1. Supported bytes,
command tags/aliases, defaults and mod policy are unchanged.

## Acceptance and reached copies

| Requirement | Positive actual consumers | Distinguishing counterexample | Review disposition |
|---|---|---|---|
| Stored reject domain | FreightRoutes validation, native field reader and full World load accept None/CityAccounting | reserved/first unnamed/u16 maximum refuse; stale native endpoint admits reserved | source/control review passed; final pin pending |
| Navigation domain | same three consumers accept zero/ShipBlocked | first invalid 4 and 255 refuse; omitted appended classification fails compilation | same |
| Command framing | codec encode/decode, Rules validation, direct typed command, native/full load | mixed bound and hostile wire/native counts; using native width in command math fails | same |
| Split HarvestState | independent handwritten ordinary bytes, nonzero mason Figure round-trip and integration oracle | new unhandled member fails build; omitted conditional row fails size/value checks | same |
| Compatibility | read-only Python oracles, independent schema bytes, all fixture identities | no writing or regeneration mode | final CI pending |

`reject.hpp` classifies each declared Reject as stored or reserved; unnamed codes
are unknown. Current stored codes remain None through CityAccounting.
NoDepositInReach is defined but RESERVED, not a valid stored code; the inventory
finds no sim producer (only its name and the existing policy explanation in tests).
The two endpoint copies were FreightRoutes::validate in freight.cpp and the
native freight_fields reader in state_fields.hpp. Both now use the shared
classification. Its exhaustive switch has no default, so a future appended code
must be deliberately classified under the project's warning-as-error build.
The control updates reject_name for its added enumerator, ensuring the new domain
switch itself, rather than the old name switch, supplies the decisive diagnostic.

NavFailReason in ship_step.hpp remains Unreachable 0, BudgetExceeded 1,
OutsideWindow 2, ShipBlocked 3. The raw freight byte uses 0 also for no failure.
Its shared exhaustive predicate replaces the same two consumers' literal >3
checks. Navigation event/route failure producers and their encodings are unchanged.
Legal endpoints, reserved and unknown values run through actual validation and
native/full loading, not only calls to the predicates; existing errors are retained.

`command_framing.hpp` preserves the public kMaxRecordPayload declaration/name and
centralizes head 12, stop 20 and action 13 byte widths with checked u64 arithmetic.
Named consumers are freight.cpp definition checks, config.cpp Rules bounds,
state_fields.hpp definition limits, and codec.cpp payload sizing/decoding.
Callers keep their own nonempty/count constraints and refusal categories.
Native stop minimum remains a separate **24**, not 20: warehouse u64 + x/y i32
+ action count u64. Command action counts are u32. Native definitions obey the
command-representability bound, not an invented 65535-byte native-save cap.

| Hand-derived payload | Last legal | First illegal |
|---|---|---|
| Stops, no actions | 12 + 3276×20 = 65532 | 12 + 3277×20 = 65552 |
| One stop | 32 + 5038×13 = 65526 | 32 + 5039×13 = 65539 |
| Two stops | 52 + 5037×13 = 65533 | 52 + 5038×13 = 65546 |

Each row reaches codec, rules/typed command or native consumers as applicable.
An individual rule-limit violation remains BadValue for direct commands; the
mixed total violation remains Unencodable. Hostile u32 wire and u64 native stop/
action counts refuse before allocating their claimed elements. Allocation probes
allow less than 1024 bytes for diagnostics and, in the native action case, its
one real stop; that finite test budget is not an engine limit. Pure checked math
also rejects UINT64_MAX inputs in constant evaluation without multiplying first.

HarvestState's aggregate still has nine members. Its truthful guard counts seven
common members in fields(HarvestState) plus two conditional mason members in
fields(Figure); member declarations and actual field order are unchanged.
This is an aggregate/layout obligation, not a claim that counting members alone
proves every serializer call. Independent ordinary hand bytes and populated
integration bytes cover order/content; the nonzero mason 2/1 round-trip separately
covers its conditional bytes. No unconditional extra fields were introduced.

## Bounded controls and source reconciliation

Private overlay headers alter counted copies, never the production tree or a
running build. Before/after production header SHA256s match. Normal source and
normal include paths are rebuilt/rechecked before final positive gates.

| Overlay | Intended failure | Legal control retained |
|---|---|---|
| Unclassified appended Reject | -Wswitch error in reject_domain | normal header compiles; stored endpoint passes |
| Unclassified appended NavFailReason | -Wswitch error in navigation predicate | normal zero/endpoint passes |
| Added unhandled HarvestState member | 10 versus 7+2 aggregate assertion | normal nine-member build passes |
| Omitted mason goal_row | conditional size and nonzero row round-trip fail | ordinary bytes and stored endpoint pass |
| Native stale bound widened to reserved | native field reader admits reserved, probe fails | legal stored endpoint still passes |
| Command stop width changed to native 24 | mixed hand-derived formula fails | endpoint/ordinary controls pass |

These six expected failing probes are retained, not counted as canonical passing
tests. The positive production-header probe executes eight checks and passes.
Full World validation is independently checked by the registered tests; the
standalone native probe isolates the field reader, whose scalar acceptance must
not be hidden by a later semantic rejection of the whole World.

All 11 frozen source/spec/test files match the committed blobs of 3387108059ed05562555d90145275580c150cd7e.
Frozen manifest SHA256: `a08943e0c78ccb650ccc7f99e3451ba85765a8ca293d737ea9918ce63abb7312`.
All **119** tracked sim fixtures match the accepted base's Git blob identities;
there are no changed/untracked fixture files. Base manifest SHA256:
`af81c1657c7cb1cfb372802808b55dda688fb50a9ded5ecd576ef61174611484`.
No pin, golden, oracle or recording is regenerated or moved.

## Gates and limits

Host: Apple M5 Max, 18 logical CPUs, 48 GiB, macOS 27.0.1, Apple Clang 21;
CMake 3.31.10 / Ninja 1.13.2. Initial load 1.82/2.27/2.11, about 1.5 TiB free;
no competing test/build observed. Owner keep-awake and coordinator untouched.
Canonical jobs 2, ANNO_TEST_THREADS=2, originals required/set. ASan/UBSan uses
2048 MiB per thread, detect_leaks=0:halt_on_error=1 and
print_stacktrace=1:halt_on_error=1. LeakSanitizer is NOT_RUN on this host.

The final focused selector covers freight, command wire, config/rules/header,
four relevant integration/identity/byte cases, population_oracles_agree and both
integration_stream_reference cases: 55 selected. The population oracle executes
`tools/sim_stream.py --check`; integration references execute read/self-test mode,
never --write. Unrelated joint-script/corpus/town matrices are excluded.
Exact argv/inventory is retained in each canonical log; selector SHA256:
`bf3248a318b6433a455ba6361dca1284e29082a0eddf5bbd645e5f99488e67c2`.
Use `python3 -I tools/test/run_selected.py --build-dir build/dev` (or build/asan)
`--select "$(cat cache/sim-limits-1/selector.txt)" --asset-policy required --jobs 2
--timeout 1800 --log cache/sim-limits-1/<log>`. Required picture contract and
mandatory preflights execute. The earlier isolated hostile case reuses the already
executed picture contract with --skip-auto-contract.

Both selected C++ targets and the sanitizer picture dependency build successfully,
with zero compiler warnings; the inherited Apple duplicate-library linker warning
is retained and is not described as a warning-free link. All five changed TUs
(freight/config/codec and both test files) also compile under x86 GCC 15.3 with
-O3/-DNDEBUG/C++20/CI warning flags/-Werror: all five exit 0, diagnostics empty,
objects nonempty ELF x86-64. This is a compiler control, not native Linux execution.
Initial build-dev-first.log failed on the new test's two-argument CAPTURE use;
the project macro takes one argument. It was corrected before consumer execution.
No initial runtime failure, timeout, memory refusal or sanitizer report is hidden.

NOT_RUN: native Windows/WSL execution, original-runtime/HE/GPU/live-window work,
LeakSanitizer, full suite/town/pins regeneration. Unaffected source-pinned checks
are reused; CI has no original assets. The Windows consumer delta is the shared
public header/predicates and four added registered tests, with unchanged formats.
No workflow, coordinator, other lane or queued DOCS-SIM-1 work is included.
Hosted exact-head CI and final review disposition are supplied in the chat handoff.

## Receipts

### first-consumers.log

`cache/sim-limits-1/first-consumers.log` SHA256 `5219b6d92236859262df0160985a89b30169407e494028246e18325d1f7f6c96`.

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=5 executed=5 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

### hostile-counts.log

`cache/sim-limits-1/hostile-counts.log` SHA256 `93e29d2b40ed26924b41ed35d77642a96793ab8081a638c54689cc8a7fbf8765`.

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

### focused-dev.log

`cache/sim-limits-1/focused-dev.log` SHA256 `cb8988581366d7556ca6f59a26705fcc8668f18a44f9bea93f0861ac7430f493`.

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=55 executed=55 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

- `external tool python3.14`: `07919b0fbf82742c24420d7dcadbd459f61bc1feb638eca02b81dccab2daf4d5` (verified against current file).
- `build/dev/src/sim/anno_sim_tests`: `4bb5ddb8f10a67b28b53f89604dd35d6d7841229233906984c9d06d93af3634f` (verified against current file).

### focused-asan.log

`cache/sim-limits-1/focused-asan.log` SHA256 `6640b8b760d5b58ca2c99ead3964e49271a53752f2b9af1c88537a799fe21781`.

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=55 executed=55 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

- `external tool python3.14`: `07919b0fbf82742c24420d7dcadbd459f61bc1feb638eca02b81dccab2daf4d5` (verified against current file).
- `build/asan/src/sim/anno_sim_tests`: `1224ffa6805cbe0627f1bd7e73a62d05ae4f3375f897a08202ca4bd4563561fc` (verified against current file).

- `cache/sim-limits-1/controls.json`: `66d797a939c9595f8eb56a61c313efa249a8797cea3bb00d212f33f025967592`.
- `cache/sim-limits-1/probe-positive.log`: `a07471d3c167e7ecb1a7fae0d8e00dd7f831579391650d972f176494342049ab`.
- `cache/sim-limits-1/probe-restored.log`: `a07471d3c167e7ecb1a7fae0d8e00dd7f831579391650d972f176494342049ab`.
- `cache/sim-limits-1/fixture-identity.json`: `d0f54a4feb2d0fdecd4905661768cccf69d676dbf9daab054c985ea649ae11a8`.
- `cache/sim-limits-1/gcc-release-results.log`: `752cd2eadd9a473a5f4d82885ba14fe76167c5c04b29dbb4de4af2769f2b4fac`.
- `cache/sim-limits-1/compile-release.sh`: `49d7e2473e5dcf031632c424ed68c636f1feed37e88fe9b67513d6db407fd3ea`.
- `cache/sim-limits-1/build-dev-first.log`: `8e617a039a2544d73ed81007ef0e83cef77b69a910bc21f866dac1b166ff9fab`.
