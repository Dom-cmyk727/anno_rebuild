// Ordered original import prefix, not BuildCmd or World wiring.
// Contract and primary addresses: docs/sim/wildlife-resource-initialization.md.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/terrain_view.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim::resource_import {

// Explicit transient metadata: absence never means a guessed zero.
struct TypeFacts {
    TypeId id{0};
    std::uint8_t goods0 = 0;
    std::uint16_t selector_tag = 0;
};
struct ClimateFacts {
    // One selected climate's rows. 16/8 are the addressed selector's row shapes,
    // not caps on type ids, worlds or the number of climates.
    std::vector<TypeId> slots;
    std::vector<TypeId> tag104, tag201, tag404;
};
struct Record {
    TypeId type{0};
    std::int32_t x = 0, y = 0;
    std::uint8_t rotation = 0;
    std::uint16_t raw_state = 0;
};
enum class Disposition : std::uint8_t { Accepted, Bounds, Occupied, Terrain };
enum class ErrorKind : std::uint8_t { Geometry, Facts, Unsupported, Exhausted };
struct Error {
    ErrorKind kind = ErrorKind::Facts;
    std::uint64_t record = 0, consumed = 0;
};
struct Output {
    TileLayer tiles;
    std::vector<std::uint8_t> variation;
    std::vector<Disposition> records;
    std::uint64_t consumed = 0;
};

// Selector itself consumes no draws; the accepted resource caller supplies one.
[[nodiscard]] formats::Result<std::uint8_t, ErrorKind> select(const TypeRegistry&,
                                                              std::span<const TypeFacts>,
                                                              const ClimateFacts&, TypeId,
                                                              std::uint32_t draw);

// Copies both planes; an error exposes no partially successful result. Initial
// objects must have resolved registry facts. Only ordinary flags 0x09/0x29 are
// supported. Coordinates/offsets must fit the existing lower writer. No RNG-global
// access, production registration, saved state, numeric tag or registry mutation.
[[nodiscard]] formats::Result<Output, Error>
evaluate(const TileLayer&, const VertexGrid&, std::span<const std::uint8_t> variation,
         const TypeRegistry&, std::span<const TypeFacts>, const ClimateFacts&,
         std::span<const Record>, std::span<const std::uint32_t> draws);

} // namespace anno::sim::resource_import
