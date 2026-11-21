// Loads a scenario (.sze) or island template (.scp) into a render::Scene,
// with the game files it needs (Textures/Felder.tex, Anlagen.dat,
// FigurenHeader.dat + Figuren*.dat for units). Rules: docs/render/iso.md,
// "Scene composition", "Island links"; docs/re/annoscene.md §3–5.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <anno/assets/figure.hpp>
#include <anno/assets/unit.hpp>
#include <anno/formats/result.hpp>
#include <anno/render/scene.hpp>

namespace anno::app {

/// A file object the sim did not place. Coordinates are island-local; the
/// origin is the island's world tile, so the world tile is origin + local.
struct OmittedObject {
    std::uint32_t island = 0;
    std::uint16_t origin_x = 0, origin_y = 0;
    std::uint16_t type = 0;
    std::uint16_t x = 0, y = 0;
};

/// A scene and the file buffers its islands and Anlagen.dat refer to. While the scene is
/// loaded the buffers are staged here; they move into the scene (render::Scene::Backing) once
/// it exists, so `buffers` is empty afterwards and the scene keeps the bytes alive.
struct LoadedScene {
    std::vector<std::vector<std::uint8_t>> buffers;
    /// Made by render::Scene::create: its last owner's release hands the destruction to the
    /// reclaimer.
    std::shared_ptr<render::Scene> scene;
    /// Messages for the user (stand-ins, skipped links or units).
    std::vector<std::string> notes;
    /// How many of scene->islands come from the file itself (the stand-ins
    /// for INSELLNK links follow).
    std::size_t file_islands = 0;
    /// Parallel to scene->islands once the scene exists. Empty for an island
    /// embedded in the scenario; the template path for a link.
    std::vector<std::string> island_sources;
    /// Building items that were in the files and not in the sim, so they were
    /// left out of the scene (docs/app/game-shell.md). Empty for --view.
    std::vector<OmittedObject> omitted;
};

/// Every facing of FigurenHeader entry `figure` (sprite set 0 and its shadow, every zoom group) in
/// `colour` (nullopt: uncoloured; ignored for a type that is not player-coloured). Reads the
/// figure volumes of `dir`; W4-OPENING-1's live ships are drawn from these.
[[nodiscard]] formats::Result<assets::unit::FacingSprites, std::string>
load_figure_facings(const std::filesystem::path& dir, std::uint16_t figure, std::uint32_t facings,
                    std::optional<assets::figure::Rgb> colour);

/// One INSELLNK whose picture is a chosen template instead of the viewer's
/// stand-in. `number` is the link head's u32 (the same value the scene island
/// keeps).
struct LinkTemplate {
    std::uint32_t number = 0;
    std::filesystem::path file;
};

/// The Anlagen ids the picture can draw: 0 .. kPictureAnlageIds - 1. A PROD2 item holds the id in
/// 12 bits (formats::insel::PlacedObject::parse masks 0x0FFF), and the picture draws buildings as
/// PROD2 items; a wider id (a mod's type: TypeId is 32 bit in the sim and on the wire) is built by
/// the sim but not drawn (a note, round ee D3). Path to lift it: an object source for the scene
/// that is not a PROD2 item.
inline constexpr std::uint32_t kPictureAnlageIds = 0x1000;

/// A building the sim placed, in island-local tiles. `island` is the island
/// table slot (the link number when it fits in 16 bits, or the embedded
/// island's number). `type` is the Anlagen id; a value that does not fit a
/// PROD2 item (12 bits) is refused with a note, not masked. `height` is the
/// height byte (`Building::byte0f`).
struct SceneBuilding {
    std::uint32_t island = 0;
    std::uint32_t type = 0;
    std::uint16_t x = 0, y = 0;
    std::uint8_t rotation = 0;
    std::uint8_t height = 0;
    friend bool operator==(const SceneBuilding&, const SceneBuilding&) = default;
};

/// A runtime or cleared non-building tile object in island-local tiles.
/// `type != 0` places or retains a tracked object (roads first; designed so crops and trees fit
/// via `phase`). `type == 0` marks a tile whose tick-0 file non-building item (trees, fields,
/// stones; §10.2) was overwritten or demolished so the picture leaves bare ground across
/// incremental updates, demolishes and collapsed resyncs.
struct SceneTileObject {
    std::uint32_t island = 0;
    std::uint32_t type = 0;
    std::uint16_t x = 0, y = 0;
    std::uint8_t rotation = 0;
    std::uint8_t height = 0;
    std::uint8_t phase = 0;
    friend bool operator==(const SceneTileObject&, const SceneTileObject&) = default;
};

/// How --play replaces building and tracked tile-object sprites. Null for --view, which is
/// unchanged. `is_building` is the scenario loader's classification (a type the layout names).
/// `buildings` is the building set the scene draws. `is_tile_object` and `tile_objects` are
/// optional tracked tile-object overrides/cleared-tile markers (roads first; later crops/trees).
struct SceneObjects {
    std::function<bool(std::uint16_t anlage)> is_building;
    std::span<const SceneBuilding> buildings;
    std::function<bool(std::uint16_t anlage)> is_tile_object;
    std::span<const SceneTileObject> tile_objects;
};

/// One island's buildings and tile objects written into its records (play mode): the per-island
/// form of apply_world_buildings, used by ScenePicture (docs/app/game-shell.md). `buildings` and
/// `tile_objects` are the sim's buildings and tracked/cleared tile objects of this island; the
/// payloads it writes go to `owned_out`, whose buffers keep their addresses when moved; notes and
/// omitted objects go to `notes` and `omitted`. With `anlagen` (the scene's Anlagen.dat), a
/// scenery item whose anchor tile lies under a building or live tile object is dropped, as the
/// sim's tile layer overwrites it (scenery_under_building); without, scenery is copied unchanged
/// unless suppressed by an explicit cleared-tile marker (`SceneTileObject::type == 0`).
void apply_island_buildings(formats::insel::Island& island,
                            const std::vector<SceneBuilding>& buildings,
                            const SceneObjects& objects,
                            std::vector<std::vector<std::uint8_t>>& owned_out,
                            std::vector<std::string>& notes, std::vector<OmittedObject>& omitted,
                            const formats::anlagen::AnlagenFile* anlagen = nullptr,
                            std::span<const SceneTileObject> tile_objects = {});

/// HEAD's flag word (bytes 0x1c-0x1d, as sim/types.cpp reads it), or nullopt without an entry or
/// a HEAD of that length.
[[nodiscard]] std::optional<std::uint16_t>
anlage_head_flags(const formats::anlagen::AnlagenFile& anlagen, std::uint32_t id);

/// A footprint in island tiles (half-open) and whether its type carries flag 0x10.
struct Cover {
    std::int64_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool overlay = false;
    [[nodiscard]] bool holds(std::int64_t x, std::int64_t y) const {
        return x >= x0 && y >= y0 && x < x1 && y < y1;
    }
};

/// The cover of a type at (`bx`, `by`, `rotation`) in island-local tiles: Anlagen HEAD bytes 0 and
/// 1, swapped for an odd rotation (sim::footprint); nullopt without a footprint or flag word.
[[nodiscard]] std::optional<Cover> cover_of(const formats::anlagen::AnlagenFile& anlagen,
                                            std::uint32_t type, std::int64_t bx, std::int64_t by,
                                            std::uint8_t rotation);

/// Whether `c` overwrites an object whose flag word is `existing`: a type with flag 0x10 keeps an
/// anchored flag-0x10 object (AnnoWelt.dll 0x1008218f).
[[nodiscard]] bool overwrites(const Cover& c, std::uint16_t existing);

/// Whether a scenery item of type `item` at island tile (`x`, `y`) (its anchor, the footprint's
/// top-left tile) is overwritten by building `b`: the tile lies in b's footprint (Anlagen HEAD
/// bytes 0 and 1, swapped for an odd rotation), unless both types carry flag 0x10, whose anchored
/// object the original keeps (AnnoWelt.dll 0x1008218f; sim::place_object_record). False when a
/// type is not in `anlagen`.
[[nodiscard]] bool scenery_under_building(const formats::anlagen::AnlagenFile& anlagen,
                                          const SceneBuilding& b, std::uint16_t item,
                                          std::uint16_t x, std::uint16_t y);

/// The game directory: `explicit_dir`, else `env_dir` (the value of
/// ANNO1503_GAME_DIR, read by the caller), else the nearest ancestor of
/// `file` that contains Anlagen.dat.
formats::Result<std::filesystem::path, std::string>
find_game_dir(const std::optional<std::filesystem::path>& explicit_dir,
              const std::filesystem::path& file,
              const std::optional<std::filesystem::path>& env_dir);

/// Resolves a `\`-separated path below `base`, ignoring case per segment
/// (the original ran on case-insensitive Windows file systems).
std::optional<std::filesystem::path> find_ci(const std::filesystem::path& base,
                                             const std::string& rel);

/// One island the sim placed, drawn from `file` at world tile (`x`, `y`).
/// `load_fixed_map` fills the same fields (slot, template path, position).
/// A coordinate outside 0..65535 is refused: an island head stores u16.
struct PlacedIsland {
    std::uint32_t number = 0;
    std::int32_t x = 0, y = 0;
    std::filesystem::path file;
};

/// Loads `file` with every island at its world position and the scenario's
/// ships and soldiers. With an empty `chosen`, INSELLNK links use the viewer's
/// stand-in (OQ-028). A chosen template replaces that stand-in for the link
/// with the same `number`. With `objects`, building items are the sim's and
/// file buildings the sim did not place are listed on `omitted`.
formats::Result<LoadedScene, std::string> load_scene(const std::filesystem::path& dir,
                                                     const std::filesystem::path& file,
                                                     std::span<const LinkTemplate> chosen = {},
                                                     const SceneObjects* objects = nullptr);

/// The play seam for a placement list (a fixed map, or the same shape): each
/// entry is one template at one position. No stand-in rule and no scenario
/// file. `objects` is the same building replacement as `load_scene`.
formats::Result<LoadedScene, std::string> load_placed_scene(const std::filesystem::path& dir,
                                                            std::string name,
                                                            std::span<const PlacedIsland> placed,
                                                            const SceneObjects* objects = nullptr);

} // namespace anno::app
