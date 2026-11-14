// Tests of the viewer/bench_frame shared configuration (ZOOM-SMOOTH-1 step 1, review
// finding B1): no GPU and no game files.
#include <doctest/doctest.h>

#include <anno/render/frame.hpp>

namespace render = anno::render;

TEST_CASE("chunk_worker_threads: three quarters of the cores, floored at 1") {
    CHECK(render::chunk_worker_threads(1) == 1);
    CHECK(render::chunk_worker_threads(2) == 1);
    CHECK(render::chunk_worker_threads(4) == 3);
    CHECK(render::chunk_worker_threads(8) == 6);
    CHECK(render::chunk_worker_threads(16) == 12);
    CHECK(render::chunk_worker_threads(0) == 1);
}

TEST_CASE("apply_frame_config: sets prewarm_far_levels and resident_level_chunks, nothing "
         "else") {
    render::FrameRenderer fr(1, false);
    const std::size_t spare_before = fr.spare_chunks;
    render::FrameConfig cfg;
    cfg.prewarm_far_levels = true;
    cfg.resident_level_chunks = 128;
    render::apply_frame_config(fr, cfg);
    CHECK(fr.prewarm_far_levels == true);
    CHECK(fr.resident_level_chunks == 128);
    CHECK(fr.spare_chunks == spare_before);

    // A caller that only wants the library defaults gets FrameRenderer's own, not 0/false by
    // omission (apply_frame_config is one assignment per field, not a partial merge).
    render::apply_frame_config(fr, {});
    CHECK(fr.prewarm_far_levels == false);
    CHECK(fr.resident_level_chunks == 16);
}

TEST_CASE("zoom_anchor_at: cam + screen-pixel offset over scale, both axes") {
    CHECK(render::zoom_anchor_at(100.0, 200.0, 0.0, 0.0, 2.0) == std::pair{100.0, 200.0});
    CHECK(render::zoom_anchor_at(100.0, 200.0, 20.0, -10.0, 2.0) == std::pair{110.0, 195.0});
    CHECK(render::zoom_anchor_at(0.0, 0.0, 50.0, 30.0, 0.5) == std::pair{100.0, 60.0});
}
