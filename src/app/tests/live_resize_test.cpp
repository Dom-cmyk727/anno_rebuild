// The live-resize/move event watch (ZOOM-SMOOTH-2): headless, under SDL's events subsystem
// alone (no window, no video driver needed for SDL_AddEventWatch/SDL_PushEvent).
#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <utility>

#include <doctest/doctest.h>

#include <anno/app/live_resize.hpp>

namespace {

SDL_Event exposed(int data1) {
    SDL_Event e{};
    e.type = SDL_EVENT_WINDOW_EXPOSED;
    e.window.data1 = data1;
    return e;
}

struct SdlEvents {
    SdlEvents() { REQUIRE(SDL_Init(SDL_INIT_EVENTS)); }
    ~SdlEvents() { SDL_Quit(); }
};

} // namespace

TEST_CASE("should_draw_from_watch: only SDL_EVENT_WINDOW_EXPOSED with data1 == 1, and never "
          "while already drawing" *
          doctest::test_suite("live_resize")) {
    CHECK(anno::app::should_draw_from_watch(exposed(1), false));
    // A plain expose (data1 == 0, an ordinary redraw request, not a live-resize tick) never
    // triggers the watch - the normal SDL_PollEvent loop already handles those.
    CHECK_FALSE(anno::app::should_draw_from_watch(exposed(0), false));
    // The reentrancy guard: a frame already being drawn blocks the watch outright, regardless
    // of the event's own type or data1.
    CHECK_FALSE(anno::app::should_draw_from_watch(exposed(1), true));
    SDL_Event other{};
    other.type = SDL_EVENT_WINDOW_RESIZED;
    CHECK_FALSE(anno::app::should_draw_from_watch(other, false));
}

TEST_CASE("the event watch draws once per EXPOSED(1), not for a plain expose, and not while a "
          "frame is already being drawn" *
          doctest::test_suite("live_resize")) {
    // Deliberately does not push a *real* nested event from inside the watch: SDL's watch-list
    // lock (confirmed held for the whole dispatch in the pinned SDL 3.4.18 source,
    // SDL_eventwatch.c) is not guaranteed recursive on every platform SDL supports, so forcing
    // a genuine nested SDL_PushEvent-from-a-watch in a test could deadlock on a platform whose
    // mutex isn't. The guard (already_drawing) is instead set directly, exactly as production
    // code sets drawing_frame_ before calling draw() - the same case should_draw_from_watch's
    // own unit test above already covers in isolation; this test additionally confirms the
    // real SDL_PushEvent -> watcher wiring reaches it.
    const SdlEvents guard;
    int draws = 0;
    bool drawing = false;
    // Mirrors Viewer::live_resize_watch exactly (userdata points at `draws`/`drawing` here
    // instead of a real Viewer, so the mechanism is tested without a window).
    auto watch = [](void* userdata, SDL_Event* event) -> bool {
        auto* ctx = static_cast<std::pair<int*, bool*>*>(userdata);
        if (anno::app::should_draw_from_watch(*event, *ctx->second)) {
            ++*ctx->first;
        }
        return true;
    };
    std::pair<int*, bool*> ctx{&draws, &drawing};
    REQUIRE(SDL_AddEventWatch(watch, &ctx));
    SDL_Event e = exposed(1);
    SDL_PushEvent(&e);
    CHECK(draws == 1);
    SDL_Event off = exposed(0);
    SDL_PushEvent(&off);
    CHECK(draws == 1); // a plain expose still does not draw
    drawing = true;    // simulates a frame step already in progress
    SDL_Event again = exposed(1);
    SDL_PushEvent(&again);
    CHECK(draws == 1); // the guard blocked it
    SDL_RemoveEventWatch(watch, &ctx);
}
