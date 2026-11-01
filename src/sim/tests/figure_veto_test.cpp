// The nearby-figure veto predicate (task MAP-2 package 2; docs/sim/ship-founding.md P1b and P12,
// docs/re/ship-founding.md section 11.4/12.2). Every expected value is derived by hand from the
// original's rules, not from the implementation. Geometry used throughout: the footprint is
// origin (10, 12), 4 x 4, i.e. the half-open rectangle x [10, 14), y [12, 16) and corners
// (10,12) (14,12) (14,16) (10,16); the placing player is 0; the query rectangle grown by 10 is
// x [0, 24), y [2, 26) on the TRUNCATED position. Positions are written in 1/256 tile.
// Each cause-specific row has a valid control that reaches the same decision, differs in the one
// input under test, and asserts the specific verdict, reason or missing fact.
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/figure_veto.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>

#include "island_helpers.hpp"

namespace sim = anno::sim;
namespace veto = anno::sim::veto;
namespace site = anno::sim::site;
namespace syn = anno::sim::synthetic;

namespace {

constexpr std::int32_t T = 256; // one tile
const sim::PlayerId kMe{0};
const sim::PlayerId kRival{1};
const site::Origin kOrigin{10, 12};

constexpr std::uint32_t kScout = 97, kSoldier = 99, kMachineLo = 108, kMachineHi = 112, kShip = 113,
                        kLastQueried = 128, kDonkey = 53;
constexpr std::uint32_t kHull15 = 384;   // 1.5 tile, exact in Q24.8 and f32
constexpr std::uint32_t kRange7 = 7 * T; // scout range, exact
constexpr std::uint32_t kRange16 = 16 * T;

/// A fully specified candidate: registered in a cell, owned by `owner`, usable, with every
/// optional fact supplied. Tests change one field.
veto::NearbyFigure fig(std::uint32_t kind, std::int32_t x, std::int32_t y,
                       sim::PlayerId owner = kMe) {
    veto::NearbyFigure f;
    f.id = sim::FigureId{1};
    f.registration = veto::Registration::SectorCell;
    f.pos = sim::FixedPos{x, y};
    f.kind = kind;
    f.state_word = 0;
    f.owner = {veto::Fact::True, owner};
    f.crew_occupied = veto::Fact::True;
    f.relation_war = 0;
    f.owner_property_1c = 0;
    f.range_88 = kRange7;
    f.hull_half_88 = kHull15;
    return f;
}

/// A hostile owner's figure: the rival, a non-zero kind-2 relation, property 0x1c not 0x11.
veto::NearbyFigure hostile(std::uint32_t kind, std::int32_t x, std::int32_t y,
                           std::uint32_t range = kRange7) {
    veto::NearbyFigure f = fig(kind, x, y, kRival);
    f.relation_war = 3;
    f.owner_property_1c = 5;
    f.range_88 = range;
    return f;
}

veto::Result check(const std::vector<veto::NearbyFigure>& v, const veto::Rules& rules = {}) {
    return veto::nearby_figures(kMe, kOrigin, 4, 4, v, rules);
}

veto::Result one(const veto::NearbyFigure& f) {
    return check({f});
}

void expect_clear(const veto::NearbyFigure& f) {
    const veto::Result r = one(f);
    CHECK(r.verdict == veto::Verdict::Clear);
    CHECK(r.reason == veto::Reason::None);
    CHECK(r.missing == veto::Missing::None);
}

void expect_veto(const veto::NearbyFigure& f, veto::Reason why) {
    const veto::Result r = one(f);
    CHECK(r.verdict == veto::Verdict::Vetoed);
    CHECK(r.reason == why);
    CHECK(r.index == 0);
}

void expect_missing(const veto::NearbyFigure& f, veto::Missing what) {
    const veto::Result r = one(f);
    CHECK(r.verdict == veto::Verdict::MissingFact);
    CHECK(r.missing == what);
    CHECK(r.reason == veto::Reason::None);
}

} // namespace

TEST_CASE("veto: the selected ship's bit 0x02 rejects it, independent of the nearby scan") {
    // Valid control: state word 0 passes.
    CHECK(veto::selected_ship_state_allows(0));
    // Only bit 0x02 changed: rejected.
    CHECK_FALSE(veto::selected_ship_state_allows(0x02));
    // Other bits of the query's exclusion mask (0x08) or of the unusable mask (0x04, 0x2000) do not
    // reject the selected ship: the predicate tests exactly 0x02 (AnnoFrame.dll 0x10048d54).
    CHECK(veto::selected_ship_state_allows(0x08));
    CHECK(veto::selected_ship_state_allows(0x04));
    CHECK(veto::selected_ship_state_allows(0x2000));
    CHECK(veto::selected_ship_state_allows(0xfffffffd));
    CHECK_FALSE(veto::selected_ship_state_allows(0xffffffff));
    // The same ship with the bit set is EXCLUDED from the nearby scan (mask 0x0a): the scan
    // is Clear although the ship stands on the footprint, so excluding it from the query does
    // not validate it; the two predicates answer different questions.
    veto::NearbyFigure ship = fig(kShip, 12 * T, 14 * T);
    expect_veto(ship, veto::Reason::PointInFootprint);
    ship.state_word = 0x02;
    expect_clear(ship);
    CHECK_FALSE(veto::selected_ship_state_allows(ship.state_word));
    // The bit is data: another rule set changes the struct.
    veto::Rules rules;
    rules.selected_ship_reject_bit = 0x40;
    CHECK(veto::selected_ship_state_allows(0x02, rules));
    CHECK_FALSE(veto::selected_ship_state_allows(0x40, rules));
}

TEST_CASE("veto: the query mask 0x0a excludes, 0x04 and 0x2000 only make a figure unusable") {
    // Control: an own ship on the footprint vetoes (path B, point in the rectangle).
    veto::NearbyFigure own = fig(kShip, 12 * T, 14 * T);
    expect_veto(own, veto::Reason::PointInFootprint);
    for (const std::uint32_t bit : {0x02U, 0x08U, 0x0aU}) {
        veto::NearbyFigure f = own;
        f.state_word = bit;
        expect_clear(f); // not returned by the query
    }
    // Bits 0x04 and 0x2000 are NOT in the query mask: the figure is still returned, merely not
    // usable, so it takes path B and still vetoes by position.
    for (const std::uint32_t bit : {0x04U, 0x2000U}) {
        veto::NearbyFigure f = own;
        f.state_word = bit;
        expect_veto(f, veto::Reason::PointInFootprint);
    }
    // The consequence of "not usable": a hostile ship in range of a corner vetoes (path A) ...
    veto::NearbyFigure h = hostile(kShip, 14 * T + 5 * T, 12 * T, kRange16); // 5 from (14,12)
    expect_veto(h, veto::Reason::HostileInRange);
    // ... but the same ship with bit 0x04 or 0x2000 is unusable, takes path B, is outside the
    // rectangle and its hull box (x 17.5..20.5 -> 17..20) does not reach it: Clear.
    for (const std::uint32_t bit : {0x04U, 0x2000U}) {
        veto::NearbyFigure f = h;
        f.state_word = bit;
        expect_clear(f);
    }
    // Bits outside both masks change nothing (0x01, 0x10, 0x4000).
    for (const std::uint32_t bit : {0x01U, 0x10U, 0x4000U}) {
        veto::NearbyFigure f = h;
        f.state_word = bit;
        expect_veto(f, veto::Reason::HostileInRange);
    }
}

TEST_CASE("veto: the class groups 97..128 are queried, nothing else") {
    // Valid control: kind 97 on the footprint vetoes.
    expect_veto(fig(kScout, 12 * T, 14 * T), veto::Reason::PointInFootprint);
    expect_veto(fig(kLastQueried, 12 * T, 14 * T), veto::Reason::PointInFootprint);
    expect_veto(fig(kSoldier, 12 * T, 14 * T), veto::Reason::PointInFootprint);
    // Defects: 96 (below), 129 (above) and the donkey (BEWOHNTYPE 53, group 3) are not queried.
    expect_clear(fig(96, 12 * T, 14 * T));
    expect_clear(fig(129, 12 * T, 14 * T));
    expect_clear(fig(kDonkey, 12 * T, 14 * T));
    expect_clear(fig(0, 12 * T, 14 * T));
    // The queried range is data.
    veto::Rules rules;
    rules.query_kind_min = 96;
    CHECK(check({fig(96, 12 * T, 14 * T)}, rules).verdict == veto::Verdict::Vetoed);
}

TEST_CASE("veto: the query rectangle is the footprint grown by 10, on the truncated position") {
    // A hostile scout (range 7) 7 tiles west of corner (10,12) is in range: control.
    // Position x = 3 tiles, y = 12.
    expect_veto(hostile(kScout, 3 * T, 12 * T), veto::Reason::HostileInRange);
    // Query lower bound x >= 0 on trunc(X): raw -255 truncates to 0 (included), raw -256 to -1
    // (excluded). With range 16 the figure at x = -255/256 is 10.996 from corner (10,12): in
    // range, so the veto proves it was returned; x = -1 tile is 11 from the corner: in range
    // too, so its Clear proves the query excluded it.
    expect_veto(hostile(kShip, -255, 12 * T, kRange16), veto::Reason::HostileInRange);
    expect_clear(hostile(kShip, -256, 12 * T, kRange16));
    // Upper bound x < 24 (= 14 + 10): raw 24*256 - 1 is 23.996 -> 23 (included), 24*256 -> 24
    // (excluded). Distance to corner (14,12): 9.996 and 10 -> both in range 16.
    expect_veto(hostile(kShip, 24 * T - 1, 12 * T, kRange16), veto::Reason::HostileInRange);
    expect_clear(hostile(kShip, 24 * T, 12 * T, kRange16));
    // Same on y: lower bound 12 - 10 = 2, upper 16 + 10 = 26.
    expect_veto(hostile(kShip, 12 * T, 2 * T, kRange16), veto::Reason::HostileInRange);
    expect_clear(hostile(kShip, 12 * T, 2 * T - 1, kRange16)); // trunc(1.996) = 1
    expect_veto(hostile(kShip, 12 * T, 26 * T - 1, kRange16), veto::Reason::HostileInRange);
    expect_clear(hostile(kShip, 12 * T, 26 * T, kRange16));
    // The margin is data: with margin 0 the figure 7 tiles west of the corner is not queried.
    veto::Rules narrow;
    narrow.query_margin = 0;
    CHECK(check({hostile(kScout, 3 * T, 12 * T)}, narrow).verdict == veto::Verdict::Clear);
    CHECK(check({hostile(kScout, 3 * T, 12 * T)}).verdict == veto::Verdict::Vetoed);
}

TEST_CASE("veto: own, friendly and hostile owners take different branches") {
    // Own scout outside the rectangle: nothing in the way.
    expect_clear(fig(kScout, 9 * T, 14 * T));
    // Own scout inside: point rule (x0 inclusive, x1 exclusive).
    expect_veto(fig(kScout, 10 * T, 12 * T), veto::Reason::PointInFootprint);
    expect_clear(fig(kScout, 14 * T, 12 * T));
    expect_clear(fig(kScout, 10 * T, 16 * T));
    expect_veto(fig(kScout, 14 * T - 1, 16 * T - 1), veto::Reason::PointInFootprint);
    expect_clear(fig(kScout, 10 * T - 1, 12 * T)); // 9.996 < 10
    // Friendly rival (relation byte 0) takes the point rule, not the range rule: 4 tiles from a
    // corner and outside the rectangle is Clear, inside the rectangle vetoes.
    veto::NearbyFigure friendly = fig(kSoldier, 18 * T, 12 * T, kRival);
    friendly.relation_war = 0;
    expect_clear(friendly);
    friendly.pos = sim::FixedPos{11 * T, 13 * T};
    expect_veto(friendly, veto::Reason::PointInFootprint);
    // The same rival with a non-zero relation byte (the hostile branch, any non-zero value) is in
    // range of the corner (14,12) when 4 tiles away: veto by range.
    for (const std::uint8_t rel : {std::uint8_t{1}, std::uint8_t{3}, std::uint8_t{255}}) {
        veto::NearbyFigure h = hostile(kSoldier, 18 * T, 12 * T);
        h.relation_war = rel;
        expect_veto(h, veto::Reason::HostileInRange);
    }
    // Relation 0 is the single "not hostile" value: control above.
    // A figure with no owner object takes the point rule too.
    veto::NearbyFigure orphan = hostile(kSoldier, 18 * T, 12 * T);
    orphan.owner = {veto::Fact::False, {}};
    expect_clear(orphan);
    orphan.pos = sim::FixedPos{11 * T, 13 * T};
    expect_veto(orphan, veto::Reason::PointInFootprint);
    // Property 0x1c == 0x11 SKIPS a usable, owned, hostile figure (AnnoFrame.dll 0x10045238 jumps
    // to the loop increment): it neither takes path A nor falls to path B. Control: 0x10 and 0x12
    // veto by range. Inside the rectangle (where path B would veto) the skipped figure is Clear.
    veto::NearbyFigure exempt = hostile(kSoldier, 18 * T, 12 * T);
    exempt.owner_property_1c = 0x11;
    expect_clear(exempt);
    exempt.owner_property_1c = 0x10;
    expect_veto(exempt, veto::Reason::HostileInRange);
    exempt.owner_property_1c = 0x12;
    expect_veto(exempt, veto::Reason::HostileInRange);
    veto::NearbyFigure skipped = hostile(kSoldier, 11 * T, 13 * T);
    skipped.owner_property_1c = 0x10;
    expect_veto(skipped, veto::Reason::HostileInRange); // control: in range of a corner
    skipped.owner_property_1c = 0x11;
    expect_clear(skipped); // not PointInFootprint
    // A hostile ship whose hull overlaps (and whose range is 0, so path A cannot hit): skipped too.
    veto::NearbyFigure hull = hostile(kShip, 9 * T + T / 2, 14 * T, 0);
    hull.owner_property_1c = 0x10;
    expect_clear(hull); // control: path A with range 0 misses every corner, and A never falls to B
    hull.relation_war = 0;
    expect_veto(hull, veto::Reason::HullOverlap); // control: the non-hostile branch sees the hull
    hull.relation_war = 3;
    hull.owner_property_1c = 0x11;
    expect_clear(hull);
    // The exemption value is data.
    veto::Rules rules;
    rules.exempt_owner_property = 0x12;
    exempt.owner_property_1c = 0x12;
    CHECK(check({exempt}, rules).verdict == veto::Verdict::Clear);
}

TEST_CASE("veto: the hostile branch measures corners with equality as a hit, not the rectangle") {
    // Range 7 around each of the four corners separately; the hit position is exactly 7 away on one
    // axis and no other corner is within range.
    expect_veto(hostile(kScout, 3 * T, 12 * T), veto::Reason::HostileInRange);  // (10,12)
    expect_veto(hostile(kScout, 21 * T, 12 * T), veto::Reason::HostileInRange); // (14,12)
    expect_veto(hostile(kScout, 21 * T, 16 * T), veto::Reason::HostileInRange); // (14,16)
    expect_veto(hostile(kScout, 3 * T, 16 * T), veto::Reason::HostileInRange);  // (10,16)
    // One raw unit further is 7.0039 tiles: out of range (the equality is the boundary).
    expect_clear(hostile(kScout, 3 * T - 1, 12 * T));
    expect_clear(hostile(kScout, 21 * T + 1, 12 * T));
    // Euclidean, not Chebyshev: (19, 7) is 5 east and 5 north of corner (14,12): sqrt(50) = 7.07.
    expect_clear(hostile(kScout, 19 * T, 7 * T));
    // (18, 7): sqrt(16 + 25) = 6.4 -> hit.
    expect_veto(hostile(kScout, 18 * T, 7 * T), veto::Reason::HostileInRange);
    // A 3-4-5 triangle: 4 east, 3 north of the corner with range 5 is exactly on the boundary.
    expect_veto(hostile(kScout, 18 * T, 9 * T, 5 * T), veto::Reason::HostileInRange);
    expect_clear(hostile(kScout, 18 * T + 1, 9 * T, 5 * T));
    // The hostile branch does not look at the rectangle: a hostile soldier in the middle of the
    // footprint with range 1 is 2.83 from every corner and does NOT veto (path A), whereas the same
    // figure on the non-hostile branch does.
    expect_clear(hostile(kSoldier, 12 * T, 14 * T, 1 * T)); // path A never falls to path B
    veto::NearbyFigure friendly = hostile(kSoldier, 12 * T, 14 * T, 1 * T);
    friendly.relation_war = 0;
    expect_veto(friendly, veto::Reason::PointInFootprint);
    // Range 0 never hits a corner except at distance 0 (equality).
    expect_veto(hostile(kScout, 10 * T, 12 * T, 0), veto::Reason::HostileInRange);
    expect_clear(hostile(kScout, 10 * T + 1, 12 * T, 0));
}

TEST_CASE("veto: the non-hostile hull box, exact boundary contacts") {
    // Own ship, hull half-extent 1.5 tiles (exact). Overlap iff xmin < x1 && xmax > x0 && ymin < y1
    // && ymax > y0 with each bound truncated toward zero. Position (X, 14) for the west contact:
    // xmax = trunc(X + 1.5) > 10  <=>  X >= 9.5.
    veto::NearbyFigure s = fig(kShip, 9 * T + T / 2, 14 * T); // X = 9.5, outside the rectangle
    expect_veto(s, veto::Reason::HullOverlap);
    s.pos.x = 9 * T + T / 2 - 1; // 9.496: xmax = trunc(10.996) = 10, not > 10
    expect_clear(s);
    // East: xmin = trunc(X - 1.5) < 14  <=>  X < 15.5.
    s.pos.x = 15 * T + T / 2 - 1; // 15.496 -> 13.996 -> 13 < 14
    expect_veto(s, veto::Reason::HullOverlap);
    s.pos.x = 15 * T + T / 2; // 15.5 -> 14, not < 14
    expect_clear(s);
    // North: ymax = trunc(Y + 1.5) > 12  <=>  Y + 1.5 >= 13  <=>  Y >= 11.5.
    s = fig(kShip, 12 * T, 11 * T + T / 2);
    expect_veto(s, veto::Reason::HullOverlap);
    s.pos.y -= 1;
    expect_clear(s);
    // South: ymin = trunc(Y - 1.5) < 16  <=>  Y < 17.5.
    s = fig(kShip, 12 * T, 17 * T + T / 2 - 1);
    expect_veto(s, veto::Reason::HullOverlap);
    s.pos.y += 1;
    expect_clear(s);
    // A diagonal approach overlaps only when both axes do: (9.5, 11.496) misses on y.
    s = fig(kShip, 9 * T + T / 2, 11 * T + T / 2 - 1);
    expect_clear(s);
    s = fig(kShip, 9 * T + T / 2, 11 * T + T / 2);
    expect_veto(s, veto::Reason::HullOverlap);
    // The hull box exists for BEWOHNTYPE 113..128 only: a scout and a soldier at the contact
    // position are clear, 112 and 129 too, 113 and 128 overlap.
    for (const std::uint32_t k : {kScout, kSoldier, 112U}) {
        veto::NearbyFigure f = fig(k, 9 * T + T / 2, 14 * T);
        f.crew_occupied = veto::Fact::True;
        expect_clear(f);
    }
    expect_veto(fig(113, 9 * T + T / 2, 14 * T), veto::Reason::HullOverlap);
    expect_veto(fig(128, 9 * T + T / 2, 14 * T), veto::Reason::HullOverlap);
    expect_clear(fig(129, 9 * T + T / 2, 14 * T));
    // A hull-less rule set: changing the hull range is data.
    veto::Rules rules;
    rules.hull_kind_min = 99;
    CHECK(check({fig(kSoldier, 9 * T + T / 2, 14 * T)}, rules).reason == veto::Reason::HullOverlap);
    // The founding ship inside the rectangle: the point rule fires first (reason Point).
    expect_veto(fig(kShip, 12 * T, 14 * T), veto::Reason::PointInFootprint);
}

TEST_CASE("veto: war machines need a crew, ships and soldiers do not") {
    // Hostile machine in range of corner (14,12), 4 tiles away, crew present: path A.
    for (const std::uint32_t k : {kMachineLo, 110U, kMachineHi}) {
        veto::NearbyFigure m = hostile(k, 18 * T, 12 * T);
        m.crew_occupied = veto::Fact::True;
        expect_veto(m, veto::Reason::HostileInRange);
        // No crew: unusable, path B: outside the rectangle, no hull (kind < 113): Clear.
        m.crew_occupied = veto::Fact::False;
        expect_clear(m);
        // Unknown crew is undecidable, never allowed.
        m.crew_occupied = veto::Fact::Unknown;
        expect_missing(m, veto::Missing::CrewOccupied);
    }
    // Without a crew the machine still vetoes by position inside the rectangle.
    veto::NearbyFigure inside = hostile(kMachineLo, 12 * T, 14 * T);
    inside.crew_occupied = veto::Fact::False;
    expect_veto(inside, veto::Reason::PointInFootprint);
    // The crew fact is irrelevant outside 108..112 (107 and 113): Unknown does not block.
    for (const std::uint32_t k : {107U, 113U}) {
        veto::NearbyFigure f = hostile(k, 18 * T, 12 * T);
        f.crew_occupied = veto::Fact::Unknown;
        expect_veto(f, veto::Reason::HostileInRange);
    }
    // A machine with state bit 0x04 is unusable before the crew is consulted: the crew fact
    // Unknown does not matter.
    veto::NearbyFigure bit = hostile(kMachineLo, 18 * T, 12 * T);
    bit.state_word = 0x04;
    bit.crew_occupied = veto::Fact::Unknown;
    expect_clear(bit);
}

TEST_CASE("veto: embarked figures are not returned by the spatial query") {
    // Valid control: a scout registered in a cell on the footprint vetoes.
    veto::NearbyFigure scout = fig(kScout, 12 * T, 14 * T);
    expect_veto(scout, veto::Reason::PointInFootprint);
    // Embarked (created without a position): the query cannot return it, so it never vetoes,
    // wherever its (inherited) position is.
    scout.registration = veto::Registration::Embarked;
    expect_clear(scout);
    // Unknown registration is undecidable once the cheaper query conditions have passed ...
    scout.registration = veto::Registration::Unknown;
    expect_missing(scout, veto::Missing::Registration);
    // ... but a figure the position/kind/state conditions exclude is decidable without it.
    scout.pos = sim::FixedPos{40 * T, 14 * T};
    expect_clear(scout);
    scout.pos = sim::FixedPos{12 * T, 14 * T};
    scout.kind = kDonkey;
    expect_clear(scout);
    scout.kind = kScout;
    scout.state_word = 0x02;
    expect_clear(scout);
}

TEST_CASE("veto: a missing fact is never read as allowed") {
    veto::NearbyFigure base = hostile(kSoldier, 18 * T, 12 * T);
    expect_veto(base, veto::Reason::HostileInRange); // control: all facts supplied

    veto::NearbyFigure f = base;
    f.owner.present = veto::Fact::Unknown;
    expect_missing(f, veto::Missing::OwnerPresence);

    f = base;
    f.owner.player = sim::PlayerId{}; // present, but no valid player
    expect_missing(f, veto::Missing::OwnerPlayer);

    f = base;
    f.relation_war.reset();
    expect_missing(f, veto::Missing::RelationWar);

    f = base;
    f.owner_property_1c.reset();
    expect_missing(f, veto::Missing::OwnerProperty1c);

    f = base;
    f.range_88.reset();
    expect_missing(f, veto::Missing::Range);

    // A fact that is not reached is not required: an own figure needs no relation, property or
    // range; a figure without an owner needs none of them either; a non-hostile ship outside the
    // rectangle needs a hull only because it has one.
    f = fig(kScout, 9 * T, 14 * T);
    f.relation_war.reset();
    f.owner_property_1c.reset();
    f.range_88.reset();
    f.hull_half_88.reset();
    expect_clear(f);
    f = fig(kShip, 20 * T, 14 * T);
    f.hull_half_88.reset();
    expect_missing(f, veto::Missing::HullHalfExtent);
    // ... unless the point rule already vetoes: a veto stands without the hull.
    f.pos = sim::FixedPos{12 * T, 14 * T};
    expect_veto(f, veto::Reason::PointInFootprint);

    // Mixed lists: a veto beats an earlier missing fact; a missing fact beats Clear.
    veto::NearbyFigure missing_one = base;
    missing_one.range_88.reset();
    veto::NearbyFigure vetoing = fig(kScout, 12 * T, 14 * T);
    veto::NearbyFigure fine = fig(kScout, 9 * T, 14 * T);
    veto::Result r = check({missing_one, vetoing});
    CHECK(r.verdict == veto::Verdict::Vetoed);
    CHECK(r.index == 1);
    r = check({fine, missing_one, fine});
    CHECK(r.verdict == veto::Verdict::MissingFact);
    CHECK(r.index == 1);
    CHECK(r.missing == veto::Missing::Range);
    r = check({fine, fine});
    CHECK(r.verdict == veto::Verdict::Clear);
    // An empty candidate list is Clear: nothing queried, nothing in the way.
    CHECK(check({}).verdict == veto::Verdict::Clear);
}

TEST_CASE("veto: no 256-result cap, a veto beyond the former cap is found") {
    // 299 harmless own scouts (queried, outside the rectangle, point rule) and one vetoing figure
    // at index 299. Control: without it the result is Clear.
    std::vector<veto::NearbyFigure> v(299, fig(kScout, 9 * T, 14 * T));
    CHECK(check(v).verdict == veto::Verdict::Clear);
    v.push_back(fig(kScout, 12 * T, 14 * T));
    const veto::Result r = check(v);
    CHECK(r.verdict == veto::Verdict::Vetoed);
    CHECK(r.index == 299);
    CHECK(r.reason == veto::Reason::PointInFootprint);
    // 1000 own soldiers outside the rectangle, then a hostile ship in range of a corner at
    // index 1000: found.
    std::vector<veto::NearbyFigure> w(1000, fig(kSoldier, 9 * T, 14 * T));
    w.push_back(hostile(kShip, 18 * T, 12 * T, kRange16));
    const veto::Result r2 = check(w);
    CHECK(r2.verdict == veto::Verdict::Vetoed);
    CHECK(r2.index == 1000);
    CHECK(r2.reason == veto::Reason::HostileInRange);
    // The reported index is the first veto, not the last.
    w.insert(w.begin() + 600, fig(kScout, 11 * T, 13 * T));
    CHECK(check(w).index == 600);
}

TEST_CASE("veto: extreme coordinates neither wrap nor veto") {
    veto::NearbyFigure f = fig(kScout, 12 * T, 14 * T);
    const std::int64_t big = std::numeric_limits<std::int64_t>::max();
    const std::int64_t small = std::numeric_limits<std::int64_t>::min();
    // A footprint at an absurd origin has nothing near the figure.
    for (const std::int64_t o : {big, small, big - 3, small + 3}) {
        const veto::Result r = veto::nearby_figures(kMe, site::Origin{o, o}, 4, 4,
                                                    std::span<const veto::NearbyFigure>(&f, 1));
        CHECK(r.verdict == veto::Verdict::Clear);
    }
    // Extreme figure positions with a normal footprint.
    veto::NearbyFigure e = hostile(kShip, std::numeric_limits<std::int32_t>::max(),
                                   std::numeric_limits<std::int32_t>::min(), 0xffffffffU);
    CHECK(one(e).verdict == veto::Verdict::Clear);
    // A huge range around a far corner cannot overflow the squared comparison.
    veto::NearbyFigure far = hostile(kShip, 12 * T, 14 * T, 0xffffffffU);
    expect_veto(far, veto::Reason::HostileInRange);
}

TEST_CASE("veto: the predicate reads and changes no world state and draws nothing") {
    sim::ScenarioSpec spec = syn::empty_scenario(1234);
    sim::ScenarioSpec::PlayerSpec ps;
    ps.id = sim::PlayerId{0};
    spec.players.push_back(ps);
    spec.islands.push_back(syn::plain_island(0, 40, 24, 6, 4));
    sim::World w = island_test::make_world(spec, {});
    const auto bytes = w.save();
    const std::uint64_t hash = w.state_hash();
    const std::uint32_t rng = w.rng().cursor();

    std::vector<veto::NearbyFigure> v = {fig(kScout, 12 * T, 14 * T),
                                         hostile(kShip, 18 * T, 12 * T)};
    const std::vector<veto::NearbyFigure> copy = v;
    const veto::Result r = check(v);
    CHECK(r.verdict == veto::Verdict::Vetoed);
    // The inputs are byte-for-byte what they were (field by field).
    for (std::size_t i = 0; i < v.size(); ++i) {
        CHECK(v[i].pos.x == copy[i].pos.x);
        CHECK(v[i].pos.y == copy[i].pos.y);
        CHECK(v[i].kind == copy[i].kind);
        CHECK(v[i].state_word == copy[i].state_word);
        CHECK(v[i].range_88 == copy[i].range_88);
    }
    CHECK(check(v) == r); // repeatable
    CHECK(w.save() == bytes);
    CHECK(w.state_hash() == hash);
    CHECK(w.rng().cursor() == rng);
}

TEST_CASE("veto: the scan rectangle of an odd rotation is B by B, the original's extents") {
    // AnnoFrame.dll 0x100450bc-0x100450f9: even rotations (A, B); rotation 1 and 3 (B, B).
    CHECK(veto::scan_extents(4, 3, 0) == veto::Extents{4, 3});
    CHECK(veto::scan_extents(4, 3, 2) == veto::Extents{4, 3});
    CHECK(veto::scan_extents(4, 3, 1) == veto::Extents{3, 3});
    CHECK(veto::scan_extents(4, 3, 3) == veto::Extents{3, 3});
    CHECK(veto::scan_extents(3, 4, 1) == veto::Extents{4, 4});
    CHECK(veto::scan_extents(4, 4, 1) == veto::Extents{4, 4}); // square: no difference
    // Consequence: a type 4 wide and 3 tall at rotation 1 scans x [10, 13) y [12, 15), not the
    // swapped x [10, 13) y [12, 16): a scout at (10.5, 15.5) is outside the first, inside the
    // second.
    const veto::Extents e = veto::scan_extents(4, 3, 1);
    veto::NearbyFigure s = fig(kScout, 10 * T + T / 2, 15 * T + T / 2);
    CHECK(veto::nearby_figures(kMe, kOrigin, e.w, e.h, std::span<const veto::NearbyFigure>(&s, 1))
              .verdict == veto::Verdict::Clear);
    CHECK(veto::nearby_figures(kMe, kOrigin, 3, 4, std::span<const veto::NearbyFigure>(&s, 1))
              .verdict == veto::Verdict::Vetoed);
}
