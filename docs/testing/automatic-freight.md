# Automatic owned-warehouse freight: consumer card

Headless route-only136, based134, ECONBK1/PRNG5/replay4/command1/native1. It contains no
salt135. A later combined main needs a fresh coordinated identity. This card does not
implement a viewer or live UI.

Use an authenticated session issuer. Keep FigureId/BuildingId returned by ordinary
import/founding/construction; a rebuilt warehouse has a new ID. Dock coordinates are Q8
world tile centers, validated by existing navigation and cargo range rules. Neither shared
visibility nor nearest visible warehouse grants cargo authority.

Submit AssignFreightRouteCmd{ship,stops} through the normal session/recorder. Each
FreightStop stores exact warehouse ID, dock_x_q8/dock_y_q8 and ordered actions. LoadTarget
is desired TOTAL aboard in first A holds; UnloadAmount is requested removal. Quantities
are whole tons; balances remain exact Q8. Original grouping folds same-direction/goods
actions into their first slot; zero later slots retain the original preparation wait.
Source shortage/full storage skips rather than waiting until full.

After accepted assignment, issue ordinary Advance only. Observe pending order/generation,
actual matching arrival, positive transfer quantities and route status. Command acceptance
is not arrival; arrival is not freight; cycle wrap can be unproductive. FreightObservation
has actual warehouse/goods/direction/amount_88, stop/action, generation and
phase/status/reason. Generic ShipArrived must match active ship/order/generation; host
event queues are observation outputs, never an authority input.

ControlFreightRouteCmd Suspend pauses and stops sailing; Resume revalidates current target
and excludes paused time. Cancel releases automatic definition/control and returns manual
use. Successful Move/Stop/Found/Load/Unload takes over; a refused manual command leaves
automatic state unchanged. Missing/foreign/out-of-range targets and failed navigation
report recoverable Failed with reason; rebuild needs a new stable target assignment.
Cancel remains available for blocked cleanup.

The verified original recipe manually sails/founds both settlements and builds
original801/20roads. The inherited Human ship wood is26t after24t founding sinks. Public
On plus bounded collection establishes768raw warehouse wood before Assign. Source
LoadTarget29t/destination Unload3t carries produced3t each cycle while restoring26t
inherited baseline after delivery. Two positive source debits/receipts1536raw plus further
genuine production fund an unchanged1310 receiving Build (50g,1wood,1tool), leaving1280raw
receiving wood. Automation begins at assignment; setup is manual. Booking is explicitly
enabled for this financial proof; default rules stay unchanged.

Five observed cuts run the complete remaining suffix: source Sailing after scheduler
issuance (initial leg may be degenerate), source actual-arrival Wait, loaded outbound
Sailing, destination actual-arrival Wait, return Sailing after first receipt. Every
continuation compares events/draws/actor/city state and final native bytes. Honest fresh-
process replay and legal changed assignment29→28 are separate executable consumers;
changed definition diverges at the checkpoint immediately after assignment, with the
earlier prefix matched.

Windows adapter work: present typed assignment/control/query and observations, validate
selected stable warehouse/dock before submitting, display shortage/failed/pause status
without inventing wait-full behavior, retain manual controls. Command tags F075/F076
append to existing variants; do not edit wire identity or reinterpret136 as combinedsalt.
Windows/Linux compilation, live window/input/GPU and original-runtime parity remain
separately unexecuted here.

## Typed example and executable proof

For the reviewed original setup, use the stable IDs returned by the two Found
results. The fixture has source `BuildingId{1}` and destination
`BuildingId{4294967297}`; general callers retain their actual returned IDs.

```cpp
using namespace anno::sim;
Command assign{authenticated_player,
    AssignFreightRouteCmd{ship, {
        {source_warehouse, 257*256+128, 134*256+128,
         {{FreightDirection::LoadTarget, wood_goods, 29}}},
        {destination_warehouse, 452*256+128, 349*256+128,
         {{FreightDirection::UnloadAmount, wood_goods, 3}}}
    }}};
auto accepted = recorder.submit(assign);
// Ordinary session advances execute navigation and cargo; never simulate arrival in UI.
recorder.submit({authenticated_player,
    ControlFreightRouteCmd{ship, FreightControl::Suspend}});
recorder.submit({authenticated_player,
    ControlFreightRouteCmd{ship, FreightControl::Resume}});
recorder.submit({authenticated_player,
    ControlFreightRouteCmd{ship, FreightControl::Cancel}});
```

Start from this branch's configured headless Debug build, with the original game
directory declared. From the source checkout, this selection runs one producer
and both dependent fresh processes. It validates originals, exit codes, actual
child stdout/stderr and required public CLI contracts. No asset skip is a pass.

```sh
python3 -I tools/test/run_selected.py --source-root "$PWD"   --build-dir "$PWD/build/route-debug" --target economy_checks   --select '^(anno_sim_tests\.freight original voyage:.*|freight_voyage_.*fresh_process)$'   --require-test freight_reference_contract   --require-test anno_sim_replay_check_contract   --require-test expansion_replay_asset_contract   --jobs 4 --timeout 1200 --log cache/automatic-freight-gate.log
```

The producer writes `build/route-debug/src/sim/automatic-freight/`: genuine and
one-byte changed recordings, five native cuts, final native state, event trace
and ledger. Honest child matches11/11 checkpoints; changed child matches3/4 and
diverges at entry491, immediately after independently legal assignment29→28.
The two children reuse the one producer through CTest fixtures. These commands
are a headless consumer contract, not evidence of a Windows executable or UI.

Short Release measurements vary independent active routes, stop count and
unrelated world content. Their per-Advance timings include the whole scheduler
and existing navigation; full-loop totals also include diagnostics/output
formatting. Higher active counts contend for NAV admission and have different
productive throughput. Matching freight counters across unrelated-world sizes
is local-work evidence, not linear latency or late-game acceptance.
