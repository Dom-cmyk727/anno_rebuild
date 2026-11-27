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
        // Force SDL's own event queue past its hard cap deterministically, rather than relying
        // on how many pushes a CI runner's own speed/contention happens to fit into this test's
        // ~10 ms window: SDL_AddEvent (SDL_events.c, the pinned 3.4.18 source) refuses and calls
        // SDL_SetError exactly once the queue already holds SDL_MAX_QUEUED_EVENTS (65535) events
        // - this test never drains it (no SDL_PollEvent/SDL_PeepEvents call), so 70,000 pushes
        // always cross that line. SDL_SetError is what allocates the per-thread TLS error buffer
        // SDL_CleanupTLS below exists to release; without forcing this, the test only leaked that
        // buffer when a slow/contended run happened to reach 65535 pushes within the main
        // thread's own 200-iteration loop - the flakiness this task was opened for.
        for (int i = 0; i < 70000; ++i) {
            SDL_Event e{};
            e.type = SDL_EVENT_WINDOW_RESIZED; // never the live-resize signal
            SDL_PushEvent(&e);
        }
        while (!stop.load(std::memory_order_relaxed)) {
            SDL_Event e{};
            e.type = SDL_EVENT_WINDOW_RESIZED; // never the live-resize signal
            SDL_PushEvent(&e);
        }
        // SDL 3.4.18's own docs (SDL_thread.h, SDL_CleanupTLS): "If you are creating your
        // threads outside of SDL and then calling SDL functions, you should call this function
        // before your thread exits, to properly clean up SDL memory." This thread is a raw
        // std::thread, not SDL_CreateThread, so without this call SDL never frees the per-thread
        // error-state buffer SDL_SetError (above, via the forced queue overflow) lazily
        // allocates on it - LeakSanitizer reports that as a leak at process exit otherwise.
        SDL_CleanupTLS();
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

TEST_CASE("should_draw_from_move: false until min_interval has passed since the last tick, true "
          "at and after it, and true before any tick has ever happened (ZOOM-SMOOTH-2e)" *
          doctest::test_suite("live_resize")) {
    anno::app::LiveResizeState state;
    const auto min_interval = std::chrono::milliseconds(16);
    // Never ticked: last_watch_tick() reads as steady_clock::time_point::min(), so any real `now`
    // already clears the threshold - matches the main loop's own events_ms_ fix, which relies on
    // the same property before the first tick ever happens.
    CHECK(state.should_draw_from_move(std::chrono::steady_clock::now(), min_interval));
    state.mark_watch_tick();
    const auto tick = state.last_watch_tick();
    CHECK_FALSE(state.should_draw_from_move(tick + std::chrono::milliseconds(10), min_interval));
    CHECK(state.should_draw_from_move(tick + min_interval, min_interval));
    CHECK(state.should_draw_from_move(tick + std::chrono::milliseconds(20), min_interval));
}

TEST_CASE("a timer tick resets the move fallback's own clock exactly as a move-triggered draw "
          "does - is_live_resize_expose's own path calls mark_watch_tick() the same way "
          "live_resize_watch's move fallback would (ZOOM-SMOOTH-2e)" *
          doctest::test_suite("live_resize")) {
    anno::app::LiveResizeState state;
    state.mark_watch_tick();
    const auto just_after = state.last_watch_tick() + std::chrono::milliseconds(5);
    CHECK_FALSE(state.should_draw_from_move(just_after, std::chrono::milliseconds(16)));
}

TEST_CASE("the move fallback draws at most once per min_interval however many WINDOW_MOVED "
          "events arrive in between, real SDL_PushEvent flood included (ZOOM-SMOOTH-2e)" *
          doctest::test_suite("live_resize")) {
    // Mirrors live_resize_watch's own move-trigger decision (should_draw_from_move, then
    // mark_watch_tick on a draw) through the real SDL_AddEventWatch/SDL_PushEvent wiring, rather
    // than calling the predicate directly - the property under test is the rate limit's own
    // effect on a real flood, not just the predicate in isolation (2c F4's own reasoning: a test
    // should exercise the real decision, not a second copy of it).
    const SdlEvents guard;
    int draws = 0;
    anno::app::LiveResizeState state;
    const auto min_interval = std::chrono::milliseconds(16);
    using Ctx = std::pair<int*, anno::app::LiveResizeState*>;
    auto watch = [](void* userdata, SDL_Event* event) -> bool {
        if (event->type != SDL_EVENT_WINDOW_MOVED) {
            return true;
        }
        auto* ctx = static_cast<Ctx*>(userdata);
        if (ctx->second->should_draw_from_move(std::chrono::steady_clock::now(),
                                               std::chrono::milliseconds(16))) {
            ctx->second->mark_watch_tick();
            ++*ctx->first;
        }
        return true;
    };
    Ctx ctx{&draws, &state};
    REQUIRE(SDL_AddEventWatch(watch, &ctx));
    SDL_Event moved{};
    moved.type = SDL_EVENT_WINDOW_MOVED;
    for (int i = 0; i < 500; ++i) {
        SDL_PushEvent(&moved);
    }
    CHECK(draws == 1); // the first push past the (never-ticked) threshold; the other 499 land
                       // inside the one min_interval window the first one just opened
    std::this_thread::sleep_for(min_interval + std::chrono::milliseconds(10));
    for (int i = 0; i < 500; ++i) {
        SDL_PushEvent(&moved);
    }
    CHECK(draws == 2); // one more, now that a full min_interval has actually elapsed
    SDL_RemoveEventWatch(watch, &ctx);
}

TEST_CASE("should_draw_from_thread: the window thread's own id draws, any other id does not - no "
          "real window, no second thread, both ids synthetic (ZOOM-SMOOTH-2f)" *
          doctest::test_suite("live_resize")) {
    const std::thread::id window = std::this_thread::get_id();
    CHECK(anno::app::should_draw_from_thread(window, window));
    // A real std::thread::id that is genuinely not `window` (default-constructed ids can compare
    // equal to a real thread's id on some implementations - not relied on here).
    std::thread::id other;
    std::thread t([&] { other = std::this_thread::get_id(); });
    t.join();
    REQUIRE(other != window);
    CHECK_FALSE(anno::app::should_draw_from_thread(other, window));
}
