// UNWIRED original-profile data binding; docs/sim/wildlife-ground-facts.md.
#pragma once
#include <span>

#include <anno/formats/figuren.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim_load/wildlife_ground_prepare.hpp>

namespace anno::sim_load {
// Standard fresh Anlagen descriptor factory, Game 0x100063a0 -> 0x1000c240,
// table 0x10021a88. Not a modern RuntimeClass or a live-object provenance seal.
enum class GroundDescriptorProfile : std::uint8_t { GogAnlagen };

// Full fresh catalogue entries paired with its immutable registry. Whole engine
// words are widened; no 14-bit masking/original unsafe pointer-array content cap.
// Returns sorted/distinct owned facts. Unknown nonzero words refuse; absent zero
// needs BOTH source and registry absence under the explicitly selected profile.
// Source HEAD/kind are authoritative and cross-checked, never guessed from a
// registry digest. Supplied buffers/registry need live only through this call.
[[nodiscard]] formats::Result<std::vector<GroundObjectFact>, std::string>
bind_wildlife_ground_facts(GroundDescriptorProfile, std::span<const std::uint32_t> words,
                           std::span<const formats::figuren::Entry>, const sim::TypeRegistry&);
} // namespace anno::sim_load
