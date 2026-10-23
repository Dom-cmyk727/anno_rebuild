// The INSELLNK resolver on synthetic links and candidates (docs/sim/state.md §10, S14
// stage 2; evidence docs/re/annoscene.md §5). Every expected pick is derived by hand
// from the steps of §5 and the draw values the script hands out, written next to the
// case before the code ran; nothing here comes from the original files.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/draw.hpp>
#include <anno/sim/island_links.hpp>
#include <anno/sim/rng.hpp>

namespace sim = anno::sim;
using Site = sim::DrawSite;

namespace {

/// A draw source that hands out scripted values and records the sites asked.
struct Script final : sim::DrawSource {
    explicit Script(std::vector<std::uint16_t> v) : values(std::move(v)) {}
    std::uint16_t next(Site s) override {
        sites.push_back(s);
        if (at >= values.size()) {
            overrun = true;
            return 0;
        }
        return values[at++];
    }
    std::vector<std::uint16_t> values;
    std::size_t at = 0;
    bool overrun = false;
    std::vector<Site> sites;
};

sim::IslandLinkSpec link(std::uint32_t number, std::uint32_t size, std::uint32_t climate) {
    sim::IslandLinkSpec l;
    l.number = number;
    l.x = 8 * number;
    l.w = size;
    l.h = size;
    l.climate = climate;
    l.path = "islands\\x_dummy.scp";
    return l;
}

/// A candidate: INFO[0] climate, [2] special, [4] + [5] capacity, [8] as given.
sim::TemplateSummary cand(std::string name, std::uint32_t size, std::uint8_t climate,
                          std::uint8_t special = 0, std::uint8_t capacity = 9,
                          std::uint8_t info8 = 0) {
    sim::TemplateSummary t;
    t.name = std::move(name);
    t.w = size;
    t.h = size;
    t.info[0] = climate;
    t.info[2] = special;
    t.info[5] = capacity;
    t.info[8] = info8;
    return t;
}

sim::LinkInputs inputs() {
    const sim::LinkTables t;
    sim::LinkInputs in;
    in.raw_materials.assign(t.raw_kinds, 0);
    in.ores.assign(t.ore_kinds, 0);
    return in;
}

sim::LinkResolution resolve(const std::vector<sim::IslandLinkSpec>& links,
                            const sim::LinkInputs& in,
                            const std::vector<sim::TemplateSummary>& cands, Script& s,
                            const sim::LinkTables& tables = {}) {
    auto r =
        sim::resolve_island_links(links, in, cands, s, tables, sim::IslandCandidateOrder::Sorted);
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return r.value();
}

std::uint32_t draws_at(const sim::LinkResolution& r, Site s) {
    return r.draws[static_cast<std::size_t>(s)];
}

constexpr std::uint8_t kNord = 2, kTundra = 1, kSteppe = 5;

} // namespace

TEST_CASE("link candidate names follow the scan's filter") {
    // D 0x10007e16-0x10007e96: ".scp" in any case, no PIRAT, no DUMMY.scp at the end.
    CHECK(sim::link_candidate_name("N_128x128_0001_1B.scp"));
    CHECK(sim::link_candidate_name("n_128x128_0001_1b.SCP"));
    CHECK_FALSE(sim::link_candidate_name("N_256x256_0010_1B_Mongolen.scp.bug"));
    CHECK_FALSE(sim::link_candidate_name("N_128x128_0003_Pirat.scp"));
    CHECK_FALSE(sim::link_candidate_name("n_128x128_dummy.scp"));
    CHECK(sim::link_candidate_name("n_128x128_dummy_2.scp")); // DUMMY not at the end
    CHECK_FALSE(sim::link_candidate_name("scp"));
    CHECK(sim::ascii_upper("Nord/n_064x064_0001.scp") == "NORD/N_064X064_0001.SCP");
}

TEST_CASE("link resolution without links draws nothing") {
    Script s({});
    const auto r = resolve({}, inputs(), {cand("NORD/A.SCP", 128, kNord)}, s);
    CHECK(r.links.empty());
    CHECK(s.sites.empty());
}

TEST_CASE("link step 2 matches size climate and INFO 8 9") {
    // One link, nord 128. Candidates: A matches; B is 64; C is tundra; D has INFO[8] = 1;
    // E has INFO[9] = 1; F matches. No raw material, ore, special or treasure: step 5 is
    // the only pick (draw 1 -> 1 % 2 = 1 -> F, the second of the sorted list A, F), then
    // step 9's one draw.
    auto e = cand("NORD/E.SCP", 128, kNord);
    e.info[9] = 1;
    const std::vector<sim::TemplateSummary> cands = {cand("NORD/A.SCP", 128, kNord),
                                                     cand("NORD/B.SCP", 64, kNord),
                                                     cand("TUNDRA/C.SCP", 128, kTundra),
                                                     cand("NORD/D.SCP", 128, kNord, 0, 9, 1),
                                                     e,
                                                     cand("NORD/F.SCP", 128, kNord)};
    Script s({1, 0});
    const auto r = resolve({link(1, 128, kNord)}, inputs(), cands, s);
    REQUIRE(r.links.size() == 1);
    CHECK(r.links[0].matched == 2);
    CHECK(r.links[0].kept == 2);
    REQUIRE(r.links[0].candidate.has_value());
    CHECK(*r.links[0].candidate == 5);
    CHECK(s.sites == std::vector<Site>{Site::LinkTemplatePick, Site::LinkTreasure});
    CHECK_FALSE(s.overrun);
}

TEST_CASE("link candidates are taken in name order whatever the caller's order") {
    // The same two matching candidates in reverse order: the sorted list is still
    // "NORD/A.SCP", "NORD/F.SCP", so draw 1 picks F (index 0 of the caller's list).
    const std::vector<sim::TemplateSummary> cands = {cand("NORD/F.SCP", 128, kNord),
                                                     cand("NORD/A.SCP", 128, kNord)};
    Script s({1, 0});
    const auto r = resolve({link(1, 128, kNord)}, inputs(), cands, s);
    REQUIRE(r.links[0].candidate.has_value());
    CHECK(cands[*r.links[0].candidate].name == "NORD/F.SCP");
}

TEST_CASE("link without candidates stays unresolved and draws no pick") {
    Script s({0});
    const auto r = resolve({link(1, 80, kTundra)}, inputs(), {cand("NORD/A.SCP", 128, kNord)}, s);
    CHECK_FALSE(r.links[0].candidate.has_value());
    CHECK(r.links[0].matched == 0);
    CHECK(s.sites == std::vector<Site>{Site::LinkTreasure});
}

TEST_CASE("link raw materials prefer links below the round count") {
    // Three nord links of width 128 (round 0 row 256, 128, 0, 0 and round 1 row 256, 128,
    // 80, 80: width 128 scores 3 in both). raw[0] = 1, raw[3] = 2 (nord's mask 0xF949 has
    // bits 0 and 3).
    // Round 0, kind 0: A, B, C score 3 -> list A, B, C, draw 0 -> A (count A 1).
    // Round 0, kind 3: A, B, C score 3, draw 1 -> B (count B 1).
    // Round 1, kind 3: A 3 + (1 < 1 ? 1 : 0) = 3, B marked, C 3 + 1 = 4 -> list C, draw 0
    // -> C. Without the bonus the list would be A, C and draw 0 would take A.
    auto in = inputs();
    in.raw_materials[0] = 1;
    in.raw_materials[3] = 2;
    Script s({0, 1, 0, /* picks */ 0, 0, 0, /* treasure */ 0});
    const auto r = resolve({link(1, 128, kNord), link(2, 128, kNord), link(3, 128, kNord)}, in,
                           {cand("NORD/A.SCP", 128, kNord)}, s);
    CHECK(r.links[0].raw_marks[0] == 1);
    CHECK(r.links[0].raw_marks[3] == 0);
    CHECK(r.links[1].raw_marks[3] == 1);
    CHECK(r.links[2].raw_marks[3] == 1);
    CHECK(r.links[2].raw_marks[0] == 0);
    CHECK(draws_at(r, Site::LinkRawMaterial) == 3);
}

TEST_CASE("link raw materials follow the climate mask") {
    // Kind 1 is not in nord's mask 0xF949 but in praerie's 0xF06B (climate 3): the one
    // praerie link takes it, the nord link never does; kind 16 only in polar's 0x18000.
    auto in = inputs();
    in.raw_materials[1] = 1;
    in.raw_materials[16] = 1;
    Script s({0, 0, 0, 0});
    const auto r = resolve({link(1, 128, kNord), link(2, 128, 3)}, in,
                           {cand("NORD/A.SCP", 128, kNord), cand("PRAERIE/B.SCP", 128, 3)}, s);
    CHECK(r.links[0].raw_marks[1] == 0);
    CHECK(r.links[1].raw_marks[1] == 1);
    CHECK(r.links[0].raw_marks[16] == 0);
    CHECK(r.links[1].raw_marks[16] == 0);
    // One raw draw (kind 1); kind 16 finds no link and draws nothing.
    CHECK(draws_at(r, Site::LinkRawMaterial) == 1);
}

TEST_CASE("link ores prefer links well below the best raw count") {
    // Links A and B, nord 128. raw[0], raw[3], raw[6] = 1 each, all draws 0: round 0 gives
    // all three to A (lists A, B with equal scores 3), best_raw = 3, count A = 3, B = 0.
    // ore[0] = 1 (nord ore mask 0x25 has bit 0), round 0: width 128 is rank 1 of 256, 128,
    // 0, 0 -> 6; A 6 + 0 + (3 - 3 > 2 ? 1 : 0) = 6, B 6 + 0 + (3 - 0 > 2) = 7 -> list B,
    // draw 0 -> B. Without that bonus the list would be A, B and draw 0 would take A.
    auto in = inputs();
    in.raw_materials[0] = in.raw_materials[3] = in.raw_materials[6] = 1;
    in.ores[0] = 1;
    Script s({0, 0, 0, 0, /* picks */ 0, 0, /* treasure */ 0});
    const auto r = resolve({link(1, 128, kNord), link(2, 128, kNord)}, in,
                           {cand("NORD/A.SCP", 128, kNord)}, s);
    CHECK(r.links[0].raw_marks[0] + r.links[0].raw_marks[3] + r.links[0].raw_marks[6] == 3);
    CHECK(r.links[0].ore_marks[0] == 0);
    CHECK(r.links[1].ore_marks[0] == 1);
    CHECK(draws_at(r, Site::LinkOre) == 1);
}

TEST_CASE("link ores respect the exclusion table and kind 6 tests bit 0") {
    // Steppe (climate 5, ore mask 0x0B: bits 0, 1, 3). Link A 128, link B 64.
    // Round 0, kind 0: A rank 1 -> 6, B none -> 0 (+ 0, best_raw 0) -> A (draw 0).
    // Round 0, kind 3: A already has 0 and row 3 excludes 0 -> only B -> B (draw 0).
    // Round 0, kind 6: bit 0 passes the mask (0x100090b4); A has 0, row 6 excludes 1..6
    // only -> A 6, B: B has 3, excluded -> A (draw 0).
    auto in = inputs();
    in.ores[0] = in.ores[3] = in.ores[6] = 1;
    Script s({0, 0, 0, /* picks */ 0, 0, /* treasure */ 0});
    const auto r =
        resolve({link(1, 128, kSteppe), link(2, 64, kSteppe)}, in,
                {cand("STEPPE/A.SCP", 128, kSteppe), cand("STEPPE/B.SCP", 64, kSteppe)}, s);
    CHECK(r.links[0].ore_marks[0] == 1);
    CHECK(r.links[0].ore_marks[3] == 0);
    CHECK(r.links[1].ore_marks[3] == 1);
    CHECK(r.links[0].ore_marks[6] == 1);
    CHECK(r.links[1].ore_marks[6] == 0);
    CHECK(draws_at(r, Site::LinkOre) == 3);
}

TEST_CASE("link capacity filter drops small candidates") {
    // One nord link with ores 0 and 2 (both in 0x25; row 2 excludes 2, 7, 8 only): two
    // marks. Candidates with capacity 1 and 2: the first is dropped, the pick takes the
    // second (kept 1, draw 0).
    auto in = inputs();
    in.ores[0] = in.ores[2] = 1;
    Script s({0, 0, 0, 0});
    const auto r =
        resolve({link(1, 128, kNord)}, in,
                {cand("NORD/A.SCP", 128, kNord, 0, 1), cand("NORD/B.SCP", 128, kNord, 0, 2)}, s);
    CHECK(r.links[0].matched == 2);
    CHECK(r.links[0].kept == 1);
    REQUIRE(r.links[0].candidate.has_value());
    CHECK(*r.links[0].candidate == 1);
}

namespace {

/// Two nord links (A 128 with an ore, B 64) and candidates: A's normal "NORD/A.SCP" and
/// special "NORD/B.SCP" (capacity 5), B's normal "NORD/C.SCP" and "NORD/D.SCP".
/// raw[0] = 1 and ore[0] = 1 both go to A (B's width scores 0 in round 0).
struct TwoLinks {
    std::vector<sim::IslandLinkSpec> links = {link(1, 128, kNord), link(2, 64, kNord)};
    std::vector<sim::TemplateSummary> cands = {
        cand("NORD/A.SCP", 128, kNord, 0, 5), cand("NORD/B.SCP", 128, kNord, 1, 5),
        cand("NORD/C.SCP", 64, kNord, 0, 0), cand("NORD/D.SCP", 64, kNord, 0, 0)};
    sim::LinkInputs in = [] {
        auto i = inputs();
        i.raw_materials[0] = 1;
        i.ores[0] = 1;
        i.special_count = 1;
        i.treasures = 1;
        return i;
    }();
};

} // namespace

TEST_CASE("link special mark and both-kind pick") {
    // Draws: raw 0 -> A; ore 0 -> A; step 5: B (one kind) draw 1 -> D; step 6 (special 1,
    // nothing special picked yet): draw 0 -> d = 0, A has both kinds -> marked; step 7: A
    // marked and special > 0 -> its special candidates (B.SCP) -> draw 0 -> B.SCP, special
    // 0; step 9: draw 1 -> B gets the treasure, then one more draw.
    TwoLinks t;
    Script s({0, 0, 1, 0, 0, 1, 7});
    const auto r = resolve(t.links, t.in, t.cands, s);
    REQUIRE(r.links[0].candidate.has_value());
    CHECK(t.cands[*r.links[0].candidate].name == "NORD/B.SCP");
    CHECK(r.links[0].special == 1);
    REQUIRE(r.links[1].candidate.has_value());
    CHECK(t.cands[*r.links[1].candidate].name == "NORD/D.SCP");
    CHECK_FALSE(r.links[0].treasure);
    CHECK(r.links[1].treasure);
    CHECK(s.sites == std::vector<Site>{Site::LinkRawMaterial, Site::LinkOre, Site::LinkTemplatePick,
                                       Site::LinkSpecialMark, Site::LinkTemplatePick,
                                       Site::LinkTreasure, Site::LinkTreasure});
    CHECK_FALSE(s.overrun);
    CHECK(draws_at(r, Site::LinkTemplatePick) == 2);
    CHECK(draws_at(r, Site::LinkTreasure) == 2);
}

TEST_CASE("link special probe wraps past link 0") {
    // As above, but step 6 draws 1: d = 1 (B, one kind), then (1 >= 2 ? 0 : 1) + 1 = 2
    // (past the links), then (2 >= 2 ? 0 : 2) + 1 = 1 again: three probes, link 0 never
    // probed, no mark. Step 7: A unmarked, special 1 > 0 -> its normal candidates
    // (A.SCP) -> draw 0 -> A.SCP. Step 9: draw 0 -> A.
    TwoLinks t;
    Script s({0, 0, 0, 1, 0, 0, 0});
    const auto r = resolve(t.links, t.in, t.cands, s);
    REQUIRE(r.links[0].candidate.has_value());
    CHECK(t.cands[*r.links[0].candidate].name == "NORD/A.SCP");
    CHECK(r.links[0].special == 0);
    CHECK(t.cands[*r.links[1].candidate].name == "NORD/C.SCP");
    CHECK(r.links[0].treasure);
}

TEST_CASE("link one-kind special lowers the special count") {
    // One nord link whose only candidate is special (one kind): step 5 takes it and the
    // count drops from 1 to 0, so step 6 draws nothing. Draws: pick 0, treasure 0.
    auto in = inputs();
    in.special_count = 1;
    Script s({0, 0});
    const auto r = resolve({link(1, 128, kNord)}, in, {cand("NORD/S.SCP", 128, kNord, 2)}, s);
    CHECK(r.links[0].special == 2);
    CHECK(draws_at(r, Site::LinkSpecialMark) == 0);
    CHECK(s.sites == std::vector<Site>{Site::LinkTemplatePick, Site::LinkTreasure});
}

TEST_CASE("link both-kind list is capped") {
    // pick_list_cap 2: of three normal candidates (and one special, so the link has both
    // kinds) only the first two are listed; draw 2 -> 2 % 2 = 0 -> the first. With no cap
    // draw 2 would take the third.
    sim::LinkTables tables;
    tables.pick_list_cap = 2;
    Script s({2, 0});
    const auto r = resolve({link(1, 128, kNord)}, inputs(),
                           {cand("NORD/A.SCP", 128, kNord), cand("NORD/B.SCP", 128, kNord),
                            cand("NORD/C.SCP", 128, kNord), cand("NORD/S.SCP", 128, kNord, 1)},
                           s, tables);
    REQUIRE(r.links[0].candidate.has_value());
    CHECK(*r.links[0].candidate == 0);
}

TEST_CASE("link treasure loop repeats and caps") {
    // Three links, two treasures: draw 0 -> link 0; draw 0 -> already flagged; draw 2 ->
    // link 2, count 0; then one more draw. Four treasure draws. No candidate, no picks.
    auto in = inputs();
    in.treasures = 2;
    Script s({0, 0, 2, 9});
    const auto r = resolve({link(1, 64, kNord), link(2, 64, kNord), link(3, 64, kNord)}, in, {}, s);
    CHECK(r.links[0].treasure);
    CHECK_FALSE(r.links[1].treasure);
    CHECK(r.links[2].treasure);
    CHECK(draws_at(r, Site::LinkTreasure) == 4);
    CHECK_FALSE(r.treasures_capped);

    // One link, three treasures: the original would loop for ever; here it stops.
    in.treasures = 3;
    Script s2({0, 0, 0});
    const auto r2 = resolve({link(1, 64, kNord)}, in, {}, s2);
    CHECK(r2.links[0].treasure);
    CHECK(r2.treasures_capped);
}

TEST_CASE("link resolution is deterministic and draws only at load") {
    // The two-link setup on the real generator, twice from the same cursor: equal
    // results, and every draw is a load-phase site.
    TwoLinks t;
    auto run = [&] {
        sim::WorldRng rng;
        rng.set_cursor(77);
        sim::GlobalDraw g(rng);
        auto r = sim::resolve_island_links(t.links, t.in, t.cands, g, sim::LinkTables{},
                                           sim::IslandCandidateOrder::Sorted);
        REQUIRE(r.ok());
        return std::make_pair(r.value(), rng.cursor());
    };
    const auto [a, cursor_a] = run();
    const auto [b, cursor_b] = run();
    CHECK(cursor_a == cursor_b);
    REQUIRE(a.links.size() == b.links.size());
    for (std::size_t i = 0; i < a.links.size(); ++i) {
        CHECK(a.links[i].candidate == b.links[i].candidate);
        CHECK(a.links[i].raw_marks == b.links[i].raw_marks);
        CHECK(a.links[i].ore_marks == b.links[i].ore_marks);
        CHECK(a.links[i].treasure == b.links[i].treasure);
    }
    std::uint32_t total = 0;
    for (std::size_t i = 0; i < sim::kDrawSiteCount; ++i) {
        total += a.draws[i];
        if (a.draws[i] != 0) {
            CHECK(sim::may_draw(static_cast<Site>(i), sim::DrawPhase::Load));
        }
    }
    CHECK(total == cursor_a - 77);
}

TEST_CASE("draw site phases") {
    for (std::size_t i = 0; i < sim::kDrawSiteCount; ++i) {
        const auto site = static_cast<Site>(i);
        const std::string name = sim::draw_site_name(site);
        INFO(name);
        const bool link_site = name.rfind("Link", 0) == 0;
        CHECK(sim::may_draw(site, sim::DrawPhase::Load) == link_site);
        CHECK(sim::may_draw(site, sim::DrawPhase::Advance) == !link_site);
    }
}

TEST_CASE("link resolution refuses bad tables inputs and the reserved order") {
    Script s({});
    auto in = inputs();
    in.ores.pop_back();
    CHECK_FALSE(sim::resolve_island_links({}, in, {}, s, sim::LinkTables{},
                                          sim::IslandCandidateOrder::Sorted)
                    .ok());
    CHECK_FALSE(sim::resolve_island_links({}, inputs(), {}, s, sim::LinkTables{},
                                          sim::IslandCandidateOrder::Cached)
                    .ok());
    sim::LinkTables t;
    t.rounds = 24; // the width tables hold 25 rows
    CHECK(sim::resolve_island_links({}, inputs(), {}, s, t, sim::IslandCandidateOrder::Sorted)
              .error() == "LinkTables.raw_width_rank: needs rounds x raw_rank_score entries");
    CHECK(sim::LinkTables{}.validate().empty());
}

TEST_CASE("link tables are the original's") {
    // Spot values of docs/re/annoscene.md §5 step 3 (D AnnoScene .rdata).
    const sim::LinkTables t;
    REQUIRE(t.raw_width_rank.size() == 100);
    REQUIRE(t.ore_width_rank.size() == 100);
    CHECK(t.raw_width_rank[1 * 4 + 2] == 80);   // round 1: 256, 128, 80, 80
    CHECK(t.raw_width_rank[5 * 4 + 0] == 80);   // rounds 2..5 start with 80
    CHECK(t.raw_width_rank[6 * 4 + 0] == 64);   // rounds 6..24 start with 64
    CHECK(t.ore_width_rank[14 * 4 + 0] == 128); // rounds 2..14 start with 128
    CHECK(t.ore_width_rank[15 * 4 + 0] == 64);  // rounds 15..24 start with 64
    CHECK(t.ore_rank_score == std::vector<std::uint32_t>{116, 6, 3, 1});
    CHECK(t.ore_exclusion[3 * 9 + 7] == 0); // kind 3 is refused by every kind but 7
    CHECK(t.ore_exclusion[3 * 9 + 8] == 1);
    CHECK(t.ore_mask_bit[6] == 0);
}
