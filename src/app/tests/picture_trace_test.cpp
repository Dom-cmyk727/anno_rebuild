// The aggregation of the picture's raw events (round dy, C4): a hand-specified stream with known
// latencies, coalescing, an offscreen commit, an unfinished edit and a reclamation delay; the
// summary must give exactly the expected numbers, and must be the same after a round trip
// through the saved text.
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/picture_trace.hpp>

namespace app = anno::app;
using K = app::TraceKind;

namespace {

std::vector<app::TraceEvent> hand_stream() {
    return {
        // Island 0: version 1 is superseded by version 2 while it builds; version 2 is committed
        // at 10 ms in generation 1, whose visible chunks are ready at 14 ms.
        {0, K::Desired, 0, 1},
        {1, K::Admitted, 0, 1},
        {2, K::Started, 0, 1},
        {3, K::Desired, 0, 2},
        {5, K::Superseded, 0, 1},
        {6, K::Admitted, 0, 2},
        {6, K::Started, 0, 2},
        {9, K::Built, 0, 2},
        {10, K::Committed, 0, 2},
        {10, K::Generation, 0, 1},
        {14, K::ChunksReady, 0, 1},
        // Island 1: committed at 20 ms in generation 2, which changes nothing in the view.
        {12, K::Desired, 1, 1},
        {12, K::Admitted, 1, 1},
        {13, K::Built, 1, 1},
        {20, K::Committed, 1, 1},
        {20, K::Generation, 0, 2},
        {20, K::NoVisibleChange, 1, 2}, // island 1's change in generation 2
        {20, K::Retired, 0, 1},
        {25, K::Reclaimed, 0, 1},
        // Island 2: never committed; the trace ends at 40 ms.
        {30, K::Desired, 2, 1},
        {30, K::Deferred, 2, 1},
        {35, K::DrainStart, 0, 0},
        {40, K::DrainEnd, 0, 0},
    };
}

void check(const app::TraceSummary& s) {
    CHECK(s.edits == 4);
    CHECK(s.object_ready == 3);
    CHECK(s.coalesced == 1);
    CHECK(s.unfinished == 1);
    CHECK(s.offscreen == 1);
    CHECK(s.visible_ready == 2);
    CHECK(s.superseded == 1);
    CHECK(s.deferred == 1);
    CHECK(s.commits == 2);
    CHECK(s.generations == 2);
    // Object-ready: 10 (v1 through v2), 7 (v2) and 8 (island 1).
    CHECK(s.object_ms.n == 3);
    CHECK(s.object_ms.p50 == doctest::Approx(8));
    CHECK(s.object_ms.max == doctest::Approx(10));
    CHECK(!s.object_ms.p95_valid);
    // Visible: 14 (v1) and 11 (v2); island 1's commit is offscreen.
    CHECK(s.visible_ms.n == 2);
    CHECK(s.visible_ms.max == doctest::Approx(14));
    CHECK(s.visible_ms.p50 == doctest::Approx(11));
    CHECK(s.reclaim_ms.n == 1);
    CHECK(s.reclaim_ms.max == doctest::Approx(5));
    // Builds: v1 waited 1 ms and ran 3 ms until it was superseded; v2 waited 0 ms and ran 3 ms.
    // Island 1's build has no Started event, so it gives no sample.
    CHECK(s.queue_ms.n == 2);
    CHECK(s.queue_ms.max == doctest::Approx(1));
    CHECK(s.build_ms.n == 2);
    CHECK(s.build_ms.max == doctest::Approx(3));
    // The unfinished edit is 10 ms old when the trace ends, as old as the oldest committed one.
    CHECK(s.max_age_ms == doctest::Approx(10));
    CHECK(s.last_commit_ms == doctest::Approx(20));
    CHECK(s.end_ms == doctest::Approx(40));
}

} // namespace

TEST_CASE("picture trace: a hand-specified stream gives its known latencies" *
          doctest::test_suite("picture_trace")) {
    const auto events = hand_stream();
    check(app::summarize(events));
}

TEST_CASE("picture trace: the summary is the same after the events are saved and read back" *
          doctest::test_suite("picture_trace")) {
    const auto events = hand_stream();
    const std::string text = app::to_tsv(events);
    const auto parsed = app::parse_tsv(text);
    REQUIRE(parsed.ok());
    REQUIRE(parsed.value().size() == events.size());
    check(app::summarize(parsed.value()));
    CHECK(app::format_summary(app::summarize(parsed.value())) ==
          app::format_summary(app::summarize(events)));
    CHECK(!app::parse_tsv("t_ms\tkind\tisland\tversion\n1.0\tbogus\t0\t0\n").ok());
}

TEST_CASE("picture trace: percentiles report their sample counts and validity" *
          doctest::test_suite("picture_trace")) {
    std::vector<double> few(19, 1.0);
    CHECK(!app::distribution(few).p95_valid);
    std::vector<double> twenty(20);
    for (std::size_t i = 0; i < twenty.size(); ++i) {
        twenty[i] = static_cast<double>(i + 1);
    }
    const auto d = app::distribution(twenty);
    CHECK(d.p95_valid);
    CHECK(!d.p99_valid);
    CHECK(d.p95 == doctest::Approx(19));
    CHECK(d.p50 == doctest::Approx(10));
    CHECK(d.max == doctest::Approx(20));
}

namespace {

std::string trace_of(const std::vector<std::string>& rows) {
    std::string out(app::kTraceHeader);
    out += "\n";
    for (const std::string& r : rows) {
        out += r + "\n";
    }
    return out;
}

/// The refusal of `text`, which must fail, and must name `reason`.
void refused(const std::string& text, const std::string& reason) {
    CAPTURE(text);
    const auto parsed = app::parse_tsv(text);
    REQUIRE(!parsed.ok());
    CAPTURE(parsed.error());
    CHECK(parsed.error().find(reason) != std::string::npos);
}

} // namespace

TEST_CASE("picture trace: malformed traces are refused for the reason they are malformed" *
          doctest::test_suite("picture_trace")) {
    refused("", "empty");
    refused("1.0\tdesired\t0\t1\n", "header");
    refused("t_ms\tkind\tisland\n1.0\tdesired\t0\n", "header");
    refused(trace_of({"1.0\tdesired\t0"}), "3 fields, expected 4");
    refused(trace_of({"1.0\tdesired\t0\t1\t9"}), "5 fields, expected 4");
    refused(trace_of({"1.5x\tdesired\t0\t1"}), "the time '1.5x'");
    refused(trace_of({"\tdesired\t0\t1"}), "the time ''");
    refused(trace_of({"nan\tdesired\t0\t1"}), "the time 'nan'");
    refused(trace_of({"inf\tdesired\t0\t1"}), "the time 'inf'");
    refused(trace_of({"-3\tdesired\t0\t1"}), "the time '-3'");
    refused(trace_of({"1e999\tdesired\t0\t1"}), "the time '1e999'");
    refused(trace_of({"1.0\tbogus\t0\t1"}), "unknown kind 'bogus'");
    refused(trace_of({"1.0\tdesired\t12abc\t1"}), "the island '12abc' is not a whole number");
    refused(trace_of({"1.0\tdesired\t-1\t1"}), "the island '-1' is not a whole number");
    refused(trace_of({"1.0\tdesired\t4294967296\t1"}), "the island '4294967296' is out of range");
    refused(trace_of({"1.0\tdesired\t0\t18446744073709551616"}), "is out of range");
    refused(trace_of({"1.0\tdesired\t0\t1.5"}), "the version '1.5' is not a whole number");
    refused(trace_of({"1.0\tdesired\t0\t1", "", "2.0\tdesired\t0\t2"}), "a blank line");
    refused(trace_of({"1.0\trun_end\t0\t0", "2.0\tdesired\t0\t2"}), "an event after run_end");
}

TEST_CASE("picture trace: an idle run, a productive run, a cut trace and a failed run are told "
          "apart" *
          doctest::test_suite("picture_trace")) {
    // A valid idle run: complete, no edit.
    const auto idle = app::parse_tsv(
        trace_of({"0.0\tdrain_start\t0\t0", "1.0\tdrain_end\t0\t0", "1.0\trun_end\t0\t0"}));
    REQUIRE(idle.ok());
    const auto si = app::summarize(idle.value());
    CHECK(si.complete);
    CHECK(si.succeeded());
    CHECK(si.edits == 0);
    // A productive run: the hand stream, ended.
    auto events = hand_stream();
    events.push_back({40, K::RunEnd, 0, 0}); // with the drain's end: the same end time
    const auto productive = app::parse_tsv(app::to_tsv(events));
    REQUIRE(productive.ok());
    const auto sp = app::summarize(productive.value());
    CHECK(sp.succeeded());
    check(sp);
    // A trace without run_end (the run stopped, or the file was cut): inspectable, not a success.
    const auto cut = app::summarize(hand_stream());
    CHECK(!cut.complete);
    CHECK(!cut.succeeded());
    CHECK(app::format_summary(cut).find("run=INCOMPLETE") == 0);
    // A header-only trace is no evidence of a run, not an idle run.
    const auto empty = app::parse_tsv(trace_of({}));
    REQUIRE(empty.ok());
    CHECK(!app::summarize(empty.value()).complete);
    // A run that ended with a failure: complete, not a success.
    events.back().version = 1;
    const auto failed = app::summarize(events);
    CHECK(failed.complete);
    CHECK(!failed.succeeded());
    CHECK(app::format_summary(failed).find("FAILED") != std::string::npos);
}

TEST_CASE("picture trace: evidence files are read and saved with every step checked" *
          doctest::test_suite("picture_trace")) {
    const auto dir = std::filesystem::temp_directory_path() / "anno_picture_trace_test";
    std::filesystem::create_directories(dir);
    const auto file = dir / "events.tsv";
    CHECK(app::save_text(file, "t_ms\tkind\tisland\tversion\n").empty());
    const auto back = app::read_text(file);
    REQUIRE(back.ok);
    CHECK(back.text == "t_ms\tkind\tisland\tversion\n");
    // A missing file, a directory in place of a file, and a file in a missing directory.
    const auto missing = app::read_text(dir / "missing.tsv");
    CHECK(!missing.ok);
    CHECK(missing.error.find("missing.tsv") != std::string::npos);
    const std::string to_dir = app::save_text(dir, "x");
    CHECK(to_dir.find(dir.string()) != std::string::npos);
    const std::string missing_dir = app::save_text(dir / "no" / "such" / "events.tsv", "x");
    CHECK(missing_dir.find("cannot open") != std::string::npos);
    std::filesystem::remove_all(dir);
}

namespace {

/// A run's evidence finalized in a fresh directory where `block` (if not empty) is a directory in
/// place of a file, the way a file that cannot be written is injected (round ea, 3e).
struct Finalized {
    std::filesystem::path dir;
    app::EvidenceOutcome outcome;
    std::string summary_given;
};
Finalized finalize_with(const std::string& name, const std::string& block, int verdict) {
    Finalized f;
    f.dir = std::filesystem::temp_directory_path() / ("anno_picture_finalize_" + name);
    std::filesystem::remove_all(f.dir);
    std::filesystem::create_directories(f.dir);
    if (!block.empty()) {
        std::filesystem::create_directories(f.dir / block / "occupied");
    }
    const std::vector<app::EvidenceFile> files{
        {"frames.tsv", "frame\n0\n"}, {"chunks.tsv", "c\n"}, {"manifest.txt", "case\n"}};
    const std::vector<app::TraceEvent> events{{0, K::DrainStart, 0, 0}, {1, K::DrainEnd, 0, 0}};
    f.outcome = app::finalize_evidence(f.dir, files, events, 2.0, verdict,
                                       [&f](const app::TraceSummary& s) {
                                           f.summary_given = app::format_summary(s) + "tail\n";
                                           return f.summary_given;
                                       });
    return f;
}
/// The saved trace's summary, or why it cannot be read (as bench_picture --aggregate reads it).
anno::formats::Result<app::TraceSummary, std::string>
saved_trace(const std::filesystem::path& dir) {
    if (!std::filesystem::is_regular_file(dir / "events.tsv")) {
        return std::string("no trace");
    }
    const auto text = app::read_text(dir / "events.tsv");
    if (!text.ok) {
        return text.error;
    }
    auto events = app::parse_tsv(text.text);
    if (!events.ok()) {
        return events.error();
    }
    return app::summarize(events.value());
}
bool no_partial_left(const std::filesystem::path& dir) {
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        if (e.path().extension() == ".partial") {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_CASE("picture evidence: a run is recorded as successful only after every file is saved" *
          doctest::test_suite("picture_trace")) {
    SUBCASE("every file saved: the trace carries the verdict, saved last") {
        const Finalized f = finalize_with("all", "", 0);
        CHECK(f.outcome.status == 0);
        CHECK(f.outcome.trace_saved);
        CHECK(f.outcome.unsaved.empty());
        for (const char* name :
             {"frames.tsv", "chunks.tsv", "manifest.txt", "summary.txt", "events.tsv"}) {
            CHECK_MESSAGE(std::filesystem::is_regular_file(f.dir / name), name);
        }
        const auto s = saved_trace(f.dir);
        REQUIRE(s.ok());
        CHECK(s.value().succeeded());
        CHECK(app::read_text(f.dir / "summary.txt").text == f.summary_given);
        CHECK(f.summary_given.find("(succeeded)") != std::string::npos);
        CHECK(no_partial_left(f.dir));
        std::filesystem::remove_all(f.dir);
    }
    SUBCASE("a failed verdict with every file saved is complete and failed") {
        const Finalized f = finalize_with("verdict1", "", 1);
        CHECK(f.outcome.status == 1);
        const auto s = saved_trace(f.dir);
        REQUIRE(s.ok());
        CHECK(s.value().complete);
        CHECK(s.value().run_status == 1);
        std::filesystem::remove_all(f.dir);
    }
    SUBCASE("frames.tsv cannot be saved: the trace records status 5, not the verdict") {
        // The round-dz reproduction: a directory named frames.tsv; the run's verdict was 0.
        const Finalized f = finalize_with("frames", "frames.tsv", 0);
        CHECK(f.outcome.status == app::kEvidenceNotSaved);
        CHECK(f.outcome.trace_saved);
        REQUIRE(f.outcome.unsaved.size() == 1);
        CHECK(f.outcome.unsaved[0].find("frames.tsv") != std::string::npos);
        const auto s = saved_trace(f.dir);
        REQUIRE(s.ok());
        CHECK(s.value().complete);
        CHECK(!s.value().succeeded());
        CHECK(s.value().run_status == 5);
        // The summary describes the trace as saved: failed with status 5.
        CHECK(f.summary_given.find("status=5 (FAILED)") != std::string::npos);
        CHECK(std::filesystem::is_directory(f.dir / "frames.tsv" / "occupied"));
        CHECK(std::filesystem::is_regular_file(f.dir / "chunks.tsv"));
        CHECK(no_partial_left(f.dir));
        std::filesystem::remove_all(f.dir);
    }
    SUBCASE("summary.txt cannot be saved: the trace records status 5") {
        const Finalized f = finalize_with("summary", "summary.txt", 0);
        CHECK(f.outcome.status == app::kEvidenceNotSaved);
        CHECK(f.outcome.trace_saved);
        REQUIRE(f.outcome.unsaved.size() == 1);
        CHECK(f.outcome.unsaved[0].find("summary.txt") != std::string::npos);
        const auto s = saved_trace(f.dir);
        REQUIRE(s.ok());
        CHECK(s.value().run_status == 5);
        CHECK(no_partial_left(f.dir));
        std::filesystem::remove_all(f.dir);
    }
    SUBCASE("the completion record itself cannot be saved: no trace and no summary remain") {
        const Finalized f = finalize_with("events", "events.tsv", 0);
        CHECK(f.outcome.status == app::kEvidenceNotSaved);
        CHECK(!f.outcome.trace_saved);
        REQUIRE(f.outcome.unsaved.size() == 2);
        CHECK(f.outcome.unsaved[0].find("events.tsv") != std::string::npos);
        CHECK(f.outcome.unsaved[1].find("summary.txt removed") != std::string::npos);
        CHECK(!saved_trace(f.dir).ok());
        CHECK(!std::filesystem::exists(f.dir / "summary.txt"));
        CHECK(no_partial_left(f.dir));
        std::filesystem::remove_all(f.dir);
    }
}

TEST_CASE("picture evidence: a file is replaced whole or left as it was" *
          doctest::test_suite("picture_trace")) {
    const auto dir = std::filesystem::temp_directory_path() / "anno_picture_save_whole";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    CHECK(app::save_whole(dir / "a.txt", "first").empty());
    CHECK(app::read_text(dir / "a.txt").text == "first");
    CHECK(app::save_whole(dir / "a.txt", "second").empty());
    CHECK(app::read_text(dir / "a.txt").text == "second");
    // The partial file cannot be written: the earlier text stays.
    std::filesystem::create_directories(dir / "a.txt.partial" / "occupied");
    const std::string error = app::save_whole(dir / "a.txt", "third");
    CHECK(error.find("a.txt.partial") != std::string::npos);
    CHECK(app::read_text(dir / "a.txt").text == "second");
    std::filesystem::remove_all(dir);
}

TEST_CASE("picture readiness: a set is ready only when all its keys are fresh at the same poll" *
          doctest::test_suite("picture_trace")) {
    // A change invalidates A and B. A is rebuilt; another change invalidates A again; B is rebuilt
    // while A is stale. The set must stay waiting until A is fresh again: having seen A fresh once
    // is not enough (the defect of round dy's bench, which erased a key once it was seen fresh).
    namespace render = anno::render;
    const render::ChunkKey a{2, 0, 0}, b{2, 1, 0};
    std::map<render::ChunkKey, bool> fresh{{a, false}, {b, false}};
    auto is_fresh = [&fresh](const render::ChunkKey& k) { return fresh.at(k); };
    app::ReadinessTracker tracker;
    std::vector<app::ReadinessTracker::KeyEvent> log;
    tracker.add({0, 1, {a, b}});
    CHECK(tracker.poll(is_fresh, 1, log).empty());
    fresh[a] = true; // A finishes
    CHECK(tracker.poll(is_fresh, 2, log).empty());
    fresh[a] = false; // a later change invalidates A again
    CHECK(tracker.poll(is_fresh, 3, log).empty());
    fresh[b] = true; // B finishes while A is stale
    CHECK(tracker.poll(is_fresh, 4, log).empty());
    CHECK(tracker.waiting() == 1);
    fresh[a] = true; // A is rebuilt
    const auto ready = tracker.poll(is_fresh, 5, log);
    REQUIRE(ready.size() == 1);
    CHECK(ready[0].island == 0);
    CHECK(ready[0].generation == 1);
    CHECK(tracker.waiting() == 0);
    // The log keeps every change of a key's state: A stale, fresh, stale, fresh; B stale, fresh.
    std::vector<std::pair<double, bool>> a_states, b_states;
    for (const auto& e : log) {
        (e.key == a ? a_states : b_states).emplace_back(e.t_ms, e.fresh);
    }
    CHECK(a_states ==
          std::vector<std::pair<double, bool>>{{1, false}, {2, true}, {3, false}, {5, true}});
    CHECK(b_states == std::vector<std::pair<double, bool>>{{1, false}, {4, true}});
    // A set with no key is never waited for (the caller records NoVisibleChange).
    tracker.add({1, 2, {}});
    CHECK(tracker.waiting() == 0);
}

TEST_CASE("picture trace: a generation of two islands is measured per island, with coalescing, "
          "supersession, unfinished work and convergence" *
          doctest::test_suite("picture_trace")) {
    const std::vector<app::TraceEvent> events{
        // Island 0, v1 then v2 while v1 builds; island 1, v1. Generation 1 commits island 0 at v2
        // and island 1 at v1 together. Island 0's visible keys are ready at 30, island 1's change
        // is offscreen: island 1's edit must not take island 0's readiness.
        {0, K::Desired, 0, 1},
        {1, K::Admitted, 0, 1},
        {1, K::Started, 0, 1},
        {2, K::Desired, 1, 1},
        {3, K::Desired, 0, 2},
        {5, K::Superseded, 0, 1},
        {6, K::Admitted, 0, 2},
        {6, K::Started, 0, 2},
        {9, K::Built, 0, 2},
        {9, K::Admitted, 1, 1},
        {9, K::Started, 1, 1},
        {12, K::Built, 1, 1},
        {20, K::Committed, 0, 2},
        {20, K::Committed, 1, 1},
        {20, K::Generation, 0, 1},
        {20, K::NoVisibleChange, 1, 1},
        {30, K::ChunksReady, 0, 1},
        {31, K::Converged, 0, 0},
        // Island 2: an edit after convergence that is never committed.
        {40, K::Desired, 2, 1},
        {50, K::DrainStart, 0, 0},
        {60, K::DrainEnd, 0, 0},
        {60, K::RunEnd, 0, 3},
    };
    const app::TraceSummary s = app::summarize(events);
    CHECK(s.edits == 4);
    CHECK(s.object_ready == 3);
    CHECK(s.coalesced == 1); // island 0, v1 through v2
    CHECK(s.superseded == 1);
    CHECK(s.unfinished == 1);                      // island 2
    CHECK(s.offscreen == 1);                       // island 1 only
    CHECK(s.visible_ready == 2);                   // island 0's two edits
    CHECK(s.object_ms.max == doctest::Approx(20)); // island 0 v1: 0 to 20
    CHECK(s.visible_ms.n == 2);
    CHECK(s.visible_ms.max == doctest::Approx(30)); // island 0 v1: 0 to 30
    CHECK(s.visible_ms.p50 == doctest::Approx(27)); // island 0 v2: 3 to 30
    // Converged at 31, but the edit at 40 was never committed: no convergence after the last edit.
    CHECK(s.converged_ms == doctest::Approx(-1));
    CHECK(s.max_age_ms == doctest::Approx(20)); // 60 - 40 = 20 for the unfinished one, 20 for v1
    CHECK(s.complete);
    CHECK(!s.succeeded()); // the run ended with status 3
}

TEST_CASE("picture trace: intermediate publications are counted against the desired version at "
          "their commit" *
          doctest::test_suite("picture_trace")) {
    const std::vector<app::TraceEvent> events{
        // Island 0: v1, v2, v3 desired; v1 committed while v3 is desired (intermediate), then v3.
        // Island 1: v1 desired and committed (not intermediate).
        {0, K::Desired, 0, 1},    {1, K::Desired, 0, 2},   {2, K::Desired, 0, 3},
        {3, K::Desired, 1, 1},    {5, K::Committed, 0, 1}, {5, K::Committed, 1, 1},
        {5, K::Generation, 0, 1}, {9, K::Committed, 0, 3}, {9, K::Generation, 0, 2},
        {10, K::RunEnd, 0, 0},
    };
    const app::TraceSummary s = app::summarize(events);
    CHECK(s.commits == 3);
    CHECK(s.intermediate_commits == 1);
    CHECK(s.object_ready == 4);
    CHECK(s.coalesced == 1); // v2 reached through v3
    CHECK(app::format_summary(s).find("intermediate_commits=1") != std::string::npos);
}
