// Whether an SDL event should trigger a live-resize/move redraw from an event watch
// (ZOOM-SMOOTH-2), without a window or a running SDL video subsystem, so tests can run it.
#pragma once

#include <SDL3/SDL_events.h>

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
/// code (Viewer::live_resize_watch, ZOOM-SMOOTH-2b F1) checks this *before* reading its own
/// (shared, atomic) reentrancy guard - a push of any other event type then never touches that
/// shared state at all, from any thread.
[[nodiscard]] inline bool is_live_resize_expose(const SDL_Event& e) {
    return e.type == SDL_EVENT_WINDOW_EXPOSED && e.window.data1 == 1;
}

/// The full decision (the event check above, and the reentrancy guard: a watch never draws
/// while a frame is already being drawn) in one call - a convenience for tests, which can pass
/// `already_drawing` as a plain bool with no shared state involved. Production code does not
/// call this directly (see is_live_resize_expose's own comment: calling this form in the watch
/// itself would read the guard unconditionally, for every event of every type, from whichever
/// thread pushed it - exactly the data race ZOOM-SMOOTH-2b F1 fixed).
[[nodiscard]] inline bool should_draw_from_watch(const SDL_Event& e, bool already_drawing) {
    return is_live_resize_expose(e) && !already_drawing;
}

} // namespace anno::app
