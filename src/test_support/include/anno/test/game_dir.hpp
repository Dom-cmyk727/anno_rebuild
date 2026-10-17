// Tests that need the original game files read ANNO1503_GAME_DIR and skip
// with a message when it is unset (CLAUDE.md, "Tests that need original
// files"). CI has no game files.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace anno::test {

/// The game directory from ANNO1503_GAME_DIR, or nullopt.
std::optional<std::filesystem::path> game_dir();

/// All files below `dir` whose extension equals `ext` (no dot, ASCII
/// case-insensitive), sorted.
std::vector<std::filesystem::path> files_with_ext(const std::filesystem::path& dir,
                                                  std::string_view ext);

/// The whole file.
std::vector<std::uint8_t> read_file(const std::filesystem::path& path);

} // namespace anno::test
