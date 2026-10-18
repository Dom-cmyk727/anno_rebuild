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
#include <anno/test/game_dir.hpp>

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

/// The bow maker 1006 (HANDWERKWAFFEN: bows and crossbows from wood and
/// ropes) with the values F1 checks: rate 9 per line, needs 8 and 4, 5 t.
sim::ProductionType bow_maker() {
    sim::ProductionType t = workshop_type(9, {8, 4}, 5);
    t.kind = sim::BuildingClass::WeaponsWorkshop;
    t.outputs = {"BOGEN", "ARMBRUST"};
    t.output_per_step = {9, 9};
    t.capacity = {5 << 8, 5 << 8};
    return t;
}

sim::StepResult step(sim::WeaponsWorkshop& w, const sim::ProductionType& t, std::uint32_t& now) {
    now += sim::kStepTicks;
    return sim::weapons_workshop_step(w, t, now);
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
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::ifstream in(*dir / "Anlagen.dat", std::ios::binary);
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
    CHECK(rate(824)[0] == 26);
    CHECK(need(824)[0] == 25);
    CHECK(types.at(824).capacity[0] == 5U << 8);
    CHECK(types.at(809).kind == sim::BuildingClass::SmallFarm);
    CHECK(rate(809) == std::vector<std::uint32_t>{13, 4});
    CHECK(need(809) == std::vector<std::uint32_t>{128, 50});
    CHECK(types.at(809).capacity[0] == 4U << 8);
    CHECK(types.at(809).max_figures[0] == 3);
    // HANDWERKWAFFEN (0x10069550): bow maker, gunsmith, weapon smith. No
    // entry uses HANDWERKDUAL (0x10068a70).
    for (const std::uint32_t id : {1006U, 1012U, 1036U}) {
        CHECK(types.at(id).kind == sim::BuildingClass::WeaponsWorkshop);
        CHECK(types.at(id).outputs.size() == 2);
    }
    CHECK(rate(1006) == std::vector<std::uint32_t>{9, 9});
    CHECK(need(1006) == std::vector<std::uint32_t>{8, 4});
    CHECK(types.at(1006).capacity[0] == 5U << 8);
    for (const auto& [id, t] : types) {
        CHECK_MESSAGE(t.kind != sim::BuildingClass::TwoOutputWorkshop, "HANDWERKDUAL " << id);
    }
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

TEST_CASE("weapons workshop: two lines share the step, one line takes all of it") {
    const auto t = bow_maker();
    auto w = sim::make_weapons_workshop(t);
    REQUIRE(w.out.size() == 2);
    w.in = {800, 800};
    std::uint32_t now = 0;
    // Both lines: 9 · (64 · 128 >> 7) >> 7 = 4 each; the inputs go in full.
    auto r = step(w, t, now);
    CHECK(r.made == std::vector<std::uint32_t>{4, 4});
    CHECK(w.in == std::vector<std::uint32_t>{792, 796});
    CHECK(w.activity == std::vector<std::uint16_t>{64, 64});
    CHECK(r.send_figure);
    // Line 1 locked: line 0 makes the full 9, line 1 keeps its activity.
    w.unlocked[1] = false;
    r = step(w, t, now);
    CHECK(r.made == std::vector<std::uint32_t>{9, 0});
    CHECK(w.out == std::vector<std::uint32_t>{13, 4});
    CHECK(w.activity == std::vector<std::uint16_t>{63 + 144, 64});
    CHECK(w.in == std::vector<std::uint32_t>{784, 792});
    // Line 0 switched off as well: nothing happens after the gate.
    w.line_on[0] = false;
    r = step(w, t, now);
    CHECK(r.stepped);
    CHECK(r.made.empty());
    CHECK(w.activity == std::vector<std::uint16_t>{207, 64});
}

TEST_CASE("weapons workshop: inputs limit both lines, nothing below half a step") {
    const auto t = bow_maker();
    auto w = sim::make_weapons_workshop(t);
    w.in = {6, 100}; // 6 of 8 wood: ratio (6 << 7) / 8 = 96
    std::uint32_t now = 0;
    auto r = step(w, t, now);
    // 9 · (64 · 96 >> 7) >> 7 = 9 · 48 >> 7 = 3; inputs 8 · 96 >> 7 = 6, 4 · 96 >> 7 = 3.
    CHECK(r.made == std::vector<std::uint32_t>{3, 3});
    CHECK(w.in == std::vector<std::uint32_t>{0, 97});
    r = step(w, t, now);
    CHECK(r.status == sim::Status::MissingInput);
    CHECK(r.made == std::vector<std::uint32_t>{0, 0});
    CHECK(w.blocked);
    CHECK(r.blocked_changed);
}

TEST_CASE("weapons workshop: each line checks the output-0 capacity on its own") {
    auto t = bow_maker();
    t.capacity = {1280, 0}; // line 1 is checked against capacity 0 too
    auto w = sim::make_weapons_workshop(t);
    w.in = {800, 800};
    w.out = {1280, 0};
    std::uint32_t now = 0;
    // Line 0 full: "storage full" is reported, line 1 still makes its half
    // (the share counts working lines, not free ones), the inputs go in full
    // and production clears the status and the blocked flag again.
    auto r = step(w, t, now);
    CHECK(r.status == sim::Status::StorageFull);
    CHECK(r.made == std::vector<std::uint32_t>{0, 4});
    CHECK(w.in == std::vector<std::uint32_t>{792, 796});
    CHECK_FALSE(w.blocked);
    CHECK_FALSE(r.blocked_changed);
    // Both full: no production, nothing used, blocked.
    w.out = {1280, 1280};
    r = step(w, t, now);
    CHECK(r.status == sim::Status::StorageFull);
    CHECK(r.made == std::vector<std::uint32_t>{0, 0});
    CHECK(w.in == std::vector<std::uint32_t>{792, 796});
    CHECK(w.blocked);
    CHECK(r.blocked_changed);
    // A full line reports even when it is switched off.
    w.line_on[1] = false;
    w.out = {0, 1280};
    r = step(w, t, now);
    CHECK(r.status == sim::Status::StorageFull);
    CHECK(r.made == std::vector<std::uint32_t>{9, 0});
}

TEST_CASE("weapons workshop: the gate, suspension and the u16 activity") {
    const auto t = bow_maker();
    auto w = sim::make_weapons_workshop(t);
    w.in = {800, 800};
    CHECK_FALSE(sim::weapons_workshop_step(w, t, 499).stepped);
    w.suspended = true;
    w.activity = {1280, 0};
    auto r = sim::weapons_workshop_step(w, t, 500);
    CHECK(r.stepped);
    CHECK(r.made.empty());
    CHECK(w.activity[0] == 1270); // aged before the suspension check
    // The activity is 16-bit: it wraps as the original's addw. A made amount
    // of 100 on one line: 65530 · 127 >> 7 = 65018, + 1600 = 66618 → 1082.
    auto big = t;
    big.output_per_step = {100, 100};
    w.suspended = false;
    w.line_on[1] = false;
    w.activity = {65530, 0};
    r = sim::weapons_workshop_step(w, big, 1000);
    CHECK(r.made[0] == 100);
    CHECK(w.activity[0] == 1082);
}

namespace {

/// The fur trapper 824 (PELZJAEGER: furs from fur animals) with the values
/// F1 checks: rate 26, need 25, 5 t.
sim::ProductionType fur_trapper() {
    sim::ProductionType t;
    t.kind = sim::BuildingClass::FurTrapper;
    t.outputs = {"PELZE"};
    t.inputs = {"PELZTIER"};
    t.output_per_step = {26, 0};
    t.input_per_step = {25, 0};
    t.capacity = {5 << 8, 0};
    t.max_figures = {1, 0, 0};
    return t;
}

} // namespace

TEST_CASE("fur trapper: produces only while no trapper is out") {
    const auto t = fur_trapper();
    sim::Farm f;
    f.raw = 100;
    std::uint32_t now = 500;
    // Full ratio: 26 made, 25 used, no trapper sent (ratio 128).
    auto r = sim::fur_trapper_step(f, t, now);
    CHECK(r.made[0] == 26);
    CHECK(f.out == 26);
    CHECK(f.raw == 75);
    CHECK(f.activity == 416);
    CHECK_FALSE(r.send_figure);
    // A trapper out: nothing made, no status, no further trapper.
    f.workers_out = 1;
    r = sim::fur_trapper_step(f, t, now += 500);
    CHECK(r.made[0] == 0);
    CHECK(r.status == sim::Status::None);
    CHECK_FALSE(r.send_figure);
    CHECK(f.raw == 75);
    CHECK(f.activity == 414); // 416 · 255 >> 8
}

TEST_CASE("fur trapper: partial steps and missing input send a trapper") {
    auto t = fur_trapper();
    t.max_figures = {0, 0, 0}; // not read by the trapper's dispatch
    sim::Farm f;
    f.raw = 20; // ratio (20 << 7) / 25 = 102
    std::uint32_t now = 500;
    auto r = sim::fur_trapper_step(f, t, now);
    CHECK(r.made[0] == 20); // 26 · 102 >> 7
    CHECK(f.raw == 1);      // 20 − (25 · 102 >> 7)
    CHECK(r.send_figure);
    r = sim::fur_trapper_step(f, t, now += 500);
    CHECK(r.status == sim::Status::MissingInput);
    CHECK(f.blocked);
    CHECK(r.blocked_changed);
    CHECK(r.send_figure);
}

TEST_CASE("fur trapper: input before capacity, both repeat after 45,000 ticks") {
    const auto t = fur_trapper();
    sim::Farm f;
    f.out = 1280;
    f.raw = 100;
    std::uint32_t now = 500;
    auto r = sim::fur_trapper_step(f, t, now);
    CHECK(r.status == sim::Status::StorageFull);
    CHECK_FALSE(r.send_figure); // full ratio: no trapper
    // Full storage and too little input: the input is reported (the farm
    // would report the storage).
    f.raw = 5;
    r = sim::fur_trapper_step(f, t, now += 500);
    CHECK(r.status == sim::Status::MissingInput);
    // The message is due after 45,000 ticks (the farm waits 60,000).
    sim::Farm g;
    std::uint32_t first = 500;
    CHECK_FALSE(sim::fur_trapper_step(g, t, first).message);
    bool due = false;
    std::uint32_t at = first;
    while (!due) {
        at += 500;
        due = sim::fur_trapper_step(g, t, at).message;
    }
    CHECK(at - first == 45000);
}

TEST_CASE("deposits: reserve, deep mining and the depletion setting") {
    sim::Deposit d{"EISENERZ", 0, 0, 2000 * sim::kTon, true};
    CHECK(sim::deposit_remaining(d, false) == 400 * sim::kTon);
    CHECK(sim::deposit_remaining(d, true) == 2000 * sim::kTon);
    // Deep with the reserve flag, or depletion off: the full rate, nothing used.
    CHECK(sim::deposit_extract(d, 21, true, true) == 21);
    CHECK(sim::deposit_extract(d, 21, false, false) == 21);
    CHECK(d.remaining == 2000 * sim::kTon);
    // Shallow: stops at the reserve.
    d.remaining = sim::kDepositReserve + 10;
    CHECK(sim::deposit_extract(d, 21, false, true) == 10);
    CHECK(d.remaining == sim::kDepositReserve);
    CHECK(sim::deposit_extract(d, 21, false, true) == 0);
    // Without the reserve flag: min(rate, remaining), deep or not.
    sim::Deposit e{"GOLD", 0, 0, 30, false};
    CHECK(sim::deposit_remaining(e, false) == 30);
    CHECK(sim::deposit_extract(e, 21, true, true) == 21);
    CHECK(sim::deposit_extract(e, 21, true, true) == 9);
    CHECK(e.remaining == 0);
}

TEST_CASE("deposits: reach is a circle of radius 7 around the footprint") {
    const sim::MineArea a{10, 10, 3, 4};     // tiles x 10–12, y 10–13
    CHECK(sim::deposit_in_reach(a, 11, 11)); // under the footprint
    CHECK(sim::deposit_in_reach(a, 10, 3));  // 7 rows above
    CHECK_FALSE(sim::deposit_in_reach(a, 10, 2));
    CHECK(sim::deposit_in_reach(a, 10, 20)); // 7 rows below row 13
    CHECK_FALSE(sim::deposit_in_reach(a, 10, 21));
    CHECK(sim::deposit_in_reach(a, 3, 10)); // 7 columns left
    CHECK_FALSE(sim::deposit_in_reach(a, 2, 10));
    CHECK(sim::deposit_in_reach(a, 19, 10)); // 7 columns right of column 12
    CHECK_FALSE(sim::deposit_in_reach(a, 20, 10));
    // Row 7 beyond: half-width 2; row 5: 5.
    CHECK(sim::deposit_in_reach(a, 8, 3));
    CHECK_FALSE(sim::deposit_in_reach(a, 7, 3));
    CHECK(sim::deposit_in_reach(a, 17, 18));
    CHECK_FALSE(sim::deposit_in_reach(a, 18, 18));
}

TEST_CASE("deposits: the mine takes the fullest in reach, the first on a tie") {
    const sim::MineArea a{10, 10, 3, 4};
    std::vector<sim::Deposit> ds = {
        {"GOLD", 11, 11, 3000 * sim::kTon, true},     // other goods
        {"EISENERZ", 40, 40, 3000 * sim::kTon, true}, // out of reach
        {"EISENERZ", 11, 9, 1800 * sim::kTon, true},  {"EISENERZ", 13, 9, 1900 * sim::kTon, true},
        {"EISENERZ", 9, 9, 1900 * sim::kTon, true},
    };
    CHECK(sim::find_deposit(ds, "EISENERZ", false, a) == std::optional<std::size_t>{3});
    CHECK(sim::find_deposit(ds, "SALZSTEIN", false, a) == std::nullopt);
}

TEST_CASE("mine: a shallow iron mine works its deposit down to the reserve") {
    sim::ProductionType t;
    t.kind = sim::BuildingClass::Mine;
    t.outputs = {"EISENERZ"};
    t.output_per_step = {21, 0};
    t.capacity = {5 << 8, 0};
    CHECK_FALSE(sim::mine_is_deep(t, 0));
    CHECK(sim::mine_is_deep(t, 0x39));
    std::vector<sim::Deposit> ds = {{"EISENERZ", 11, 11, sim::kDepositReserve + 30, true}};
    const sim::MineArea a{10, 10, 3, 4};
    sim::Mine m;
    m.deep = false;
    m.deposit = sim::find_deposit(ds, "EISENERZ", m.deep, a);
    REQUIRE(m.deposit == std::optional<std::size_t>{0});
    auto r = sim::mine_step(m, t, 500, ds, a, true);
    CHECK(r.made[0] == 21);
    CHECK(r.notice == 0);
    // 9 left: a short step; the search finds nothing left, deep ore remains.
    r = sim::mine_step(m, t, 1000, ds, a, true);
    CHECK(r.made[0] == 9);
    CHECK(r.notice == 0x15B);
    CHECK(m.out == 30);
    CHECK(m.activity == static_cast<std::uint16_t>((336 * 63 >> 6) + 144));
    // Nothing left: "missing input".
    r = sim::mine_step(m, t, 1500, ds, a, true);
    CHECK(r.status == sim::Status::MissingInput);
    CHECK(r.made[0] == 0);
    CHECK(m.blocked);
}

TEST_CASE("mine: full storage first, no deposit, and no depletion") {
    sim::ProductionType t;
    t.kind = sim::BuildingClass::Mine;
    t.outputs = {"GOLD"};
    t.output_per_step = {21, 0};
    t.capacity = {5 << 8, 0};
    std::vector<sim::Deposit> ds = {{"GOLD", 0, 0, 10, false}};
    const sim::MineArea a{0, 0, 3, 4};
    sim::Mine m;
    m.out = 1280;
    auto r = sim::mine_step(m, t, 500, ds, a, true);
    CHECK(r.status == sim::Status::StorageFull);
    m.out = 0;
    r = sim::mine_step(m, t, 1000, ds, a, true);
    CHECK(r.status == sim::Status::MissingInput); // no deposit attached
    // Depletion off: the full rate from any deposit, nothing used up.
    m.deposit = 0;
    for (std::uint32_t k = 3; k < 10; ++k) {
        r = sim::mine_step(m, t, k * 500, ds, a, false);
        CHECK(r.made[0] == 21);
    }
    CHECK(ds[0].remaining == 10);
    // Depletion on: 10 left without a reserve; the search keeps the empty
    // deposit, no notice for goods other than iron ore.
    r = sim::mine_step(m, t, 5000, ds, a, true);
    CHECK(r.made[0] == 10);
    CHECK(r.notice == 0);
}

namespace {

/// The small farm 809 (UNIVERSAL: alcohol from potatoes, food from grass)
/// with the values F1 checks: rates 13 and 4, needs 128 and 50, 4 t, at
/// most 3 animals out.
sim::ProductionType small_farm() {
    sim::ProductionType t;
    t.kind = sim::BuildingClass::SmallFarm;
    t.outputs = {"ALKOHOL", "NAHRUNG"};
    t.inputs = {"KARTOFFEL", "GRAS"};
    t.output_per_step = {13, 4};
    t.input_per_step = {128, 50};
    t.capacity = {4 << 8, 4 << 8};
    t.max_figures = {3, 0, 0};
    return t;
}

} // namespace

TEST_CASE("small farm: two farm lines, workers and animals") {
    const auto t = small_farm();
    auto f = sim::make_small_farm(t);
    f.raw = {200, 0};
    // Line 0 makes 13 from 128; line 1 has nothing: an animal of the
    // climate's kind goes out (polar: 107), no line-0 worker (it made some).
    auto r = sim::small_farm_step(f, t, 500, 0);
    CHECK(r.made == std::vector<std::uint32_t>{13, 0});
    CHECK(f.raw == std::vector<std::uint32_t>{72, 0});
    CHECK(f.activity == std::vector<std::uint16_t>{208, 0});
    CHECK_FALSE(r.send_figure);
    CHECK(r.send_animal);
    CHECK(r.animal_kind == 107);
    CHECK(r.status == sim::Status::MissingInput);
    // 72 left: ratio 72, 72 · 13 >> 7 = 7 made, all 72 used.
    r = sim::small_farm_step(f, t, 1000, 0);
    CHECK(r.made == std::vector<std::uint32_t>{7, 0});
    CHECK(f.raw[0] == 0);
    CHECK_FALSE(r.send_figure);
    // Line 0 now misses its input too: its worker is due.
    r = sim::small_farm_step(f, t, 1500, 0);
    CHECK(r.made == std::vector<std::uint32_t>{0, 0});
    CHECK(r.send_figure);
    // A line-0 worker out: no second one.
    f.figures_out[0] = 1;
    CHECK_FALSE(sim::small_farm_step(f, t, 2000, 0).send_figure);
    // Tundra: kind 110, one at a time; prairie: 109.
    CHECK(sim::small_farm_step(f, t, 2500, 1).animal_kind == 110);
    CHECK(sim::small_farm_step(f, t, 3000, 3).animal_kind == 109);
}

TEST_CASE("small farm: animal limits per kind and in total") {
    const auto t = small_farm();
    auto f = sim::make_small_farm(t);
    f.line_on[0] = false;
    // Kind 107 (counter 1) allows 2 out.
    f.figures_out = {0, 2, 0, 0, 0};
    CHECK_FALSE(sim::small_farm_step(f, t, 500, 0).send_animal);
    // A step that made something counts one fewer: 100 grass, ratio 128.
    f.raw[1] = 100;
    auto r = sim::small_farm_step(f, t, 1000, 0);
    CHECK(r.made[1] == 4);
    CHECK(f.raw[1] == 50);
    CHECK(r.send_animal);
    // All animals together are limited by max_figures(0) = 3.
    f.figures_out = {0, 1, 0, 1, 1};
    f.raw[1] = 0;
    CHECK_FALSE(sim::small_farm_step(f, t, 1500, 0).send_animal);
}

TEST_CASE("small farm: a full line keeps the status, suspension ages both lines") {
    const auto t = small_farm();
    auto f = sim::make_small_farm(t);
    f.out = {1024, 0};
    f.raw = {0, 100};
    // Line 0 full ("storage full", blocked), line 1 produces but does not
    // clear the status, since line 0 is switched on and full.
    auto r = sim::small_farm_step(f, t, 500, 0);
    CHECK(r.made == std::vector<std::uint32_t>{0, 4});
    CHECK(f.blocked);
    CHECK(f.status.code == sim::Status::StorageFull);
    // Line 0 switched off: line 1 clears it.
    f.line_on[0] = false;
    r = sim::small_farm_step(f, t, 1000, 0);
    CHECK_FALSE(f.blocked);
    CHECK(r.blocked_changed);
    // Suspended: both lines age, even the one switched off.
    f.suspended = true;
    f.activity = {256, 256};
    r = sim::small_farm_step(f, t, 1500, 0);
    CHECK(r.made.empty());
    CHECK(f.activity == std::vector<std::uint16_t>{255, 255});
}

TEST_CASE("small farm: missing input never raises its message") {
    // The repeat interval is 75,000 ticks, but the status helper measures
    // the elapsed time in 16 bits ((t - time) & 0xffff, compared signed with
    // the interval, 0x10072f1a-0x10072f22): it never reaches 75,000, so the
    // original never raises this message. The status code is still set.
    const auto t = small_farm();
    auto f = sim::make_small_farm(t);
    f.line_on[1] = false;
    bool any = false;
    for (std::uint32_t k = 1; k <= 400; ++k) { // 200,000 ticks
        const auto r = sim::small_farm_step(f, t, k * 500, 0);
        any = any || r.message;
        CHECK(r.status == sim::Status::MissingInput);
    }
    CHECK_FALSE(any);
}
