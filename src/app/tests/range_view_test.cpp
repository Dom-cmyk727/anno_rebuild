// UI-RANGE-1 B1: the lit city and the dark tiles of the range display (range_view.hpp,
// docs/re/ui.md section 12).
#include <cstdint>
#include <optional>

#include <doctest/doctest.h>

#include <anno/app/range_view.hpp>

namespace {

using anno::app::RangeCity;
using anno::app::RangeInput;
using anno::app::RangeTriggers;
using anno::app::TerritoryIsland;
using anno::app::TerritoryView;
using anno::sim::TileXY;

constexpr std::uint32_t kNeutral = 15;

/// Island 0 at (10, 10), 4 x 2: city 1 owns the left two columns, city 2 the third, the
/// fourth is neutral. Island 1 at (30, 10), 2 x 2, all city 1 (another island's city 1).
TerritoryView two_islands() {
    TerritoryView v;
    v.neutral_owner = kNeutral;
    TerritoryIsland a;
    a.index = 0;
    a.x0 = 10;
    a.y0 = 10;
    a.w = 4;
    a.h = 2;
    a.owner = {1, 1, 2, kNeutral, 1, 1, 2, kNeutral};
    TerritoryIsland b;
    b.index = 1;
    b.x0 = 30;
    b.y0 = 10;
    b.w = 2;
    b.h = 2;
    b.owner = {1, 1, 1, 1};
    v.islands = {a, b};
    return v;
}

} // namespace

TEST_CASE("range: a selected Kontor lights its city and darkens every other tile") {
    const TerritoryView v = two_islands();
    RangeInput in;
    in.selected_prodtype = 35; // KONTOR
    in.selected_tile = TileXY{11, 10};
    const auto c = anno::app::range_city(in, RangeTriggers{}, v);
    REQUIRE(c.has_value());
    CHECK(*c == RangeCity{0, 1});
    CHECK_FALSE(anno::app::range_dark(v, 0, 0, *c)); // city 1
    CHECK(anno::app::range_dark(v, 0, 2, *c));       // city 2
    CHECK(anno::app::range_dark(v, 0, 3, *c));       // neutral
    CHECK(anno::app::range_dark(v, 1, 0, *c));       // the other island's city 1
    CHECK(anno::app::range_dark_count(v, *c) == 4 + 4);
}

TEST_CASE("range: Kontor and market show the same combined range (one owner test)") {
    const TerritoryView v = two_islands();
    RangeInput kontor;
    kontor.selected_prodtype = 35;
    kontor.selected_tile = TileXY{10, 10};
    RangeInput market;
    market.selected_prodtype = 33;
    market.selected_tile = TileXY{11, 11};
    const auto a = anno::app::range_city(kontor, RangeTriggers{}, v);
    const auto b = anno::app::range_city(market, RangeTriggers{}, v);
    REQUIRE(a.has_value());
    CHECK(a == b);
}

TEST_CASE("range: an armed market lights the city under the pointer, over the selection") {
    const TerritoryView v = two_islands();
    RangeInput in;
    in.selected_prodtype = 35;
    in.selected_tile = TileXY{10, 10}; // city 1
    in.armed_prodtype = 33;
    in.pointer_tile = TileXY{12, 11}; // city 2
    const auto c = anno::app::range_city(in, RangeTriggers{}, v);
    REQUIRE(c.has_value());
    CHECK(*c == RangeCity{0, 2});
    in.pointer_tile = TileXY{13, 10}; // neutral: nothing lit (OQ-797)
    CHECK_FALSE(anno::app::range_city(in, RangeTriggers{}, v).has_value());
    in.pointer_tile = TileXY{50, 50}; // open sea
    CHECK_FALSE(anno::app::range_city(in, RangeTriggers{}, v).has_value());
}

TEST_CASE("range: an armed road darkens, and the trigger list is data") {
    const TerritoryView v = two_islands();
    RangeInput in;
    in.armed_road = true;
    in.pointer_tile = TileXY{31, 11};
    const auto c = anno::app::range_city(in, RangeTriggers{}, v);
    REQUIRE(c.has_value());
    CHECK(*c == RangeCity{1, 1});
    RangeTriggers off;
    off.armed_road = false;
    CHECK_FALSE(anno::app::range_city(in, off, v).has_value());
}

TEST_CASE("range: nothing selected or armed, or an unlisted type, darkens nothing") {
    const TerritoryView v = two_islands();
    CHECK_FALSE(anno::app::range_city(RangeInput{}, RangeTriggers{}, v).has_value());
    RangeInput house;
    house.selected_prodtype = 1; // not in the selected list
    house.selected_tile = TileXY{10, 10};
    house.armed_prodtype = 1; // not in the armed list
    house.pointer_tile = TileXY{10, 10};
    CHECK_FALSE(anno::app::range_city(house, RangeTriggers{}, v).has_value());
}

TEST_CASE("range: two cities on one island each light only their own tiles") {
    const TerritoryView v = two_islands();
    const RangeCity one{0, 1};
    const RangeCity two{0, 2};
    CHECK(anno::app::range_dark_count(v, one) == 4 + 4);
    CHECK(anno::app::range_dark_count(v, two) == 6 + 4);
    CHECK(anno::app::range_dark(v, 0, 0, two));
    CHECK_FALSE(anno::app::range_dark(v, 0, 6, two));
}
