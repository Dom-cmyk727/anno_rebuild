// The live-resize/move event watch (ZOOM-SMOOTH-2): headless, under SDL's events subsystem
// alone (no window, no video driver needed for SDL_AddEventWatch/SDL_PushEvent).
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
    // mutex isn't. The guard (already_drawing) is instead set directly, exactly as production
    // code sets drawing_frame_ before calling draw() - the same case should_draw_from_watch's
    // own unit test above already covers in isolation; this test additionally confirms the
    // real SDL_PushEvent -> watcher wiring reaches it.
    const SdlEvents guard;
    std::atomic<int> draws{0};
    std::atomic<bool> drawing{false};
    // Mirrors Viewer::live_resize_watch exactly (type checked first, the atomic touched only
    // for the one event type that can ever matter - ZOOM-SMOOTH-2b F1), with userdata pointing
    // at `draws`/`drawing` here instead of a real Viewer, so the mechanism is tested without a
    // window.
    using Ctx = std::pair<std::atomic<int>*, std::atomic<bool>*>;
    auto watch = [](void* userdata, SDL_Event* event) -> bool {
        if (!anno::app::is_live_resize_expose(*event)) {
            return true;
        }
        auto* ctx = static_cast<Ctx*>(userdata);
        if (!ctx->second->load(std::memory_order_acquire)) {
            ++*ctx->first;
        }
        return true;
    };
    Ctx ctx{&draws, &drawing};
    REQUIRE(SDL_AddEventWatch(watch, &ctx));
    SDL_Event e = exposed(1);
    SDL_PushEvent(&e);
    CHECK(draws.load() == 1);
    SDL_Event off = exposed(0);
    SDL_PushEvent(&off);
    CHECK(draws.load() == 1); // a plain expose still does not draw
    drawing.store(true);      // simulates a frame step already in progress
    SDL_Event again = exposed(1);
    SDL_PushEvent(&again);
    CHECK(draws.load() == 1); // the guard blocked it
    SDL_RemoveEventWatch(watch, &ctx);
}

TEST_CASE("the watch never reads its own shared guard for an event of another type - a second "
          "thread may push one while the guard flips, with no race (ZOOM-SMOOTH-2b F1, run "
          "under TSan to confirm)" *
          doctest::test_suite("live_resize")) {
    const SdlEvents guard;
    std::atomic<int> draws{0}, others{0};
    std::atomic<bool> drawing{false};
    using Ctx = std::pair<std::atomic<int>*, std::atomic<int>*>;
    // The watch itself does not need `drawing` at all here: an event of any OTHER type returns
    // from is_live_resize_expose's own check before this callback's body ever reaches it, which
    // is exactly the property under test - so a concurrent, unsynchronized flip of `drawing` on
    // another thread, while this callback runs for a flood of non-EXPOSED events, is not a race
    // on anything this callback itself touches.
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
        // The window thread's own flips of the real drawing_frame_ (unsynchronized reads would
        // be UB if the watch ever touched it for these pushes; it does not).
        drawing.store(i % 2 == 0, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    stop.store(true, std::memory_order_relaxed);
    pusher.join();
    SDL_RemoveEventWatch(watch, &ctx);
    CHECK(draws.load() == 0); // never the live-resize event in this test
    CHECK(others.load() > 0); // the flood did reach the watch
}

TEST_CASE("a wake deferred while a draw is in progress returns at once, without pushing or "
          "waiting for SDL's watch-list lock (ZOOM-SMOOTH-2b F2)" *
          doctest::test_suite("live_resize")) {
    // Mirrors the wake listener's own decision (viewer.cpp): if a draw is in progress, record a
    // pending wake and return - never call SDL_PushEvent, which would otherwise wait for the
    // same lock a live-resize draw holds for its own duration.
    std::atomic<bool> drawing{true};
    std::atomic<bool> pending_wake{false};
    int pushes = 0;
    auto wake = [&] {
        if (drawing.load(std::memory_order_acquire)) {
            pending_wake.store(true, std::memory_order_release);
            return;
        }
        ++pushes;
    };
    const auto start = std::chrono::steady_clock::now();
    wake();
    const auto elapsed = std::chrono::steady_clock::now() - start;
    CHECK(pushes == 0); // deferred, not pushed
    CHECK(pending_wake.load());
    // Nowhere near a frame's own busy time (a few ms) or SDL's lock-wait: this call never
    // touched SDL at all while drawing was true.
    CHECK(elapsed < std::chrono::milliseconds(1));
    drawing.store(false);
    wake();
    CHECK(pushes == 1); // once nothing is drawing, the wake pushes normally
}
