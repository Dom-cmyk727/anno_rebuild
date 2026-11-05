#pragma once
#include <anno/sim/scenario.hpp>
#include <anno/sim/types.hpp>
namespace anno::sim_load {
// Generated fixed-map/ship-start adapters use their base only for data/settings,
// replacing its players/islands. Their explicit output contract is script-free.
// Validate supplied scripts completely and reject every nonempty kernel before
// discarding metadata/mappings. This is NEVER an ordinary scenario fallback.
// A refusal leaves spec unchanged; missing source is allowed only by this
// generated-output contract. Original scenario imports keep Missing/Pending.
[[nodiscard]] std::string make_generated_script_free(sim::ScenarioSpec&, const sim::TypeRegistry&,
                                                     const sim::SimConfig&);
} // namespace anno::sim_load
