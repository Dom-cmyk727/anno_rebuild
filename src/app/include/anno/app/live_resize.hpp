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
/// `already_drawing` is the reentrancy guard: a watch never draws while a frame is already
/// being drawn. draw() may itself process window messages synchronously (a GPU present, a
/// surface reconfigure), which could re-enter the watch from inside an already-running frame
/// step; without the guard that would recurse into drawing a second frame before the first
/// one finished.
[[nodiscard]] inline bool should_draw_from_watch(const SDL_Event& e, bool already_drawing) {
    return !already_drawing && e.type == SDL_EVENT_WINDOW_EXPOSED && e.window.data1 == 1;
}

} // namespace anno::app
