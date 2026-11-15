// Tests of the viewer/bench_frame shared configuration (ZOOM-SMOOTH-1 step 1, review
// finding B1) and the chunk memory budget (ZOOM-SMOOTH-1b steps 2 and 6): no GPU and no game
// files.
#include <memory>
#include <vector>

#include <doctest/doctest.h>

#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>

namespace render = anno::render;

namespace {
std::shared_ptr<render::Scene> empty_scene() {
    return std::make_shared<render::Scene>(
        "frame_config_test", std::vector<anno::formats::insel::Island>{},
        anno::assets::island::GroundTextures{}, anno::formats::anlagen::AnlagenFile{},
        render::kHeightLift);
}
} // namespace

TEST_CASE("chunk_worker_threads: three quarters of the cores, floored at 1") {
    CHECK(render::chunk_worker_threads(1) == 1);
    CHECK(render::chunk_worker_threads(2) == 1);
    CHECK(render::chunk_worker_threads(4) == 3);
    CHECK(render::chunk_worker_threads(8) == 6);
    CHECK(render::chunk_worker_threads(16) == 12);
    CHECK(render::chunk_worker_threads(0) == 1);
}

TEST_CASE("apply_frame_config: sets prewarm_far_levels, resident_level_chunks and budget, "
          "nothing else") {
    render::FrameRenderer fr(1, false);
    const std::size_t spare_before = fr.spare_chunks;
    render::FrameConfig cfg;
    cfg.prewarm_far_levels = true;
    cfg.resident_level_chunks = 128;
    cfg.budget.total_bytes = 123;
    render::apply_frame_config(fr, cfg);
    CHECK(fr.prewarm_far_levels == true);
    CHECK(fr.resident_level_chunks == 128);
    CHECK(fr.budget.total_bytes == 123);
    CHECK(fr.spare_chunks == spare_before);

    // A caller that only wants the library defaults gets FrameRenderer's own, not 0/false by
    // omission (apply_frame_config is one assignment per field, not a partial merge).
    render::apply_frame_config(fr, {});
    CHECK(fr.prewarm_far_levels == false);
    CHECK(fr.resident_level_chunks == 16);
    CHECK(fr.budget.total_bytes == 0);
}

TEST_CASE("default_chunk_budget: discrete/integrated/software, floored and capped "
          "(ZOOM-SMOOTH-1b step 2)") {
    constexpr std::uint64_t kGiB = std::uint64_t{1} << 30;
    constexpr std::uint64_t kMiB = std::uint64_t{1} << 20;
    // Discrete ignores system RAM.
    CHECK(render::default_chunk_budget(render::ChunkBudgetAdapter::Discrete, 0).total_bytes ==
          4 * kGiB);
    CHECK(render::default_chunk_budget(render::ChunkBudgetAdapter::Discrete, 1024 * kGiB)
              .total_bytes == 4 * kGiB);
    // Integrated/unknown is an eighth of system RAM, floored and capped.
    CHECK(render::default_chunk_budget(render::ChunkBudgetAdapter::IntegratedOrUnknown, 0)
              .total_bytes == 256 * kMiB); // unknown RAM: the floor
    CHECK(render::default_chunk_budget(render::ChunkBudgetAdapter::IntegratedOrUnknown,
                                       16 * kGiB)
              .total_bytes == 2 * kGiB); // an eighth of 16 GiB is 2 GiB: the cap, not over it
    CHECK(render::default_chunk_budget(render::ChunkBudgetAdapter::IntegratedOrUnknown, 8 * kGiB)
              .total_bytes == 1 * kGiB); // an eighth of 8 GiB, between the floor and the cap
    // Software gets the floor alone, regardless of RAM.
    CHECK(render::default_chunk_budget(render::ChunkBudgetAdapter::Software, 1024 * kGiB)
              .total_bytes == 256 * kMiB);
}

TEST_CASE("cache_bytes stays equal to cached_bytes (the O(n) sum) through inserts and "
          "replacements (ZOOM-SMOOTH-1b step 2)") {
    render::FrameRenderer fr(1, false);
    CHECK(fr.cache_bytes() == 0);
    CHECK(fr.cache_bytes() == fr.cached_bytes());
    auto make = [](std::size_t pixels) {
        render::Chunk c;
        c.ground.assign(pixels, 0);
        return c;
    };
    fr.insert_chunk({0, 0, 0}, make(1000));
    CHECK(fr.cache_bytes() == fr.cached_bytes());
    CHECK(fr.cache_bytes() > 0);
    fr.insert_chunk({0, 1, 0}, make(2000));
    CHECK(fr.cache_bytes() == fr.cached_bytes());
    // A replacement at the same key changes the total by the new chunk's size, not by adding to
    // the old one's.
    const std::size_t before_replace = fr.cache_bytes();
    fr.insert_chunk({0, 0, 0}, make(500));
    CHECK(fr.cache_bytes() == fr.cached_bytes());
    CHECK(fr.cache_bytes() < before_replace);
}

TEST_CASE("trim rate-limits eviction to max_evict_bytes_per_frame, spread over several frames "
          "(ZOOM-SMOOTH-1b step 6)") {
    const auto scene = empty_scene();
    render::FrameRenderer frames(1, false);
    frames.set_workers(std::make_unique<render::ChunkWorkers>(1));
    constexpr std::size_t kChunkSize = 2 * 1024 * 1024; // ~2 MiB of ground pixels each
    constexpr std::size_t kPixels = kChunkSize / sizeof(std::uint32_t);
    for (int i = 0; i < 10; ++i) {
        render::Chunk c;
        c.ground.assign(kPixels, 0);
        frames.insert_chunk({0, i, 0}, std::move(c));
    }
    const std::size_t total_inserted = frames.cache_bytes();
    REQUIRE(total_inserted >= 10 * kChunkSize);
    frames.budget.total_bytes = 5 * kChunkSize;
    frames.budget.high_watermark = 1.0; // evict as soon as strictly over total_bytes
    frames.budget.low_watermark = 0.0;  // without the per-frame cap, one call would evict
                                        // everything down to (near) 0
    frames.budget.max_evict_bytes_per_frame = kChunkSize; // about one chunk per trim() call
    // Far from any of the ten inserted keys: none of them are visible or kept.
    const render::Rect away{-200000, -200000, 1, 1};
    frames.update_chunks(scene, away, 0);
    const std::size_t after_one = frames.cache_bytes();
    CHECK(after_one < total_inserted);                  // something was evicted
    CHECK(after_one > total_inserted - 2 * kChunkSize); // but not everything in one call
    frames.update_chunks(scene, away, 0);
    CHECK(frames.cache_bytes() < after_one); // a later frame evicts more of the overshoot
}

TEST_CASE("zoom_anchor_at: cam + screen-pixel offset over scale, both axes") {
    CHECK(render::zoom_anchor_at(100.0, 200.0, 0.0, 0.0, 2.0) == std::pair{100.0, 200.0});
    CHECK(render::zoom_anchor_at(100.0, 200.0, 20.0, -10.0, 2.0) == std::pair{110.0, 195.0});
    CHECK(render::zoom_anchor_at(0.0, 0.0, 50.0, 30.0, 0.5) == std::pair{100.0, 60.0});
}
