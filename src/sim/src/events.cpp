// The timed-event dispatch of World::advance, step 2 (docs/sim/state.md §5.7, stage
// S12; evidence: AnnoWelt.dll pop 0x100add90 with the kind switch to the handlers
// 0x100adae0 (kind 0, tile cleanup) and 0x100adb80 (kind 1, delayed hit); the queue
// itself and pop_due are events.hpp and scheduler.hpp).
#include <cstdint>
#include <vector>

#include <anno/sim/draw.hpp>
#include <anno/sim/events.hpp>
#include <anno/sim/scheduler.hpp>
#include <anno/sim/seams.hpp>
#include <anno/sim/tick.hpp>
#include <anno/sim/world.hpp>

#include "scheduler_internal.hpp"

namespace anno::sim {

PopResult Scheduler::run_events(World& w, const TickContext& ctx, CountingDraw& draw) {
    // Every due event, in (due, seq) order, with the original's saved-next rule
    // (pop_due); the event is out of the queue before its handler runs. Kinds 0 and 1
    // are the only ones (the enum and the loader allow no other; the original drops
    // any other kind silently). A handler without a function is the documented
    // default: the event is consumed and nothing happens (O-18, O-19).
    return pop_due(
        w.events_, ctx.tick, w.rules_.events_drain_new,
        [&](const TimedEvent& e) {
            const Seam<Seams::EventHandler>& seam =
                e.kind == EventKind::TileCleanup ? w.seams_.tile_cleanup : w.seams_.delayed_hit;
            if (seam.fn == nullptr) {
                return;
            }
            Effects fx;
            {
                SerialWorld serial(w.islands_, w.config_.layout, w.figures_, w.players_, fx);
                const auto before = draw.counts;
                seam.fn(seam.ctx, ctx, serial, e);
                fx.draws = draw_records_between(before, draw.counts);
            }
            // C2: merged before the next event is popped, so it sees these effects; an
            // event the handler enqueues gets its key here and is found or not by the
            // saved-next rule of pop_due.
            merge(w, fx, ctx.tick, ctx.t);
        },
        w.rules_.events_max_dispatch);
}

} // namespace anno::sim
