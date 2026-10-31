// A synthetic registry with the three figure types of the expedition (task MAP-2), shared by
// ship_state_test.cpp and ship_start_load_test.cpp.
#pragma once

#include <cstdint>
#include <memory>

#include <doctest/doctest.h>

#include <anno/sim/synthetic.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/walk_cells.hpp>

namespace ship_test {

namespace sim = anno::sim;

inline constexpr std::uint32_t kShip = 1502, kScout = 717, kDonkey = 718;

/// The synthetic registry plus figure types 1502, 717 and 718 (their walking values are 0:
/// nothing in these tests moves).
inline std::shared_ptr<const sim::TypeRegistry> ship_registry() {
    auto base = sim::synthetic::make_synthetic_types();
    REQUIRE(base);
    sim::TypeRegistry::Builder b;
    b.walk_tables(sim::synthetic_walk_tables());
    for (const sim::BuildingType& t : base.value().registry->types()) {
        b.add(t);
    }
    b.figure_type(sim::FigureType{kShip, 0, 0, 0, 0})
        .figure_type(sim::FigureType{kScout, 0, 0, 0, 0})
        .figure_type(sim::FigureType{kDonkey, 0, 0, 0, 0});
    auto built = b.build();
    REQUIRE(built);
    return built.value();
}

inline sim::GoodsId goods(const sim::TypeRegistry& types, const char* name) {
    const auto id = types.goods_by_name(name);
    REQUIRE(id.has_value());
    return *id;
}

} // namespace ship_test
