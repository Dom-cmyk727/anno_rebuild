// A recorded developer-tool session and its headless replay (W4.4 round ee, E3; review of E3):
// the files ANNO_DEV_SESSION writes and --replay-hashes reads, and the replay that compares them.
// A session is: the applied commands (commands_file.hpp format, with the turn each ran in), the
// state hash after the turns PlayRunner records (set_record_hashes), every kept outcome, and the
// invocation (map file and speed). The replay runs the commands as the runner's script on a
// freshly loaded world at the recorded speed and compares every recorded hash and, in order,
// every outcome's turn, state, reason and per-record results (not its origin or number: the
// replay's commands are the script's).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <anno/app/cli.hpp>
#include <anno/app/command_outcome.hpp>
#include <anno/app/commands_file.hpp>
#include <anno/app/play.hpp>
#include <anno/formats/result.hpp>
#include <anno/sim/world.hpp>

namespace anno::app {

/// "<turn> <hash, 16 hex digits>" per line.
[[nodiscard]] std::string format_hashes(const std::vector<PlayRunner::TurnHash>& hashes);
[[nodiscard]] formats::Result<std::vector<PlayRunner::TurnHash>, std::string>
parse_hashes(std::string_view text);

/// One line per outcome: "<origin> #<n> turn <t> <State> <Reason> <record reasons...>", and a
/// first line "lost <n> from <seq>" when the store dropped some.
[[nodiscard]] std::string format_outcomes(const OutcomeRead& read);

/// The session's invocation: "map <path>", "speed <percent>" and "rules <original|living>"
/// lines (PLAY-LIVE-1a: the last names the --rules profile a plain --play recording was made
/// with, read back by a later --replay-hashes of it).
[[nodiscard]] std::string format_invocation(const std::string& map, unsigned speed,
                                            RulesProfile profile);

/// The "rules" line of a recorded invocation (format_invocation), or nullopt when the text has
/// none or names a profile this build does not know.
[[nodiscard]] std::optional<RulesProfile> parse_invocation_rules(std::string_view text);

struct ReplayReport {
    bool agree = false;
    std::size_t hashes = 0, outcomes = 0;
    /// The first difference, or the summary when everything agreed.
    std::string message;
};

/// Runs `commands` on `world` at `speed` (PlayRunner's speed percent, as the live run) up to the
/// last recorded turn and compares the recorded `hashes` and, when given, the recorded
/// `outcomes` text (format_outcomes; a session that lost outcomes cannot be compared: an error).
[[nodiscard]] ReplayReport replay_session(sim::World world, unsigned speed,
                                          std::vector<ScriptedCommand> commands,
                                          const std::vector<PlayRunner::TurnHash>& hashes,
                                          std::optional<std::string_view> outcomes);

} // namespace anno::app
