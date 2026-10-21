#pragma once
// Per-stage timing of a chunk build (T1f, docs/perf/README.md "chunk build
// profile"). Off unless a thread attaches a sink: a stage scope then costs
// one test of a thread-local pointer. Times are exclusive: a stage nested in
// another (a sprite decoded while the objects are drawn) is counted in its
// own stage only, so the stages of one build add up to its time.
#include <array>
#include <cstddef>
#include <string_view>

namespace anno::render::profile {

/// The stages of render_chunk (frame.cpp) and what they call.
enum class Stage : std::size_t {
    ground,        ///< Scene::ground_with_probe: ground tiles and the probe
    ground_px,     ///< the ground canvas over the water colour (Chunk::ground)
    objects,       ///< Scene::render of the objects (cull and blit)
    decode,        ///< SpriteCache misses: frames decoded on first use
    sea_field,     ///< sea::water_field: cover, shore, depth per pixel
    island_water,  ///< sea::IslandWater::build, once per island
    river_sprites, ///< water_field_from: river, fall and mouth sprites drawn
    fall_masks,    ///< apply_fall_masks (blurred sprite alpha)
    river_blend,   ///< blend_rivers_into_sea (chamfer distances)
    field_pack,    ///< the four field planes interleaved
    terrain,       ///< Chunk::terrain: terrain_at per pixel
    flow,          ///< Scene::flow_plane: the lattice slice
    apron,         ///< flow_plane: ground probes of the strips around the chunk
    flow_solve,    ///< the river flow of an island, solved on first use
    count,
};

inline constexpr std::array<std::string_view, static_cast<std::size_t>(Stage::count)> kStageNames{
    "ground",       "ground px",     "objects",    "decode",      "sea field",
    "island water", "river sprites", "fall masks", "river blend", "field pack",
    "terrain",      "flow",          "apron",      "flow solve",
};

/// Milliseconds per stage.
struct Times {
    std::array<double, static_cast<std::size_t>(Stage::count)> ms{};
    [[nodiscard]] double& operator[](Stage s) { return ms[static_cast<std::size_t>(s)]; }
    [[nodiscard]] double operator[](Stage s) const { return ms[static_cast<std::size_t>(s)]; }
};

/// Profiles this thread into `sink` from now on (nullptr: off). Time spent
/// outside every stage is not counted.
void attach(Times* sink);

/// Counts the time until its end in `stage` (minus nested scopes).
class Scope {
  public:
    explicit Scope(Stage stage);
    ~Scope();
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

  private:
    bool active_ = false;
};

} // namespace anno::render::profile
