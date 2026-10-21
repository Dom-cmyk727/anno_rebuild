// Tests of the atlas residency model (docs/render/gpu-chunks.md 4.2-4.3): no
// GPU and no game files.
#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include <doctest/doctest.h>

#include <anno/render/frame_table.hpp>
#include <anno/render/residency.hpp>

namespace render = anno::render;
using render::AtlasClass;
using render::AtlasResidency;
using render::EnsureStatus;
using render::FrameRef;

namespace {

// A layer is 64 x 64 texels = 16,384 bytes: small enough to fill by hand.
constexpr std::uint32_t kSize = 64;
constexpr std::uint64_t kLayer = std::uint64_t{kSize} * kSize * 4;

/// A frame that fills a whole layer.
FrameRef full(render::FrameId id) {
    return {id, kSize, kSize};
}

std::vector<render::FrameId> evicted(AtlasResidency& r) {
    return r.take_evicted();
}

} // namespace

TEST_CASE("residency: the least recently used layer of the class is evicted first") {
    AtlasResidency r(kSize, 3 * kLayer);
    CHECK(r.max_layers() == 3);
    // Frame 1: A, B, C each fill a layer.
    for (render::FrameId id : {10U, 11U, 12U}) {
        const FrameRef f = full(id);
        CHECK(r.ensure({&f, 1}, AtlasClass::Static).status == EnsureStatus::Uploaded);
    }
    CHECK(r.stats().layers == 3);
    // Frame 2: B is read again.
    r.begin_frame();
    const FrameRef b = full(11);
    CHECK(r.ensure({&b, 1}, AtlasClass::Static).status == EnsureStatus::Resident);
    // Frame 3: D needs a layer. A and C were last used in frame 1, B in 2:
    // the tie is broken by the lower layer index, which holds A.
    r.begin_frame();
    const FrameRef d = full(13);
    CHECK(r.ensure({&d, 1}, AtlasClass::Static).status == EnsureStatus::Uploaded);
    CHECK(evicted(r) == std::vector<render::FrameId>{10});
    CHECK(!r.resident(10));
    CHECK(r.resident(11));
    CHECK(r.resident(12));
    // Frame 4: E takes C's layer (last used 1; B 2, D 3), frame 5: F takes B's.
    r.begin_frame();
    const FrameRef e = full(14);
    r.ensure({&e, 1}, AtlasClass::Static);
    CHECK(evicted(r) == std::vector<render::FrameId>{12});
    r.begin_frame();
    const FrameRef f = full(15);
    r.ensure({&f, 1}, AtlasClass::Static);
    CHECK(evicted(r) == std::vector<render::FrameId>{11});
    CHECK(r.stats().evicted_layers == 3);
    CHECK(r.stats().layers == 3);
    CHECK(r.stats().chunks_cpu_fallback == 0);
}

TEST_CASE("residency: layers read this frame are pinned") {
    AtlasResidency r(kSize, 3 * kLayer);
    const std::vector<FrameRef> three = {full(1), full(2), full(3)};
    CHECK(r.ensure(three, AtlasClass::Static).status == EnsureStatus::Uploaded);
    // The same frame: everything is pinned, a fourth frame does not fit.
    const FrameRef w = full(4);
    const auto over = r.ensure({&w, 1}, AtlasClass::Static);
    CHECK(over.status == EnsureStatus::Overflow);
    CHECK(over.uploads.empty());
    CHECK(r.stats().chunks_cpu_fallback == 1);
    CHECK(r.resident(1));
    CHECK(r.resident(2));
    CHECK(r.resident(3));
    // The next frame: frame 1 and 3 are read (pinned), so frame 2's layer goes.
    r.begin_frame();
    const std::vector<FrameRef> keep = {full(1), full(3)};
    CHECK(r.ensure(keep, AtlasClass::Static).status == EnsureStatus::Resident);
    CHECK(r.ensure({&w, 1}, AtlasClass::Static).status == EnsureStatus::Uploaded);
    CHECK(evicted(r) == std::vector<render::FrameId>{2});
    CHECK(r.resident(1));
    CHECK(r.resident(3));
    // A chunk that needs one frame of a pinned layer and a new one never evicts
    // the layer of the frame it has just been given.
    r.begin_frame();
    const std::vector<FrameRef> mix = {full(4), full(5)};
    CHECK(r.ensure(mix, AtlasClass::Static).status == EnsureStatus::Uploaded);
    CHECK(r.resident(4));
    CHECK(r.resident(5));
    CHECK(!r.resident(1) != !r.resident(3));
}

TEST_CASE("residency: a set that cannot fit is reported, frames already placed stay") {
    AtlasResidency r(kSize, 3 * kLayer);
    // Four layer-sized frames in one call against three layers.
    const std::vector<FrameRef> four = {full(1), full(2), full(3), full(4)};
    const auto res = r.ensure(four, AtlasClass::Static);
    CHECK(res.status == EnsureStatus::Overflow);
    CHECK(res.uploads.size() == 3);
    CHECK(r.stats().chunks_cpu_fallback == 1);
    CHECK(r.stats().resident_frames == 3);
    // A frame larger than a layer never fits, whatever is evicted.
    r.begin_frame();
    const FrameRef big{9, kSize + 1, 4};
    CHECK(r.ensure({&big, 1}, AtlasClass::Static).status == EnsureStatus::Overflow);
    CHECK(r.stats().chunks_cpu_fallback == 2);
    CHECK(!r.resident(9));
    // An empty frame cannot be placed either.
    const FrameRef empty{8, 0, 4};
    CHECK(r.ensure({&empty, 1}, AtlasClass::Static).status == EnsureStatus::Overflow);
    // A budget below one layer holds nothing.
    AtlasResidency none(kSize, kLayer - 1);
    const FrameRef small{1, 4, 4};
    CHECK(none.max_layers() == 0);
    CHECK(none.ensure({&small, 1}, AtlasClass::Static).status == EnsureStatus::Overflow);
}

TEST_CASE("residency: the upload budget defers a set without placing any of it") {
    AtlasResidency r(kSize, 4 * kLayer);
    const std::vector<FrameRef> set = {{1, 32, 32}, {2, 32, 32}};
    const std::uint64_t need = 2 * 32 * 32 * 4;
    const auto deferred = r.ensure(set, AtlasClass::Static, need - 1);
    CHECK(deferred.status == EnsureStatus::Deferred);
    CHECK(r.stats().resident_frames == 0);
    CHECK(r.stats().layers == 0);
    CHECK(r.stats().chunks_cpu_fallback == 0);
    const auto done = r.ensure(set, AtlasClass::Static, need);
    CHECK(done.status == EnsureStatus::Uploaded);
    CHECK(done.upload_bytes == need);
    // Resident frames cost nothing, so a zero budget is enough for them.
    CHECK(r.ensure(set, AtlasClass::Static, 0).status == EnsureStatus::Resident);
}

TEST_CASE("residency: frame ids stay the same across evictions and re-uploads") {
    // The ids come from a FrameTable, which never reuses them; the residency
    // only maps id -> place.
    render::FrameTable table;
    auto intern = [&](std::uint32_t n) {
        return *table.intern(render::FrameKey::building({n, 0, false, 0, 0}), [&] {
            auto img = std::make_shared<anno::assets::Rgba>();
            img->width = kSize;
            img->height = kSize;
            img->pixels.assign(std::size_t{kSize} * kSize * 4, 255);
            return std::pair{anno::formats::figuren::Frame{},
                             std::shared_ptr<const anno::assets::Rgba>(img)};
        });
    };
    std::vector<render::FrameId> ids;
    for (std::uint32_t n = 0; n < 5; ++n) {
        ids.push_back(intern(n + 100));
    }
    CHECK(ids == std::vector<render::FrameId>{0, 1, 2, 3, 4});
    AtlasResidency r(kSize, 2 * kLayer);
    std::map<render::FrameId, render::AtlasPlace> first;
    for (const render::FrameId id : ids) {
        r.begin_frame();
        const FrameRef f = full(id);
        const auto res = r.ensure({&f, 1}, AtlasClass::Static);
        CHECK(res.status == EnsureStatus::Uploaded);
        first[id] = *r.place(id);
    }
    // 0, 1, 2 and 3 were evicted along the way, 3 and 4 are resident.
    CHECK(!r.resident(0));
    CHECK(r.resident(3));
    CHECK(r.resident(4));
    // The table still answers for an evicted id, and interning the same key
    // again gives the same id.
    CHECK(table.get(0) != nullptr);
    CHECK(intern(100) == 0);
    CHECK(table.size() == 5);
    // Re-uploading frame 0 is the same id, in whatever place there is.
    r.begin_frame();
    const FrameRef f0 = full(0);
    CHECK(r.ensure({&f0, 1}, AtlasClass::Static).status == EnsureStatus::Uploaded);
    CHECK(r.place(0).has_value());
}

TEST_CASE("residency: hot and static frames live in separate layers") {
    AtlasResidency r(kSize, 3 * kLayer);
    const FrameRef hot = {1, 8, 8};
    const FrameRef stat = {2, 8, 8};
    r.ensure({&hot, 1}, AtlasClass::Hot);
    r.ensure({&stat, 1}, AtlasClass::Static);
    CHECK(r.place(1)->layer != r.place(2)->layer);
    // Static frames fill the other two layers; the static LRU never takes the
    // hot layer, however old it is.
    for (render::FrameId id : {3U, 4U, 5U, 6U}) {
        r.begin_frame();
        const FrameRef f = full(id);
        r.ensure({&f, 1}, AtlasClass::Static);
    }
    CHECK(r.resident(1));
    // A frame already resident in the other class is used where it is.
    r.begin_frame();
    CHECK(r.ensure({&hot, 1}, AtlasClass::Static).status == EnsureStatus::Resident);
}

TEST_CASE("residency: shelf packing keeps frames inside the layer and apart") {
    AtlasResidency r(kSize, 12 * kLayer);
    std::vector<FrameRef> set;
    std::uint32_t s = 12345;
    auto next = [&] {
        s = s * 1664525U + 1013904223U;
        return s >> 16;
    };
    for (render::FrameId id = 0; id < 120; ++id) {
        set.push_back({id, 4 + next() % 17, 4 + next() % 17});
    }
    const auto res = r.ensure(set, AtlasClass::Static);
    REQUIRE(res.status == EnsureStatus::Uploaded);
    CHECK(res.uploads.size() == set.size());
    for (std::size_t i = 0; i < res.uploads.size(); ++i) {
        const auto& p = res.uploads[i].second;
        CHECK(p.x + p.width <= kSize);
        CHECK(p.y + p.height <= kSize);
        CHECK(p.layer < 12);
        for (std::size_t j = i + 1; j < res.uploads.size(); ++j) {
            const auto& q = res.uploads[j].second;
            const bool apart = p.layer != q.layer || p.x + p.width <= q.x || q.x + q.width <= p.x ||
                               p.y + p.height <= q.y || q.y + q.height <= p.y;
            CHECK(apart);
        }
    }
    // The same call on a fresh model gives the same places, also when the frames
    // are listed in another order.
    AtlasResidency again(kSize, 12 * kLayer);
    std::reverse(set.begin(), set.end());
    const auto res2 = again.ensure(set, AtlasClass::Static);
    REQUIRE(res2.uploads.size() == res.uploads.size());
    for (std::size_t i = 0; i < res.uploads.size(); ++i) {
        CHECK(res.uploads[i].first == res2.uploads[i].first);
        CHECK(res.uploads[i].second == res2.uploads[i].second);
    }
}
