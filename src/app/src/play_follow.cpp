// The window's side of play (see the header).
#include <anno/app/play_follow.hpp>

namespace anno::app {

std::size_t PlayFollower::follow(render::FrameRenderer& frames, WindowTrace* trace) {
    // The snapshot before the take: every batch of its turn and earlier is in the mailbox (the
    // runner pushes a turn's batch before it publishes the turn's snapshot), so after the take
    // they are all in the picture's desired state. Tick 0: no turn published yet.
    const auto snap = runner_.snapshot();
    const auto batches = runner_.take_building_batches();
    if (snap->tick > 0) {
        followed_turn_ = snap->turn;
    }
    if (trace != nullptr && !batches.empty()) {
        const double t = trace->now_ms();
        for (const auto& batch : batches) {
            trace->event(t, TraceKind::BatchTaken, 0, batch->turn);
        }
    }
    if (!batches.empty()) {
        picture_.apply(batches);
    }
    // Commits what is ready (a full reclaim backlog skips it until a later frame) and invalidates
    // the change's keys: the cached chunks keep drawing until their rebuilds arrive.
    const std::size_t committed = picture_.commit(frames);
    if (trace != nullptr && committed > 0) {
        trace->event_now(TraceKind::Generation, 0, picture_.scene()->generation());
    }
    return committed;
}

bool PlayFollower::has_work() const {
    return runner_.batches_waiting() || picture_.pending() > 0;
}

} // namespace anno::app
