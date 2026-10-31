// The aggregation of the picture's raw events (round dy, C4): a hand-specified stream with known
// latencies, coalescing, an offscreen commit, an unfinished edit and a reclamation delay; the
// summary must give exactly the expected numbers, and must be the same after a round trip
// through the saved text.
#include <filesystem>
#include <string>
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
        {20, K::NoVisibleChange, 0, 2},
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
