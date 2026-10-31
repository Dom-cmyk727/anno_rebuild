// The aggregation of the picture's raw events (round dy, C4): a hand-specified stream with known
// latencies, coalescing, an offscreen commit, an unfinished edit and a reclamation delay; the
// summary must give exactly the expected numbers, and must be the same after a round trip
// through the saved text.
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
