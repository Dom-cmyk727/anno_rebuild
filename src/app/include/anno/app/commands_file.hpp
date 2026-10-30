// The --commands file (docs/app/game-shell.md, "The command file"): scripted
// Build and Demolish commands for the play runner's turns. The same commands
// reach a turn whether they come from this file or from the window (W4.4).
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/sim/command.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/world.hpp>

namespace anno::app {

/// One command and the turn it is applied in (the turn index the runner counts
/// from 0).
struct ScriptedCommand {
    std::uint64_t turn = 0;
    sim::Command command;
};

/// The developer player: the player whose cities the file's commands name
/// (player slot 1, the owner of the developer start, MAP.1, OQ-490).
[[nodiscard]] sim::PlayerId developer_player();

/// Parses the text format against `world`. A build names the city of `player` on
/// the island that holds its first placement; a line with no such city is an
/// error (no city is invented). Errors name the line.
[[nodiscard]] formats::Result<std::vector<ScriptedCommand>, std::string>
parse_commands(std::string_view text, const sim::World& world, sim::PlayerId player);

/// Reads `file` and parses it with parse_commands.
[[nodiscard]] formats::Result<std::vector<ScriptedCommand>, std::string>
load_commands(const std::filesystem::path& file, const sim::World& world, sim::PlayerId player);

} // namespace anno::app
