// FISHER-1f: docs/re/fisher.md section 10.2; synthetic cases use the read's vectors.
#include <array>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/fisher.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/walk_search.hpp>

#include "fisher_helpers.hpp"

namespace {
namespace sim = anno::sim;
using namespace harvest_test;
sim::Building& closed_home(sim::World& w) {
    for (auto& list : sim::WorldTestAccess::islands(w)[0]->cities[0]->index.lists)
        for (auto& b : list.slots)
            if (b && b->type == kBayFishery)
                return *b;
    FAIL("missing fishery");
    return *sim::WorldTestAccess::islands(w)[0]->cities[0]->index.lists[0].slots[0];
}
sim::Figure* worker(sim::World& w, sim::FigureId id = {}) {
    for (auto& f : sim::WorldTestAccess::figures(w).list.slots)
        if (f && (id.valid() ? f->id == id : f->kind == kFisher && f->home.valid()))
            return &*f;
    return nullptr;
}
using Fields = std::map<std::string, std::string>;
Fields read_fields(const std::string& s) {
    Fields out;
    std::istringstream in(s);
    for (std::string word; in >> word;) {
        const auto at = word.find('=');
        REQUIRE(at != std::string::npos);
        REQUIRE(out.emplace(word.substr(0, at), word.substr(at + 1)).second);
    }
    return out;
}
unsigned num(const Fields& f, const std::string& key) {
    return static_cast<unsigned>(std::stoul(f.at(key)));
}
} // namespace

TEST_CASE(
    "fisher closed: paired mode-4 point flood permits departure but never closed-goal admission") {
    auto types = fishery_registry();
    auto rig = fishery_rig(false, false);
    auto& spec = rig.spec.islands[0];
    std::vector<std::uint8_t> heights(spec.w * spec.h, 128);
    for (unsigned y : {12U, 13U})
        for (unsigned x : {11U, 12U})
            heights[y * spec.w + x] = 24;
    spec.terrain = sim::make_terrain(std::move(heights), {}, {});
    auto w = create_fishery(rig, types, true);
    auto& mutable_is = *sim::WorldTestAccess::islands(w)[0];
    mutable_is.tiles.set_walk_mask(mutable_is.tile_index(111, 111), 511);
    const auto& is = *w.islands()[0];
    const auto& tables = *types->walk_tables();
    const std::array starts{sim::WalkPoint{{111, 111}, 2, 1}};
    const std::array goals{sim::WalkPoint{{111, 112}, 1, 1}};
    sim::WalkCells sea(is, *types, tables, is.rect(), 4, {}, sim::WalkCellMode::FisherSea);
    REQUIRE(sea.get(starts[0].tile).high == 7);
    REQUIRE(sea.get(goals[0].tile).high == 0);
    REQUIRE(sim::walk_record(is, *types, starts[0].tile).mask9 == 511);
    const auto forward = sim::search(sea, tables, 2, starts, goals, [](auto&) {
        return sim::SearchDecision{sim::GoalAction::Stop, true};
    });
    CHECK(forward.complete);
    CHECK(forward.selected.has_value());
    sim::WalkCells back(is, *types, tables, is.rect(), 4, {}, sim::WalkCellMode::FisherSea);
    const auto reverse = sim::search(back, tables, 2, goals, starts, [](auto&) {
        return sim::SearchDecision{sim::GoalAction::Stop, true};
    });
    CHECK(reverse.complete);
    CHECK_FALSE(reverse.selected.has_value());
}

TEST_CASE(
    "fisher closed: all five closed_home vectors use the runtime and saved integer end wait") {
    std::ifstream input(std::filesystem::path(ANNO_FIXTURES_DIR) /
                        "sim/fisher_entrance_vectors.txt");
    REQUIRE(input.good());
    unsigned count = 0;
    for (std::string line; std::getline(input, line);) {
        std::istringstream prefix(line);
        std::string kind, label;
        prefix >> kind >> label;
        if (kind != "closed_home")
            continue;
        ++count;
        INFO(label);
        const auto arrow = line.find("=>");
        REQUIRE(arrow != std::string::npos);
        const auto begin = line.find(' ', line.find(' ') + 1);
        const auto args = read_fields(line.substr(begin, arrow - begin));
        const auto expected = read_fields(line.substr(arrow + 2));
        const auto comma = args.at("means").find(',');
        REQUIRE(comma != std::string::npos);
        const auto first = std::stoul(args.at("means").substr(0, comma));
        const auto second = std::stoul(args.at("means").substr(comma + 1));
        REQUIRE(first >= 32);
        REQUIRE(second >= 32);
        auto rig = fishery_rig(false, false);
        auto& spec = rig.spec.islands[0];
        std::vector<std::uint8_t> heights(spec.w * spec.h, 48);
        for (unsigned y : {11U, 12U}) {
            heights[y * spec.w + 11] = static_cast<std::uint8_t>(2 * first - second);
            heights[y * spec.w + 12] = static_cast<std::uint8_t>(second);
            heights[y * spec.w + 13] = static_cast<std::uint8_t>(second);
        }
        spec.terrain = sim::make_terrain(std::move(heights), {}, {});
        auto types = fishery_registry();
        auto w = create_fishery(rig, types, true);
        for (unsigned i = 0; i < 1000 && !worker(w); ++i)
            w.advance(2);
        auto* f = worker(w);
        REQUIRE(f);
        REQUIRE(f->harvest);
        auto& b = closed_home(w);
        auto& farm = std::get<sim::Farm>(b.payload);
        const auto id = f->id;
        f->pos = num(args, "inside") ? sim::entrance_position(b, *types->find(b.type), 0)
                                     : sim::tile_centre({120, 120});
        const auto position = f->pos;
        f->cargo_goods = num(args, "goods");
        f->cargo_88 = num(args, "cargo");
        f->harvest->start_delay = 0;
        f->harvest->step = sim::HarvestStep::Start;
        farm.raw = num(args, "raw");
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        const auto cursor = w.rng().cursor();
        w.advance(2);
        f = worker(w, id);
        REQUIRE(f);
        REQUIRE(f->harvest);
        CHECK(f->pos == position);
        CHECK(f->cargo_88 == num(args, "cargo"));
        CHECK(farm.raw == num(expected, "raw"));
        CHECK(farm.workers_out == 1 - num(expected, "released"));
        CHECK_FALSE(f->home.valid());
        CHECK(unsigned(f->expiry_stamp.has_value()) == num(expected, "started"));
        CHECK(stats.forestry.raw_delivered_88 == num(expected, "credit"));
        CHECK(w.rng().cursor() - cursor == num(expected, "consumed"));
        if (num(expected, "failed")) {
            REQUIRE_FALSE(f->expiry_stamp.has_value());
            CHECK(f->harvest->wait == 7499); // 7500 nominal ticks, one tick of surplus consumed.
            REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
            // A forced end must cancel the outstanding wait without releasing twice.
            auto forced = *f;
            sim::Effects effects;
            REQUIRE(sim::end_figure(forced, static_cast<std::uint32_t>(w.clock().tick()), effects));
            CHECK(forced.harvest->wait == 0);
            CHECK(forced.expiry_stamp.has_value());
            CHECK(effects.releases.empty());
            auto invalid = w.figures();
            for (auto& slot : invalid.list.slots)
                if (slot && slot->id == id)
                    slot->harvest->wait = 0;
            CHECK_FALSE(sim::validate_figures(invalid, w.ids(), true).empty());
            farm.switched_on = false;
            const auto snapshot = w.save();
            auto loaded = sim::World::load(snapshot, types);
            REQUIRE_MESSAGE(loaded, (loaded ? "" : loaded.error()));
            CHECK(loaded.value().state_hash() == w.state_hash());
            w.advance(0);
            loaded.value().advance(0);
            CHECK(worker(w, id)->harvest->wait == 7499);
            w.advance(2);
            loaded.value().advance(2);
            CHECK(worker(w, id)->harvest->wait == 7498);
            w.advance(14994);
            loaded.value().advance(14994);
            CHECK(worker(w, id)->harvest->wait == 1);
            CHECK_FALSE(worker(w, id)->expiry_stamp.has_value());
            w.advance(4);
            loaded.value().advance(4); // Last tick plus surplus, no re-arm.
            CHECK(worker(w, id)->harvest->wait == 0);
            CHECK(worker(w, id)->expiry_stamp.has_value());
            CHECK(worker(w, id)->pos == position);
            CHECK(w.state_hash() == loaded.value().state_hash());
        } else {
            CHECK(f->expiry_stamp.has_value());
            CHECK(f->harvest->wait == 0);
        }
        REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    }
    CHECK(count == 5);
}
