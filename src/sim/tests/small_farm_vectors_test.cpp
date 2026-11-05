// MAP-SUPPLY-1 S1: the existing small farm kernel against the hand-derived vectors of
// fixtures/synthetic/sim/small_farm_vectors.txt (checked independently by tools/small_farm_ref.py).
// Runs the record kinds the engine already implements (gate, line, tick); `credit`, `pick` and
// `cut` belong to S2/S3 and are only counted here (docs/sim/small-farm-supply.md section 6).
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/building.hpp>
#include <anno/sim/draw.hpp>
#include <anno/sim/driver.hpp>
#include <anno/sim/harvest.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/rng.hpp>

namespace {

namespace sim = anno::sim;
using Kv = std::map<std::string, std::string>;

Kv kv_of(const std::vector<std::string>& t, std::size_t from, std::size_t to) {
    Kv m;
    for (std::size_t i = from; i < to; ++i) {
        const auto eq = t[i].find('=');
        REQUIRE_MESSAGE(eq != std::string::npos, t[i]);
        m[t[i].substr(0, eq)] = t[i].substr(eq + 1);
    }
    return m;
}

std::vector<std::int64_t> list_of(const std::string& s) {
    std::vector<std::int64_t> out;
    std::istringstream is(s);
    for (std::string w; std::getline(is, w, ',');) {
        out.push_back(std::stoll(w));
    }
    return out;
}

std::string join(const std::vector<std::int64_t>& v) {
    std::string out;
    for (std::size_t i = 0; i < v.size(); ++i) {
        out += (i ? "," : "") + std::to_string(v[i]);
    }
    return out;
}

/// The small farm 809 with the vector file's constants (as production_test.cpp's helper).
sim::ProductionType small_farm_809(std::uint32_t rate0, std::uint32_t need0, std::uint32_t rate1,
                                   std::uint32_t need1, std::uint32_t cap) {
    sim::ProductionType t;
    t.kind = sim::BuildingClass::SmallFarm;
    t.outputs = {"ALKOHOL", "NAHRUNG"};
    t.inputs = {"KARTOFFEL", "GRAS"};
    t.output_per_step = {rate0, rate1};
    t.input_per_step = {need0, need1};
    t.capacity = {cap, cap};
    t.max_figures = {3, 0, 0};
    return t;
}

struct OneDraw {
    std::uint16_t value = 0;
    std::uint32_t calls = 0;
};

} // namespace

TEST_CASE("small farm vectors: the kernel agrees with the gate, line and tick records") {
    std::ifstream in(std::string(ANNO_FIXTURES_DIR) + "/sim/small_farm_vectors.txt");
    REQUIRE(in.good());
    int run = 0, skipped = 0, unchecked_keys = 0;
    for (std::string line; std::getline(in, line);) {
        line = line.substr(0, line.find('#'));
        std::istringstream is(line);
        std::vector<std::string> t;
        for (std::string w; is >> w;) {
            t.push_back(w);
        }
        if (t.empty()) {
            continue;
        }
        std::size_t arrow = 0;
        for (std::size_t i = 0; i < t.size(); ++i) {
            arrow = t[i] == "=>" ? i : arrow;
        }
        REQUIRE(arrow > 1);
        const Kv in_kv = kv_of(t, 2, arrow);
        const Kv want = kv_of(t, arrow + 1, t.size());
        Kv got;
        if (t[0] == "gate" || t[0] == "line") {
            // one line switched on, nothing else: the gate and the line step through the real tick
            const bool is_gate = t[0] == "gate";
            const auto rate = is_gate ? 13 : std::stoul(in_kv.at("rate"));
            const auto need = is_gate ? 128 : std::stoul(in_kv.at("need"));
            const auto cap = is_gate ? 1024 : std::stoul(in_kv.at("cap"));
            const sim::ProductionType ty =
                small_farm_809(static_cast<std::uint32_t>(rate), static_cast<std::uint32_t>(need),
                               4, 50, static_cast<std::uint32_t>(cap));
            sim::SmallFarm f = sim::make_small_farm(ty);
            f.line_on = {true, false};
            f.last = static_cast<std::uint16_t>(is_gate ? std::stoul(in_kv.at("last")) : 0);
            if (!is_gate) {
                f.raw[0] = static_cast<std::uint32_t>(std::stoul(in_kv.at("raw")));
                f.out[0] = static_cast<std::uint32_t>(std::stoul(in_kv.at("out")));
            }
            auto rng = sim::WorldRng::from_table({0});
            REQUIRE(rng.has_value());
            const sim::StepResult r = sim::small_farm_step(
                f, ty, is_gate ? static_cast<std::uint32_t>(std::stoul(in_kv.at("t"))) : 500,
                sim::DrawRef::of(*rng));
            if (is_gate) {
                got = {{"run", r.stepped ? "1" : "0"}, {"last", std::to_string(f.last)}};
            } else {
                got = {{"made", std::to_string(r.made.empty() ? 0 : r.made[0])},
                       {"raw", std::to_string(f.raw[0])},
                       {"out", std::to_string(f.out[0])},
                       {"status", std::to_string(r.status == sim::Status::MissingInput  ? 1
                                                 : r.status == sim::Status::StorageFull ? 2
                                                                                        : 0)}};
            }
        } else if (t[0] == "tick") {
            if (in_kv.count("fail") != 0) {
                ++skipped; // a failed send is the driver's (consume / scheduler), not the kernel's
                continue;
            }
            const sim::ProductionType ty = small_farm_809(13, 128, 4, 50, 1024);
            sim::SmallFarm f = sim::make_small_farm(ty);
            f.line_on = {in_kv.at("s0") == "1", in_kv.at("s1") == "1"};
            f.suspended = in_kv.at("susp") == "1";
            const auto c = list_of(in_kv.at("c"));
            for (std::size_t i = 0; i < c.size(); ++i) {
                f.figures_out[i] = static_cast<std::uint32_t>(c[i]);
            }
            const auto raw = list_of(in_kv.at("raw")), out = list_of(in_kv.at("out")),
                       act = list_of(in_kv.at("act"));
            for (std::size_t i = 0; i < 2; ++i) {
                f.raw[i] = static_cast<std::uint32_t>(raw[i]);
                f.out[i] = static_cast<std::uint32_t>(out[i]);
                f.activity[i] = static_cast<std::uint16_t>(act[i]);
            }
            f.last = static_cast<std::uint16_t>(std::stoul(in_kv.at("last")));
            OneDraw d;
            d.value = static_cast<std::uint16_t>(std::stoul(in_kv.at("draw")));
            sim::DrawRef draw{&d, [](void* p) {
                                  auto* o = static_cast<OneDraw*>(p);
                                  ++o->calls;
                                  return o->value;
                              }};
            const sim::StepResult r = sim::small_farm_step(
                f, ty, static_cast<std::uint32_t>(std::stoul(in_kv.at("t"))), draw);
            const std::vector<std::int64_t> made{r.made.size() > 0 ? r.made[0] : 0,
                                                 r.made.size() > 1 ? r.made[1] : 0};
            got = {{"run", r.stepped ? "1" : "0"},
                   {"made", join(made)},
                   {"raw", join({f.raw[0], f.raw[1]})},
                   {"out", join({f.out[0], f.out[1]})},
                   {"act", join({f.activity[0], f.activity[1]})},
                   {"last", std::to_string(f.last)},
                   {"worker", r.send_figure ? "1" : "0"},
                   {"animal", r.send_animal ? std::to_string(r.animal_kind) : "0"},
                   {"draws", std::to_string(d.calls)}};
        } else if (t[0] == "credit") {
            // The raw slot of a delivery (engine: input_slot_for, the merge adds to that slot) and
            // the add itself. ADAPT: the engine's raw stock is a u32 (no hardcoded limits), the
            // original's a u16 that wraps; the sums below 65536 are identical.
            sim::BuildingType ty;
            ty.input_goods = {68, static_cast<sim::GoodsId>(std::stoul(in_kv.at("in1")))};
            const auto raw = list_of(in_kv.at("raw"));
            std::vector<std::int64_t> after = raw;
            after.at(sim::input_slot_for(
                ty, static_cast<sim::GoodsId>(std::stoul(in_kv.at("goods"))))) +=
                std::stoll(in_kv.at("amount"));
            got = {{"raw", join(after)}};
        } else if (t[0] == "pick") {
            // the step-A callback as a pure function (harvest_pick_tile), limit 0 for the worker,
            // 20 for the animal
            std::vector<sim::HarvestGoalNode> nodes;
            std::istringstream ns(in_kv.at("nodes"));
            for (std::string n; std::getline(ns, n, ';');) {
                std::istringstream one(n);
                std::string x, y, s, c;
                std::getline(one, x, ':');
                std::getline(one, y, ':');
                std::getline(one, s, ':');
                std::getline(one, c, ':');
                nodes.push_back(sim::HarvestGoalNode{static_cast<std::int32_t>(std::stol(x)),
                                                     static_cast<std::int32_t>(std::stol(y)),
                                                     static_cast<std::uint16_t>(std::stoul(s)),
                                                     c == "1"});
            }
            const std::uint16_t limit = static_cast<std::uint16_t>(std::stoul(in_kv.at("limit")));
            CHECK(limit == sim::harvest_search_limit(in_kv.at("mode") == "animal" ? 0x2e : 0x2a));
            const auto tile = sim::harvest_pick_tile(limit, nodes);
            got = {
                {"tile", tile ? std::to_string(tile->x) + ":" + std::to_string(tile->y) : "none"}};
        } else {
            ++skipped;
            continue;
        }
        ++run;
        for (const auto& [k, v] : want) {
            if (got.count(k) == 0) {
                ++unchecked_keys; // counters/cargo of the send belong to the driver (S3)
                continue;
            }
            // ADAPT (docs/sim/small-farm-supply.md 2.1): the original's output stock is a u16 and
            // wraps; the engine's is a u32 (no hardcoded limits), identical for every capacity
            // below 65536.
            const std::string expect = (t[1] == "L-out-wrap" && k == "out") ? std::string("65543")
                                       : (t[1] == "C-wrap" && k == "raw")   ? std::string("66000,0")
                                                                            : v;
            CHECK_MESSAGE(got.at(k) == expect, t[0] << " " << t[1] << ": " << k);
        }
    }
    CHECK(run == 48);
    CHECK(skipped == 5);        // cut (3) and two failed sends (driver level)
    CHECK(unchecked_keys == 4); // counters and cargo of the two successful-send tick records
}
