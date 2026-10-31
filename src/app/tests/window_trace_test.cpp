// The window trace (round ec, B and C; docs/app/window-trace.md): the production helpers the viewer
// calls for every attempted GPU frame, the bounded recorder, its finalization, and the causal rules
// of the presentation boundary. No window and no game files: the viewer's own use is the traced
// canary (docs/perf/README.md).
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/picture_trace.hpp>
#include <anno/app/window_trace.hpp>

namespace app = anno::app;
using K = app::TraceKind;

namespace {

bool contradicts(const std::vector<app::TraceEvent>& events, const std::string& what) {
    const app::TraceSummary s = app::summarize(events);
    for (const std::string& c : s.contradictions) {
        if (c.find(what) != std::string::npos) {
            return !s.succeeded();
        }
    }
    return false;
}

/// A window trace of one edit of island 0 committed in generation 1, GPU-usable in frame 2
/// (submitted at 5 ms), with `tail` and a RunEnd at 20 ms.
std::vector<app::TraceEvent> window_trace(std::vector<app::TraceEvent> tail) {
    std::vector<app::TraceEvent> e{
        {0, K::GpuMode, 0, 1},        {0.5, K::BatchPublished, 0, 7}, {0.6, K::BatchTaken, 0, 7},
        {0.7, K::Desired, 0, 1},      {1, K::Committed, 0, 1},        {1, K::Generation, 0, 1},
        {2, K::FrameSubmitted, 1, 1}, {2.5, K::FramePresented, 1, 1}, {5, K::FrameSubmitted, 1, 2},
        {5, K::ChunksReady, 0, 1},    {5, K::GpuReady, 0, 1}};
    e.insert(e.end(), tail.begin(), tail.end());
    e.push_back({20, K::RunEnd, 0, 0});
    return e;
}

} // namespace

TEST_CASE("window trace: each acquire status is handled as the viewer handles it" *
          doctest::test_suite("window_trace")) {
    using A = app::AcquireAction;
    CHECK(app::classify_acquire(app::kAcquireSuccessOptimal) == A::Draw);
    CHECK(app::classify_acquire(app::kAcquireSuccessSuboptimal) == A::DrawThenReconfigure);
    CHECK(app::classify_acquire(app::kAcquireOutdated) == A::Reconfigure);
    CHECK(app::classify_acquire(app::kAcquireLost) == A::Reconfigure);
    CHECK(app::classify_acquire(app::kAcquireTimeout) == A::BackOff);
    CHECK(app::classify_acquire(app::kAcquireOccluded) == A::BackOff);
    CHECK(app::classify_acquire(app::kAcquireError) == A::Fatal);
    CHECK(app::classify_acquire(0) == A::Fatal);
    CHECK(app::classify_acquire(0x7FFFFFFF) == A::Fatal);
    // Only the two successes give a texture to draw into.
    for (const std::uint32_t s : {app::kAcquireTimeout, app::kAcquireOutdated, app::kAcquireLost,
                                  app::kAcquireError, app::kAcquireOccluded, std::uint32_t{0}}) {
        CAPTURE(s);
        CHECK(!app::acquire_gives_texture(app::classify_acquire(s)));
    }
    CHECK(app::acquire_gives_texture(A::Draw));
    CHECK(app::acquire_gives_texture(A::DrawThenReconfigure));
    // The statuses are named as the headers name them; an unknown one keeps its number.
    CHECK(app::acquire_status_name(app::kAcquireOccluded) == "Occluded");
    CHECK(app::acquire_status_name(app::kAcquireLost) == "Lost");
    CHECK(app::acquire_status_name(99) == "unknown(99)");
    // Retry, then back off, then give up: the bounds the viewer counts against.
    CHECK(app::kAcquireBackOffAfter < app::kAcquireGiveUpAfter);
}

TEST_CASE("window trace: a turn's outcome comes from the sim's rejections, never from submission" *
          doctest::test_suite("window_trace")) {
    app::TurnRecord t;
    CHECK(app::turn_outcome(t) == "none");
    t.applied = 1;
    CHECK(app::turn_outcome(t) == "accepted");
    t.rejected = 1;
    CHECK(app::turn_outcome(t) == "rejected");
    t.applied = 3;
    CHECK(app::turn_outcome(t) == "ambiguous"); // which of the three is not known
    t.rejected = 3;
    CHECK(app::turn_outcome(t) == "rejected");
}

TEST_CASE("window trace: a change completes at the first successful present of a frame that draws "
          "it, a failed or earlier frame completes nothing, and one island never completes "
          "another" *
          doctest::test_suite("window_trace")) {
    app::PresentCorrelator c;
    c.gpu_ready(0, 1, 5); // island 0, generation 1: GPU-usable in frame 5
    c.gpu_ready(1, 1, 7); // island 1, same generation: frame 7
    CHECK(c.waiting() == 2);
    // Frame 4 was submitted before the change was usable: it shows the old pixels.
    CHECK(c.presented(4).empty());
    // Frame 5's present failed: the viewer never calls presented() for it. Frame 6 succeeds.
    const auto six = c.presented(6);
    REQUIRE(six.size() == 1);
    CHECK(six[0] == std::pair<std::uint32_t, std::uint64_t>{0, 1});
    CHECK(c.waiting() == 1);
    // Island 0's present does not complete island 1 (usable only from frame 7).
    CHECK(c.presented(6).empty());
    const auto seven = c.presented(7);
    REQUIRE(seven.size() == 1);
    CHECK(seven[0] == std::pair<std::uint32_t, std::uint64_t>{1, 1});
    // Each change completes once.
    CHECK(c.presented(8).empty());
    CHECK(c.waiting() == 0);
    // A generation handle does not imply fresh pixels: a later generation of the same island is
    // its own change.
    c.gpu_ready(0, 2, 9);
    CHECK(c.presented(8).empty());
    CHECK(c.presented(9).size() == 1);
}

TEST_CASE("window trace: the visible keys are the frame group's chunks over its view" *
          doctest::test_suite("window_trace")) {
    const auto keys = app::view_keys(anno::render::Rect{0, 0, 1, 1}, 2);
    REQUIRE(keys.size() == 1);
    CHECK(std::get<0>(keys[0]) == 2);
    CHECK(std::get<1>(keys[0]) == 0);
    CHECK(std::get<2>(keys[0]) == 0);
    // A view across a chunk border on both axes touches four chunks, all of the frame's group.
    const int c = anno::render::kChunk;
    const auto four = app::view_keys(anno::render::Rect{c - 1, c - 1, 2, 2}, 1);
    CHECK(four.size() == 4);
    for (const auto& k : four) {
        CHECK(std::get<0>(k) == 1);
    }
    CHECK(app::view_keys(anno::render::Rect{0, 0, 0, 0}, 2).empty());
}

TEST_CASE("window trace: the presentation rules refuse what a successful present cannot be" *
          doctest::test_suite("window_trace")) {
    // The positive control: presented with frame 3's successful present, after frame 2 made the
    // change GPU-usable.
    {
        const auto events = window_trace(
            {{6, K::FrameSubmitted, 1, 3}, {7, K::FramePresented, 1, 3}, {7, K::Presented, 0, 1}});
        const app::TraceSummary s = app::summarize(events);
        CHECK(s.consistent());
        CHECK(s.succeeded());
        CHECK(s.window);
        CHECK(s.presented == 1);
        CHECK(s.present_ms.p50 == doctest::Approx(6.3));
        CHECK(app::format_summary(s).find("presentation=present_call_return") != std::string::npos);
    }
    // Presented with no GPU readiness of that pair (another island's readiness does not count).
    CHECK(contradicts(window_trace({{6, K::FrameSubmitted, 1, 3},
                                    {7, K::FramePresented, 1, 3},
                                    {7, K::Desired, 1, 1},
                                    {7, K::Presented, 1, 1}}),
                      "presented of island 1 generation 1"));
    // GPU-before-presented violated: presented at 4 ms, the GPU readiness at 5 ms.
    CHECK(contradicts(window_trace({{4, K::FramePresented, 1, 9},
                                    {4, K::Presented, 0, 1},
                                    {3.9, K::FrameSubmitted, 1, 9}}),
                      "before its gpu_ready"));
    // No successful present at that time (the frame failed, and only frame_failed is there).
    CHECK(contradicts(
        window_trace(
            {{6, K::FrameSubmitted, 1, 3}, {7, K::FrameFailed, 1, 3}, {7, K::Presented, 0, 1}}),
        "at a time with no frame_presented"));
    // A failed frame cannot also be presented.
    CHECK(contradicts(window_trace({{6, K::FrameSubmitted, 1, 3},
                                    {7, K::FramePresented, 1, 3},
                                    {7, K::FrameFailed, 1, 3},
                                    {7, K::Presented, 0, 1}}),
                      "frame 3 both presented and failed"));
    // A present of a frame submitted before the change was usable (frame 1 at 2.5 ms, re-timed
    // to after the readiness): stale pixels.
    CHECK(contradicts(window_trace({{6, K::FramePresented, 1, 1}, {6, K::Presented, 0, 1}}),
                      "frame_presented of frame 1 twice"));
    CHECK(contradicts(window_trace({{6, K::FramePresented, 1, 0},
                                    {6, K::Presented, 0, 1},
                                    {0.1, K::FrameSubmitted, 1, 0}}),
                      "before the frame 2 that made it GPU-usable"));
    // Reported late: frame 3 presented at 7 ms, the change marked presented with frame 4 at 9.
    CHECK(contradicts(window_trace({{6, K::FrameSubmitted, 1, 3},
                                    {7, K::FramePresented, 1, 3},
                                    {8, K::FrameSubmitted, 1, 4},
                                    {9, K::FramePresented, 1, 4},
                                    {9, K::Presented, 0, 1}}),
                      "but frame 3 presented it earlier"));
    // A present call result for a frame never submitted.
    CHECK(contradicts(window_trace({{7, K::FramePresented, 1, 12}}),
                      "frame_presented of frame 12 without its frame_submitted"));
    // A batch taken before it was published.
    CHECK(contradicts(window_trace({{3, K::BatchTaken, 0, 8}, {4, K::BatchPublished, 0, 8}}),
                      "batch_taken of turn 8 without its batch_published before it"));
    // Presented twice.
    CHECK(contradicts(window_trace({{6, K::FrameSubmitted, 1, 3},
                                    {7, K::FramePresented, 1, 3},
                                    {7, K::Presented, 0, 1},
                                    {8, K::FrameSubmitted, 1, 4},
                                    {9, K::FramePresented, 1, 4},
                                    {9, K::Presented, 0, 1}}),
                      "presented of island 0 generation 1 2 times"));
    // GPU-usable but never presented (every later present failed): a consistent measurement
    // that reports the change as unfinished, not as presented.
    {
        const app::TraceSummary s =
            app::summarize(window_trace({{6, K::FrameSubmitted, 1, 3}, {7, K::FrameFailed, 1, 3}}));
        CHECK(s.consistent());
        CHECK(s.presented == 0);
        CHECK(s.present_unfinished == 1);
    }
}

TEST_CASE("window trace: the recorder keeps what fits its bounds, counts the rest as dropped, and "
          "a trace with drops is written without run_end" *
          doctest::test_suite("window_trace")) {
    const auto dir = std::filesystem::temp_directory_path() / "anno_window_trace_test";
    SUBCASE("complete") {
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        app::WindowTrace t(16, 4, 4);
        t.event_now(K::GpuMode, 0, 1);
        app::FrameRecord f;
        f.frame = 1;
        f.epoch = 1;
        f.acquire_status = app::kAcquireSuccessOptimal;
        f.texture = true;
        f.present_status = app::kPresentSuccess;
        f.outcome = "presented";
        t.frame(f);
        t.turn({3, t.now_ms(), 1, 0, "", true, 2});
        t.note("adapter test");
        CHECK(t.dropped() == 0);
        const app::EvidenceOutcome out = t.finalize(dir, "case\n", "", 0);
        CHECK(out.status == 0);
        CHECK(out.trace_saved);
        const auto frames = app::read_text(dir / "frames.tsv");
        REQUIRE(frames.ok);
        CHECK(frames.text.find("\tSuccessOptimal\t1\t") != std::string::npos);
        CHECK(frames.text.find("\tSuccess\tpresented\n") != std::string::npos);
        const auto sim = app::read_text(dir / "sim.tsv");
        REQUIRE(sim.ok);
        CHECK(sim.text.find("\t1\t0\taccepted\t-\t1\t2\n") != std::string::npos);
        const auto manifest = app::read_text(dir / "manifest.txt");
        REQUIRE(manifest.ok);
        CHECK(manifest.text.find("adapter test\n") != std::string::npos);
        CHECK(manifest.text.find("dropped=0") != std::string::npos);
        const auto events = app::read_text(dir / "events.tsv");
        REQUIRE(events.ok);
        const auto parsed = app::parse_tsv(events.text);
        REQUIRE(parsed.ok());
        CHECK(app::summarize(parsed.value()).succeeded());
    }
    SUBCASE("a bound reached: incomplete, never a success") {
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        app::WindowTrace t(2, 1, 1);
        for (int i = 0; i < 5; ++i) {
            t.event_now(K::Converged, 0, 0);
        }
        t.frame({});
        t.frame({});
        t.turn({});
        t.turn({});
        CHECK(t.dropped() == 3 + 1 + 1);
        CHECK(t.events().size() == 2);
        const app::EvidenceOutcome out = t.finalize(dir, "case\n", "", 0);
        CHECK(out.status == app::kEvidenceNotSaved);
        const auto events = app::read_text(dir / "events.tsv");
        REQUIRE(events.ok);
        const auto parsed = app::parse_tsv(events.text);
        REQUIRE(parsed.ok());
        const app::TraceSummary s = app::summarize(parsed.value());
        CHECK(!s.complete);
        CHECK(!s.succeeded());
        const auto summary = app::read_text(dir / "summary.txt");
        REQUIRE(summary.ok);
        CHECK(summary.text.find("recording=INCOMPLETE dropped=5") != std::string::npos);
        CHECK(summary.text.find("run=INCOMPLETE") != std::string::npos);
    }
    SUBCASE("a file that cannot be written: no successful run_end") {
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir / "frames.tsv" / "occupied");
        app::WindowTrace t(16, 4, 4);
        t.event_now(K::GpuMode, 0, 1);
        const app::EvidenceOutcome out = t.finalize(dir, "case\n", "", 0);
        CHECK(out.status == app::kEvidenceNotSaved);
        CHECK(!out.unsaved.empty());
    }
    SUBCASE("the window's own failure is the run's status") {
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        app::WindowTrace t(16, 4, 4);
        const app::EvidenceOutcome out = t.finalize(dir, "case\n", "window_error x\n", 1);
        CHECK(out.status == 1);
        const auto summary = app::read_text(dir / "summary.txt");
        REQUIRE(summary.ok);
        CHECK(summary.text.find("window_error x") != std::string::npos);
    }
    std::filesystem::remove_all(dir);
}

TEST_CASE("window trace: recording from two threads at once keeps every record within the bound "
          "and counts the rest" *
          doctest::test_suite("window_trace")) {
    app::WindowTrace t(1000, 10, 10);
    std::atomic<bool> go{false};
    auto writer = [&](std::uint32_t island) {
        while (!go.load()) {
            std::this_thread::yield();
        }
        for (int i = 0; i < 800; ++i) {
            t.event_now(K::Converged, island, static_cast<std::uint64_t>(i));
        }
    };
    std::thread a(writer, 0), b(writer, 1);
    go.store(true);
    a.join();
    b.join();
    CHECK(t.events().size() == 1000);
    CHECK(t.dropped() == 600);
}

TEST_CASE("window trace: the frame tracer records skipped and retried acquires, resizes and failed "
          "presents, and a change completes only at a successful present of a frame that draws "
          "it" *
          doctest::test_suite("window_trace")) {
    // The viewer's own bookkeeping (FrameTracer), driven through one failure of each kind. Times
    // come from the trace's clock, so every record is in the order it was made.
    app::WindowTrace trace(1000, 100, 10);
    app::FrameTracer tracer(trace);
    auto now = [&trace] { return trace.now_ms(); };
    const anno::render::ChunkKey near{2, 0, 0}, far{2, 50, 50};
    const anno::render::Rect view{0, 0, 10, 10};
    trace.event_now(K::GpuMode, 0, 1);
    trace.event_now(K::Desired, 0, 1);
    trace.event_now(K::Desired, 1, 1);
    trace.event_now(K::Committed, 0, 1);
    trace.event_now(K::Committed, 1, 1);
    trace.event_now(K::Generation, 0, 1);
    tracer.committed(0, 1, {near});
    tracer.committed(1, 1, {far}); // offscreen: no key in the view
    bool cpu = false, gpu = false;
    const app::FrameTracer::Ready fresh = [&cpu](const anno::render::ChunkKey&) { return cpu; };
    const app::FrameTracer::Ready usable = [&gpu](const anno::render::ChunkKey&) { return gpu; };
    auto attempt = [&](std::uint32_t acquire) {
        tracer.begin({}, view, 2);
        const double t = now();
        tracer.acquired(t, now(), acquire);
    };
    auto present = [&](std::uint32_t status, bool idle) {
        const double t = now();
        tracer.presented(t, now(), now(), status, fresh, usable, idle);
    };
    attempt(app::kAcquireTimeout); // 1: skipped (backs off)
    tracer.failed("acquire_skipped");
    attempt(app::kAcquireOutdated); // 2: retried on a reconfigured surface
    tracer.failed("acquire_retry");
    tracer.reconfigured();
    attempt(app::kAcquireSuccessOptimal); // 3: drawn before the change is ready
    present(app::kPresentSuccess, false);
    cpu = true;
    attempt(app::kAcquireSuccessOptimal); // 4: CPU-ready, GPU pending; the present fails
    present(app::kPresentError, false);
    tracer.reconfigured(); // a resize
    gpu = true;
    attempt(app::kAcquireSuccessSuboptimal); // 5: GPU-usable, but the present fails
    present(app::kPresentError, false);
    CHECK(tracer.outstanding() == 1);     // usable, not presented
    attempt(app::kAcquireSuccessOptimal); // 6: presented
    present(app::kPresentSuccess, true);
    CHECK(tracer.outstanding() == 0);
    CHECK(tracer.frames() == 6);
    CHECK(tracer.epoch() == 3);

    const auto rows = trace.frames();
    REQUIRE(rows.size() == 6);
    const std::vector<std::string> outcomes{"acquire_skipped", "acquire_retry",  "presented",
                                            "present_failed",  "present_failed", "presented"};
    const std::vector<std::uint64_t> epochs{1, 1, 2, 2, 3, 3};
    for (std::size_t i = 0; i < rows.size(); ++i) {
        CAPTURE(i);
        CHECK(rows[i].frame == i + 1);
        CHECK(rows[i].outcome == outcomes[i]);
        CHECK(rows[i].epoch == epochs[i]);
        CHECK(rows[i].texture == (i >= 2));
        CHECK((rows[i].present_status != 0) == (i >= 2));
    }
    CHECK(rows[4].acquire_status == app::kAcquireSuccessSuboptimal);

    // The events tell the same story, and every causal rule holds.
    trace.event_now(K::RunEnd, 0, 0);
    const app::TraceSummary s = app::summarize(trace.events());
    CHECK(s.consistent());
    CHECK(s.succeeded());
    CHECK(s.offscreen == 1);        // island 1
    CHECK(s.visible_ready == 1);    // island 0 at frame 4
    CHECK(s.gpu_ready == 1);        // at frame 5
    CHECK(s.presented == 1);        // at frame 6, not 5
    CHECK(s.frames_submitted == 4); // 3 to 6
    CHECK(s.frames_presented == 2); // 3 and 6
    CHECK(s.frames_failed == 4);    // 1, 2, 4, 5
    double gpu_t = -1, presented_t = -1, frame6_t = -1;
    for (const auto& e : trace.events()) {
        if (e.kind == K::GpuReady) {
            gpu_t = e.t_ms;
        } else if (e.kind == K::Presented) {
            CHECK(e.island == 0);
            presented_t = e.t_ms;
        } else if (e.kind == K::FramePresented && e.version == 6) {
            frame6_t = e.t_ms;
        }
    }
    CHECK(gpu_t >= 0);
    CHECK(presented_t == frame6_t);
    CHECK(presented_t > gpu_t);
}

TEST_CASE(
    "window trace: a session that ends with a change GPU-usable but not presented is complete "
    "and reports it unfinished, never presented" *
    doctest::test_suite("window_trace")) {
    const auto dir = std::filesystem::temp_directory_path() / "anno_window_trace_shutdown";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    app::WindowTrace trace(1000, 100, 10);
    app::FrameTracer tracer(trace);
    const app::FrameTracer::Ready yes = [](const anno::render::ChunkKey&) { return true; };
    trace.event_now(K::GpuMode, 0, 1);
    trace.event_now(K::Desired, 0, 1);
    trace.event_now(K::Committed, 0, 1);
    trace.event_now(K::Generation, 0, 1);
    tracer.committed(0, 1, {{2, 0, 0}});
    tracer.begin({}, anno::render::Rect{0, 0, 10, 10}, 2);
    const double t = trace.now_ms();
    tracer.acquired(t, t, app::kAcquireSuccessOptimal);
    tracer.presented(trace.now_ms(), trace.now_ms(), trace.now_ms(), app::kPresentError, yes, yes,
                     true);
    CHECK(tracer.outstanding() == 1);
    // The window closes here: the trace is finalized with the change still waiting.
    const app::EvidenceOutcome out = trace.finalize(dir, "case\n", "", 0);
    CHECK(out.status == 0);
    const auto text = app::read_text(dir / "events.tsv");
    REQUIRE(text.ok);
    const auto parsed = app::parse_tsv(text.text);
    REQUIRE(parsed.ok());
    const app::TraceSummary s = app::summarize(parsed.value());
    CHECK(s.complete);
    CHECK(s.consistent());
    CHECK(s.presented == 0);
    CHECK(s.present_unfinished == 1);
    CHECK(s.converged_ms < 0); // never converged: the change was never presented
    std::filesystem::remove_all(dir);
}
