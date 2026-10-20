// The state of buildings and cities in the stream: field lists, perturbation,
// save and load, the derived indexes and the random property tests (docs/sim/
// state.md §9.1, §9.3, §14.2, §14.4, stage S6).
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/building.hpp>
#include <anno/sim/city.hpp>
#include <anno/sim/field_count.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>

#include "building_helpers.hpp"
#include "golden.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
using namespace building_test;
using island_test::Stream;

namespace {

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

/// The bytes of a status tracker as the stream has them: code, icons, time.
void status_bytes(Stream& s, std::uint32_t code, std::uint32_t icons, std::uint32_t time) {
    s.u8(code);
    s.u8(icons);
    s.u16(time);
}

sim::StatusTracker status(sim::Status code, std::uint8_t icons, std::uint16_t time) {
    sim::StatusTracker t;
    t.code = code;
    t.icons = icons;
    t.time = time;
    t.shows_icons = false; // derived: not in the stream
    return t;
}

/// A building with the common fields set to distinct values and `payload`.
sim::Building common(sim::Payload payload) {
    sim::Building b;
    b.id = sim::BuildingId{0x0102030405060708ULL};
    b.type = sim::TypeId{0x11223344U};
    b.x = -5;
    b.y = 6;
    b.rotation = 3;
    b.byte0e = 0x82;
    b.byte0f = 9;
    b.payload = std::move(payload);
    return b;
}

/// The expected bytes of the common part, from the text of §9.3.
Stream common_bytes(std::uint32_t tag) {
    Stream s;
    s.u64(0x0102030405060708ULL);
    s.u32(0x11223344U);
    s.i32(-5);
    s.i32(6);
    s.u8(3);
    s.u8(0x82);
    s.u8(9);
    s.u8(tag);
    return s;
}

struct LayoutRow {
    const char* name;
    sim::Building building;
    Stream expect;
    std::size_t primitives; // scalars the trace sees (scope markers excluded)
};

std::vector<LayoutRow> layout_rows() {
    std::vector<LayoutRow> rows;
    {
        sim::Inert p;
        p.last = 0x1234;
        p.blocked = true;
        p.suspended = false;
        p.status = status(sim::Status::MissingInput, 6, 0x5678);
        Stream s = common_bytes(0);
        s.u16(0x1234);
        s.boolean(true);
        s.boolean(false);
        status_bytes(s, 1, 6, 0x5678);
        rows.push_back({"Inert", common(p), s, 8 + 3 + 3});
    }
    {
        sim::Workshop p;
        p.last = 0x0102;
        p.switched_on = false;
        p.suspended = true;
        p.blocked = true;
        p.out = {7, 8};
        p.in = {9};
        p.activity = {10, 11, 12};
        p.carriers_out = 13;
        p.status = status(sim::Status::StorageFull, 4, 0x0a0b);
        Stream s = common_bytes(1);
        s.u16(0x0102);
        s.boolean(false);
        s.boolean(true);
        s.boolean(true);
        s.u64(2);
        s.u32(7);
        s.u32(8);
        s.u64(1);
        s.u32(9);
        s.u64(3);
        s.u32(10);
        s.u32(11);
        s.u32(12);
        s.u32(13);
        status_bytes(s, 2, 4, 0x0a0b);
        // 8 common + last + 3 bools + (count + 2) + (count + 1) + (count + 3) + carriers + 3.
        rows.push_back({"Workshop", common(p), s, 8 + 1 + 3 + 3 + 2 + 4 + 1 + 3});
    }
    {
        sim::WeaponsWorkshop p;
        p.last = 0x0304;
        p.line_on = {};
        p.line_on.assign(3, true);
        p.line_on[1] = false;
        p.unlocked.assign(3, true); // derived: not in the stream
        p.suspended = false;
        p.blocked = true;
        p.out = {1, 2};
        p.in = {3};
        p.activity = {4, 5};
        p.carriers_out = 6;
        p.status = status(sim::Status::None, 0, 0);
        Stream s = common_bytes(2);
        s.u16(0x0304);
        s.u64(3);
        s.boolean(true);
        s.boolean(false);
        s.boolean(true);
        s.boolean(false);
        s.boolean(true);
        s.u64(2);
        s.u32(1);
        s.u32(2);
        s.u64(1);
        s.u32(3);
        s.u64(2);
        s.u16(4);
        s.u16(5);
        s.u32(6);
        status_bytes(s, 0, 0, 0);
        rows.push_back({"WeaponsWorkshop", common(p), s, 8 + 1 + 4 + 2 + 3 + 2 + 3 + 1 + 3});
    }
    {
        sim::Farm p;
        p.last = 0x0506;
        p.switched_on = true;
        p.suspended = true;
        p.blocked = false;
        p.out = 0x01020304;
        p.raw = 0x05060708;
        p.activity = 0x0a0b;
        p.workers_out = 3;
        p.status = status(sim::Status::MissingInput, 2, 9);
        Stream s = common_bytes(3);
        s.u16(0x0506);
        s.boolean(true);
        s.boolean(true);
        s.boolean(false);
        s.u32(0x01020304);
        s.u32(0x05060708);
        s.u16(0x0a0b);
        s.u32(3);
        status_bytes(s, 1, 2, 9);
        rows.push_back({"Farm", common(p), s, 8 + 1 + 3 + 3 + 1 + 3});
    }
    {
        sim::SmallFarm p;
        p.last = 0x0708;
        p.line_on.assign(2, true);
        p.line_on[1] = false;
        p.suspended = true;
        p.blocked = true;
        p.out = {1, 2};
        p.raw = {3, 4};
        p.activity = {5, 6};
        p.figures_out = {7, 8, 9, 10, 11};
        p.status = status(sim::Status::StorageFull, 1, 2);
        Stream s = common_bytes(4);
        s.u16(0x0708);
        s.u64(2);
        s.boolean(true);
        s.boolean(false);
        s.boolean(true);
        s.boolean(true);
        s.u64(2);
        s.u32(1);
        s.u32(2);
        s.u64(2);
        s.u32(3);
        s.u32(4);
        s.u64(2);
        s.u16(5);
        s.u16(6);
        s.u64(5);
        for (std::uint32_t v = 7; v <= 11; ++v) {
            s.u32(v);
        }
        status_bytes(s, 2, 1, 2);
        rows.push_back({"SmallFarm", common(p), s, 8 + 1 + 3 + 2 + 3 + 3 + 3 + 6 + 3});
    }
    {
        sim::Mine p;
        p.last = 0x090a;
        p.switched_on = false;
        p.deep = true;
        p.blocked = true;
        p.out = 0x0c0d0e0f;
        p.activity = 0x1011;
        p.deposit = 5;
        p.status = status(sim::Status::None, 3, 4);
        Stream s = common_bytes(5);
        s.u16(0x090a);
        s.boolean(false);
        s.boolean(true);
        s.boolean(true);
        s.u32(0x0c0d0e0f);
        s.u16(0x1011);
        s.boolean(true);
        s.u64(5);
        status_bytes(s, 0, 3, 4);
        rows.push_back({"Mine with a deposit", common(p), s, 8 + 1 + 3 + 2 + 2 + 3});
        sim::Mine none = p;
        none.deposit.reset();
        Stream t = common_bytes(5);
        t.u16(0x090a);
        t.boolean(false);
        t.boolean(true);
        t.boolean(true);
        t.u32(0x0c0d0e0f);
        t.u16(0x1011);
        t.boolean(false);
        status_bytes(t, 0, 3, 4);
        rows.push_back({"Mine without a deposit", common(none), t, 8 + 1 + 3 + 2 + 1 + 3});
    }
    return rows;
}

} // namespace

TEST_CASE("field count canary per struct") {
    // The numbers the library's static_asserts hold, restated with the members in
    // the comment: a member added without a line in fields() fails the build there.
    CHECK(sim::field_count<sim::StatusTracker>() == 4); // code, icons, shows_icons*, time
    CHECK(sim::field_count<sim::Inert>() == 4);         // last, blocked, suspended, status
    CHECK(sim::field_count<sim::Workshop>() == 9);
    CHECK(sim::field_count<sim::WeaponsWorkshop>() == 10); // unlocked*
    CHECK(sim::field_count<sim::Farm>() == 9);
    CHECK(sim::field_count<sim::SmallFarm>() == 9);
    CHECK(sim::field_count<sim::Mine>() == 8);
    CHECK(sim::field_count<sim::Building>() == 8);
    CHECK(sim::field_count<sim::BuildingIndex>() == 4); // service_counters*, house_counts*
    CHECK(sim::field_count<sim::City>() == 8);
    CHECK(sim::field_count<sim::CityStock>() == 2);
    CHECK(sim::field_count<sim::GoodsSetting>() == 2);
    CHECK(sim::field_count<sim::BuildGoods>() == 2);
    CHECK(sim::field_count<sim::TradeEntry>() == 3);
    CHECK(sim::field_count<sim::IslandIndexCache>() == 2); // derived, no field list
    CHECK(sim::field_count<sim::Island>() == 14);          // cache*
    CHECK(sim::field_count<sim::PlacementRecord>() == 5);
    // (* = derived, not in the stream). Every Payload alternative has a stream tag.
    CHECK(std::variant_size_v<sim::Payload> == 6);
    CHECK(sim::kPayloadTags.size() == 6);
    for (std::size_t i = 0; i < sim::kPayloadTags.size(); ++i) {
        CHECK(sim::kPayloadTags[i] == i);
    }
    CHECK(sim::kPayloadTags[static_cast<std::size_t>(sim::PayloadTag::Mine)] == 5);
}

TEST_CASE("building stream layout per payload") {
    for (LayoutRow& row : layout_rows()) {
        INFO("payload " << row.name);
        sim::WriteArchive wa;
        fields(wa, std::as_const(row.building));
        CHECK(wa.data() == row.expect.v);
        // The trace sees the same bytes and the same number of scalars.
        sim::TraceArchive trace;
        fields(trace, std::as_const(row.building));
        CHECK(trace.bytes_written() == row.expect.v);
        std::size_t prims = 0;
        for (const sim::TraceEntry& e : trace.entries()) {
            if (e.kind != sim::Prim::ScopeOpen && e.kind != sim::Prim::ScopeClose) {
                ++prims;
            }
        }
        // Counts of vectors are scalars of their own in the trace.
        CHECK(prims == row.primitives);
        // The raw hash is the FNV of the bytes.
        sim::StateHasher fnv;
        fnv.bytes(row.expect.v);
        sim::HashArchive raw(sim::HashOptions{nullptr, 0, true});
        fields(raw, std::as_const(row.building));
        CHECK(raw.finish() == fnv.value());
        // Reading it back gives the same bytes; the derived fields come back at their
        // defaults (a Building default-constructed has shows_icons true).
        sim::Building back;
        sim::ReadArchive r(row.expect.v);
        fields(r, back);
        REQUIRE_MESSAGE(!r.failed(), r.error());
        CHECK(r.remaining() == 0);
        sim::WriteArchive again;
        fields(again, std::as_const(back));
        CHECK(again.data() == row.expect.v);
        CHECK(back.payload.index() == row.building.payload.index());
        std::visit([](const auto& p) { CHECK(p.status.shows_icons); }, back.payload);
        if (const auto* w = std::get_if<sim::WeaponsWorkshop>(&back.payload)) {
            CHECK(w->unlocked.empty());
        }
        // Every strict prefix fails cleanly.
        for (std::size_t n = 0; n < row.expect.v.size(); ++n) {
            sim::Building partial;
            sim::ReadArchive pr(std::span<const std::uint8_t>(row.expect.v.data(), n));
            fields(pr, partial);
            REQUIRE(pr.failed());
        }
    }
}

TEST_CASE("a building stream refuses a payload tag and a status code that do not exist") {
    LayoutRow row = layout_rows()[1]; // the Workshop
    for (const std::uint8_t tag : {std::uint8_t{6}, std::uint8_t{7}, std::uint8_t{255}}) {
        std::vector<std::uint8_t> bad = row.expect.v;
        bad[8 + 4 + 4 + 4 + 3] = tag; // the tag byte
        sim::Building b;
        sim::ReadArchive r(bad);
        fields(r, b);
        CHECK(r.failed());
    }
    // The status code is an enum of three values: 3 is refused (the byte is the
    // third from the end of the status record).
    std::vector<std::uint8_t> bad = row.expect.v;
    bad[bad.size() - 4] = 3;
    sim::Building b;
    sim::ReadArchive r(bad);
    fields(r, b);
    CHECK(r.failed());
    // A bool byte of 2 in the payload fails too.
    bad = row.expect.v;
    bad[26] = 2; // switched_on: after the common part (23 bytes), the tag and last (2)
    sim::ReadArchive r2(bad);
    sim::Building b2;
    fields(r2, b2);
    CHECK(r2.failed());
}

namespace {

sim::Building& bld(sim::World& w, std::uint32_t city, sim::ListId list, std::size_t slot) {
    return *city_of(w, 0, city).index.lists[list].slots[slot];
}

template <class P>
P& pay(sim::World& w, sim::ListId list, std::size_t slot, std::uint32_t city = 0) {
    return std::get<P>(bld(w, city, list, slot).payload);
}

using Mutation = std::pair<const char*, void (*)(sim::World&)>;

std::vector<Mutation> stream_mutations() {
    return {
        // ---- the city
        {"city owner", [](sim::World& w) { city_of(w, 0, 0).owner = sim::PlayerId{1}; }},
        {"city number", [](sim::World& w) { city_of(w, 0, 0).number = sim::CityNo{7}; }},
        {"city tick_stamp", [](sim::World& w) { city_of(w, 0, 0).tick_stamp = 5; }},
        {"stock capacity", [](sim::World& w) { city_of(w, 0, 0).stock.capacity += 1; }},
        {"stock entry first", [](sim::World& w) { city_of(w, 0, 0).stock.stock[0] += 1; }},
        {"stock entry last", [](sim::World& w) { city_of(w, 0, 0).stock.stock[55] += 1; }},
        {"stock size", [](sim::World& w) { city_of(w, 0, 0).stock.stock.push_back(0); }},
        {"goods buy", [](sim::World& w) { city_of(w, 0, 0).goods[3].buy = true; }},
        // (a hash perturbation: the state need not satisfy check_invariants)
        {"goods list_index", [](sim::World& w) { city_of(w, 0, 0).goods[55].list_index = 0; }},
        {"build goods setting", [](sim::World& w) { city_of(w, 0, 0).build_goods[4].setting = 1; }},
        {"build goods spent", [](sim::World& w) { city_of(w, 0, 0).build_goods[0].spent = 1; }},
        {"trade entry added",
         [](sim::World& w) { city_of(w, 0, 0).trade_lists[1].push_back(sim::TradeEntry{}); }},
        {"trade entry goods",
         [](sim::World& w) {
             city_of(w, 0, 0).trade_lists[0].push_back(sim::TradeEntry{});
             city_of(w, 0, 0).trade_lists[0][0].goods = 5;
         }},
        {"trade entry amount",
         [](sim::World& w) {
             city_of(w, 0, 0).trade_lists[0].push_back(sim::TradeEntry{});
             city_of(w, 0, 0).trade_lists[0][0].amount_88 = 5;
         }},
        {"trade entry price",
         [](sim::World& w) {
             city_of(w, 0, 0).trade_lists[0].push_back(sim::TradeEntry{});
             city_of(w, 0, 0).trade_lists[0][0].price = 5;
         }},
        {"trade entries in two cities",
         [](sim::World& w) {
             city_of(w, 0, 0).trade_lists[0].push_back(sim::TradeEntry{});
             city_of(w, 0, 1).trade_lists[0].push_back(sim::TradeEntry{});
         }},
        {"index stamp", [](sim::World& w) { city_of(w, 0, 0).index.stamp = 1; }},
        // ---- the lists
        {"list hint", [](sim::World& w) { city_of(w, 0, 0).index.lists[4].hint = 3; }},
        {"list cursor", [](sim::World& w) { city_of(w, 0, 0).index.lists[4].cursor = 3; }},
        {"list of another id",
         [](sim::World& w) {
             std::swap(city_of(w, 0, 0).index.lists[0], city_of(w, 0, 0).index.lists[1]);
         }},
        {"list grown by one hole",
         [](sim::World& w) { city_of(w, 0, 0).index.lists[1].slots.emplace_back(); }},
        {"building moved to another slot",
         [](sim::World& w) {
             auto& l = city_of(w, 0, 0).index.lists[4].slots;
             l[8] = std::move(l[0]);
             l[0].reset();
         }},
        {"building removed",
         [](sim::World& w) { city_of(w, 0, 0).index.lists[4].slots[5].reset(); }},
        // ---- the common part of a building
        {"building id", [](sim::World& w) { bld(w, 0, 4, 0).id = sim::BuildingId{99}; }},
        {"building type", [](sim::World& w) { bld(w, 0, 4, 0).type = sim::TypeId{9002}; }},
        {"building x", [](sim::World& w) { bld(w, 0, 4, 0).x += 1; }},
        {"building y", [](sim::World& w) { bld(w, 0, 4, 0).y += 1; }},
        {"building rotation", [](sim::World& w) { bld(w, 0, 4, 0).rotation = 1; }},
        {"building byte0e", [](sim::World& w) { bld(w, 0, 4, 0).byte0e = 0x87; }},
        {"building byte0f", [](sim::World& w) { bld(w, 0, 4, 0).byte0f = 1; }},
        {"building payload alternative",
         [](sim::World& w) { bld(w, 0, 3, 0).payload = sim::Farm{}; }},
        // ---- Inert (the house)
        {"inert last", [](sim::World& w) { pay<sim::Inert>(w, 3, 0).last = 1; }},
        {"inert blocked", [](sim::World& w) { pay<sim::Inert>(w, 3, 0).blocked = true; }},
        {"inert suspended", [](sim::World& w) { pay<sim::Inert>(w, 3, 0).suspended = true; }},
        {"inert status code",
         [](sim::World& w) { pay<sim::Inert>(w, 3, 0).status.code = sim::Status::StorageFull; }},
        {"inert status icons", [](sim::World& w) { pay<sim::Inert>(w, 3, 0).status.icons = 4; }},
        {"inert status time", [](sim::World& w) { pay<sim::Inert>(w, 3, 0).status.time = 4; }},
        // ---- Workshop (the weaver)
        {"workshop last", [](sim::World& w) { pay<sim::Workshop>(w, 4, 0).last = 1; }},
        {"workshop switched_on",
         [](sim::World& w) { pay<sim::Workshop>(w, 4, 0).switched_on = false; }},
        {"workshop suspended", [](sim::World& w) { pay<sim::Workshop>(w, 4, 0).suspended = true; }},
        {"workshop blocked", [](sim::World& w) { pay<sim::Workshop>(w, 4, 0).blocked = true; }},
        {"workshop out", [](sim::World& w) { pay<sim::Workshop>(w, 4, 0).out[0] = 1; }},
        {"workshop in", [](sim::World& w) { pay<sim::Workshop>(w, 4, 0).in[0] += 1; }},
        {"workshop activity", [](sim::World& w) { pay<sim::Workshop>(w, 4, 0).activity[0] = 1; }},
        {"workshop carriers_out",
         [](sim::World& w) { pay<sim::Workshop>(w, 4, 0).carriers_out = 1; }},
        {"workshop status time",
         [](sim::World& w) { pay<sim::Workshop>(w, 4, 0).status.time = 1; }},
        // ---- WeaponsWorkshop (the bow maker)
        {"weapons last", [](sim::World& w) { pay<sim::WeaponsWorkshop>(w, 4, 4).last = 1; }},
        {"weapons line_on first",
         [](sim::World& w) { pay<sim::WeaponsWorkshop>(w, 4, 4).line_on[0] = false; }},
        {"weapons line_on second",
         [](sim::World& w) { pay<sim::WeaponsWorkshop>(w, 4, 4).line_on[1] = false; }},
        {"weapons suspended",
         [](sim::World& w) { pay<sim::WeaponsWorkshop>(w, 4, 4).suspended = true; }},
        {"weapons blocked",
         [](sim::World& w) { pay<sim::WeaponsWorkshop>(w, 4, 4).blocked = true; }},
        {"weapons out", [](sim::World& w) { pay<sim::WeaponsWorkshop>(w, 4, 4).out[1] = 1; }},
        {"weapons in", [](sim::World& w) { pay<sim::WeaponsWorkshop>(w, 4, 4).in[1] = 1; }},
        {"weapons activity",
         [](sim::World& w) { pay<sim::WeaponsWorkshop>(w, 4, 4).activity[1] = 1; }},
        {"weapons carriers_out",
         [](sim::World& w) { pay<sim::WeaponsWorkshop>(w, 4, 4).carriers_out = 1; }},
        {"weapons status icons",
         [](sim::World& w) { pay<sim::WeaponsWorkshop>(w, 4, 4).status.icons = 1; }},
        // ---- Farm (the grain farm and the fur trapper)
        {"farm last", [](sim::World& w) { pay<sim::Farm>(w, 4, 1).last = 1; }},
        {"farm switched_on", [](sim::World& w) { pay<sim::Farm>(w, 4, 1).switched_on = false; }},
        {"farm suspended", [](sim::World& w) { pay<sim::Farm>(w, 4, 1).suspended = true; }},
        {"farm blocked", [](sim::World& w) { pay<sim::Farm>(w, 4, 1).blocked = true; }},
        {"farm out", [](sim::World& w) { pay<sim::Farm>(w, 4, 1).out = 1; }},
        {"farm raw", [](sim::World& w) { pay<sim::Farm>(w, 4, 1).raw = 1; }},
        {"farm activity", [](sim::World& w) { pay<sim::Farm>(w, 4, 1).activity = 1; }},
        {"farm workers_out", [](sim::World& w) { pay<sim::Farm>(w, 4, 1).workers_out = 1; }},
        {"farm status code",
         [](sim::World& w) { pay<sim::Farm>(w, 4, 1).status.code = sim::Status::MissingInput; }},
        {"fur trapper workers_out", [](sim::World& w) { pay<sim::Farm>(w, 4, 2).workers_out = 1; }},
        // ---- SmallFarm
        {"small farm last", [](sim::World& w) { pay<sim::SmallFarm>(w, 4, 3).last = 1; }},
        {"small farm line_on first",
         [](sim::World& w) { pay<sim::SmallFarm>(w, 4, 3).line_on[0] = false; }},
        {"small farm line_on second",
         [](sim::World& w) { pay<sim::SmallFarm>(w, 4, 3).line_on[1] = false; }},
        {"small farm suspended",
         [](sim::World& w) { pay<sim::SmallFarm>(w, 4, 3).suspended = true; }},
        {"small farm blocked", [](sim::World& w) { pay<sim::SmallFarm>(w, 4, 3).blocked = true; }},
        {"small farm out", [](sim::World& w) { pay<sim::SmallFarm>(w, 4, 3).out[1] = 1; }},
        {"small farm raw", [](sim::World& w) { pay<sim::SmallFarm>(w, 4, 3).raw[1] = 1; }},
        {"small farm activity",
         [](sim::World& w) { pay<sim::SmallFarm>(w, 4, 3).activity[1] = 1; }},
        {"small farm figures_out first",
         [](sim::World& w) { pay<sim::SmallFarm>(w, 4, 3).figures_out[0] = 1; }},
        {"small farm figures_out last",
         [](sim::World& w) { pay<sim::SmallFarm>(w, 4, 3).figures_out[4] = 1; }},
        {"small farm status time",
         [](sim::World& w) { pay<sim::SmallFarm>(w, 4, 3).status.time = 1; }},
        // ---- Mine
        {"mine last", [](sim::World& w) { pay<sim::Mine>(w, 4, 5).last = 1; }},
        {"mine switched_on", [](sim::World& w) { pay<sim::Mine>(w, 4, 5).switched_on = false; }},
        {"mine deep", [](sim::World& w) { pay<sim::Mine>(w, 4, 5).deep = true; }},
        {"mine blocked", [](sim::World& w) { pay<sim::Mine>(w, 4, 5).blocked = true; }},
        {"mine out", [](sim::World& w) { pay<sim::Mine>(w, 4, 5).out = 1; }},
        {"mine activity", [](sim::World& w) { pay<sim::Mine>(w, 4, 5).activity = 1; }},
        {"mine deposit removed", [](sim::World& w) { pay<sim::Mine>(w, 4, 5).deposit.reset(); }},
        {"mine deposit index", [](sim::World& w) { pay<sim::Mine>(w, 4, 5).deposit = 1; }},
        {"mine status icons", [](sim::World& w) { pay<sim::Mine>(w, 4, 5).status.icons = 1; }},
        // ---- the island's serial counter follows the buildings
        {"island next_serial", [](sim::World& w) { isl(w, 0).next_serial += 1; }},
    };
}

} // namespace

TEST_CASE("perturbation moves the hash for every field of a city and a building") {
    const Kit k = make_kit();
    const sim::World base = all_classes_world(k);
    REQUIRE(base.check_invariants() == "");
    // The mutations assume this layout: the iron mine found deposit 0.
    REQUIRE(std::get<sim::Mine>(base.islands()[0]->cities[0]->index.lists[4].slots[5]->payload)
                .deposit == std::optional<std::size_t>{0});
    const std::uint64_t base_hash = base.state_hash();
    sim::HashTree base_tree;
    static_cast<void>(base.state_hash(base_tree, 0));
    std::vector<std::uint64_t> seen;
    for (const Mutation& m : stream_mutations()) {
        INFO("mutation " << m.first);
        sim::World w = base;
        m.second(w);
        const std::uint64_t h = w.state_hash();
        CHECK(h != base_hash);
        // Only the islands scope moves.
        sim::HashTree tree;
        static_cast<void>(w.state_hash(tree, 0));
        REQUIRE(tree.root.children.size() == base_tree.root.children.size());
        for (std::size_t i = 0; i < tree.root.children.size(); ++i) {
            if (tree.root.children[i].name == "islands") {
                CHECK(tree.root.children[i].hash != base_tree.root.children[i].hash);
            } else {
                CHECK(tree.root.children[i].hash == base_tree.root.children[i].hash);
            }
        }
        seen.push_back(h);
    }
    // No two different single-field changes collide (a hash that ignores which field
    // changed would show here).
    std::vector<std::uint64_t> sorted = seen;
    std::sort(sorted.begin(), sorted.end());
    CHECK(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());
    CHECK(seen.size() == stream_mutations().size());
    CHECK(seen.size() >= 87);
}

TEST_CASE("derived fields are not part of the hash or the save") {
    const Kit k = make_kit();
    const sim::World base = all_classes_world(k);
    const std::uint64_t base_hash = base.state_hash();
    const std::vector<std::uint8_t> base_save = base.save();
    using Edit = void (*)(sim::World&);
    const std::vector<std::pair<const char*, Edit>> edits = {
        {"service counters", [](sim::World& w) { city_of(w, 0, 0).index.service_counters[3] = 9; }},
        {"house counts", [](sim::World& w) { city_of(w, 0, 0).index.house_counts[4] = 9; }},
        {"id map", [](sim::World& w) { isl(w, 0).cache.by_id.clear(); }},
        {"grids", [](sim::World& w) { isl(w, 0).cache.grids.clear(); }},
        {"shows_icons",
         [](sim::World& w) { pay<sim::Workshop>(w, 4, 0).status.shows_icons = false; }},
        {"unlocked", [](sim::World& w) { pay<sim::WeaponsWorkshop>(w, 4, 4).unlocked.clear(); }},
    };
    for (const auto& [name, edit] : edits) {
        INFO("edit " << name);
        sim::World w = base;
        edit(w);
        CHECK(w.state_hash() == base_hash);
        CHECK(w.save() == base_save);
    }
}

TEST_CASE("the hash tree names the island city list and building") {
    const Kit k = make_kit();
    sim::World w = all_classes_world(k);
    sim::HashTree tree;
    CHECK(w.state_hash(tree, 8) == w.state_hash());
    auto child = [](const sim::HashNode& n, const char* name, std::size_t ordinal) {
        for (const sim::HashNode& c : n.children) {
            if (c.name == name && (!c.indexed || c.index == ordinal)) {
                return &c;
            }
        }
        return static_cast<const sim::HashNode*>(nullptr);
    };
    const sim::HashNode* islands = child(tree.root, "islands", 0);
    REQUIRE(islands != nullptr);
    const sim::HashNode* island = child(*islands, "island", 0);
    REQUIRE(island != nullptr);
    const sim::HashNode* cities = child(*island, "cities", 0);
    REQUIRE(cities != nullptr);
    CHECK(cities->children.size() == 16);
    const sim::HashNode* city = child(*cities, "city", 0);
    REQUIRE(city != nullptr);
    const sim::HashNode* index = child(*city, "index", 0);
    REQUIRE(index != nullptr);
    REQUIRE(index->children.size() == 6);
    for (std::size_t l = 0; l < 6; ++l) {
        CHECK(index->children[l].name == "list");
        CHECK(index->children[l].indexed);
        CHECK(index->children[l].index == l);
    }
    const sim::HashNode& production = index->children[4];
    CHECK(production.children.size() == city_of(w, 0, 0).index.lists[4].slots.size());
    for (std::size_t s = 0; s < production.children.size(); ++s) {
        CHECK(production.children[s].name == "building");
        CHECK(production.children[s].index == s);
    }
    // The absent city slots and the empty building slots hash as the byte 0.
    sim::StateHasher absent;
    absent.u8(0);
    CHECK(child(*cities, "city", 5)->hash == absent.value());
    CHECK(production.children[9].hash == absent.value());
    // A change inside one building moves exactly its building scope, its list, the
    // index, the city, the cities scope, the island and the table.
    pay<sim::Workshop>(w, 4, 0).carriers_out = 1;
    sim::HashTree after;
    static_cast<void>(w.state_hash(after, 8));
    const sim::HashNode* a_islands = child(after.root, "islands", 0);
    const sim::HashNode* a_island = child(*a_islands, "island", 0);
    const sim::HashNode* a_cities = child(*a_island, "cities", 0);
    const sim::HashNode* a_city = child(*a_cities, "city", 0);
    const sim::HashNode* a_index = child(*a_city, "index", 0);
    CHECK(a_islands->hash != islands->hash);
    CHECK(a_island->hash != island->hash);
    CHECK(a_cities->hash != cities->hash);
    CHECK(a_city->hash != city->hash);
    CHECK(a_index->hash != index->hash);
    CHECK(a_index->children[4].hash != production.hash);
    CHECK(a_index->children[4].children[0].hash != production.children[0].hash);
    CHECK(a_index->children[4].children[1].hash == production.children[1].hash);
    for (const std::size_t l : {0U, 1U, 2U, 3U, 5U}) {
        CHECK(a_index->children[l].hash == index->children[l].hash);
    }
    CHECK(child(*a_cities, "city", 1)->hash == child(*cities, "city", 1)->hash);
    CHECK(child(*a_island, "tiles", 0)->hash == child(*island, "tiles", 0)->hash);
    // At depth 3 the building scopes are not recorded.
    sim::HashTree shallow;
    static_cast<void>(w.state_hash(shallow, 3));
    const sim::HashNode* s_city = child(
        *child(*child(*child(shallow.root, "islands", 0), "island", 0), "cities", 0), "city", 0);
    REQUIRE(s_city != nullptr);
    CHECK(s_city->children.empty());
}

TEST_CASE("save load save with buildings") {
    const Kit k = make_kit();
    sim::World w = all_classes_world(k);
    // History: holes in a list, a removed market building, a grown list.
    const auto key = [&](sim::ListId list, sim::Slot slot) {
        return sim::BuildingKey{{sim::IslandId{0}, sim::CityNo{0}}, list, slot};
    };
    REQUIRE(sim::WorldTestAccess::remove(w, key(4, 1)));
    REQUIRE(sim::WorldTestAccess::remove(w, key(5, 0)));
    for (int i = 0; i < 12; ++i) {
        REQUIRE(
            sim::WorldTestAccess::create(w, 0, 0, record(k.weaver, 101 + 2 * i, 130), 777).ok());
    }
    w.advance(10);
    // The scheduler moves the cursor and the shared stamp of a visited city (stage
    // S7), so the values to be saved are set after the advance.
    city_of(w, 0, 0).index.lists[4].cursor = 7;
    city_of(w, 0, 0).index.stamp = 123;
    REQUIRE(w.check_invariants() == "");
    const std::vector<std::uint8_t> bytes = w.save();
    sim::World back = load_world(k, bytes);
    CHECK(back.state_hash() == w.state_hash());
    CHECK(back.save() == bytes);
    CHECK(back.check_invariants() == "");
    // The derived indexes were rebuilt: equal to the live world's.
    CHECK(back.islands()[0]->cache.by_id == w.islands()[0]->cache.by_id);
    CHECK(back.islands()[0]->cache.grids.size() >= 6);
    for (std::size_t l = 0; l < 6; ++l) {
        const auto& have = back.islands()[0]->cache.grids[l];
        const auto& want = l < w.islands()[0]->cache.grids.size() ? w.islands()[0]->cache.grids[l]
                                                                  : std::vector<std::uint32_t>{};
        if (!have.empty() && !want.empty()) {
            CHECK(have == want);
        }
    }
    CHECK(city_of(back, 0, 0).index.service_counters == city_of(w, 0, 0).index.service_counters);
    CHECK(city_of(back, 0, 0).index.house_counts == city_of(w, 0, 0).index.house_counts);
    // The lists came back slot for slot, with the hint and the cursor.
    for (std::size_t l = 0; l < 6; ++l) {
        const sim::BuildingList& a = city_of(w, 0, 0).index.lists[l];
        const sim::BuildingList& b = city_of(back, 0, 0).index.lists[l];
        CHECK(a.hint == b.hint);
        CHECK(a.cursor == b.cursor);
        REQUIRE(a.slots.size() == b.slots.size());
        for (std::size_t s = 0; s < a.slots.size(); ++s) {
            CHECK(a.slots[s].has_value() == b.slots[s].has_value());
        }
    }
    CHECK(city_of(back, 0, 0).index.lists[4].cursor == 7);
    CHECK(city_of(back, 0, 0).index.stamp == 123);
    // Queries answer the same on the loaded world.
    const sim::CityRef c0{sim::IslandId{0}, sim::CityNo{0}};
    for (int y = 100; y < 135; ++y) {
        for (int x = 100; x < 135; ++x) {
            const sim::Building* a = w.building_at(c0, x, y);
            const sim::Building* b = back.building_at(c0, x, y);
            REQUIRE((a == nullptr) == (b == nullptr));
            if (a != nullptr) {
                CHECK(a->id == b->id);
            }
        }
    }
    // The flat stream (the hash's own) reads back to the same hash, derived indexes
    // included, and the two worlds go on in step.
    sim::World flat = make_world(k, syn::empty_scenario());
    CHECK(island_test::read_flat(flat, island_test::flat_stream(w)) == "");
    CHECK(flat.state_hash() == w.state_hash());
    CHECK(flat.check_invariants() == "");
    for (int i = 0; i < 100; ++i) {
        w.advance(10);
        back.advance(10);
    }
    CHECK(back.state_hash() == w.state_hash());
    // A building created after the load gets the next serial of the same counter.
    auto a = sim::WorldTestAccess::create(w, 0, 0, record(k.weaver, 101, 150));
    auto b = sim::WorldTestAccess::create(back, 0, 0, record(k.weaver, 101, 150));
    REQUIRE(a.ok());
    REQUIRE(b.ok());
    CHECK(a.value() == b.value());
    CHECK(back.state_hash() == w.state_hash());
}

TEST_CASE("check_invariants detects corrupted counters and indexes") {
    const Kit k = make_kit();
    const sim::World good = all_classes_world(k);
    REQUIRE(good.check_invariants() == "");
    struct Row {
        const char* name;
        void (*mutate)(sim::World&);
        const char* expect;
    };
    const Row rows[] = {
        {"service counter raised",
         [](sim::World& w) { city_of(w, 0, 0).index.service_counters[4] += 1; },
         "cities[0].index.service_counters: differ from a recomputation"},
        {"service counter of another city",
         [](sim::World& w) { city_of(w, 0, 1).index.service_counters[0] += 1; },
         "cities[1].index.service_counters: differ from a recomputation"},
        {"house count raised", [](sim::World& w) { city_of(w, 0, 0).index.house_counts[0] += 1; },
         "cities[0].index.house_counts: differ from a recomputation"},
        {"house count lowered", [](sim::World& w) { city_of(w, 0, 0).index.house_counts[1] = 0; },
         "cities[0].index.house_counts: differ from a recomputation"},
        {"counter vector of the wrong size",
         [](sim::World& w) { city_of(w, 0, 0).index.service_counters.pop_back(); },
         "service_counters: differ from a recomputation"},
        {"id map without an entry",
         [](sim::World& w) { isl(w, 0).cache.by_id.erase(isl(w, 0).cache.by_id.begin()); },
         "cache.by_id"},
        {"id map with an invented entry",
         [](sim::World& w) {
             isl(w, 0).cache.by_id[sim::make_building_id(sim::IslandId{0}, 500)] =
                 sim::BuildingKey{{sim::IslandId{0}, sim::CityNo{0}}, 4, 0};
         },
         "cache.by_id"},
        {"id map with a wrong place",
         [](sim::World& w) { isl(w, 0).cache.by_id.begin()->second.slot += 1; }, "cache.by_id"},
        {"footprint grid cell cleared",
         [](sim::World& w) {
             const std::uint32_t at = isl(w, 0).tile_index(101, 101);
             isl(w, 0).cache.grids[4][at] = 0;
         },
         "cache.grids[4]: differs from a recomputation"},
        {"footprint grid cell of the wrong serial",
         [](sim::World& w) {
             const std::uint32_t at = isl(w, 0).tile_index(101, 101);
             isl(w, 0).cache.grids[4][at] = 77;
         },
         "cache.grids[4]: differs from a recomputation"},
        {"footprint grid cell in the wrong list",
         [](sim::World& w) {
             const std::uint32_t at = isl(w, 0).tile_index(101, 101);
             isl(w, 0).cache.grids[3].assign(isl(w, 0).tiles.tile_count(), 0);
             isl(w, 0).cache.grids[3][at] = 1;
         },
         "cache.grids[3]: differs from a recomputation"},
        {"two buildings with one id",
         [](sim::World& w) { bld(w, 0, 4, 1).id = bld(w, 0, 4, 0).id; },
         "two buildings share an id"},
    };
    for (const Row& row : rows) {
        INFO("row " << row.name);
        sim::World w = good;
        row.mutate(w);
        const std::string err = w.check_invariants();
        CHECK(contains(err, row.expect));
        CHECK(contains(err, "islands[0]."));
    }
}

namespace {

/// A deterministic LCG for the property tests (the seeds are in the test).
struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<std::uint32_t>(s >> 33);
    }
    std::uint32_t below(std::uint32_t n) { return next() % n; }
};

/// One random operation on island 0 of `w`: create a random building or remove a
/// random one. `junk` is called between the steps of the operation.
template <class Junk>
void random_op(sim::World& w, const Kit& k, Lcg& rng, std::vector<sim::BuildingKey>& live,
               Junk&& junk) {
    const sim::TypeId types[] = {k.weaver,        k.farm,      k.small_farm, k.bow_maker,
                                 k.pioneer_house, k.kontor,    k.church,     k.market_stall,
                                 k.barracks,      k.big_house, k.tall_house, k.iron_mine};
    if (!live.empty() && rng.below(100) < 35) {
        const std::size_t i = rng.below(static_cast<std::uint32_t>(live.size()));
        junk();
        REQUIRE(sim::WorldTestAccess::remove(w, live[i]));
        live.erase(live.begin() + static_cast<std::ptrdiff_t>(i));
        return;
    }
    const sim::TypeId t = types[rng.below(12)];
    const std::uint32_t city = rng.below(2);
    const int x = 100 + static_cast<int>(rng.below(64));
    const int y = 100 + static_cast<int>(rng.below(64));
    const auto rot = static_cast<std::uint8_t>(rng.below(4));
    junk();
    auto r = sim::WorldTestAccess::create(w, 0, city, record(t, x, y, rot), rng.next());
    if (r.ok()) {
        live.push_back(r.value());
    }
}

} // namespace

TEST_CASE("allocation pattern independence") {
    // The same operations twice in one process with junk allocations of random size
    // and lifetime interleaved, so that containers land at different addresses: the
    // result must not depend on where anything lives (a pointer-ordered container
    // or an address-dependent tie-break would show here).
    const Kit k = make_kit();
    auto run = [&](bool with_junk) {
        sim::World w = make_world(k, kit_scenario());
        Lcg rng{12345};
        Lcg junk_rng{999};
        std::vector<std::unique_ptr<std::vector<char>>> junk;
        auto junk_step = [&] {
            if (!with_junk) {
                return;
            }
            for (int n = static_cast<int>(junk_rng.below(5)); n > 0; --n) {
                junk.push_back(std::make_unique<std::vector<char>>(junk_rng.below(5000) + 1));
            }
            if (!junk.empty() && junk_rng.below(3) == 0) {
                junk.erase(junk.begin() + static_cast<std::ptrdiff_t>(junk_rng.below(
                                              static_cast<std::uint32_t>(junk.size()))));
            }
        };
        std::vector<sim::BuildingKey> live;
        for (int i = 0; i < 400; ++i) {
            random_op(w, k, rng, live, junk_step);
            if (i % 50 == 0) {
                junk_step();
                w.advance(10);
            }
        }
        return std::make_pair(w.state_hash(), w.save());
    };
    const auto plain = run(false);
    const auto junked = run(true);
    CHECK(plain.first == junked.first);
    CHECK(plain.second == junked.second);
}

TEST_CASE("random create and remove follow the list model and keep the indexes") {
    // The reference is the original's insert loop, literally (docs/sim/state.md §5.2):
    // the scan starts at hint + 1, wraps, the first null wins; a full list grows to
    // n + n / 4 but at least 10 and the new element takes slot n. After every
    // operation the world is checked against it, and check_invariants (which rebuilds
    // the id map, the footprint grids and the counters from scratch) is empty.
    const Kit k = make_kit();
    for (const bool lowest : {false, true}) {
        INFO("lowest free " << lowest);
        sim::Rules rules;
        if (lowest) {
            rules.slot_policy = sim::SlotPolicy::LowestFree;
        }
        sim::World w = make_world(k, kit_scenario(), {}, rules);
        struct Model {
            std::vector<int> used{0};
            long hint = -1;
        };
        std::vector<std::vector<Model>> model(2, std::vector<Model>(6));
        Lcg rng{2024};
        // The buildings that exist, with the record they were created from: the model of
        // the footprint tiles' object (the type id while the building stands, 0 after
        // its removal, the scenario's own value elsewhere) needs the rotated footprint.
        struct Placed {
            sim::BuildingKey key;
            sim::TypeId type;
            int x, y;
            std::uint8_t rotation;
        };
        std::vector<Placed> live;
        const sim::TileLayer& layer = isl(w, 0).tiles;
        std::vector<std::uint32_t> objects(static_cast<std::size_t>(layer.tile_count()));
        for (std::size_t i = 0; i < objects.size(); ++i) {
            objects[i] = layer.object(i);
        }
        auto set_footprint = [&](const Placed& p, std::uint32_t value) {
            const auto [fw, fh] = sim::footprint(*k.registry->find(p.type), p.rotation);
            for (std::uint32_t dy = 0; dy < fh; ++dy) {
                for (std::uint32_t dx = 0; dx < fw; ++dx) {
                    const auto tile =
                        layer.index_of(p.x + static_cast<int>(dx), p.y + static_cast<int>(dy));
                    REQUIRE(tile != sim::kNoSlot);
                    objects[tile] = value;
                }
            }
        };
        // Non-square types and every rotation: a removal that used the unturned footprint
        // would leave stale tiles, grid cells and objects behind.
        const sim::TypeId types[] = {k.weaver,   k.farm,      k.pioneer_house,
                                     k.kontor,   k.church,    k.market_stall,
                                     k.barracks, k.big_house, k.tall_house};
        for (int step = 0; step < 600; ++step) {
            if (!live.empty() && rng.below(100) < 40) {
                const std::size_t i = rng.below(static_cast<std::uint32_t>(live.size()));
                const Placed gone = live[i];
                const sim::BuildingKey key = gone.key;
                REQUIRE(sim::WorldTestAccess::remove(w, key));
                set_footprint(gone, 0);
                model[key.city.city.v][key.list].used[key.slot] = 0; // hint untouched
                live.erase(live.begin() + static_cast<std::ptrdiff_t>(i));
            } else {
                const sim::TypeId t = types[rng.below(9)];
                const std::uint32_t city = rng.below(2);
                const int x = 100 + static_cast<int>(rng.below(64));
                const int y = 100 + static_cast<int>(rng.below(64));
                const auto rotation = static_cast<std::uint8_t>(rng.below(4));
                auto r = sim::WorldTestAccess::create(w, 0, city, record(t, x, y, rotation), 0);
                if (!r.ok()) {
                    continue; // the footprint was taken: nothing changed
                }
                const sim::BuildingKey key = r.value();
                Model& m = model[city][key.list];
                const long n = static_cast<long>(m.used.size());
                long slot = n;
                if (lowest) {
                    for (long i = 0; i < n; ++i) {
                        if (m.used[static_cast<std::size_t>(i)] == 0) {
                            slot = i;
                            break;
                        }
                    }
                } else {
                    for (long i = 0; i < n; ++i) {
                        const long idx = (m.hint + 1 + i) % n;
                        if (m.used[static_cast<std::size_t>(idx)] == 0) {
                            slot = idx;
                            break;
                        }
                    }
                }
                if (slot == n) {
                    long grown = n + n / 4;
                    if (grown < 10) {
                        grown = 10;
                    }
                    m.used.resize(static_cast<std::size_t>(grown), 0);
                }
                m.used[static_cast<std::size_t>(slot)] = 1;
                m.hint = slot;
                REQUIRE(key.slot == static_cast<sim::Slot>(slot));
                const Placed placed{key, t, x, y, rotation};
                set_footprint(placed, t.v);
                live.push_back(placed);
            }
            // The footprint tiles carry exactly what the model says.
            std::vector<std::uint32_t> actual(objects.size());
            for (std::size_t i = 0; i < actual.size(); ++i) {
                actual[i] = layer.object(i);
            }
            REQUIRE(actual == objects);
            // The world agrees with the model about every list of both cities.
            for (std::uint32_t c = 0; c < 2; ++c) {
                for (std::uint32_t l = 0; l < 6; ++l) {
                    const sim::BuildingList& list = city_of(w, 0, c).index.lists[l];
                    const Model& m = model[c][l];
                    REQUIRE(list.slots.size() == m.used.size());
                    REQUIRE(list.hint == m.hint);
                    for (std::size_t s = 0; s < m.used.size(); ++s) {
                        REQUIRE(list.slots[s].has_value() == (m.used[s] != 0));
                    }
                }
            }
            const std::string problem = w.check_invariants();
            REQUIRE_MESSAGE(problem.empty(), problem);
        }
        CHECK(live.size() > 20);
    }
}

TEST_CASE("golden s6 G1 city with a weaver and a farm") {
    auto types = syn::make_synthetic_types();
    REQUIRE(types.ok());
    // The one literal of tools/sim_stream.py for this golden.
    CHECK(types.value().registry->digest() == 0x00d140f84bf28924ULL);
    auto created =
        sim::World::create(syn::g1_scenario(types.value(), 1234), types.value().registry);
    REQUIRE_MESSAGE(created.ok(), (created.ok() ? std::string() : created.error()));
    sim::World w = std::move(created).value();
    int done = 0;
    for (const int checkpoint : {0, 100}) {
        while (done < checkpoint) {
            w.advance(10);
            ++done;
        }
        const std::string name =
            checkpoint == 0 ? "s6_g1_create" : "s6_g1_step" + std::to_string(checkpoint);
        std::string update_line;
        const std::string problem =
            anno::test::check_golden(name, "S6", w.state_hash(), update_line);
        if (anno::test::update_mode()) {
            MESSAGE("golden line: " << update_line);
        }
        INFO(name);
        CHECK(problem == "");
    }
    // The semantic side of the golden, whatever the hash says: the city holds the two
    // buildings at the slots and with the stocks the scenario gave them.
    const sim::City& city = *w.islands()[0]->cities[0];
    CHECK(city.index.lists[4].slots.size() == 10);
    CHECK(city.index.lists[4].hint == 1);
    CHECK(city.index.lists[4].live() == 2);
    CHECK(w.check_invariants() == "");
    CHECK(w.clock().ms == 1000);
}

TEST_CASE("city stream layout") {
    // number, owner and the rest of an empty city in the order of docs/sim/state.md 9.3,
    // with a number and an owner that differ (the scenario cities have number == owner).
    const sim::SimConfig cfg;
    const sim::City city = sim::make_city(sim::CityNo{3}, sim::PlayerId{7}, cfg);
    island_test::Stream want;
    island_test::append_empty_city(want, 3, 7);
    sim::WriteArchive wa;
    fields(wa, std::as_const(city));
    CHECK(wa.data() == want.v);
    sim::TraceArchive trace;
    fields(trace, std::as_const(city));
    CHECK(trace.bytes_written() == want.v);
    sim::City back;
    sim::ReadArchive r(want.v);
    fields(r, back);
    REQUIRE_MESSAGE(!r.failed(), r.error());
    CHECK(r.remaining() == 0);
    CHECK(back.number.v == 3);
    CHECK(back.owner.v == 7);
    // Every strict prefix fails.
    for (std::size_t n = 0; n < want.v.size(); n += 7) {
        sim::City partial;
        sim::ReadArchive pr(std::span<const std::uint8_t>(want.v.data(), n));
        fields(pr, partial);
        REQUIRE(pr.failed());
    }
}
