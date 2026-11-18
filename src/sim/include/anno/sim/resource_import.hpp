// Ordered original import prefix, not BuildCmd or World wiring.
// Contract and primary addresses: docs/sim/wildlife-resource-initialization.md.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/sim/fisher.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/terrain_view.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim::resource_import {

// Explicit transient metadata: absence never means a guessed zero.
struct TypeFacts {
    TypeId id;
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
    TypeId type;
    std::int32_t x = 0, y = 0;
    std::uint8_t rotation = 0;
    std::uint16_t raw_state = 0;
};
enum class Disposition : std::uint8_t { Accepted, Bounds, Occupied, Terrain, Coverage };
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

struct Effect {
    Disposition disposition;
    std::uint64_t draws = 0;
    std::optional<std::uint8_t> variant, height;
};
// A narrow serial import session. Registry and grid backing must outlive it;
// facts, climate rows, scratch tiles and variation are owned. Failed sessions
// cannot resume/finish successfully; diagnostic extraction is explicitly partial.
class Session {
  public:
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) = default;
    Session& operator=(Session&&) = default;
    [[nodiscard]] static formats::Result<Session, Error>
    start(const TileLayer&, const VertexGrid&, std::span<const std::uint8_t>, const TypeRegistry&,
          std::span<const TypeFacts>, const ClimateFacts&);
    [[nodiscard]] formats::Result<Effect, Error> apply(const Record&,
                                                       std::span<const std::uint32_t> draws);
    // GELAENDE epilogue island +0x198, 0x10092b40..0x10092c94. Draw-free,
    // clipped OR of the nine walk-mask bits on mean<threshold; all other fields kept.
    [[nodiscard]] formats::Result<std::uint64_t, Error>
    close_import_block(std::int32_t x, std::int32_t y, std::uint32_t w, std::uint32_t h,
                       std::uint8_t threshold = kFisherWaterThreshold);
    [[nodiscard]] formats::Result<Output, Error> finish();
    [[nodiscard]] Output diagnostic() &&;
    [[nodiscard]] std::uint64_t consumed() const { return out_.consumed; }

  private:
    Session(Output out, VertexGrid grid, const TypeRegistry& types, std::vector<TypeFacts> facts,
            ClimateFacts climate)
        : out_(std::move(out)), grid_(grid), types_(&types), facts_(std::move(facts)),
          climate_(std::move(climate)) {}
    Output out_;
    VertexGrid grid_;
    const TypeRegistry* types_;
    std::vector<TypeFacts> facts_;
    ClimateFacts climate_;
    std::optional<Error> failed_;
};

// Copies both planes; an error exposes no partially successful result. Initial
// objects must have resolved registry facts. Only ordinary flags 0x09/0x29 are
// supported, plus flags 0x3a or 0x12 / kind group 0x141 (wildlife-cliff.md). Coordinates/offsets
// must fit the existing lower writer. No RNG-global
// access, production registration, saved state, numeric tag or registry mutation.
[[nodiscard]] formats::Result<Output, Error>
evaluate(const TileLayer&, const VertexGrid&, std::span<const std::uint8_t> variation,
         const TypeRegistry&, std::span<const TypeFacts>, const ClimateFacts&,
         std::span<const Record>, std::span<const std::uint32_t> draws);

} // namespace anno::sim::resource_import
