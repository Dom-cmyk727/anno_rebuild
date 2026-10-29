// One loader for the original-data type registry (MAP.1).
//
// The same build used to live twice: scenario_registry() in
// src/tools/sim_replay/main.cpp and original_registry() in
// src/sim/tests/scenario_test.cpp. Both read Anlagen.dat, FigurenHeader.dat,
// the AnnoWelt walk tables and Data/BGruppen.dat, in that order, and call
// TypeRegistry::build with the same arguments. This function is that build.
// The two call sites keep their own success and failure text.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include <anno/sim/types.hpp>

namespace anno::sim_load {

/// How a missing or unreadable FigurenHeader.dat is reported.
/// Required: the test's failure, whose text is the parser's own error.
/// OptionalNote: sim_replay's behaviour, a registry without figure types and
/// `figure_note` set to the line it used to print.
enum class FigureEntries : std::uint8_t { Required, OptionalNote };

/// The registry, plus the texts the two old call sites printed.
/// `error` is empty on success and is the parenthetical sim_replay printed
/// after "registry empty (". `figure_note` is set when FigurenHeader.dat was
/// skipped under OptionalNote, including when a later file then fails, so the
/// caller can print the note before the error (the old order).
struct OriginalRegistryLoad {
    std::shared_ptr<const sim::TypeRegistry> types;
    std::string figure_note;
    std::string error;
};

/// Reads the four original-data inputs of `game_dir` and builds the registry.
/// Does not read the environment. A hard failure leaves `types` empty and
/// `error` set; it does not throw.
[[nodiscard]] OriginalRegistryLoad load_original_registry(const std::filesystem::path& game_dir,
                                                          FigureEntries figures);

} // namespace anno::sim_load
