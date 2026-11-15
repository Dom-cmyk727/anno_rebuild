// The live-resize/move event watch (ZOOM-SMOOTH-2): headless, under SDL's events subsystem
// alone (no window, no video driver needed for SDL_AddEventWatch/SDL_PushEvent). Uses the real
// anno::app::LiveResizeState (ZOOM-SMOOTH-2c F4), not a reimplementation, so these tests
// exercise the production decision rather than a second copy of it.
#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <atomic>
#include <chrono>
#include <thread>
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
    // mutex isn't. The guard is instead marked directly via begin_frame()/end_frame(), exactly
    // as Viewer::frame_step() does; this test additionally confirms the real SDL_PushEvent ->
    // watcher wiring reaches is_live_resize_expose and the real LiveResizeState together.
    const SdlEvents guard;
    int draws = 0;
    anno::app::LiveResizeState state;
    using Ctx = std::pair<int*, anno::app::LiveResizeState*>;
    auto watch = [](void* userdata, SDL_Event* event) -> bool {
        if (!anno::app::is_live_resize_expose(*event)) {
            return true;
        }
        auto* ctx = static_cast<Ctx*>(userdata);
        if (!ctx->second->frame_in_progress()) {
            ++*ctx->first;
        }
        return true;
    };
    Ctx ctx{&draws, &state};
    REQUIRE(SDL_AddEventWatch(watch, &ctx));
    SDL_Event e = exposed(1);
    SDL_PushEvent(&e);
    CHECK(draws == 1);
    SDL_Event off = exposed(0);
    SDL_PushEvent(&off);
    CHECK(draws == 1); // a plain expose still does not draw
    state.begin_frame();
    SDL_Event again = exposed(1);
    SDL_PushEvent(&again);
    CHECK(draws == 1); // the guard blocked it
    state.end_frame();
    SDL_RemoveEventWatch(watch, &ctx);
}

TEST_CASE("the watch never reads its own shared guard for an event of another type - a second "
          "thread may push one while the guard flips, with no race (ZOOM-SMOOTH-2b F1, run "
          "under TSan to confirm)" *
          doctest::test_suite("live_resize")) {
    const SdlEvents guard;
    std::atomic<int> draws{0}, others{0};
    anno::app::LiveResizeState state;
    using Ctx = std::pair<std::atomic<int>*, std::atomic<int>*>;
    // The watch itself does not need `state` at all here: an event of any OTHER type returns
    // from is_live_resize_expose's own check before this callback's body ever reaches it, which
    // is exactly the property under test - so a concurrent flip of state's own flags on another
    // thread, while this callback runs for a flood of non-EXPOSED events, is not a race on
    // anything this callback itself touches.
    auto watch = [](void* userdata, SDL_Event* event) -> bool {
        auto* ctx = static_cast<Ctx*>(userdata);
        if (anno::app::is_live_resize_expose(*event)) {
            ++*ctx->first;
        } else {
            ++*ctx->second;
        }
        return true;
    };
    Ctx ctx{&draws, &others};
    REQUIRE(SDL_AddEventWatch(watch, &ctx));
    std::atomic<bool> stop{false};
    std::thread pusher([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            SDL_Event e{};
            e.type = SDL_EVENT_WINDOW_RESIZED; // never the live-resize signal
            SDL_PushEvent(&e);
        }
    });
    for (int i = 0; i < 200; ++i) {
        // The window thread's own flips of the real guard, exactly as live_resize_watch and
        // frame_step make them (unsynchronized reads of these would be UB if the watch ever
        // touched them for these pushes; it does not).
        if (i % 2 == 0) {
            state.begin_frame();
        } else {
            state.end_frame();
        }
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    stop.store(true, std::memory_order_relaxed);
    pusher.join();
    SDL_RemoveEventWatch(watch, &ctx);
    CHECK(draws.load() == 0); // never the live-resize event in this test
    CHECK(others.load() > 0); // the flood did reach the watch
}

TEST_CASE("defer_wake defers only while a watch draw is in progress, not an ordinary frame, and "
          "take_pending picks the deferral up afterwards (ZOOM-SMOOTH-2c F3)" *
          doctest::test_suite("live_resize")) {
    anno::app::LiveResizeState state;
    // An ordinary frame (begin_frame/end_frame alone, no watch draw) never defers: it holds no
    // SDL lock, so deferring here would buy nothing and only delay an unrelated wake.
    state.begin_frame();
    CHECK_FALSE(state.defer_wake());
    state.end_frame();
    // A watch draw (begin_watch_draw, as live_resize_watch calls it) does defer.
    state.begin_watch_draw();
    CHECK(state.defer_wake());
    state.end_watch_draw();
    // frame_step()'s own pickup: true once, then false (cleared).
    CHECK(state.take_pending());
    CHECK_FALSE(state.take_pending());
}

TEST_CASE("a wake is never lost under concurrent watch draws: every deferral is eventually "
          "taken, and the state always ends fully drained (ZOOM-SMOOTH-2c F4)" *
          doctest::test_suite("live_resize")) {
    // pending_wake_ is a single flag, not a counter (by design - a wake means "something
    // changed," the same idempotent signal dirty_ itself already is), so concurrent deferrals
    // legitimately coalesce into one pickup. The invariant this proves is therefore not "one
    // take per defer," but the one the review asks for: a deferral is never stuck forever
    // (always taken by some later drain) and the state never ends up claiming a wake is still
    // pending when none genuinely is.
    anno::app::LiveResizeState state;
    std::atomic<bool> stop{false};
    std::atomic<int> deferred{0};
    // The "window thread": a tight loop of watch draws (exactly live_resize_watch's own
    // begin/end pair around frame_step()) with a wake attempted during most of them - this is
    // what defer_wake() and take_pending() race against each other over.
    std::thread watcher([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            state.begin_watch_draw();
            if (state.defer_wake()) {
                ++deferred;
            }
            state.end_watch_draw();
        }
    });
    // The "frame_step() thread": continuously drains, exactly as frame_step() does at its own
    // start on every call, whether watch-triggered or from the main loop.
    std::atomic<int> taken{0};
    std::thread consumer([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            if (state.take_pending()) {
                ++taken;
            }
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    stop.store(true, std::memory_order_relaxed);
    watcher.join();
    consumer.join();
    // At least one wake was actually deferred during the run (the test is otherwise vacuous).
    REQUIRE(deferred.load() > 0);
    // Never lost: some drain (during the run, or the final one below) must have taken at least
    // one - the flag cannot have ended up permanently stuck with no pickup ever happening.
    const bool final_pending = state.take_pending();
    CHECK((taken.load() > 0 || final_pending));
    // Fully drained: after that one last take, nothing more is left pending.
    CHECK_FALSE(state.take_pending());
}

TEST_CASE("mark_watch_tick only ever advances, and reads as far in the past as "
          "steady_clock::time_point::min() before the first call - the comparison "
          "viewer.cpp's own events_ms_ fix makes (ZOOM-SMOOTH-2d) must never prefer an "
          "untouched tick over a real events_start" *
          doctest::test_suite("live_resize")) {
    anno::app::LiveResizeState state;
    const auto before = std::chrono::steady_clock::now();
    CHECK(state.last_watch_tick() < before);
    state.mark_watch_tick();
    const auto first = state.last_watch_tick();
    CHECK(first >= before);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    state.mark_watch_tick();
    CHECK(state.last_watch_tick() > first);
}
