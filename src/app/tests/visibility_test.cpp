// Where an applied command's change is on its way to the screen (W4.4 round ee review, R2): the
// stages between "the sim applied it" and "a frame that draws it was presented", the settled
// outcomes that are not "shown" (offscreen, nothing to draw, no change), coalesced and independent
// islands, and the memory bounds. Synthetic: the boundaries are driven by hand (takes, commits,
// frame observations); the lan5 cases with real chunks are in dev_visibility_test.cpp.
#include <cstdint>
#include <memory>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/visibility.hpp>

namespace app = anno::app;
namespace render = anno::render;
using V = app::Visibility;

namespace {

using Batches = std::vector<std::shared_ptr<const app::BuildingBatch>>;
using Commit = std::vector<app::ScenePicture::CommittedIsland>;

Batches batch_at(std::uint64_t turn, bool resync = false) {
    auto b = std::make_shared<app::BuildingBatch>();
    b->turn = turn;
    b->resync = resync;
    return {b};
}

Commit commit_of(std::size_t island, std::uint64_t version, std::vector<render::ChunkKey> keys) {
    return {app::ScenePicture::CommittedIsland{island, version, std::move(keys)}};
}

/// The chunk (cx, cy) of zoom group 1, and a view that holds chunk (0, 0) only / far away.
constexpr render::ChunkKey kHere{1, 0, 0}, kThere{1, 40, 40};
const render::Rect kViewHere{0, 0, 400, 300}, kViewAway{20000, 20000, 400, 300};

const app::FrameTracer::Ready kYes = [](const render::ChunkKey&) { return true; };
const app::FrameTracer::Ready kNo = [](const render::ChunkKey&) { return false; };

/// One frame attempt that reaches its present call.
void frame(app::ChangeVisibility& v, render::Rect view, bool present_ok,
           const app::FrameTracer::Ready& fresh, const app::FrameTracer::Ready& usable) {
    v.begin_frame(view, 1);
    v.frame_done(present_ok, fresh, usable);
}

} // namespace

TEST_CASE("visibility: a change passes every boundary in order and is shown only by a presented "
          "frame that draws all of it" *
          doctest::test_suite("visibility")) {
    app::ChangeVisibility v;
    CHECK(v.classify(5) == V::NotInPicture); // nothing followed yet
    v.taken(5, batch_at(5), {0, 0}, {1, 0});
    CHECK(v.classify(5) == V::Committing); // desired, not committed
    CHECK(v.classify(6) == V::NotInPicture);
    CHECK(v.classify(4) == V::NoVisualChange); // a quiet turn before the batch
    v.committed(commit_of(0, 1, {kHere}));
    CHECK(v.classify(5) == V::AwaitingFrame); // committed, no frame has looked
    // A frame whose view holds it, chunks stale on the CPU: still not drawn.
    frame(v, kViewHere, true, kNo, kNo);
    CHECK(v.classify(5) == V::ChunksStale);
    // Fresh on the CPU, the GPU copy not usable (upload or dissolve): not drawn.
    frame(v, kViewHere, true, kYes, kNo);
    CHECK(v.classify(5) == V::NotOnGpu);
    // Usable, but the present call failed: nothing is shown.
    frame(v, kViewHere, false, kYes, kYes);
    CHECK(v.classify(5) == V::NotPresented);
    CHECK(v.waiting() == 1);
    // A successful present of a frame that draws every chunk of the change.
    frame(v, kViewHere, true, kYes, kYes);
    CHECK(v.classify(5) == V::Visible);
    CHECK(v.waiting() == 0);
    // The settled answer is kept for later asks.
    CHECK(v.classify(5) == V::Visible);
    CHECK(app::visibility_note(V::Visible, 5).empty());
    for (const V w : {V::NotInPicture, V::Committing, V::AwaitingFrame, V::ChunksStale, V::NotOnGpu,
                      V::NotPresented, V::OutOfView}) {
        CAPTURE(app::visibility_name(w));
        CHECK(app::visibility_waiting(w));
        CHECK_FALSE(app::visibility_note(w, 5).empty());
    }
    for (const V s : {V::Visible, V::Offscreen, V::NothingToDraw, V::NoVisualChange, V::Unknown}) {
        CAPTURE(app::visibility_name(s));
        CHECK_FALSE(app::visibility_waiting(s));
    }
}

TEST_CASE("visibility: offscreen, out of view, nothing to draw and no change are explicit "
          "answers, and offscreen work is never labelled visible" *
          doctest::test_suite("visibility")) {
    {
        // Committed while none of its chunks is in view: Offscreen, decided by the first attempt.
        app::ChangeVisibility v;
        v.taken(3, batch_at(3), {0}, {1});
        v.committed(commit_of(0, 1, {kThere}));
        frame(v, kViewHere, true, kYes, kYes);
        CHECK(v.classify(3) == V::Offscreen);
        CHECK(v.waiting() == 0); // never waited for
        // A later frame over it does not turn it into Visible: it was never presented as drawn.
        frame(v, render::Rect{40 * 512, 40 * 512, 400, 300}, true, kYes, kYes);
        CHECK(v.classify(3) == V::Offscreen);
    }
    {
        // In view first, then the view moves away: it waits as OutOfView, and a frame over it
        // later settles it.
        app::ChangeVisibility v;
        v.taken(3, batch_at(3), {0}, {1});
        v.committed(commit_of(0, 1, {kHere}));
        frame(v, kViewHere, true, kNo, kNo);
        frame(v, kViewAway, true, kYes, kYes);
        CHECK(v.classify(3) == V::OutOfView);
        CHECK(v.waiting() == 1);
        frame(v, kViewHere, true, kYes, kYes);
        CHECK(v.classify(3) == V::Visible);
    }
    {
        // A commit that invalidated no chunk draws nothing new.
        app::ChangeVisibility v;
        v.taken(3, batch_at(3), {0}, {1});
        v.committed(commit_of(0, 1, {}));
        CHECK(v.classify(3) == V::NothingToDraw);
    }
    {
        // A batch that changed no island of the picture, and a turn with no batch: no change.
        app::ChangeVisibility v;
        v.taken(9, batch_at(9), {2, 2}, {2, 2});
        CHECK(v.classify(9) == V::NoVisualChange);
        v.taken(12, {}, {2, 2}, {2, 2});
        CHECK(v.classify(11) == V::NoVisualChange);
        CHECK(v.classify(13) == V::NotInPicture);
    }
}

TEST_CASE("visibility: coalesced commits, independent islands and a resync" *
          doctest::test_suite("visibility")) {
    {
        // Two takes of one island, one newer commit covers both: both wait, then both are shown
        // together by the frame that draws the (union of the) chunks.
        app::ChangeVisibility v;
        v.taken(5, batch_at(5), {0}, {1});
        v.taken(6, batch_at(6), {1}, {2});
        v.committed(commit_of(0, 2, {kHere}));
        CHECK(v.classify(5) == V::AwaitingFrame);
        CHECK(v.classify(6) == V::AwaitingFrame);
        frame(v, kViewHere, true, kYes, kYes);
        CHECK(v.classify(5) == V::Visible);
        CHECK(v.classify(6) == V::Visible);
    }
    {
        // The island commits version 1, and version 2 before any frame: each commit is its own
        // part, judged and shown on its own, settled in version order. The first chunk draws and is
        // presented: version 1 is shown; version 2's chunk is not fresh, so version 2 is not.
        app::ChangeVisibility v;
        v.taken(5, batch_at(5), {0}, {1});
        v.taken(6, batch_at(6), {1}, {2});
        v.committed(commit_of(0, 1, {kHere}));
        v.committed(commit_of(0, 2, {render::ChunkKey{1, 1, 0}}));
        const app::FrameTracer::Ready only_first = [](const render::ChunkKey& k) {
            return k == render::ChunkKey{1, 0, 0};
        };
        frame(v, render::Rect{0, 0, 1000, 300}, true, only_first, only_first);
        CHECK(v.classify(5) == V::Visible);
        CHECK(v.classify(6) == V::ChunksStale); // the second commit's chunk is not fresh
        frame(v, render::Rect{0, 0, 1000, 300}, true, kYes, kYes);
        CHECK(v.classify(6) == V::Visible);
    }
    {
        // Two islands in one take: shown only when both are; the stage is the one holding it back.
        app::ChangeVisibility v;
        v.taken(5, batch_at(5), {0, 0}, {1, 1});
        v.committed(commit_of(0, 1, {kHere}));
        CHECK(v.classify(5) == V::Committing); // island 1 is not committed yet
        v.committed(commit_of(1, 1, {render::ChunkKey{1, 1, 0}}));
        const app::FrameTracer::Ready first_only = [](const render::ChunkKey& k) {
            return k == render::ChunkKey{1, 0, 0};
        };
        frame(v, render::Rect{0, 0, 1000, 300}, true, first_only, first_only);
        CHECK(v.classify(5) == V::ChunksStale); // island 0 drew, island 1 did not
        frame(v, render::Rect{0, 0, 1000, 300}, true, kYes, kYes);
        CHECK(v.classify(5) == V::Visible);
    }
    {
        // A resync batch answers for the whole range it covers (it replaced the list).
        app::ChangeVisibility v;
        v.taken(4, {}, {}, {});
        v.taken(10, batch_at(10, true), {1}, {2});
        CHECK(v.classify(7) == V::Committing);
        CHECK(v.classify(10) == V::Committing);
        CHECK(v.classify(4) == V::NoVisualChange); // before the resync's range
    }
}

TEST_CASE("visibility: the memory is bounded, merged takes wait longer and never lose a change, "
          "and a command older than the history reads Unknown" *
          doctest::test_suite("visibility")) {
    app::ChangeVisibility v;
    const std::size_t n = app::ChangeVisibility::kMaxTakes + 50;
    for (std::size_t i = 1; i <= n; ++i) {
        std::vector<std::uint64_t> before(i, 0), after(i, 0);
        after[i - 1] = 1; // each take changes its own island, to version 1
        v.taken(i, batch_at(i), before, after);
    }
    CHECK(v.waiting() <= app::ChangeVisibility::kMaxTakes);
    CHECK(v.classify(1) == V::Committing); // the oldest turn is still answered for (merged)
    CHECK(v.classify(n) == V::Committing);
    // Commit and show everything.
    Commit all;
    for (std::size_t i = 1; i <= n; ++i) {
        all.push_back({i - 1, 1, {kHere}});
    }
    v.committed(all);
    frame(v, kViewHere, true, kYes, kYes);
    CHECK(v.waiting() == 0);
    CHECK(v.classify(1) == V::Visible);
    CHECK(v.classify(n) == V::Visible);

    // More settled takes than the history keeps: the oldest turns read Unknown, the newest answer.
    app::ChangeVisibility h;
    const std::size_t m = app::ChangeVisibility::kMaxHistory + 10;
    for (std::size_t i = 1; i <= m; ++i) {
        h.taken(i, batch_at(i), {0}, {i}); // island 0, version i
        h.committed(commit_of(0, i, {kHere}));
        frame(h, kViewHere, true, kYes, kYes);
    }
    CHECK(h.classify(m) == V::Visible);
    CHECK(h.classify(1) == V::Unknown);
}

TEST_CASE("visibility: turn 0, a batch newer than the snapshot, an older stuck change that must "
          "not relabel a later one, and a newer commit whose chunks are offscreen" *
          doctest::test_suite("visibility")) {
    {
        // Turn indices start at 0.
        app::ChangeVisibility v;
        v.taken(0, batch_at(0), {0}, {1});
        CHECK(v.classify(0) == V::Committing);
        v.committed(commit_of(0, 1, {kHere}));
        frame(v, kViewHere, true, kYes, kYes);
        CHECK(v.classify(0) == V::Visible);
    }
    {
        // The follower read the snapshot of turn 4 and then took a batch of turn 6 (the mailbox is
        // filled before the snapshot): turn 6 is in the desired state, turn 5 had no batch.
        app::ChangeVisibility v;
        v.taken(4, batch_at(6), {0}, {1});
        CHECK(v.classify(6) == V::Committing);
        CHECK(v.classify(5) == V::NoVisualChange);
        v.taken(6, {}, {1}, {1});
        CHECK(v.classify(6) == V::Committing);
        CHECK(v.classify(7) == V::NotInPicture);
    }
    {
        // An older take on island 1 is stuck (never committed). Turn 2's change on island 0 ends
        // offscreen; turn 3's, later on the same island, is presented. Turn 2 stays Offscreen.
        app::ChangeVisibility v;
        v.taken(1, batch_at(1), {0, 0}, {0, 1}); // island 1, version 1: stuck
        v.taken(2, batch_at(2), {0, 1}, {1, 1}); // island 0, version 1
        v.committed(commit_of(0, 1, {kThere}));
        frame(v, kViewHere, true, kYes, kYes);
        CHECK(v.classify(2) == V::Offscreen);
        v.taken(3, batch_at(3), {1, 1}, {2, 1}); // island 0, version 2
        v.committed(commit_of(0, 2, {kHere}));
        frame(v, kViewHere, true, kYes, kYes);
        CHECK(v.classify(3) == V::Visible);
        CHECK(v.classify(2) == V::Offscreen); // not relabelled by the later presentation
        CHECK(v.classify(1) == V::Committing);
    }
    {
        // A newer commit of an island adds chunks that are not in view while the older commit's
        // chunk is: the older one is shown, the newer one is offscreen, never Visible.
        app::ChangeVisibility v;
        v.taken(1, batch_at(1), {0}, {1});
        v.committed(commit_of(0, 1, {kHere}));
        frame(v, kViewHere, true, kNo, kNo); // watched, chunk stale
        v.taken(2, batch_at(2), {1}, {2});
        v.committed(commit_of(0, 2, {kThere}));
        frame(v, kViewHere, true, kYes, kYes);
        CHECK(v.classify(1) == V::Visible);
        CHECK(v.classify(2) == V::Offscreen);
    }
    {
        // More commits of one island than parts are kept: the oldest merge (a stricter wait) and
        // the island still settles.
        app::ChangeVisibility v;
        const std::size_t n = app::ChangeVisibility::kMaxParts + 20;
        for (std::size_t i = 0; i < n; ++i) {
            v.taken(i, batch_at(i), {i}, {i + 1});
            v.committed(commit_of(0, i + 1, {kHere}));
        }
        frame(v, kViewHere, true, kYes, kYes);
        CHECK(v.classify(0) == V::Visible);
        CHECK(v.classify(n - 1) == V::Visible);
        CHECK(v.waiting() == 0);
    }
}
