// RENDER-DIRECT-2a: the static-instance resolver and its chunk bucketing, synthetic (no game
// files). The corpus proof against place_prepared/object_record (the two .sze scenes, lan5.txt,
// a mutation control) lives in chunk_instances_corpus_test.cpp (anno_render_tests) and
// chunk_instances_lan5_test.cpp (anno_app_tests).
// RENDER-DIRECT-2c: the patch (place/demolish/upgrade) tests below, synthetic. The real-island
// proof (Single_01 island 0) and the opt-in patch-timing measurement live in
// chunk_instances_corpus_test.cpp too.
#include <array>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/anlagen.hpp>
#include <anno/formats/chunk.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/chunk_instances.hpp>
#include <anno/render/frame.hpp>

namespace render = anno::render;
namespace fig = anno::formats::figuren;
namespace anl = anno::formats::anlagen;
namespace fchunk = anno::formats::chunk;
namespace insel = anno::formats::insel;

namespace {

/// `anim_payload`'s own storage must outlive the returned AnlagenFile (its `Bytes` fields are
/// non-owning spans, anlagen.hpp): callers pass a `static constexpr` array.
anl::AnlagenFile one_entry_file(std::uint32_t anlage_id, anno::formats::Bytes anim_payload,
                                fig::Frame frame = {}) {
    fig::GfxSet set;
    set.frame_sets.push_back(fig::FrameSet{0, {frame}});
    fchunk::Chunk anim;
    anim.name_field = *fchunk::name_field("ANIM");
    anim.payload = anim_payload;
    set.records.push_back(std::move(anim));
    fig::Entry entry;
    entry.id = anlage_id;
    entry.gfx.push_back(std::move(set));
    anl::AnlagenFile file;
    file.entries.push_back(std::move(entry));
    return file;
}

/// Several static (n=1) entries in one file, for the place/demolish/upgrade patch tests (each
/// needs at least two distinct anlage ids coexisting). `kOneFrame` (n=1 ms=50) for every one.
anl::AnlagenFile multi_entry_file(std::span<const std::uint32_t> ids,
                                  anno::formats::Bytes anim_payload, fig::Frame frame = {}) {
    anl::AnlagenFile file;
    for (const std::uint32_t id : ids) {
        fig::GfxSet set;
        set.frame_sets.push_back(fig::FrameSet{0, {frame}});
        fchunk::Chunk anim;
        anim.name_field = *fchunk::name_field("ANIM");
        anim.payload = anim_payload;
        set.records.push_back(std::move(anim));
        fig::Entry entry;
        entry.id = id;
        entry.gfx.push_back(std::move(set));
        file.entries.push_back(std::move(entry));
    }
    return file;
}

/// A chunk-bucketed structure, element for element (not a set): every chunk key present in both,
/// with its instances' keys equal in the same order.
void check_buckets_equal(const render::ChunkInstanceBuckets& a,
                         const render::ChunkInstanceBuckets& b) {
    REQUIRE(a.size() == b.size());
    for (const auto& [chunk, insts] : a) {
        CAPTURE(chunk.first);
        CAPTURE(chunk.second);
        REQUIRE(b.count(chunk) == 1);
        const auto& other_insts = b.at(chunk);
        REQUIRE(insts.size() == other_insts.size());
        for (std::size_t i = 0; i < insts.size(); ++i) {
            CHECK(insts[i].key == other_insts[i].key);
            CHECK(insts[i].draw.anchor == other_insts[i].draw.anchor);
        }
    }
}

} // namespace

TEST_CASE("is_static_instance: one frame or a zero interval is static, several frames with a "
          "nonzero interval is not") {
    // ANIM payload (iso::animation, iso.cpp): bytes [2..3] = frames per rotation, [4..5] = ms,
    // little-endian.
    static constexpr std::array<std::uint8_t, 6> kOneFrame{0, 0, 1, 0, 50, 0}; // n=1, ms=50
    static constexpr std::array<std::uint8_t, 6> kZeroMs{0, 0, 4, 0, 0, 0};    // n=4, ms=0
    static constexpr std::array<std::uint8_t, 6> kAnimated{0, 0, 4, 0, 50, 0}; // n=4, ms=50

    CHECK(render::is_static_instance(one_entry_file(100, kOneFrame), 100));
    CHECK(render::is_static_instance(one_entry_file(101, kZeroMs), 101));
    CHECK_FALSE(render::is_static_instance(one_entry_file(102, kAnimated), 102));
    const anl::AnlagenFile empty;
    CHECK(render::is_static_instance(empty, 999)); // missing entry: place_prepared always skips it
}

TEST_CASE("resolve_static_instances: a static entry's draw is the same at a non-zero time_ms too "
          "(is_static_instance's own claim, not assumed)") {
    // n=1 frame per rotation: frame_index's step is (time/ms + phase) % 1 == 0 for every time_ms.
    static constexpr std::array<std::uint8_t, 6> kOneFrame{0, 0, 1, 0, 50, 0};
    const auto file = one_entry_file(200, kOneFrame);
    insel::PlacedObject o;
    o.anlage = 200;
    o.x = 10;
    o.y = 10;
    const std::vector<render::Prepared> list{{o, {1, 1}}};
    const auto statics = render::resolve_static_instances(list, file, 0, render::kHeightLift);
    REQUIRE(statics.size() == 1);
    std::vector<render::SpriteDraw> at_time;
    render::place_prepared(
        list, file, 0, render::kHeightLift, 123456, [](int, int, int, int) { return true; },
        at_time);
    REQUIRE(at_time.size() == 1);
    CHECK(statics[0].draw.anchor == at_time[0].anchor);
    CHECK(statics[0].draw.frame == at_time[0].frame);
}

TEST_CASE("bucket_by_chunk: the chunk keys match frame.hpp's own visible_chunks of the global "
          "reach rect") {
    static constexpr std::array<std::uint8_t, 6> kOneFrame{0, 0, 1, 0, 50, 0};
    fig::Frame frame;
    frame.width = 800; // wider than kChunk (512): the reach spans at least two chunks
    frame.height = 10;
    frame.anchor_x = 0;
    frame.anchor_y = 0;
    const auto file = one_entry_file(201, kOneFrame, frame);
    insel::PlacedObject o;
    o.anlage = 201;
    o.x = 10;
    o.y = 10;
    const std::vector<render::Prepared> list{{o, {1, 1}}};
    const auto statics = render::resolve_static_instances(list, file, 0, render::kHeightLift);
    REQUIRE(statics.size() == 1);
    const std::pair<int, int> offset{1000, 1000};
    const auto buckets = render::bucket_by_chunk(statics, offset);

    auto local_reach = statics[0].draw.reach();
    const render::Rect global{local_reach[0] + offset.first, local_reach[1] + offset.second,
                              local_reach[2], local_reach[3]};
    const auto expected = render::visible_chunks(global);
    REQUIRE(expected.size() > 1); // the point of this case: more than one chunk
    CHECK(buckets.size() == expected.size());
    const std::pair<int, int> expected_anchor{statics[0].draw.anchor.first + offset.first,
                                              statics[0].draw.anchor.second + offset.second};
    for (const auto& chunk : expected) {
        REQUIRE(buckets.count(chunk) == 1);
        REQUIRE(buckets.at(chunk).size() == 1);
        CHECK(buckets.at(chunk)[0].draw.anchor == expected_anchor);
        CHECK(buckets.at(chunk)[0].list_index == statics[0].list_index);
    }
}

namespace {

/// A single-tile object at (x, y) of anlage `id`, as one `other` entry.
render::Prepared one_object(std::uint32_t id, std::uint16_t x, std::uint16_t y) {
    insel::PlacedObject o;
    o.anlage = static_cast<std::uint16_t>(id);
    o.x = x;
    o.y = y;
    return {o, {1, 1}};
}

render::ChunkInstanceBuckets fresh_build(std::span<const render::Prepared> list,
                                         const anl::AnlagenFile& file, std::pair<int, int> offset) {
    return render::bucket_by_chunk(
        render::resolve_static_instances(list, file, 0, render::kHeightLift), offset);
}

} // namespace

TEST_CASE("patch_chunk_instances: place, demolish and upgrade each match a fresh build "
          "element for element (RENDER-DIRECT-2c)") {
    static constexpr std::array<std::uint32_t, 2> kIds{300, 301};
    static constexpr std::array<std::uint8_t, 6> kOneFrame{0, 0, 1, 0, 50, 0};
    const auto file = multi_entry_file(kIds, kOneFrame);
    const std::pair<int, int> offset{0, 0};

    SUBCASE("place: an object appears") {
        const std::vector<render::Prepared> before{one_object(300, 10, 10)};
        const std::vector<render::Prepared> after{one_object(300, 10, 10), one_object(300, 20, 20)};
        auto buckets = fresh_build(before, file, offset);
        render::patch_chunk_instances(buckets, before, after, file, 0, render::kHeightLift, offset);
        check_buckets_equal(buckets, fresh_build(after, file, offset));
    }

    SUBCASE("demolish: an object disappears") {
        const std::vector<render::Prepared> before{one_object(300, 10, 10),
                                                   one_object(300, 20, 20)};
        const std::vector<render::Prepared> after{one_object(300, 10, 10)};
        auto buckets = fresh_build(before, file, offset);
        render::patch_chunk_instances(buckets, before, after, file, 0, render::kHeightLift, offset);
        check_buckets_equal(buckets, fresh_build(after, file, offset));
    }

    SUBCASE("upgrade: the same tile, a different anlage - remove-old-key plus add-new-key, no "
            "special case") {
        const std::vector<render::Prepared> before{one_object(300, 10, 10)};
        const std::vector<render::Prepared> after{one_object(301, 10, 10)};
        auto buckets = fresh_build(before, file, offset);
        render::patch_chunk_instances(buckets, before, after, file, 0, render::kHeightLift, offset);
        check_buckets_equal(buckets, fresh_build(after, file, offset));
        // Confirm it actually changed (not a vacuous pass: before == after by coincidence).
        REQUIRE(buckets.size() == 1);
        CHECK(buckets.begin()->second.at(0).key.anlage == 301);
    }
}
