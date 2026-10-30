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
    std::unique_ptr<render::Scene> scene;
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

/// One INSELLNK whose picture is a chosen template instead of the viewer's
/// stand-in. `number` is the link head's u32 (the same value the scene island
/// keeps).
struct LinkTemplate {
    std::uint32_t number = 0;
    std::filesystem::path file;
};

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

/// How --play replaces building sprites. Null for --view, which is unchanged.
/// `is_building` is the scenario loader's classification (a type the layout
/// names). `buildings` is the set the scene draws.
struct SceneObjects {
    std::function<bool(std::uint16_t anlage)> is_building;
    std::span<const SceneBuilding> buildings;
};

/// One island's buildings written into its records (play mode): the per-island form of
/// apply_world_buildings, used by ScenePicture (docs/app/game-shell.md). `buildings` are the
/// sim's buildings of this island; the payloads it writes go to `owned_out`, whose buffers keep
/// their addresses when moved; notes and omitted objects go to `notes` and `omitted`.
void apply_island_buildings(formats::insel::Island& island,
                            const std::vector<SceneBuilding>& buildings,
                            const SceneObjects& objects,
                            std::vector<std::vector<std::uint8_t>>& owned_out,
                            std::vector<std::string>& notes, std::vector<OmittedObject>& omitted);

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
