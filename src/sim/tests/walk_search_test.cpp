#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/building.hpp>
#include <anno/sim/walk_search.hpp>
namespace sim = anno::sim;
namespace {
struct Grid {
    sim::Island island;
    std::shared_ptr<const sim::TypeRegistry> types = sim::TypeRegistry::empty();
    sim::WalkTables tables = *sim::synthetic_walk_tables();
    Grid(std::uint32_t w, std::uint32_t h) {
        REQUIRE(island.tiles.reset(0, 0, w, h, 4));
        for (auto code : {0U, 1U, 2U, 4U}) {
            tables.layers[0][code] = 0xff;
            for (std::uint32_t j = 0; j < 8; ++j) {
                tables.layers[0][0x400 + 8 * code + j] =
                    static_cast<std::uint8_t>(j % 2 == 0 ? 10 : 13);
            }
        }
    }
    sim::WalkCells cells(std::optional<sim::SearchArea> disc = {}) const {
        sim::WalkCells c(island, *types, tables, island.rect(), 4, std::move(disc));
        for (std::uint32_t y = 0; y < island.tiles.height(); ++y) {
            for (std::uint32_t x = 0; x < island.tiles.width(); ++x) {
                c.set({static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)}, {0, 6});
            }
        }
        return c;
    }
    void price(std::uint32_t code, std::uint32_t j, std::uint8_t value) {
        tables.layers[0][0x400 + 8 * code + j] = value;
    }
};
void open(sim::WalkCells& c, sim::TileXY tile, std::uint16_t code = 0, std::uint16_t low = 0xfff) {
    REQUIRE(c.set(tile, {low, code}));
}
sim::WalkPoint point(std::int32_t x, std::int32_t y, std::uint32_t col = 1, std::uint32_t row = 1) {
    return {{x, y}, col, row};
}
const sim::SearchCallback first = [](const sim::SearchVisit&) {
    return sim::SearchDecision{sim::GoalAction::Stop, true};
};
sim::SearchResult run(const sim::WalkCells& cells, const sim::WalkTables& tables,
                      sim::WalkPoint start, sim::WalkPoint goal) {
    const std::array starts{start};
    const std::array goals{goal};
    return sim::search(cells, tables, 0, starts, goals, first);
}
} // namespace
TEST_CASE("walk_search: start flood uses blocked subcells and reached edge keys") {
    CHECK(sim::start_keys(0, 1, 1) == 0xf000);
    CHECK(sim::start_keys(0x92, 0, 1) == 0xb000); // blocked middle column
    CHECK(sim::start_keys(0x92, 2, 1) == 0xe000);
    CHECK(sim::start_keys(0x92, 1, 1) == 0xf000); // seed replaces blocked centre
    CHECK(sim::start_keys(0x1ff, 1, 1) == 0);
    CHECK(sim::start_keys(0x1ff, 0, 1) == 0x1000);
    CHECK(sim::start_keys(0, 3, 0) == 0);
    Grid g(2, 1);
    sim::BuildingType blocked;
    blocked.id = sim::TypeId{100};
    blocked.walk_mask = {7, 7, 7};
    sim::TypeRegistry::Builder builder;
    builder.add(blocked);
    auto types = builder.build();
    REQUIRE(types);
    g.types = types.value();
    g.island.tiles.set_object(1, 100);
    auto cells = g.cells();
    open(cells, {0, 0});
    open(cells, {1, 0}, 4); // geometry alone cannot supply the start keys
    auto result = run(cells, g.tables, point(1, 0), point(0, 0));
    CHECK(result.error.empty());
    CHECK_FALSE(result.selected);
    CHECK(result.enqueued == 1);
    result = run(cells, g.tables, point(1, 0, 0, 1), point(0, 0));
    REQUIRE(result.selected);
    CHECK(result.cost == 10);
    CHECK(result.route == std::vector<sim::TileXY>{{1, 0}, {0, 0}});
}
TEST_CASE("walk_search: straight and turning corridors return the marked chain") {
    Grid g(3, 2);
    auto c = g.cells();
    open(c, {0, 0});
    open(c, {1, 0});
    open(c, {2, 0});
    const auto before = c.get({1, 0});
    auto r = run(c, g.tables, point(0, 0), point(2, 0));
    REQUIRE(r.selected);
    CHECK(r.error.empty());
    CHECK(r.complete);
    CHECK(r.stopped);
    CHECK(r.cost == 20);
    CHECK(r.route == std::vector<sim::TileXY>{{0, 0}, {1, 0}, {2, 0}});
    CHECK((r.selected->high & 0x3ff) == 0x84);
    CHECK((r.selected->high & 0x400) != 0);
    CHECK(c.get({1, 0}) == before); // all marks are on the search copy
    open(c, {1, 1});
    c.set({2, 0}, {0, 6});
    r = run(c, g.tables, point(0, 0), point(1, 1));
    REQUIRE(r.selected);
    CHECK(r.cost == 13); // first10 plus destination C3-C2=13-10
    CHECK(r.route == std::vector<sim::TileXY>{{0, 0}, {1, 0}, {1, 1}});
    // A geometry path and a nonzero admission byte are both required.
    c.set({1, 0}, {0, 0});
    CHECK_FALSE(run(c, g.tables, point(0, 0), point(1, 1)).selected);
    open(c, {1, 0}, 7);
    CHECK_FALSE(run(c, g.tables, point(0, 0), point(1, 1)).selected);
}
TEST_CASE("walk_search: all came-from groups keys and enumeration orders") {
    const std::array<sim::TileXY, 4> starts{{{2, 1}, {1, 0}, {0, 1}, {1, 2}}};
    const std::array<std::array<sim::TileXY, 3>, 4> exits{{{{{1, 0}, {1, 2}, {0, 1}}},
                                                           {{{0, 1}, {2, 1}, {1, 2}}},
                                                           {{{1, 0}, {1, 2}, {2, 1}}},
                                                           {{{0, 1}, {2, 1}, {1, 0}}}}};
    // For g bit0/1/2 respectively: destination and admission key, derived independently from
    // state5.9.
    const std::array<std::array<sim::TileXY, 3>, 4> by_bit{{{{{1, 2}, {0, 1}, {1, 0}}},
                                                            {{{2, 1}, {1, 2}, {0, 1}}},
                                                            {{{1, 0}, {2, 1}, {1, 2}}},
                                                            {{{0, 1}, {1, 0}, {2, 1}}}}};
    const std::array<std::array<std::uint8_t, 3>, 4> keys{
        {{{4, 1, 64}}, {{16, 4, 1}}, {{64, 16, 4}}, {{1, 64, 16}}}};
    for (std::size_t arrival = 0; arrival < 4; ++arrival) {
        for (std::uint32_t bits : {1U, 2U, 4U, 7U}) {
            Grid g(3, 3);
            auto c = g.cells();
            open(c, starts[arrival]);
            const auto low = static_cast<std::uint16_t>((0xfffU & ~(7U << (3 * arrival))) |
                                                        (bits << (3 * arrival)));
            open(c, {1, 1}, 4, low);
            for (const auto p : exits[arrival]) {
                open(c, p);
            }
            if (bits != 7) {
                const std::size_t bit = bits == 1 ? 0 : (bits == 2 ? 1 : 2);
                g.tables.layers[0][0] = keys[arrival][bit];
            } else {
                for (std::uint32_t j = 1; j < 8; j += 2) {
                    g.price(0, j, 20);
                } // all next scores tie at20
            }
            std::vector<sim::WalkPoint> goals;
            for (const auto p : exits[arrival]) {
                goals.push_back({p, 1, 1});
            }
            std::vector<sim::TileXY> visits;
            const std::array seed{sim::WalkPoint{starts[arrival], 1, 1}};
            const auto r = sim::search(c, g.tables, 0, seed, goals, [&](const sim::SearchVisit& v) {
                visits.push_back(v.point.tile);
                return sim::SearchDecision{sim::GoalAction::Skip, true};
            });
            CHECK(r.error.empty());
            REQUIRE(r.selected);
            if (bits == 7) {
                CHECK(visits ==
                      std::vector<sim::TileXY>(exits[arrival].begin(), exits[arrival].end()));
                CHECK(r.cost == 20);
            } else {
                const std::size_t bit = bits == 1 ? 0 : (bits == 2 ? 1 : 2);
                CHECK(visits == std::vector<sim::TileXY>{by_bit[arrival][bit]});
                CHECK(r.cost == (bits == 2 ? 20 : 13));
            }
        }
    }
}
TEST_CASE("walk_search: first discovery wins even when a cheaper route appears") {
    Grid g(2, 2);
    auto c = g.cells();
    open(c, {0, 0});
    open(c, {0, 1}, 1);
    open(c, {1, 0}, 2);
    open(c, {1, 1});
    for (std::uint32_t j = 0; j < 8; ++j) {
        g.price(2, j, 20);
    }
    g.price(0, 3, 51);
    g.price(0, 4, 1);
    g.price(0, 2, 50);
    const auto r = run(c, g.tables, point(0, 0), point(1, 1));
    REQUIRE(r.selected);
    CHECK(r.error.empty());
    CHECK(r.cost == 60); // down10 discovers goal at10+(51-1), before right20 can offer20+(51-50)=21
    CHECK(r.route == std::vector<sim::TileXY>{{0, 0}, {0, 1}, {1, 1}});
    CHECK(r.enqueued == 4);
    CHECK(r.popped == 4);
}
TEST_CASE("walk_search: default equal scores pop FIFO and admission keys differ") {
    Grid g(3, 3);
    auto c = g.cells();
    open(c, {1, 1});
    const std::array<sim::TileXY, 4> order{{{1, 0}, {1, 2}, {0, 1}, {2, 1}}};
    std::vector<sim::WalkPoint> goals;
    for (const auto p : order) {
        open(c, p);
        goals.push_back({p, 1, 1});
    }
    const std::array starts{point(1, 1)};
    std::vector<sim::TileXY> visits;
    auto r = sim::search(c, g.tables, 0, starts, goals, [&](const sim::SearchVisit& v) {
        visits.push_back(v.point.tile);
        return sim::SearchDecision{sim::GoalAction::Skip, false};
    });
    CHECK(r.error.empty());
    CHECK(visits == std::vector<sim::TileXY>(order.begin(), order.end()));
    g.tables.layers[0][0] = 1;
    visits.clear();
    r = sim::search(c, g.tables, 0, starts, goals, [&](const sim::SearchVisit& v) {
        visits.push_back(v.point.tile);
        return sim::SearchDecision{sim::GoalAction::Skip, true};
    });
    CHECK(visits == std::vector<sim::TileXY>{{0, 1}});
    CHECK(r.enqueued == 2);
}
TEST_CASE("walk_search: u16 costs wrap and pop as unsigned") {
    Grid g(2, 2);
    auto c = g.cells();
    open(c, {0, 0});
    open(c, {0, 1}, 1);
    open(c, {1, 0}, 2);
    open(c, {1, 1});
    for (std::uint32_t j = 0; j < 8; ++j) {
        g.price(2, j, 0);
    }
    g.price(0, 3, 0);
    g.price(0, 2, 255);
    const std::array starts{point(0, 0)};
    const std::array goals{point(0, 1), point(1, 1)};
    std::vector<std::uint16_t> scores;
    const auto r = sim::search(c, g.tables, 0, starts, goals, [&](const sim::SearchVisit& v) {
        scores.push_back(v.score);
        return sim::SearchDecision{v.point.tile.x == 1 ? sim::GoalAction::Stop
                                                       : sim::GoalAction::Skip,
                                   v.point.tile.x == 1};
    });
    CHECK(scores == std::vector<std::uint16_t>{10, 65281});
    CHECK(r.cost == 65281);
    Grid line(259, 1);
    auto l = line.cells();
    for (std::int32_t x = 0; x < 259; ++x) {
        open(l, {x, 0});
    }
    line.price(0, 4, 255);
    const auto positive = run(l, line.tables, point(0, 0), point(258, 0));
    REQUIRE(positive.selected);
    CHECK(positive.cost == 254);
    CHECK(positive.route.size() == 259);
}
TEST_CASE("walk_search: arrival-ordered goal subcells and combined callback replies") {
    const std::array<sim::TileXY, 5> origins{{{2, 1}, {1, 0}, {0, 1}, {1, 2}, {1, 1}}};
    for (std::size_t arrival = 0; arrival < 5; ++arrival) {
        Grid g(3, 3);
        auto c = g.cells();
        open(c, origins[arrival]);
        open(c, {1, 1}, 0x8000);
        for (std::size_t row = 0; row < 5; ++row) {
            for (std::size_t k = 0; k < 9; ++k) {
                const auto bit = (k + 2 * row) % 9;
                g.tables.goal_order[row][k] = {static_cast<std::uint16_t>(1U << bit),
                                               static_cast<std::uint8_t>(bit % 3),
                                               static_cast<std::uint8_t>(bit / 3)};
            }
        }
        std::vector<sim::WalkPoint> goals;
        for (std::uint32_t bit = 0; bit < 9; ++bit) {
            goals.push_back(point(1, 1, bit % 3, bit / 3));
        }
        goals.push_back(point(1, 1, 0, 0)); // duplicate point => one mask bit, not another callback
        const std::array starts{sim::WalkPoint{origins[arrival], 1, 1}};
        std::vector<std::uint32_t> visits;
        const auto r = sim::search(c, g.tables, 0, starts, goals, [&](const sim::SearchVisit& v) {
            CHECK((v.high & 0x8400) == 0x8400);
            visits.push_back(v.point.col + 3 * v.point.row);
            return sim::SearchDecision{visits.size() == 1 ? sim::GoalAction::Skip
                                                          : sim::GoalAction::Expand,
                                       visits.size() == 1};
        });
        CHECK(r.error.empty());
        REQUIRE(r.selected);
        CHECK_FALSE(r.stopped);
        REQUIRE(visits.size() == 9);
        for (std::size_t k = 0; k < 9; ++k) {
            CHECK(visits[k] == (k + 2 * arrival) % 9);
        }
        CHECK(r.cost == (arrival == 4 ? 0 : 10));
        CHECK(r.route.size() == (arrival == 4 ? 1 : 2));
        std::uint32_t stop_calls = 0;
        const auto stopped =
            sim::search(c, g.tables, 0, starts, goals, [&](const sim::SearchVisit&) {
                ++stop_calls;
                return sim::SearchDecision{sim::GoalAction::Stop, true};
            });
        CHECK(stopped.error.empty());
        CHECK(stopped.stopped);
        REQUIRE(stopped.selected);
        CHECK(stop_calls == 1);
        CHECK(stopped.selected->point.col + 3 * stopped.selected->point.row == 2 * arrival % 9);
    }
    for (const bool skip : {false, true}) {
        Grid g(3, 1);
        auto c = g.cells();
        for (std::int32_t x = 0; x < 3; ++x) {
            open(c, {x, 0});
        }
        const std::array starts{point(0, 0)};
        const std::array goals{point(1, 0, 0, 0), point(1, 0, 2, 2), point(2, 0)};
        std::vector<sim::WalkPoint> visits;
        const auto r = sim::search(c, g.tables, 0, starts, goals, [&](const sim::SearchVisit& v) {
            visits.push_back(v.point);
            const auto action =
                v.point.tile.x == 2
                    ? sim::GoalAction::Stop
                    : (skip && v.point.col == 0 ? sim::GoalAction::Skip : sim::GoalAction::Expand);
            return sim::SearchDecision{action, true};
        });
        CHECK(r.error.empty());
        CHECK(visits.size() == (skip ? 2 : 3));
        CHECK(r.stopped == !skip);
        REQUIRE(r.selected);
        CHECK(r.selected->point.tile.x == (skip ? 1 : 2));
    }
}
TEST_CASE("walk_search: disc closure missing goal points duplicate starts and errors") {
    Grid g(5, 1);
    sim::SearchArea area;
    area.cx0 = area.cx1 = 0;
    area.cy0 = area.cy1 = 0;
    area.radius = 1;
    area.half = {1, 0};
    // Use the real fill instead of overriding disc-closed high words.
    sim::WalkCells disc(g.island, *g.types, g.tables, g.island.rect(), 4, area);
    CHECK_FALSE(run(disc, g.tables, point(0, 0), point(3, 0)).selected);
    auto c = g.cells();
    for (std::int32_t x = 0; x < 5; ++x) {
        open(c, {x, 0});
    }
    c.set({2, 0}, {0xfff, 0x400}); // bit10 but no goal point => still expanded
    CHECK(run(c, g.tables, point(0, 0), point(4, 0)).cost == 40);
    const std::array starts{point(0, 0), point(0, 0), point(-1, 0), point(99, 0),
                            point(0, 0, 3, 0)};
    const std::array goals{point(4, 0)};
    const auto duplicate = sim::search(c, g.tables, 0, starts, goals, first);
    CHECK(duplicate.enqueued == 6);
    CHECK(duplicate.route.size() == 5);
    CHECK_FALSE(sim::search(c, g.tables, 99, starts, goals, first).error.empty());
    CHECK_FALSE(sim::search(c, g.tables, 0, starts, goals, {}).error.empty());
    auto bad = g.tables;
    bad.layers[0][7] = 1;
    CHECK_FALSE(sim::search(c, bad, 0, starts, goals, first).error.empty());
    bad = g.tables;
    bad.goal_order[0][0].mask = 3;
    CHECK_FALSE(sim::search(c, bad, 0, starts, goals, first).error.empty());
}
TEST_CASE("walk_search: generated grids are deterministic and never enter closed admission") {
    std::uint32_t random = 0x155033;
    for (std::uint32_t trial = 0; trial < 100; ++trial) {
        Grid g(5, 5);
        auto c = g.cells();
        std::vector<sim::WalkPoint> goals;
        std::vector<sim::WalkCell> before;
        for (std::int32_t y = 0; y < 5; ++y) {
            for (std::int32_t x = 0; x < 5; ++x) {
                random = random * 1664525U + 1013904223U;
                const auto code = static_cast<std::uint16_t>((random & 7) == 0 ? 6 : 0);
                open(c, {x, y}, code, static_cast<std::uint16_t>((random >> 5) & 0xfff));
                before.push_back(c.get({x, y}));
                goals.push_back(point(x, y));
            }
        }
        const std::array starts{point(2, 2)};
        std::vector<sim::SearchVisit> a, b;
        const auto one = sim::search(c, g.tables, 0, starts, goals, [&](const sim::SearchVisit& v) {
            a.push_back(v);
            return sim::SearchDecision{sim::GoalAction::Expand, true};
        });
        const auto two = sim::search(c, g.tables, 0, starts, goals, [&](const sim::SearchVisit& v) {
            b.push_back(v);
            return sim::SearchDecision{sim::GoalAction::Expand, true};
        });
        CHECK(one.error.empty());
        CHECK(one == two);
        CHECK(a == b);
        CHECK(one.popped <= 25);
        for (const auto& v : a) {
            const auto index = static_cast<std::size_t>(v.point.tile.y * 5 + v.point.tile.x);
            if (v.point.tile != sim::TileXY{2, 2}) {
                CHECK(sim::admission(g.tables, 0, before[index].high) != 0);
            }
        }
        for (std::int32_t y = 0; y < 5; ++y) {
            for (std::int32_t x = 0; x < 5; ++x) {
                CHECK(c.get({x, y}) == before[static_cast<std::size_t>(y * 5 + x)]);
            }
        }
    }
}

TEST_CASE("walk_search: unsupported records are reported and coordinate extremes stay bounded") {
    Grid g(2, 1);
    sim::BuildingType partial;
    partial.id = sim::TypeId{100};
    partial.walk_mask = {7, 0, 0};
    sim::TypeRegistry::Builder b;
    b.add(partial);
    auto types = b.build();
    REQUIRE(types);
    g.types = types.value();
    g.island.tiles.set_object(1, 100);
    sim::WalkCells unsupported(g.island, *g.types, g.tables, g.island.rect());
    const auto r = run(unsupported, g.tables, point(0, 0), point(1, 0));
    CHECK(r.error.empty());
    CHECK_FALSE(r.complete);
    CHECK_FALSE(r.selected);
    const auto start = run(unsupported, g.tables, point(1, 0), point(0, 0));
    CHECK_FALSE(start.complete);
    CHECK(start.enqueued == 0);
    Grid edge(2, 1);
    const auto x = std::numeric_limits<std::int32_t>::max(),
               y = std::numeric_limits<std::int32_t>::min();
    REQUIRE(edge.island.tiles.reset(x - 1, y, 2, 1, 4));
    sim::WalkCells cells(edge.island, *edge.types, edge.tables, edge.island.rect());
    const auto extreme = run(cells, edge.tables, point(x, y), point(x - 1, y));
    CHECK(extreme.error.empty());
    REQUIRE(extreme.selected);
    CHECK(extreme.cost == 10);
    CHECK(extreme.route.size() == 2);
}
