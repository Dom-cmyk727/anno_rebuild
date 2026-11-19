// UNWIRED numerical leaf; docs/sim/wildlife-ground-prepare.md.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/sim/ids.hpp>

namespace anno::sim_load {

enum class GroundProperty : std::uint8_t { NullDescriptor, Absent, Unsuccessful, Value };

struct GroundObjectFact {
    // Whole engine TileLayer object word, including 0; never masked to a type id.
    std::uint32_t object = 0;
    GroundProperty property = GroundProperty::NullDescriptor;
    std::uint32_t kind = 0;
    std::uint8_t value = 0;
    friend bool operator==(const GroundObjectFact&, const GroundObjectFact&) = default;
};

struct WildlifeGroundInput {
    sim::TileRect island;
    std::span<const std::uint16_t> gfx;
    std::span<const std::uint32_t> objects;
    std::span<const std::uint8_t> ground, lookup, kernel;
    std::span<const GroundObjectFact> facts;
};

// Synchronous borrowed views, owned result. Full island ONLY (no partial-update
// interface). Supplied facts must describe the no-city-prewalk mathematical
// profile; this does NOT establish an original city's absence, source provenance,
// FRUCHTBAR completion or loader success. No callback, fill, RNG or metadata write.
// Refusal leaves every input unchanged. Ordinary project vector allocation policy.
[[nodiscard]] formats::Result<std::vector<std::uint8_t>, std::string>
prepare_wildlife_ground(const WildlifeGroundInput&);

} // namespace anno::sim_load
