// Tests that need the original game files read ANNO1503_GAME_DIR and skip
// with a message when it is unset (CLAUDE.md, "Tests that need original
// files"). CI has no game files. Set to a folder that is not the game
// folder, every such test fails with the same clear message instead of
// follow-on errors (owner request 2026-10-18, after a run on a PC without
// the files).
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace anno::test {

/// The game directory from ANNO1503_GAME_DIR, or nullopt when it is unset
/// or empty. Throws std::runtime_error with the reason (game_dir_problem)
/// when it is set to a folder that is not the game folder.
std::optional<std::filesystem::path> game_dir();

/// Why `dir` is not the game folder (missing, not a folder, or without
/// Anlagen.dat, naming a subfolder that has it), or an empty string.
std::string game_dir_problem(const std::filesystem::path& dir);

/// All files below `dir` whose extension equals `ext` (no dot, ASCII
/// case-insensitive), sorted.
std::vector<std::filesystem::path> files_with_ext(const std::filesystem::path& dir,
                                                  std::string_view ext);

/// The whole file.
std::vector<std::uint8_t> read_file(const std::filesystem::path& path);

} // namespace anno::test
