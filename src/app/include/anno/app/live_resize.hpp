// Whether an SDL event should trigger a live-resize/move redraw from an event watch
// (ZOOM-SMOOTH-2), and the watch's own reentrancy/wake-defer state (ZOOM-SMOOTH-2b/2c), without
// a window or a running SDL video subsystem, so tests can run it.
#pragma once

#include <SDL3/SDL_events.h>
#include <atomic>
#include <chrono>

namespace anno::app {

/// SDL_EVENT_WINDOW_EXPOSED with data1 == 1 is SDL's own live-resize/move update
/// (SDL_windowsevents.c's WM_TIMER -> SDL_OnWindowLiveResizeUpdate -> SDL_SendWindowEvent,
/// verified in the SDL 3.4.18 source this build pins: SDL_PushEvent calls every registered
/// event watch synchronously, under the watch list's own lock, before the event is queued -
/// so a watch genuinely runs during the native Windows modal move/resize loop, which blocks
/// the ordinary SDL_PollEvent loop). The X11/Wayland/Cocoa backends never send this signal
/// (only SDL_windowsevents.c calls SDL_OnWindowLiveResizeUpdate), so this is false on Linux
/// and macOS by construction - no platform check is needed here.
///
/// Reads only `e` (local to the caller, never shared): an event watch runs on whichever thread
/// calls SDL_PushEvent, the sim's own wake push (PlayRunner, play.cpp) included, so production
/// code (Viewer::live_resize_watch, ZOOM-SMOOTH-2b F1) checks this *before* reading any of
/// LiveResizeState's shared, atomic fields below - a push of any other event type then never
/// touches shared state at all, from any thread.
[[nodiscard]] inline bool is_live_resize_expose(const SDL_Event& e) {
    return e.type == SDL_EVENT_WINDOW_EXPOSED && e.window.data1 == 1;
}

/// The full decision (the event check above, and a plain reentrancy guard) in one call - a
/// convenience for a simple, decoupled logic test; takes `already_drawing` as a plain bool, with
/// no shared state involved. Production code does not call this directly: calling this form from
/// inside the real watch would read a shared guard unconditionally, for every event of every
/// type, from whichever thread pushed it - exactly the data race ZOOM-SMOOTH-2b F1 fixed.
[[nodiscard]] inline bool should_draw_from_watch(const SDL_Event& e, bool already_drawing) {
    return is_live_resize_expose(e) && !already_drawing;
}

/// The live-resize watch's own state (ZOOM-SMOOTH-2c, replacing a separate pair of ad hoc
/// atomics in Viewer with one small type both viewer.cpp and the tests use directly, so a test
/// exercises the real decision instead of a second copy of it - the 2b review's F4).
///
/// Two distinct things are tracked, not one (2c's own F3 fix):
/// - **frame_in_progress**, the reentrancy guard: true for the whole duration of *any*
///   frame_step() call, the main loop's ordinary draws included, so a watch never nests a frame
///   step inside one already running.
/// - **a watch draw specifically**: only a watch-triggered frame_step() holds SDL's watch-list
///   lock for its own duration (an ordinary main-loop draw holds no SDL lock at all), so only
///   *that* is the signal the wake listener's defer_wake() must act on - deferring during an
///   ordinary draw would buy nothing and only delay an unrelated wake (a command outcome, an
///   opening's ships moving, neither of which necessarily publishes a batch) by up to the main
///   loop's own idle wait.
///
/// **A residual, bounded race is accepted, not fixed** (2c review): defer_wake() can still read
/// its watch-draw flag as clear after SDL has already taken its own watch-list lock for this
/// dispatch but before begin_watch_draw() below has run - a few instructions wide, set by the
/// order live_resize_watch calls these in, not a helper-thread-sized window. In that narrow case
/// the wake pushes as normal and genuinely waits for SDL's lock, as it always did before 2b, for
/// however long is left of that one dispatch - not "never", a bounded few instructions.
class LiveResizeState {
  public:
    /// The reentrancy guard's own read, for frame_step()'s own early return.
    [[nodiscard]] bool frame_in_progress() const {
        return frame_in_progress_.load(std::memory_order_acquire);
    }
    /// frame_step()'s own begin/end around its body, once the reentrancy check above has passed.
    void begin_frame() { frame_in_progress_.store(true, std::memory_order_release); }
    void end_frame() { frame_in_progress_.store(false, std::memory_order_release); }

    /// live_resize_watch's own begin/end, around its call to frame_step() - set first, right
    /// after the event's own type is confirmed (is_live_resize_expose), before frame_step() is
    /// even called, so the window between "SDL holds its lock" and "this flag is true" is as
    /// small as the two calls here allow.
    void begin_watch_draw() { watch_drawing_.store(true, std::memory_order_release); }
    void end_watch_draw() { watch_drawing_.store(false, std::memory_order_release); }

    /// The wake listener's own decision. Returns true (deferred: the caller must not push now)
    /// only while a watch draw is in progress; otherwise false (push as normal - the common
    /// case, including every ordinary main-loop draw, where a push never waits on anything of
    /// ours).
    [[nodiscard]] bool defer_wake() {
        if (!watch_drawing_.load(std::memory_order_acquire)) {
            return false;
        }
        pending_wake_.store(true, std::memory_order_release);
        return true;
    }

    /// frame_step()'s own pickup of a wake deferred while this instance's watch draw was in
    /// progress: true if one was waiting (and clears it in the same call).
    [[nodiscard]] bool take_pending() {
        return pending_wake_.exchange(false, std::memory_order_acq_rel);
    }

    /// The watch's own last tick (ZOOM-SMOOTH-2d): call once per live_resize_watch invocation
    /// whose type test passes, drawn or guard-skipped alike. A caller measuring "time spent
    /// handling window events" across one SDL_PollEvent call can then start from here instead
    /// of a point captured before the native modal move/resize loop began ticking this watch
    /// repeatedly - the fix for the events_ms_ staleness the owner's own trace found (5763 ms
    /// reported against an 18 ms real gap, viewer.cpp's own main loop).
    void mark_watch_tick() {
        last_watch_tick_ns_.store(std::chrono::steady_clock::now().time_since_epoch().count(),
                                  std::memory_order_release);
    }
    /// The last mark_watch_tick(), or steady_clock::time_point::min() if none has happened yet
    /// (so it never wins a "is this later than events_start" comparison before the first tick).
    [[nodiscard]] std::chrono::steady_clock::time_point last_watch_tick() const {
        return std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration(
            last_watch_tick_ns_.load(std::memory_order_acquire)));
    }

  private:
    std::atomic<bool> frame_in_progress_{false};
    std::atomic<bool> watch_drawing_{false};
    std::atomic<bool> pending_wake_{false};
    std::atomic<std::chrono::steady_clock::rep> last_watch_tick_ns_{
        std::chrono::steady_clock::time_point::min().time_since_epoch().count()};
};

} // namespace anno::app
