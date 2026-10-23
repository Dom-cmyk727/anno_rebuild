// Loading a scenario with its island links from a game directory (docs/sim/state.md
// §10, S14 stage 2). The file I/O the sim does not do: the scan of
// Scenes/Islands for the link selection's candidates (as the original's island cache
// scan, D AnnoScene 0x10007c50) and the reading of the chosen templates. Everything
// else is anno_sim: the adapter (load_scenario), the resolver (resolve_scenario_links)
// and the placement (place_island_links).
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/result.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/island_links.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim_load {

/// The link selection's candidates of a game directory: one summary per template,
/// and the file it came from at the same index.
struct IslandTemplates {
    std::vector<sim::TemplateSummary> summaries;
    std::vector<std::filesystem::path> files;
};

/// Scans `game_dir`/Scenes/Islands and every folder below it for the files the
/// original's cache scan keeps (sim::link_candidate_name, sim::template_summary).
/// The result is sorted by summary name; an unreadable folder or file is left out.
/// No folder: an empty list.
IslandTemplates scan_island_templates(const std::filesystem::path& game_dir);

/// A scenario with its links resolved and placed.
struct ResolvedScenario {
    sim::LoadedScenario loaded;
    sim::LinkResolution resolution;
    /// spec.seed before the selection drew (the cursor it started from).
    std::uint32_t seed_before = 0;
};

/// load_scenario, then resolve_scenario_links over `templates`, the chosen templates
/// read from their files, then place_island_links. A template that cannot be read
/// is a refusal naming it.
formats::Result<ResolvedScenario, std::string>
load_scenario_with_links(formats::Bytes file, const IslandTemplates& templates,
                         const sim::TypeRegistry& types, const sim::SimConfig& config,
                         const sim::Rules& rules);

} // namespace anno::sim_load
