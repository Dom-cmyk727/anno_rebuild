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

/// The same registry with the ship type navigating: the facts of the shipped type 1502 (speed 0.036
/// and turn 0.11 as q32, 6000 hp, holds 4/2, class 0, stamp radius 5 tiles). `nav_id` is the
/// navigating ship type (1502 by default; another id makes 1502 an ordinary figure type).
inline std::shared_ptr<const sim::TypeRegistry> nav_ship_registry(std::uint32_t nav_id = kShip) {
    auto base = sim::synthetic::make_synthetic_types();
    REQUIRE(base);
    sim::TypeRegistry::Builder b;
    b.walk_tables(sim::synthetic_walk_tables());
    for (const sim::BuildingType& t : base.value().registry->types()) {
        b.add(t);
    }
    sim::FigureType ship{nav_id, 0, 0, 0, 0};
    ship.bewohntype = 113;
    ship.nav = sim::NavFacts{154618816U, 472446400U, 6000U, 4U, 2U, 0U, 1280U};
    if (nav_id != kShip) {
        sim::FigureType plain{kShip, 0, 0, 0, 0};
        plain.bewohntype = 113;
        b.figure_type(plain);
    }
    b.figure_type(ship)
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
