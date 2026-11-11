// MAP-INTEGRATE-2 I3: an authored (not original-data) world in which the real mine/tool,
// quarry/brick, cart and automatic-freight paths are live together. Setup is ScenarioSpec data;
// every runtime intervention is a public command.
#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/driver.hpp>
#include <anno/sim/freight.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/world.hpp>

#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
namespace {

constexpr sim::TypeId kWarehouse{1302}, kMine{9006}, kMarket{9021}, kSmelter{9601},
    kToolsmith{9602}, kMason{9603}, kQuarry{9605}, kRoad{9010};
constexpr std::uint32_t kShipKind = 9510, kMasonKind = 9604;
constexpr sim::FigureId kShip{1};
constexpr sim::PlayerId kOwner{0};
constexpr sim::CityRef kSource{sim::IslandId{0}, sim::CityNo{0}};

struct Authored {
    std::shared_ptr<const sim::TypeRegistry> types;
    sim::World world;
    sim::GoodsId ore = 0, iron = 0, tools = 0, stone = 0, bricks = 0;
    sim::BuildingId mine, quarry, mason, toolsmith;
    std::vector<sim::BuildingId> smelters;
};

sim::ScenarioSpec::BuildingSpec at(sim::TypeId type, std::int32_t x, std::int32_t y,
                                   std::optional<sim::Payload> payload = std::nullopt) {
    sim::ScenarioSpec::BuildingSpec b;
    b.type = type;
    b.x = x;
    b.y = y;
    b.payload = std::move(payload);
    return b;
}

std::shared_ptr<const sim::TypeRegistry> authored_types() {
    auto transport = syn::make_transport_types();
    auto cargo = syn::make_cargo_types();
    REQUIRE(transport);
    REQUIRE(cargo);
    sim::TypeRegistry::Builder b;
    b.walk_tables(sim::synthetic_walk_tables());
    for (auto type : transport.value().registry->types()) {
        if (type.id == kMarket)
            type.action_radius = 32;
        b.add(std::move(type));
    }
    b.entrances(kMine, {{3, 3}});
    b.add(*cargo.value().registry->find(kWarehouse));
    b.add_workshop(kSmelter, "EISEN", "EISENERZ", 128, 128, 10, 1)
        .entrances(kSmelter, {{3, 3}})
        .action_radius(kSmelter, 32)
        .figure_ids(kSmelter, {203});
    b.add_workshop(kToolsmith, "WERKZEUG", "EISEN", 128, 128, 10, 1)
        .entrances(kToolsmith, {{3, 3}})
        .action_radius(kToolsmith, 32)
        .figure_ids(kToolsmith, {203});
    b.add_farm(kMason, "ZIEGEL", "STEINE", 128, 128, 10, 1)
        .entrances(kMason, {{3, 3}})
        .action_radius(kMason, 32)
        .figure_ids(kMason, {kMasonKind});
    auto quarry = *transport.value().registry->find(kMine);
    quarry.id = kQuarry;
    quarry.prodtype = 13;
    quarry.cls = sim::RuntimeClass::Quarry;
    quarry.output_goods = {*transport.value().registry->goods_by_name("STEINE")};
    quarry.input_goods.clear();
    quarry.entrances = {{3, 3}};
    quarry.prod.id = kQuarry.v;
    quarry.prod.prodtype = "STEINBRUCH";
    quarry.prod.outputs = {"STEINE"};
    quarry.prod.inputs.clear();
    quarry.prod.capacity = {7 * 256};
    b.add(std::move(quarry));
    for (auto figure : transport.value().registry->figure_types()) {
        if (figure.id == 203)
            figure.speed_q24 = 4000;
        b.figure_type(std::move(figure));
    }
    b.figure_type(*cargo.value().registry->find_figure_type(kShipKind));
    auto mason = *transport.value().registry->find_figure_type(203);
    mason.id = kMasonKind;
    mason.speed_q24 = 4000;
    mason.route = 0x2c;
    b.figure_type(std::move(mason));
    auto built = b.build();
    REQUIRE_MESSAGE(built, (built ? "" : built.error()));
    return std::move(built).value();
}

const sim::City& city(const sim::World& w, unsigned island) {
    REQUIRE(w.islands().at(island));
    REQUIRE(w.islands().at(island)->cities.at(0));
    return *w.islands().at(island)->cities.at(0);
}

const sim::Building* building_type(const sim::World& w, unsigned island, sim::TypeId type,
                                   unsigned ordinal = 0) {
    unsigned found = 0;
    for (const auto& list : city(w, island).index.lists)
        for (const auto& slot : list.slots)
            if (slot && slot->type == type && found++ == ordinal)
                return &*slot;
    return nullptr;
}

Authored make_authored() {
    auto types = authored_types();
    const auto goods = [&](const char* name) {
        const auto id = types->goods_by_name(name);
        REQUIRE_MESSAGE(id, name);
        return *id;
    };
    sim::ScenarioSpec spec = syn::empty_scenario(13803);
    sim::ScenarioSpec::PlayerSpec player;
    player.id = kOwner;
    player.money = 100000;
    player.flags = 0xffffffffU;
    spec.players.push_back(player);
    auto source = syn::plain_island(0, 100, 100, 64, 32);
    source.owner_runs = {{0, 64U * 32U}};
    syn::give_flat_terrain(source, 0x30);
    for (int x = 111; x <= 123; ++x)
        source.objects.push_back(
            {static_cast<std::uint32_t>((114 - 100) * 64 + (x - 100)), kRoad.v, 0});
    source.objects.push_back(
        {static_cast<std::uint32_t>((113 - 100) * 64 + (111 - 100)), kRoad.v, 0});
    sim::ScenarioSpec::DepositSpec deposit;
    deposit.goods = "EISENERZ";
    deposit.x = 111;
    deposit.y = 115;
    deposit.remaining = 200 * 256;
    source.deposits.push_back(deposit);
    sim::ScenarioSpec::CitySpec source_city = syn::plain_city(0, 0);
    source_city.capacity = 100 * 256;
    source_city.stock = std::vector<std::uint32_t>(128, 0);
    sim::Mine mine;
    mine.deposit = 0;
    sim::Quarry quarry;
    quarry.claims = {0};
    source_city.buildings = {
        at(kWarehouse, 100, 108),
        at(kMarket, 110, 110),
        at(kMine, 111, 115, sim::Payload{mine}),
        at(kSmelter, 113, 115, sim::Payload{sim::make_workshop(types->find(kSmelter)->prod)}),
        at(kSmelter, 115, 115, sim::Payload{sim::make_workshop(types->find(kSmelter)->prod)}),
        at(kToolsmith, 117, 115, sim::Payload{sim::make_workshop(types->find(kToolsmith)->prod)}),
        at(kQuarry, 121, 115, sim::Payload{quarry}),
        at(kMason, 123, 115, sim::Payload{sim::Farm{}}),
    };
    source.cities.push_back(std::move(source_city));

    auto destination = syn::plain_island(1, 250, 100, 64, 32);
    destination.owner_runs = {{0, 64U * 32U}};
    syn::give_flat_terrain(destination, 0x30);
    auto destination_city = syn::plain_city(0, 0);
    destination_city.capacity = 100 * 256;
    destination_city.stock = std::vector<std::uint32_t>(128, 0);
    destination_city.buildings = {at(kWarehouse, 250, 108)};
    destination.cities.push_back(std::move(destination_city));
    spec.islands = {std::move(source), std::move(destination)};

    sim::ScenarioSpec::FigureSpec ship;
    ship.kind = kShipKind;
    ship.owner = kOwner;
    ship.pos = {98 * 256 + 128, 110 * 256 + 128};
    ship.dir16 = 8;
    ship.holds = std::vector<sim::ShipHold>(4);
    spec.figures.push_back(ship);
    sim::Rules rules;
    rules.freight.work_per_visit = 512;
    sim::SimConfig config;
    config.warehouse_goods = 128;
    config.figure_min_growth = 1;
    auto made = sim::World::create(spec, types, config, rules);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    Authored out{types,
                 std::move(made).value(),
                 goods("EISENERZ"),
                 goods("EISEN"),
                 goods("WERKZEUG"),
                 goods("STEINE"),
                 goods("ZIEGEL"),
                 {},
                 {},
                 {},
                 {},
                 {}};
    out.mine = building_type(out.world, 0, kMine)->id;
    out.smelters = {building_type(out.world, 0, kSmelter, 0)->id,
                    building_type(out.world, 0, kSmelter, 1)->id};
    out.toolsmith = building_type(out.world, 0, kToolsmith)->id;
    out.quarry = building_type(out.world, 0, kQuarry)->id;
    out.mason = building_type(out.world, 0, kMason)->id;
    REQUIRE_MESSAGE(out.world.check_invariants().empty(), out.world.check_invariants());
    return out;
}

sim::ApplyResult public_apply(sim::World& w, sim::Body body) {
    auto encoded = sim::encode_record(body);
    REQUIRE(encoded);
    auto decoded = sim::decode_record(encoded.value());
    REQUIRE(decoded);
    return w.apply({kOwner, std::move(decoded).value()});
}

std::uint64_t stock(const sim::World& w, unsigned island, sim::GoodsId goods) {
    return city(w, island).stock.stock.at(goods);
}

std::uint64_t holds(const sim::World& w, sim::GoodsId goods) {
    std::uint64_t amount = 0;
    for (const auto& f : w.figures().list.slots)
        if (f)
            for (const auto& hold : f->holds)
                if (hold.goods == goods)
                    amount += hold.amount_88;
    return amount;
}

std::uint64_t carried(const sim::World& w, sim::GoodsId goods) {
    std::uint64_t amount = 0;
    for (const auto& f : w.figures().list.slots)
        if (f && !f->expiry_stamp && f->cargo_goods == goods)
            amount += f->cargo_88;
    return amount;
}

std::uint64_t output(const sim::World& w, sim::BuildingId id) {
    const auto* b = w.find_building(id);
    REQUIRE(b);
    const auto* p = std::get_if<sim::Workshop>(&b->payload);
    REQUIRE(p);
    return p->out.at(0);
}

std::size_t mine_reservers(const Authored& a, const sim::World& w, std::uint64_t& sum) {
    std::size_t count = 0;
    sum = 0;
    for (const auto& f : w.figures().list.slots)
        if (f && !f->expiry_stamp && f->transport.target == a.mine && f->transport.reserved_88) {
            ++count;
            sum += f->transport.reserved_88;
        }
    return count;
}

bool live_boundary(const Authored& a, const sim::World& w) {
    const auto* ship = sim::find_figure(w.figures(), kShip);
    const auto* route = w.freight_route(kShip);
    std::uint64_t reserved = 0;
    const auto* quarry = w.find_building(a.quarry);
    const auto* mine = w.find_building(a.mine);
    return ship && route && route->phase == sim::FreightPhase::Sailing && holds(w, a.tools) > 0 &&
           quarry && std::get<sim::Quarry>(quarry->payload).claims[0] != 0 && mine &&
           mine_reservers(a, w, reserved) >= 2 &&
           std::get<sim::Mine>(mine->payload).reserved == reserved;
}

template <class P> bool advance_until(sim::World& w, unsigned max, std::uint32_t ms, P pred) {
    for (unsigned i = 0; i < max; ++i) {
        w.advance(ms);
        REQUIRE_MESSAGE(!w.faulted(), w.fault());
        if (pred())
            return true;
    }
    return false;
}

sim::AssignFreightRouteCmd tool_route(const Authored& a) {
    const auto* source = building_type(a.world, 0, kWarehouse);
    const auto* destination = building_type(a.world, 1, kWarehouse);
    REQUIRE(source);
    REQUIRE(destination);
    using D = sim::FreightDirection;
    return {kShip,
            {{source->id, 98 * 256 + 128, 110 * 256 + 128, {{D::LoadTarget, a.tools, 2}}},
             {destination->id, 248 * 256 + 128, 110 * 256 + 128, {{D::UnloadAmount, a.tools, 2}}}}};
}

} // namespace

TEST_CASE("integration authored cross-feature: mine tools freight quarry and exact reload") {
    Authored a = make_authored();
    sim::TransportStats stats;
    a.world.set_transport_stats(&stats);
    const bool tools_ready =
        advance_until(a.world, 12000, 100, [&] { return stock(a.world, 0, a.tools) >= 512; });
    INFO("ore " << stock(a.world, 0, a.ore) << " iron " << stock(a.world, 0, a.iron) << " tools "
                << stock(a.world, 0, a.tools) << " mine-out "
                << std::get<sim::Mine>(a.world.find_building(a.mine)->payload).out
                << " smelter0-in/out "
                << std::get<sim::Workshop>(a.world.find_building(a.smelters[0])->payload).in[0]
                << "/" << output(a.world, a.smelters[0]) << " smelter1-in/out "
                << std::get<sim::Workshop>(a.world.find_building(a.smelters[1])->payload).in[0]
                << "/" << output(a.world, a.smelters[1]) << " smith-in/out "
                << std::get<sim::Workshop>(a.world.find_building(a.toolsmith)->payload).in[0] << "/"
                << output(a.world, a.toolsmith));
    REQUIRE(tools_ready);
    REQUIRE(public_apply(a.world, tool_route(a)).ok());
    bool saw_route_cargo = false, saw_quarry_claim = false, saw_two_reservers = false;
    const bool boundary_ready = advance_until(a.world, 12000, 100, [&] {
        const auto* r = a.world.freight_route(kShip);
        saw_route_cargo = saw_route_cargo || (r && r->phase == sim::FreightPhase::Sailing &&
                                              holds(a.world, a.tools) > 0);
        saw_quarry_claim =
            saw_quarry_claim ||
            std::get<sim::Quarry>(a.world.find_building(a.quarry)->payload).claims[0] != 0;
        std::uint64_t transient_reserved = 0;
        saw_two_reservers =
            saw_two_reservers || mine_reservers(a, a.world, transient_reserved) >= 2;
        return live_boundary(a, a.world);
    });
    std::uint64_t reserved_at_end = 0;
    const auto* route_at_end = a.world.freight_route(kShip);
    INFO("route-phase "
         << (route_at_end ? static_cast<int>(route_at_end->phase) : -1) << " cycles "
         << (route_at_end ? route_at_end->completed_cycles : 0) << " holds "
         << holds(a.world, a.tools) << " quarry-claim "
         << static_cast<unsigned>(
                std::get<sim::Quarry>(a.world.find_building(a.quarry)->payload).claims[0])
         << " mine-reservers " << mine_reservers(a, a.world, reserved_at_end) << " mine-reserved "
         << std::get<sim::Mine>(a.world.find_building(a.mine)->payload).reserved << "/"
         << reserved_at_end << " saw-route/claim/two " << saw_route_cargo << "/" << saw_quarry_claim
         << "/" << saw_two_reservers);
    REQUIRE(boundary_ready);

    const auto boundary = a.world.save();
    auto loaded = sim::World::load(boundary, a.types);
    REQUIRE_MESSAGE(loaded, (loaded ? "" : loaded.error()));
    CHECK(loaded.value().save() == boundary);
    auto uninterrupted = a.world;
    uninterrupted.set_transport_stats(nullptr);
    auto shorter = loaded.value();
    for (unsigned i = 0; i < 800; ++i) {
        uninterrupted.advance(10);
        loaded.value().advance(10);
        if (i != 799)
            shorter.advance(10);
    }
    CHECK(loaded.value().state_hash() == uninterrupted.state_hash());
    CHECK(loaded.value().rng().cursor() == uninterrupted.rng().cursor());
    CHECK(loaded.value().save() == uninterrupted.save());
    CHECK(shorter.state_hash() != uninterrupted.state_hash());

    const auto made = stats.forestry.workshops[a.toolsmith].made_88.at(0);
    const auto in_transit = carried(a.world, a.tools) + holds(a.world, a.tools);
    const auto in_stores =
        output(a.world, a.toolsmith) + stock(a.world, 0, a.tools) + stock(a.world, 1, a.tools);
    CHECK(made == in_transit + in_stores);
    CHECK(stats.forestry.mine_made_88[a.mine] > 0);
    CHECK(stats.loaded_by_home[{a.mine, a.smelters[0]}] +
              stats.loaded_by_home[{a.mine, a.smelters[1]}] >
          0);
    CHECK(stats.forestry.workshops[a.smelters[0]].made_88[0] +
              stats.forestry.workshops[a.smelters[1]].made_88[0] >
          0);
    CHECK((stock(a.world, 1, a.tools) > 0 || holds(a.world, a.tools) > 0));

    std::uint64_t observed_cycles = a.world.freight_route(kShip)->completed_cycles;
    std::uint64_t observed_stock = stock(a.world, 1, a.tools);
    unsigned delivery_cycles = 0;
    REQUIRE(advance_until(a.world, 12000, 100, [&] {
        const auto* route = a.world.freight_route(kShip);
        if (route && route->completed_cycles > observed_cycles) {
            if (stock(a.world, 1, a.tools) > observed_stock)
                ++delivery_cycles;
            observed_cycles = route->completed_cycles;
            observed_stock = stock(a.world, 1, a.tools);
        }
        return delivery_cycles >= 2;
    }));
    REQUIRE(advance_until(a.world, 12000, 100, [&] {
        const auto* route = a.world.freight_route(kShip);
        return route && route->phase == sim::FreightPhase::Sailing && holds(a.world, a.tools) > 0;
    }));
    const auto aboard_at_demolition = holds(a.world, a.tools);
    REQUIRE(aboard_at_demolition > 0);
    sim::DemolishCmd demolish{kSource, {{111, 115}}};
    REQUIRE(public_apply(a.world, demolish).ok());
    REQUIRE(a.world.find_building(a.mine) == nullptr);

    // The first delivery after demolition could be the cargo already aboard. Observe that cargo
    // leave, then a later nonempty sailing leg: that load necessarily happened at the source
    // warehouse after the mine was gone.
    bool post_demolition_empty = false;
    REQUIRE(advance_until(a.world, 12000, 100, [&] {
        const auto* route = a.world.freight_route(kShip);
        post_demolition_empty = post_demolition_empty || holds(a.world, a.tools) == 0;
        return route && post_demolition_empty && route->phase == sim::FreightPhase::Sailing &&
               holds(a.world, a.tools) > 0;
    }));
    const auto fresh_post_demolition_load = holds(a.world, a.tools);
    const auto delivered_before_fresh_load = stock(a.world, 1, a.tools);
    REQUIRE(fresh_post_demolition_load > 0);
    REQUIRE(advance_until(a.world, 12000, 100, [&] {
        return stock(a.world, 1, a.tools) >=
               delivered_before_fresh_load + fresh_post_demolition_load;
    }));
    CHECK(a.world.freight_route(kShip)->completed_cycles >= 2);
    std::uint64_t stale = 0;
    CHECK(mine_reservers(a, a.world, stale) == 0);
    CHECK(stale == 0);
    const auto made_after_demolition = stats.forestry.workshops[a.toolsmith].made_88.at(0);
    const auto transit_after_demolition = carried(a.world, a.tools) + holds(a.world, a.tools);
    const auto stores_after_demolition =
        output(a.world, a.toolsmith) + stock(a.world, 0, a.tools) + stock(a.world, 1, a.tools);
    CHECK(made_after_demolition == transit_after_demolition + stores_after_demolition);
    CHECK(a.world.check_invariants().empty());
}

TEST_CASE("integration authored cross-feature: suspend cancel returns manual ship control while "
          "land chains keep running") {
    Authored a = make_authored();
    sim::TransportStats stats;
    a.world.set_transport_stats(&stats);
    REQUIRE(advance_until(a.world, 12000, 100, [&] { return stock(a.world, 0, a.tools) >= 512; }));
    REQUIRE(public_apply(a.world, tool_route(a)).ok());
    REQUIRE(advance_until(a.world, 12000, 100, [&] { return live_boundary(a, a.world); }));
    const auto mine_made = stats.forestry.mine_made_88[a.mine];
    const auto stone_lifted = stats.forestry.quarry_lifted_88[a.quarry];
    const auto bricks_made = stats.forestry.farms[a.mason].made_88;
    const auto tools_carted = stats.unload_offered[a.tools];
    const auto bricks_carted = stats.unload_offered[a.bricks];
    REQUIRE(public_apply(a.world, sim::ControlFreightRouteCmd{kShip, sim::FreightControl::Suspend})
                .ok());
    REQUIRE(a.world.freight_route(kShip)->status == sim::FreightStatus::Suspended);
    REQUIRE(advance_until(a.world, 12000, 100, [&] {
        return stats.forestry.mine_made_88[a.mine] > mine_made &&
               stats.forestry.quarry_lifted_88[a.quarry] > stone_lifted &&
               stats.forestry.farms[a.mason].made_88 > bricks_made &&
               stats.unload_offered[a.tools] > tools_carted &&
               stats.unload_offered[a.bricks] > bricks_carted;
    }));
    CHECK(a.world.freight_route(kShip)->status == sim::FreightStatus::Suspended);
    REQUIRE(public_apply(a.world, sim::ControlFreightRouteCmd{kShip, sim::FreightControl::Cancel})
                .ok());
    CHECK(a.world.freight_route(kShip) == nullptr);
    REQUIRE(public_apply(a.world, sim::MoveCmd{kShip, 94 * 256, 110 * 256}).ok());
    const auto* ship = sim::find_figure(a.world.figures(), kShip);
    REQUIRE(ship);
    CHECK(ship->nav->order.has_value());
    CHECK_FALSE(ship->freight.has_value());
    CHECK(a.world.check_invariants().empty());
}

TEST_CASE("integration authored cross-feature: targeted seam omissions fail beside the legal "
          "combined control") {
    Authored a = make_authored();
    REQUIRE(advance_until(a.world, 12000, 100, [&] { return stock(a.world, 0, a.tools) >= 512; }));
    REQUIRE(public_apply(a.world, tool_route(a)).ok());
    REQUIRE(advance_until(a.world, 12000, 100, [&] { return live_boundary(a, a.world); }));
    const auto legal = a.world.state_hash();
    auto missing_route = a.world;
    sim::find_figure(sim::WorldTestAccess::figures(missing_route), kShip)->freight.reset();
    CHECK(missing_route.state_hash() != legal);
    auto missing_claim = a.world;
    auto* q = sim::find_building_mut(*sim::WorldTestAccess::islands(missing_claim)[0], a.quarry);
    REQUIRE(q);
    std::get<sim::Quarry>(q->payload).claims[0] = 0;
    CHECK(missing_claim.state_hash() != legal);
    auto missing_quarry_case = a.world;
    q = sim::find_building_mut(*sim::WorldTestAccess::islands(missing_quarry_case)[0], a.quarry);
    REQUIRE(q);
    q->payload = sim::Inert{};
    CHECK(missing_quarry_case.check_invariants().find("class needs Quarry") != std::string::npos);
    auto swapped_mine_tag = a.world;
    auto* m = sim::find_building_mut(*sim::WorldTestAccess::islands(swapped_mine_tag)[0], a.mine);
    REQUIRE(m);
    m->payload = sim::Quarry{};
    CHECK(swapped_mine_tag.check_invariants().find("class needs Mine") != std::string::npos);
}
