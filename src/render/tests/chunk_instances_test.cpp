// RENDER-DIRECT-2a: the static-instance resolver and its chunk bucketing, synthetic (no game
// files). The corpus proof against Scene::objects_draws (digest equality + mutation control, the
// three scenes + lan5.txt) is a separate, pending test - see the packet, docs/orchestration/
// tasks/render-direct-2a.md.
#include <doctest/doctest.h>

#include <anno/formats/anlagen.hpp>
#include <anno/formats/chunk.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/render/chunk_instances.hpp>

namespace render = anno::render;
namespace fig = anno::formats::figuren;
namespace anl = anno::formats::anlagen;
namespace fchunk = anno::formats::chunk;

namespace {

/// `anim_payload`'s own storage must outlive the returned AnlagenFile (its `Bytes` fields are
/// non-owning spans, anlagen.hpp): callers pass a `static constexpr` array.
anl::AnlagenFile one_entry_file(std::uint32_t anlage_id, anno::formats::Bytes anim_payload) {
    fig::GfxSet set;
    set.frame_sets.push_back(fig::FrameSet{0, {fig::Frame{}}});
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
