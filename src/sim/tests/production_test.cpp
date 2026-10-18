// Production rules against the fixtures of docs/re/production.md §9 (F1–F8,
// F10). F1 needs ANNO1503_GAME_DIR (Anlagen.dat); the others use the type
// values F1 names, so they run everywhere.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>

#include <doctest/doctest.h>

#include <anno/formats/anlagen.hpp>
#include <anno/sim/production.hpp>

namespace sim = anno::sim;

namespace {

/// A HANDWERK type with the given per-step values (as F1 lists them).
sim::ProductionType workshop_type(std::uint32_t rate, std::vector<std::uint32_t> need,
                                  std::uint32_t cap_tons, std::uint32_t max_figures = 1) {
    sim::ProductionType t;
    t.kind = sim::BuildingClass::Workshop;
    t.outputs = {"OUT"};
    for (std::size_t i = 0; i < need.size(); ++i) {
        t.inputs.push_back("IN" + std::to_string(i));
    }
    t.output_per_step = {rate, 0};
    need.resize(2, 0);
    t.input_per_step = need;
    t.capacity = {cap_tons << 8, 0};
    t.max_figures = {max_figures, 0, 0};
    return t;
}

/// Runs the workshop for one step (the next multiple of 500 ticks).
sim::StepResult step(sim::Workshop& w, const sim::ProductionType& t, std::uint32_t& now) {
    now += sim::kStepTicks;
    return sim::workshop_step(w, t, now);
}

} // namespace

TEST_CASE("F2: weaver's hut with 5 t of wool") {
    const auto weaver = workshop_type(6, {11}, 5); // 1001: 6 / 11, cap 1280
    auto w = sim::make_workshop(weaver);
    w.in = {1280};
    std::uint32_t now = 0;
    for (int i = 0; i < 116; ++i) {
        const auto r = step(w, weaver, now);
        REQUIRE(r.stepped);
        CHECK(r.made[0] == 6);
    }
    CHECK(w.out[0] == 696);
    CHECK(w.in[0] == 4);
    const auto r = step(w, weaver, now);
    CHECK(r.made[0] == 0);
    CHECK(r.status == sim::Status::MissingInput);
    // A message is only due after the condition has lasted 45,000 ticks.
    CHECK_FALSE(r.message);
    CHECK(r.blocked_changed);
    CHECK(w.blocked);
}

TEST_CASE("F3: proportional production from less than one step of input") {
    const auto weaver = workshop_type(6, {11}, 5);
    auto w = sim::make_workshop(weaver);
    w.in = {8};
    std::uint32_t now = 0;
    auto r = step(w, weaver, now); // ratio (8 << 7) / 11 = 93
    CHECK(r.made[0] == 4);
    CHECK(w.in[0] == 1);
    r = step(w, weaver, now);
    CHECK(r.status == sim::Status::MissingInput);
}

TEST_CASE("F4: the output exceeds its capacity by up to one step") {
    const auto weaver = workshop_type(6, {11}, 5);
    auto w = sim::make_workshop(weaver);
    w.in = {50000};
    w.out = {1279};
    std::uint32_t now = 0;
    step(w, weaver, now);
    CHECK(w.out[0] == 1285);
    const auto r = step(w, weaver, now);
    CHECK(r.status == sim::Status::StorageFull);
    CHECK(w.out[0] == 1285);
}

TEST_CASE("F5: ore smelter limited by its second input") {
    const auto smelter = workshop_type(17, {21, 13}, 5); // 1017: 17 / 21 / 13
    auto w = sim::make_workshop(smelter);
    w.in = {1280, 25};
    std::uint32_t now = 0;
    auto r = step(w, smelter, now);
    CHECK(r.made[0] == 17);
    CHECK(w.in == std::vector<std::uint32_t>{1259, 12});
    r = step(w, smelter, now); // ratio (12 << 7) / 13 = 118
    CHECK(r.made[0] == 15);
    CHECK(w.in == std::vector<std::uint32_t>{1240, 1});
    r = step(w, smelter, now);
    CHECK(r.status == sim::Status::MissingInput);
}

TEST_CASE("F6: grain farm with one worker at home") {
    sim::ProductionType farm; // 811: 37 / 80, max workers 1
    farm.kind = sim::BuildingClass::Farm;
    farm.output_per_step = {37, 0};
    farm.input_per_step = {80, 0};
    farm.capacity = {7 << 8, 0};
    farm.max_figures = {1, 0, 0};
    sim::Farm f;
    f.raw = 160;
    std::uint32_t now = 0;
    now += sim::kStepTicks;
    auto r = sim::farm_step(f, farm, now);
    CHECK(f.raw == 80);
    CHECK(f.out == 37);
    CHECK_FALSE(r.send_figure); // one worker, something was made
    now += sim::kStepTicks;
    sim::farm_step(f, farm, now);
    CHECK(f.raw == 0);
    CHECK(f.out == 74);
    now += sim::kStepTicks;
    r = sim::farm_step(f, farm, now);
    CHECK(r.status == sim::Status::MissingInput);
    CHECK(r.send_figure); // nothing made: the worker goes harvesting
    // While the single worker is out, the farm does not produce.
    f.raw = 160;
    f.workers_out = 1;
    now += sim::kStepTicks;
    r = sim::farm_step(f, farm, now);
    CHECK(r.made[0] == 0);
    CHECK(f.raw == 160);
}

TEST_CASE("F7: workshop activity average") {
    const auto weaver = workshop_type(6, {11}, 5);
    auto w = sim::make_workshop(weaver);
    w.in = {1000000};
    std::uint32_t now = 0, a = 0;
    for (int i = 0; i < 50; ++i) {
        step(w, weaver, now);
        a = (a * 127 >> 7) + 48;
        REQUIRE(w.activity[0] == a);
    }
}

TEST_CASE("step gate: one step per 500 ticks, the timer advances by exactly 500") {
    const auto weaver = workshop_type(6, {11}, 5);
    auto w = sim::make_workshop(weaver);
    w.in = {100000};
    CHECK_FALSE(sim::workshop_step(w, weaver, 499).stepped);
    CHECK(sim::workshop_step(w, weaver, 1234).stepped);
    CHECK(w.last == 500);
    // A late tick runs one step; the next follows 500 after the previous.
    CHECK(sim::workshop_step(w, weaver, 1234).stepped);
    CHECK(w.last == 1000);
    CHECK_FALSE(sim::workshop_step(w, weaver, 1234).stepped);
    // 16-bit wrap of the timer.
    w.last = 65400;
    CHECK(sim::workshop_step(w, weaver, 65536 + 400).stepped);
    CHECK(w.last == 364);
    // Switched off: the step passes without production.
    w.switched_on = false;
    const auto r = sim::workshop_step(w, weaver, 2000000);
    CHECK(r.stepped);
    CHECK(r.made.empty());
}

TEST_CASE("status messages: due after the interval, then on every report (0x10072ed0)") {
    sim::StatusTracker s;
    const std::uint32_t rep = sim::kRepeatWorkshop;
    // A new (higher) code starts the time; no message before `rep` ticks.
    CHECK_FALSE(s.report(sim::Status::MissingInput, 1000, rep));
    CHECK(s.code == sim::Status::MissingInput);
    CHECK_FALSE(s.report(sim::Status::MissingInput, 1000 + rep - 1, rep));
    CHECK(s.report(sim::Status::MissingInput, 1000 + rep, rep));
    CHECK(s.icons == 0b10);
    // The time becomes t − rep: due again at every later report.
    CHECK(s.report(sim::Status::MissingInput, 1000 + rep + 500, rep));
    // A lower code is ignored; a higher one restarts the time.
    CHECK_FALSE(s.report(sim::Status::None, 99999, rep));
    CHECK_FALSE(s.report(sim::Status::StorageFull, 50000, rep));
    CHECK(s.code == sim::Status::StorageFull);
    // The time is 16 bits: elapsed wraps (50000 + 45000 − 65536).
    CHECK(s.report(sim::Status::StorageFull, 50000 + rep, rep));
    // Production clears codes 1–3 only if one of their icons is shown.
    s.clear(0x0E);
    CHECK(s.code == sim::Status::None);
    CHECK(s.icons == 0);
    // The status time is 16 bits: 70,000 ticks later is 4,464 "elapsed".
    sim::StatusTracker wrap;
    wrap.report(sim::Status::MissingInput, 0, rep);
    CHECK_FALSE(wrap.report(sim::Status::MissingInput, 70000, rep));
    // Without the owner check, no icon is shown and the code is never cleared.
    sim::StatusTracker ai;
    ai.shows_icons = false;
    ai.report(sim::Status::MissingInput, 0, rep);
    CHECK(ai.report(sim::Status::MissingInput, rep, rep));
    ai.clear(0x0E);
    CHECK(ai.code == sim::Status::MissingInput);
    sim::StatusTracker quiet;
    quiet.report(sim::Status::MissingInput, 0, rep); // never raised: no icon
    quiet.clear(0x0E);
    CHECK(quiet.code == sim::Status::MissingInput);
}

TEST_CASE("F8: growing fields and forests") {
    sim::ProductionType field; // 712: P 15500, S 5, k 2
    field.kind = sim::BuildingClass::GrowingItem;
    field.growth_period = 15500;
    field.stages = 5;
    field.stage_shift = 2;
    sim::GrowthTile tile{712, 0};
    std::uint32_t changed_at = 0;
    for (std::uint32_t t = 0; t <= 200000; t += 2000) { // one chunk visit per 2000 ticks
        if (sim::grow(tile, field, t)) {
            changed_at = t;
            break;
        }
    }
    CHECK(tile.object == 713);
    // 20 parity flips of t / 7750: the 20th flip happens at t = 155,000.
    CHECK(changed_at == 156000);
    CHECK((tile.phase & 0x3F) == 0);
    // k = 0 (ROHSTDUERR-like, S = 5): advances after 5 flips; the parity bit
    // is cleared with the counter (andb $0x80, 0x1008b6ca).
    sim::ProductionType dry;
    dry.growth_period = 1000;
    dry.stages = 5;
    dry.stage_shift = 0;
    sim::GrowthTile d{714, 0x80};
    int flips = 0;
    for (std::uint32_t t = 500; flips < 10; t += 500) {
        ++flips;
        if (sim::grow(d, dry, t)) {
            break;
        }
    }
    CHECK(flips == 5);
    CHECK(d.object == 715);
    CHECK(d.phase == 0x80);
    // A type without a growth period never grows.
    sim::GrowthTile still{104, 0};
    CHECK_FALSE(sim::grow(still, sim::ProductionType{}, 999999));
}

TEST_CASE("F10: warehouse capacity") {
    sim::CityStock c;
    c.stock.assign(sim::kOriginalWarehouseGoods, 0);
    c.stock[9] = 12700; // wool
    CHECK(c.add(9, 300) == 100);
    CHECK(c.stock[9] == 12800);
    CHECK(c.add(sim::kOriginalWarehouseGoods, 5) == 0); // not a warehouse good
    // A stock above the capacity is lowered to it (u32 wrap of cap − stock).
    c.stock[3] = 13000;
    CHECK(c.add(3, 10) == static_cast<std::uint32_t>(12800 - 13000));
    CHECK(c.stock[3] == 12800);
    CHECK(c.remove(9, 20000) == 12800);
    CHECK(c.stock[9] == 0);
}

TEST_CASE("F1: production types of all Anlagen.dat entries") {
    const char* env = std::getenv("ANNO1503_GAME_DIR");
    if (env == nullptr) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::ifstream in(std::filesystem::path(env) / "Anlagen.dat", std::ios::binary);
    const std::vector<std::uint8_t> buf(std::istreambuf_iterator<char>(in), {});
    auto file = anno::formats::anlagen::AnlagenFile::parse(buf);
    REQUIRE(file);
    std::map<std::uint32_t, sim::ProductionType> types;
    for (const auto& e : file.value().entries) {
        auto t = sim::production_type(e);
        REQUIRE(t);
        if (t.value()) {
            types.emplace(e.id, *t.value());
        }
    }
    CHECK(types.size() == 921);
    auto rate = [&](std::uint32_t id) { return types.at(id).output_per_step; };
    auto need = [&](std::uint32_t id) { return types.at(id).input_per_step; };
    CHECK(rate(1001)[0] == 6);
    CHECK(need(1001) == std::vector<std::uint32_t>{11, 0});
    CHECK(types.at(1001).capacity[0] == 1280);
    CHECK(types.at(1001).inputs == std::vector<std::string>{"WOLLE"});
    CHECK(types.at(1001).outputs == std::vector<std::string>{"STOFFE"});
    CHECK(rate(1002)[0] == 13);
    CHECK(need(1002)[0] == 15);
    CHECK(rate(1004)[0] == 26);
    CHECK(need(1004)[0] == 25);
    CHECK(rate(1017)[0] == 17);
    CHECK(need(1017) == std::vector<std::uint32_t>{21, 13});
    CHECK(rate(1019)[0] == 9);
    CHECK(need(1019) == std::vector<std::uint32_t>{8, 2});
    CHECK(rate(811)[0] == 37);
    CHECK(need(811)[0] == 80);
    CHECK(types.at(811).max_figures[0] == 1);
    CHECK(rate(1205)[0] == 21);
    CHECK(types.at(712).growth_period == 15500);
    CHECK(types.at(712).display_rate0 == 49);
    CHECK(types.at(104).growth_period == 24000);
    CHECK(types.at(714).growth_period == 62000);
    // u16 period: ROHSTDUERR 706 computes 66,000, stored as 464 (+0x1fe).
    CHECK(types.at(706).growth_period == 464);
    CHECK(types.at(706).display_rate0 == 6620);
    CHECK(types.at(824).kind == sim::BuildingClass::FurTrapper);
    // The two entries whose integers would differ at x87 24-bit precision
    // (docs/sim/production.md): the 53/64-bit values.
    CHECK(need(1010)[1] == 6);
    CHECK(need(3979)[1] == 3);
    // Every workshop and farm input has a need > 0, so the guards against
    // division by zero never apply to the original data.
    for (const auto& [id, t] : types) {
        if (t.kind == sim::BuildingClass::Workshop) {
            for (std::size_t i = 0; i < t.inputs.size(); ++i) {
                CHECK_MESSAGE(t.input_per_step[i] != 0, "workshop " << id << " input " << i);
            }
        }
        if (t.kind == sim::BuildingClass::Farm) {
            CHECK_MESSAGE(t.output_per_step[0] != 0, "farm " << id);
            CHECK_MESSAGE(t.input_per_step[0] != 0, "farm " << id);
        }
    }
}
