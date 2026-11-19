#include <algorithm>
#include <utility>
#include <vector>

#include <anno/sim/terrain_view.hpp>
#include <anno/sim_load/wildlife_graphics.hpp>

namespace anno::sim_load {

formats::Result<FinalizedWildlifeGraphics, std::string>
finalize_wildlife_graphics(const sim::Island& island) {
    const auto fail =
        [](const char* reason) -> formats::Result<FinalizedWildlifeGraphics, std::string> {
        return std::string("wildlife graphics: ") + reason;
    };
    const auto width = island.tiles.width();
    const auto height = island.tiles.height();
    if (width == 0 || height == 0)
        return fail("positive tile geometry required");
    if (!island.terrain)
        return fail("terrain absent");
    const auto count = std::uint64_t{width} * height; // u32 * u32 cannot overflow u64.
    const auto& source = *island.terrain;
    if (source.height.size() != count)
        return fail("HEIGHT length must equal width * height");
    if (source.gfx.size() != count)
        return fail("GFXID length must equal width * height");
    if (!source.fertility.empty() && source.fertility.size() != count)
        return fail("fertility must be absent or width * height");
    if (!sim::terrain_matches(source))
        return fail("terrain digest does not match columns");
    const auto vertices = sim::VertexGrid::of(island);
    if (!vertices.ok())
        return fail("HEIGHT vertex view unavailable");

    auto graphics = source.gfx; // One output plane, never a full-map copy per tile.
    std::uint64_t cleared = 0;
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            auto& word = graphics[std::uint64_t{y} * width + x];
            // Exact high-byte equality, not a bit test (0x100852d1–0x100852e0).
            if ((word & 0xff00U) != 0x2000U)
                continue;
            std::uint8_t maximum = 0;
            // Original four-by-four stencil, not configurable content geometry.
            // Inclusive W/H via VertexGrid; outside supplies zero (85303–8532d).
            for (std::int64_t dy = -1; dy <= 2; ++dy)
                for (std::int64_t dx = -1; dx <= 2; ++dx)
                    maximum = std::max(maximum, vertices
                                                    .at(std::int64_t{vertices.x0()} + x + dx,
                                                        std::int64_t{vertices.y0()} + y + dy)
                                                    .value_or(0));
            if (maximum == 0) { // Whole u16 clear, including low bits (8534f–85353).
                word = 0;
                ++cleared;
            }
        }
    }
    // Each column copied/materialized once; digest computed once after all writes.
    return FinalizedWildlifeGraphics{
        sim::make_terrain(source.height, std::move(graphics), source.fertility), cleared};
}

} // namespace anno::sim_load
