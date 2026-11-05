// MAP-NEEDS-2 N1: the existing farm and workshop kernels and the harvest choice against the
// hand-derived vectors of fixtures/synthetic/sim/early_needs_vectors.txt (checked independently by
// tools/early_needs_ref.py). Runs the record kinds the engine already implements (gate, farm,
// wstep, pick); `route`, `credit` (Farm), `trip`, `offer` and the `*run` records belong to N2 and
// are only counted here (docs/sim/early-needs.md section 7).
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/building.hpp>
#include <anno/sim/harvest.hpp>
#include <anno/sim/production.hpp>

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

std::uint32_t u(const Kv& m, const char* k) {
    return static_cast<std::uint32_t>(std::stoul(m.at(k)));
}

int status_code(sim::Status s) {
    return s == sim::Status::MissingInput ? 1 : s == sim::Status::StorageFull ? 2 : 0;
}

} // namespace

TEST_CASE("early needs vectors: the farm and workshop kernels and the harvest choice agree") {
    std::ifstream in(std::string(ANNO_FIXTURES_DIR) + "/sim/early_needs_vectors.txt");
    REQUIRE(in.good());
    int run = 0, pending = 0;
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
        if (t[0] == "gate") {
            sim::ProductionType ty;
            ty.kind = sim::BuildingClass::Farm;
            ty.output_per_step = {6};
            ty.input_per_step = {87};
            ty.capacity = {1792};
            ty.max_figures = {5, 0, 0};
            sim::Farm f;
            f.last = static_cast<std::uint16_t>(u(in_kv, "last"));
            const sim::StepResult r = sim::farm_step(f, ty, u(in_kv, "t"));
            got = {{"run", r.stepped ? "1" : "0"}, {"last", std::to_string(f.last)}};
        } else if (t[0] == "farm") {
            sim::ProductionType ty;
            ty.kind = sim::BuildingClass::Farm;
            ty.output_per_step = {u(in_kv, "rate")};
            ty.input_per_step = {u(in_kv, "need")};
            ty.capacity = {u(in_kv, "cap")};
            ty.max_figures = {u(in_kv, "max"), 0, 0};
            sim::Farm f;
            f.switched_on = in_kv.at("on") == "1";
            f.suspended = in_kv.at("susp") == "1";
            f.workers_out = u(in_kv, "wout");
            f.raw = u(in_kv, "raw");
            f.out = u(in_kv, "out");
            f.activity = static_cast<std::uint16_t>(u(in_kv, "act"));
            f.last = static_cast<std::uint16_t>(u(in_kv, "last"));
            const sim::StepResult r = sim::farm_step(f, ty, u(in_kv, "t"));
            got = {{"run", r.stepped ? "1" : "0"},
                   {"made", std::to_string(r.made.empty() ? 0 : r.made[0])},
                   {"raw", std::to_string(f.raw)},
                   {"out", std::to_string(f.out)},
                   {"act", std::to_string(f.activity)},
                   {"status", std::to_string(status_code(r.status))},
                   {"send", r.send_figure ? "1" : "0"}};
        } else if (t[0] == "wstep") {
            sim::ProductionType ty;
            ty.kind = sim::BuildingClass::Workshop;
            ty.output_per_step = {u(in_kv, "rate")};
            ty.input_per_step = {u(in_kv, "need")};
            ty.capacity = {u(in_kv, "cap")};
            ty.max_figures = {u(in_kv, "max"), 0, 0};
            sim::Workshop w = sim::make_workshop(ty);
            w.suspended = in_kv.at("susp") == "1";
            w.in = {u(in_kv, "in")};
            w.out = {u(in_kv, "out")};
            w.carriers_out = u(in_kv, "carriers");
            const sim::StepResult r = sim::workshop_step(w, ty, 500);
            got = {{"made", std::to_string(r.made.empty() ? 0 : r.made[0])},
                   {"in", std::to_string(w.in[0])},
                   {"out", std::to_string(w.out[0])},
                   {"status", std::to_string(status_code(r.status))},
                   {"send", r.send_figure ? "1" : "0"}};
        } else if (t[0] == "pick") {
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
            const auto tile =
                sim::harvest_pick_tile(static_cast<std::uint16_t>(u(in_kv, "limit")), nodes);
            got = {
                {"tile", tile ? std::to_string(tile->x) + ":" + std::to_string(tile->y) : "none"}};
        } else {
            ++pending; // route, credit, trip, offer, farmrun, wrun: N2
            continue;
        }
        ++run;
        for (const auto& [k, v] : want) {
            REQUIRE_MESSAGE(got.count(k) == 1, t[0] << " " << t[1] << ": " << k);
            CHECK_MESSAGE(got.at(k) == v, t[0] << " " << t[1] << ": " << k);
        }
    }
    CHECK(run == 35);
    CHECK(pending == 31);
}
