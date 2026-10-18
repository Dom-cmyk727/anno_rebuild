// Loads a scenario (.sze) or island template (.scp) into a render::Scene,
// with the game files it needs (Textures/Felder.tex, Anlagen.dat,
// FigurenHeader.dat + Figuren*.dat for units). Rules: docs/render/iso.md,
// "Scene composition", "Island links"; docs/re/annoscene.md §3–5.
#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/render/scene.hpp>

namespace anno::app {

/// A scene and the file buffers its islands and Anlagen.dat refer to.
struct LoadedScene {
    std::vector<std::vector<std::uint8_t>> buffers;
    std::unique_ptr<render::Scene> scene;
    /// Messages for the user (stand-ins, skipped links or units).
    std::vector<std::string> notes;
};

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

/// Loads `file` with every island at its world position (INSELLNK links as
/// stand-ins, see load_scene's notes) and the scenario's ships and soldiers.
formats::Result<LoadedScene, std::string> load_scene(const std::filesystem::path& dir,
                                                     const std::filesystem::path& file);

} // namespace anno::app
