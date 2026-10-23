// The adapter's side of the island links (docs/sim/state.md §10, S14 stage 2): the
// ROHSTOFF / SCHATZ / ERZE byte maps, the INSELLNK records, the template summaries,
// the resolution on the scenario's own generator and the placement, each warning kind
// of the links, and the draw phases through World::advance. The .sze and .scp files are
// built here byte by byte (our own data, nothing from the original files); every
// expected value follows from the bytes written and the evidence the adapter cites.
// (The byte builders repeat the few lines of scenario_formats_test.cpp that they need.)
#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/draw.hpp>
#include <anno/sim/island_links.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/world.hpp>

namespace sim = anno::sim;
using Buf = std::vector<std::uint8_t>;
using Kind = sim::ScenarioWarningKind;

namespace {

void put16(Buf& b, std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}
void put32(Buf& b, std::uint32_t v) {
    put16(b, v & 0xffffU);
    put16(b, v >> 16);
}
void append(Buf& b, const Buf& more) {
    b.insert(b.end(), more.begin(), more.end());
}

Buf chunk(std::string_view name, const Buf& payload) {
    Buf b(name.begin(), name.end());
    b.resize(12, 0);
    put32(b, static_cast<std::uint32_t>(payload.size()));
    append(b, payload);
    return b;
}

Buf record(std::string_view name, const Buf& payload) {
    Buf b(name.begin(), name.end());
    b.resize(6, 0);
    put16(b, static_cast<std::uint32_t>(payload.size()));
    append(b, payload);
    return b;
}

/// SCENE/HEAD of a 1024 × 1024 world, property 6 = 0 (the 300 s event draws).
Buf head_bytes() {
    Buf b;
    put32(b, 1024);
    put32(b, 1024);
    b.resize(32, 0);
    return b;
}

Buf player(std::uint32_t index) {
    Buf h;
    put32(h, 8);    // human
    put32(h, 5000); // money
    h.resize(108, 0);
    Buf b;
    put32(b, 0);
    put32(b, index);
    append(b, record("HEAD", h));
    return chunk("SPIELER", b);
}

/// An INSELLNK: the head and the NUL-terminated path.
Buf link(std::uint32_t x, std::uint32_t y, std::uint32_t size, std::uint32_t number,
         std::string_view path) {
    Buf b;
    put16(b, x);
    put16(b, y);
    put16(b, size);
    put16(b, size);
    put32(b, number);
    b.insert(b.end(), path.begin(), path.end());
    b.push_back(0);
    return chunk("INSELLNK", b);
}

/// An INSEL5 of size × size tiles (one block, size <= 32) with INFO (climate, INFO[2]
/// special, INFO[5] capacity), HEIGHT, GFXID and FRUCHTBAR.
Buf insel5(std::uint32_t size, std::uint8_t climate, std::uint8_t special, std::uint8_t capacity,
           bool with_info = true) {
    Buf p;
    put16(p, 0);
    put16(p, 0);
    put16(p, size);
    put16(p, size);
    put32(p, 0);
    if (with_info) {
        Buf info(12, 0);
        info[0] = climate;
        info[2] = special;
        info[5] = capacity;
        append(p, chunk("INFO", info));
    }
    Buf height = {0, 0, 0, 0, static_cast<std::uint8_t>(size), static_cast<std::uint8_t>(size)};
    Buf gfx = height;
    for (std::uint32_t i = 0; i < size * size; ++i) {
        height.push_back(static_cast<std::uint8_t>(i % 5));
        put16(gfx, 0x1100 + (i % 3));
    }
    append(p, chunk("HEIGHT", height));
    append(p, chunk("GFXID", gfx));
    append(p, chunk("FRUCHTBAR", Buf(size * size, 2)));
    return chunk("INSEL5", p);
}

/// An island template file: root ANNO with WARETYPE, the INSEL5 and optionally a
/// SPIELER block (the 60-byte trailer of the shipped templates is left out).
Buf scp(std::uint32_t size, std::uint8_t climate, std::uint8_t special = 0,
        std::uint8_t capacity = 9, bool players = false) {
    Buf kids = chunk("WARETYPE", Buf(4, 0));
    append(kids, insel5(size, climate, special, capacity));
    if (players) {
        append(kids, chunk("SPIELER", Buf(8, 0)));
    }
    return chunk("ANNO", kids);
}

std::shared_ptr<const sim::TypeRegistry> registry() {
    return sim::TypeRegistry::empty();
}

sim::LoadedScenario load(const Buf& file) {
    auto r = sim::load_scenario(file, *registry(), sim::SimConfig{});
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return std::move(r).value();
}

std::string refusal(const Buf& file) {
    auto r = sim::load_scenario(file, *registry(), sim::SimConfig{});
    REQUIRE_FALSE(r.ok());
    return r.error();
}

std::size_t warnings_of(const sim::LoadedScenario& s, Kind k) {
    return static_cast<std::size_t>(std::count_if(s.warnings.begin(), s.warnings.end(),
                                                  [&](const auto& w) { return w.kind == k; }));
}

Buf scene(const Buf& children) {
    Buf kids = chunk("HEAD", head_bytes());
    append(kids, player(0));
    append(kids, children);
    return chunk("ANNO", chunk("SCENE", kids));
}

Buf erze(Buf bytes = Buf(10, 0)) {
    return chunk("ERZE", bytes);
}

} // namespace

TEST_CASE("scenario ROHSTOFF SCHATZ ERZE byte maps") {
    // D AnnoScene 0x1000a076-0x1000a275 (docs/re/annoscene.md §5): ROHSTOFF bytes 0..9
    // are the raw kinds 1, 2, 4, 5, ..., 11 (bytes 10..15 not read); SCHATZ the u16 of
    // treasures; ERZE bytes 0..3 ore kinds 0..3, 4 -> 5, 5 -> 6, 6 -> the special count
    // (bytes 7..9 not read by the selection).
    Buf raw;
    for (std::uint8_t b = 0; b < 16; ++b) {
        raw.push_back(static_cast<std::uint8_t>(10 + b));
    }
    Buf treasure;
    put16(treasure, 0x0102);
    const Buf ores = {30, 31, 32, 33, 34, 35, 36, 37, 38, 39};
    Buf kids = link(0, 0, 64, 1, "islands\\nord\\n_064x064_dummy.scp");
    append(kids, chunk("ROHSTOFF", raw));
    append(kids, chunk("SCHATZ", treasure));
    append(kids, erze(ores));
    const sim::LoadedScenario s = load(scene(kids));
    const sim::LinkInputs& in = s.link_inputs;
    REQUIRE(in.raw_materials.size() == 20);
    const std::vector<std::uint16_t> want_raw = {0,  10, 11, 0, 12, 13, 14, 15, 16, 17,
                                                 18, 19, 0,  0, 0,  0,  0,  0,  0,  0};
    CHECK(in.raw_materials == want_raw);
    CHECK(in.treasures == 0x0102);
    REQUIRE(in.ores.size() == 9);
    const std::vector<std::uint16_t> want_ores = {30, 31, 32, 33, 0, 34, 35, 0, 0};
    CHECK(in.ores == want_ores);
    CHECK(in.special_count == 36);
    // Shorter records read as zeros after their end (the zero-filled buffers).
    Buf kids2 = link(0, 0, 64, 1, "islands\\nord\\n_064x064_dummy.scp");
    append(kids2, chunk("ROHSTOFF", Buf{7}));
    append(kids2, erze(Buf{5, 6}));
    const sim::LoadedScenario t = load(scene(kids2));
    CHECK(t.link_inputs.raw_materials[1] == 7);
    CHECK(t.link_inputs.raw_materials[2] == 0);
    CHECK(t.link_inputs.ores[1] == 6);
    CHECK(t.link_inputs.special_count == 0);
}

TEST_CASE("scenario link records: climate prefix, dummy test, order against ERZE") {
    // D 0x10009ec6-0x10009f73: the file name after the last backslash against po_ t_ n_
    // pr_ d_ s_ w_ (any case); no match keeps 0. The dummy test is on the last 9
    // characters, any case. Links after ERZE and links of a file without ERZE are
    // never resolved (the selection runs from the ERZE case only).
    Buf kids = link(0, 0, 64, 1, "islands\\polar\\po_064x064_dummy.scp");
    append(kids, link(64, 0, 64, 2, "islands\\tundra\\T_064x064_DUMMY.SCP"));
    append(kids, link(128, 0, 64, 3, "islands\\nord\\n_064x064_dummy.scp"));
    append(kids, link(192, 0, 64, 4, "islands\\praerie\\pr_064x064_dummy.scp"));
    append(kids, link(256, 0, 64, 5, "islands\\dschungel\\d_064x064_dummy.scp"));
    append(kids, link(320, 0, 64, 6, "islands\\steppe\\s_064x064_dummy.scp"));
    append(kids, link(384, 0, 64, 7, "w_064x064_dummy.scp")); // no backslash
    append(kids, link(448, 0, 64, 8, "islands\\x\\q_064x064_dummy.scp"));
    append(kids, link(512, 0, 64, 9, "islands\\nord\\n_064x064_0001.scp")); // not a dummy
    append(kids, erze());
    append(kids, link(576, 0, 64, 10, "islands\\nord\\n_064x064_dummy.scp")); // after ERZE
    const sim::LoadedScenario s = load(scene(kids));
    REQUIRE(s.links.size() == 8);
    const std::vector<std::uint32_t> climates = {0, 1, 2, 3, 4, 5, 6, 0};
    for (std::size_t i = 0; i < s.links.size(); ++i) {
        INFO(i);
        CHECK(s.links[i].climate == climates[i]);
        CHECK(s.links[i].number == i + 1);
        CHECK(s.links[i].x == 64 * i);
        CHECK(s.links[i].w == 64);
    }
    CHECK(s.links_total == 10);
    CHECK(warnings_of(s, Kind::LinkNamesTemplate) == 1);
    CHECK(warnings_of(s, Kind::LinkUnresolved) == 1);
    // No ERZE: every link stays unresolved.
    const sim::LoadedScenario n = load(scene(link(0, 0, 64, 1, "n_064x064_dummy.scp")));
    CHECK(n.links.empty());
    CHECK(warnings_of(n, Kind::LinkUnresolved) == 1);
    CHECK(sim::count(n).links_unresolved == 1);
}

TEST_CASE("scenario link records are refused when the reader's buffers overflow") {
    CHECK(refusal(scene(chunk("ROHSTOFF", Buf(17, 1)))).find("ROHSTOFF") != std::string::npos);
    CHECK(refusal(scene(chunk("SCHATZ", Buf(3, 1)))).find("SCHATZ") != std::string::npos);
    CHECK(refusal(scene(erze(Buf(11, 1)))).find("ERZE") != std::string::npos);
    Buf two = erze();
    append(two, erze());
    CHECK(refusal(scene(two)).find("second ERZE") != std::string::npos);
}

TEST_CASE("template summary as GetAnnoInselFileData") {
    // D AnnoWelt 0x100b25a0: root ANNO, the INSEL5 head's size and its INFO record.
    const Buf t = scp(16, 2, 1, 7);
    const std::optional<sim::TemplateSummary> s = sim::template_summary(t, "NORD/A.SCP");
    REQUIRE(s.has_value());
    CHECK(s->name == "NORD/A.SCP");
    CHECK(s->w == 16);
    CHECK(s->h == 16);
    CHECK(s->info[0] == 2);
    CHECK(s->info[2] == 1);
    CHECK(s->info[5] == 7);
    // No INFO: the bytes stay 0 (0x100b2770 returns 1 without one).
    const Buf no_info = chunk("ANNO", insel5(16, 2, 0, 0, false));
    const auto z = sim::template_summary(no_info, "X");
    REQUIRE(z.has_value());
    CHECK(z->info == std::array<std::uint8_t, 12>{});
    // Not ANNO, or no island: no candidate.
    CHECK_FALSE(sim::template_summary(chunk("ANNOADDON1", insel5(16, 2, 0, 0)), "X"));
    CHECK_FALSE(sim::template_summary(chunk("ANNO", chunk("WARETYPE", Buf(4, 0))), "X"));
    CHECK_FALSE(sim::template_summary(Buf{1, 2, 3}, "X"));
}

namespace {

/// Two nord links of 16 × 16 (numbers 0x10003 and 5, so the first's slot is 3) at
/// (64, 64) and (128, 64), and one tundra link of 24 × 24 with no template; one
/// treasure, no raw material, ore or special.
Buf links_scene(std::uint16_t treasures = 1) {
    Buf kids = link(64, 64, 16, 0x10003, "islands\\nord\\n_016x016_dummy.scp");
    append(kids, link(128, 64, 16, 5, "islands\\nord\\n_016x016_dummy.scp"));
    append(kids, link(200, 64, 24, 6, "islands\\tundra\\t_024x024_dummy.scp"));
    Buf t;
    put16(t, treasures);
    append(kids, chunk("SCHATZ", t));
    append(kids, erze());
    return scene(kids);
}

/// Two nord templates of 16 x 16, "NORD/A.SCP" and "NORD/B.SCP"; B (or both) with a
/// SPIELER block.
struct Templates {
    std::vector<Buf> files;
    std::vector<sim::TemplateSummary> summaries;
    explicit Templates(bool both_players = false)
        : files{scp(16, 2, 0, 9, both_players), scp(16, 2, 0, 9, true)} {
        summaries.push_back(*sim::template_summary(files[0], "NORD/A.SCP"));
        summaries.push_back(*sim::template_summary(files[1], "NORD/B.SCP"));
    }
};

/// Resolve and place as the game-directory glue does.
sim::LoadedScenario resolve_and_place(const Buf& file, const Templates& t, sim::LinkResolution& res,
                                      std::uint32_t seed = 1) {
    sim::LoadedScenario s = load(file);
    s.spec.seed = seed;
    auto r = sim::resolve_scenario_links(s, t.summaries, sim::SimConfig{}, sim::Rules{});
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    res = r.value();
    std::vector<anno::formats::Bytes> views(s.links.size());
    for (std::size_t i = 0; i < res.links.size(); ++i) {
        if (res.links[i].candidate) {
            views[i] = t.files[*res.links[i].candidate];
        }
    }
    auto e = sim::place_island_links(s, res, views, *registry(), sim::SimConfig{});
    REQUIRE_MESSAGE(!e.has_value(), (e ? *e : std::string()));
    return s;
}

} // namespace

TEST_CASE("scenario links resolve on the scenario's generator and place their templates") {
    // The cursor starts at spec.seed = 3. The table is MSVC rand() from 1 (41, 18467,
    // 6334, 26500, 19169, 15724, 11478, 29358, ...) and next() returns T[cursor + 1].
    // Draws (docs/re/annoscene.md §5): no raw or ore; step 5 one pick for each nord link
    // (T[4] = 19169 % 2 = 1 -> B.SCP, T[5] = 15724 % 2 = 0 -> A.SCP); the tundra link
    // matches nothing and draws nothing; step 9 draws T[6] = 11478 % 3 = 0 -> the first
    // link's treasure, then T[7]. Four draws: the cursor ends at 7.
    Templates t;
    sim::LinkResolution res;
    const sim::LoadedScenario s = resolve_and_place(links_scene(), t, res, 3);
    CHECK(s.spec.seed == 7);
    REQUIRE(res.links.size() == 3);
    CHECK(res.links[0].candidate == std::optional<std::uint32_t>{1});
    CHECK(res.links[1].candidate == std::optional<std::uint32_t>{0});
    CHECK_FALSE(res.links[2].candidate.has_value());
    CHECK(res.links[0].treasure);
    CHECK_FALSE(res.links[1].treasure);
    REQUIRE(s.spec.islands.size() == 2);
    // Slot = number & 0xFFFF (D 0x10008503-0x1000850d), at the link's position, the
    // link's climate (island property 7).
    CHECK(s.spec.islands[0].slot == sim::IslandId{3});
    CHECK(s.spec.islands[0].x0 == 64);
    CHECK(s.spec.islands[0].y0 == 64);
    CHECK(s.spec.islands[0].w == 16);
    CHECK(s.spec.islands[0].climate == 2);
    CHECK(s.spec.islands[1].slot == sim::IslandId{5});
    CHECK(s.spec.islands[1].x0 == 128);
    CHECK(s.links_placed == 2);
    const sim::ScenarioCounts c = sim::count(s);
    CHECK(c.links == 3);
    CHECK(c.links_resolved == 2);
    CHECK(c.links_unresolved == 1);
    CHECK(warnings_of(s, Kind::LinkUnresolved) == 1);
    CHECK(warnings_of(s, Kind::LinkTreasureUnplaced) == 1);
    CHECK(warnings_of(s, Kind::LinkTemplatePlayers) == 1); // B.SCP holds a SPIELER
    CHECK(warnings_of(s, Kind::UnknownRecord) == 1);       // WARETYPE, once
    CHECK(warnings_of(s, Kind::TileOwnersUndecoded) == 2);

    // The world starts where the selection left the cursor.
    auto w = sim::World::create(s.spec, registry(), sim::SimConfig{});
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    CHECK(w.value().rng().cursor() == 7);
    CHECK(w.value().check_invariants().empty());
}

TEST_CASE("scenario links with raw materials and ores report what is not placed") {
    // One nord link of 16 x 16 (width 16 is in no rank row: every score is 0, and the
    // bonuses are 0 for the one link), raw kind 6 (ROHSTOFF byte 4) and ore kind 0
    // (ERZE byte 0): one raw and one ore draw, then the pick and the one treasure draw
    // (SCHATZ is absent: 0 treasures, D 0x10009c3a draws before it tests the count).
    Buf raw(16, 0);
    raw[4] = 1;
    Buf kids = link(64, 64, 16, 1, "islands\\nord\\n_016x016_dummy.scp");
    append(kids, chunk("ROHSTOFF", raw));
    Buf o(10, 0);
    o[0] = 1;
    append(kids, erze(o));
    Templates t;
    sim::LinkResolution res;
    const sim::LoadedScenario s = resolve_and_place(scene(kids), t, res);
    CHECK(res.links[0].raw_marks[6] == 1);
    CHECK(res.links[0].ore_marks[0] == 1);
    CHECK(res.draws[static_cast<std::size_t>(sim::DrawSite::LinkRawMaterial)] == 1);
    CHECK(res.draws[static_cast<std::size_t>(sim::DrawSite::LinkOre)] == 1);
    CHECK(warnings_of(s, Kind::LinkRawMaterials) == 1);
    CHECK(warnings_of(s, Kind::LinkOresUnplaced) == 1);
    CHECK(s.spec.seed == 1 + 4);
}

TEST_CASE("scenario links cap the treasures") {
    // Four treasures for three links (one matches nothing): the treasure loop flags all
    // three and would then run for ever in the original.
    Templates t;
    sim::LinkResolution res;
    const sim::LoadedScenario s = resolve_and_place(links_scene(4), t, res);
    CHECK(res.treasures_capped);
    CHECK(warnings_of(s, Kind::LinkTreasuresCapped) == 1);
}

TEST_CASE("every link warning kind is reached and the link sites never draw in advance") {
    // Every kind named Link* in one file: a non-dummy link (LinkNamesTemplate), a link
    // that matches nothing (LinkUnresolved), raw material and ore on a placed link,
    // 9 treasures for 4 links (capped, and on the placed ones), templates with SPIELER.
    // Then 160 000 ticks of the placed world: the 300 s event draws (Event300, an
    // advance site), and no load-phase site appears in any advance.
    Templates t(true);
    sim::LinkResolution res;
    Buf raw(16, 0);
    raw[4] = 1;
    Buf kids = link(64, 64, 16, 1, "islands\\nord\\n_016x016_dummy.scp");
    append(kids, link(128, 64, 16, 2, "islands\\nord\\n_016x016_dummy.scp"));
    append(kids, link(200, 64, 24, 3, "islands\\tundra\\t_024x024_dummy.scp"));
    append(kids, link(300, 64, 16, 4, "islands\\nord\\n_016x016_0001.scp"));
    append(kids, chunk("ROHSTOFF", raw));
    Buf treasures;
    put16(treasures, 9);
    append(kids, chunk("SCHATZ", treasures));
    Buf o(10, 0);
    o[0] = 1;
    append(kids, erze(o));
    const sim::LoadedScenario s = resolve_and_place(scene(kids), t, res);
    for (const Kind k : sim::all_warning_kinds()) {
        if (sim::warning_kind_name(k).starts_with("Link")) {
            INFO(sim::warning_kind_name(k));
            CHECK(warnings_of(s, k) > 0);
        }
    }
    for (std::size_t i = 0; i < sim::kDrawSiteCount; ++i) {
        if (res.draws[i] != 0) {
            CHECK(sim::may_draw(static_cast<sim::DrawSite>(i), sim::DrawPhase::Load));
        }
    }
    auto w = sim::World::create(s.spec, registry(), sim::SimConfig{});
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    sim::World world = std::move(w).value();
    std::uint64_t advance_draws = 0;
    while (world.clock().tick() < 160000) {
        world.advance(100);
        for (const sim::DrawRecord& d : world.last_output().draws) {
            INFO(sim::draw_site_name(d.site));
            CHECK(sim::may_draw(d.site, sim::DrawPhase::Advance));
            advance_draws += d.count;
        }
    }
    CHECK(advance_draws > 0);
}
