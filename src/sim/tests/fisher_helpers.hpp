#pragma once
#include <anno/sim/driver.hpp>
#include <anno/sim/walk_cells.hpp>

#include "harvest_helpers.hpp"
namespace harvest_test {
// FISHER-1b: own synthetic input; no default registry or existing fixture changes.
inline constexpr sim::TypeId kBayFishery{9390};
inline std::shared_ptr<const sim::TypeRegistry> fishery_registry(std::uint32_t wait = 7) {
    auto base = registry();
    auto farm = *base->find(kFisherFarm);
    farm.id = kBayFishery;
    farm.prodtype = 6; // FISCHER's original PRODTYPE (fisher.md §1)
    farm.prod.inputs = {"FISCH"};
    farm.prod.outputs = {"NAHRUNG"};
    farm.prod.input_per_step = {128};
    farm.prod.output_per_step = {51};
    farm.prod.capacity = {1280};
    farm.input_goods = {goods_id("FISCH")};
    farm.output_goods = {goods_id("NAHRUNG")};
    sim::TypeRegistry::Builder b;
    auto figure = worker_type(kFisher, 768, 45);
    figure.speed_q24 = 1U << 24; // fixture speed: one tile per tick
    figure.load_ticks = wait;
    b.add(farm)
        .footprint(kBayFishery, 3, 4)
        .entrances(kBayFishery, {{6, 9}, {12, 9}})
        .action_radius(kBayFishery, 7)
        .figure_ids(kBayFishery, {kFisher})
        .add(resource_type(sim::TypeId{10}, 226, 0))
        .add(resource_type(sim::TypeId{11}, 225, goods_id("FISCH")))
        .figure_type(figure)
        .walk_tables(sim::synthetic_walk_tables());
    auto made = b.build();
    REQUIRE_MESSAGE(made.ok(), (made.ok() ? "" : made.error()));
    return made.value();
}
inline Rig fishery_rig(bool fish = true, bool water = true, std::uint8_t rotation = 0) {
    auto r = make_rig();
    auto& is = r.spec.islands[0];
    std::vector<std::uint8_t> heights(std::uint64_t{is.w} * is.h,
                                      water ? std::uint8_t{0} : std::uint8_t{128});
    // A closed control resource, separated from the three accessible water resources.
    for (std::uint32_t y = 16; y <= 17; ++y)
        for (std::uint32_t x = 14; x <= 15; ++x)
            heights[std::uint64_t{y} * is.w + x] = 128;
    is.terrain = sim::make_terrain(std::move(heights), {}, {});
    if (fish) {
        r.object(115, 112, sim::TypeId{11});
        r.object(116, 112, sim::TypeId{11});
        r.object(117, 112, sim::TypeId{11});
    }
    r.object(114, 116, sim::TypeId{11});
    r.place(kBayFishery, 110, 110, sim::Farm{});
    is.cities[0].buildings.back().rotation = rotation;
    return r;
}
inline sim::World
create_fishery(const Rig& r, const std::shared_ptr<const sim::TypeRegistry>& types, bool enabled) {
    sim::ExtensionContext rules;
    rules.fisher_rules.enabled = enabled ? std::uint8_t{1} : std::uint8_t{0};
    auto made = sim::World::create(r.spec, types, {}, {}, {}, sim::serial_executor(), rules);
    REQUIRE_MESSAGE(made.ok(), (made.ok() ? "" : made.error()));
    return std::move(made).value();
}

} // namespace harvest_test
