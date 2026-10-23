// The only door to the world PRNG (spec: docs/sim/state.md §4.2, §6).
//
// Every draw site names itself. The table of sites is the single list the
// per-site telemetry, the "PRNG timeline" manifest and, later, the parallel
// island executor are derived from; a new site is a new enumerator, and
// -Wswitch-enum makes a predicate table over the sites incomplete at compile
// time. Production rules do not include the generator: they take a DrawRef
// (production.hpp), which the driver builds over a SiteDraw.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <anno/sim/rng.hpp>

namespace anno::sim {

/// Where in the original a draw takes place (docs/sim/state.md §6).
/// draw_site_ported says which are implemented; the others are declared so the
/// manifest of unported sites is complete and are never drawn yet.
enum class DrawSite : std::uint8_t {
    /// D: AnnoWelt.dll 0x1006f810–0x1006f838: once per small-farm step with
    /// line 1 on, after the gate, before the limit tests.
    SmallFarmAnimal,
    /// D: 0x100939b5: once per island per 300 s gate, before the climate and
    /// city tests, unless bit 0x04 of world property 6 is set (D: the gate at
    /// 0x100939a0-0x100939c6 is followed by `test al,4` on property 6, and
    /// the draw is skipped when the bit is set).
    Event300,
    /// D: 0x100ae140: draw #1 of the 7.5 minute event (r % 10).
    Event750Gate,
    /// D: 0x100ae140: draw #2, add-on only, only when #1 % 10 == 0.
    Event750Island,
    // Unported:
    /// 0x1002ba80.
    CityRandomEvents,
    /// 0x1009366f (state dependent count).
    WildlifeSweep,
    /// CompLogic.
    Ai,
    // The island-link selection at load (D AnnoScene 0x10009550, docs/re/annoscene.md
    // §5; drawn before the world's first advance, DrawPhase::Load):
    /// Step 3, raw materials: one draw per kind and round that finds a link
    /// (0x100092dd, `draw mod` the best list).
    LinkRawMaterial,
    /// Step 3, ores: as LinkRawMaterial (0x10009502).
    LinkOre,
    /// Steps 5 and 7: the template of a link (0x10009996, 0x10009b85).
    LinkTemplatePick,
    /// Step 6: the start of the probe for one special mark (0x10009a55).
    LinkSpecialMark,
    /// Step 9: one draw before the treasure count is tested and one per round
    /// (0x10009c42, 0x10009c8c).
    LinkTreasure,
    // Unported load-time sites (the placement 0x10008490 and the scene loader):
    /// Step 10: one draw per ore put on a mountain object (0x100086ad).
    LinkOrePlace,
    /// Step 10: a native settlement's player name (NATIVE_SPIELER, 0x10008a23), then its
    /// city name (NATIVE_STADT, 0x10008aa3), each only when its list exists.
    LinkNativeName,
    /// Step 10: the treasure's kind on climates 4..6 (0x10008b69).
    LinkTreasureKind,
    /// Step 10: the treasure spot (0x10008cc0, the draw at 0x10008e9a).
    LinkTreasurePlace,
    /// The name of an unnamed computer player after the file is read
    /// (0x100025a0, docs/re/annoscene.md §1).
    ComputerName,
    /// Number of sites (not a site).
    Count_
};

/// When a site draws: while a scenario is loaded (before the world's first
/// advance) or inside World::advance.
enum class DrawPhase : std::uint8_t { Load, Advance };

/// The name of a draw site: the single list the PRNG-timeline manifest is derived
/// from (docs/sim/state.md §6). A string literal, stable across builds.
[[nodiscard]] constexpr const char* draw_site_name(DrawSite site) {
    switch (site) {
    case DrawSite::SmallFarmAnimal:
        return "SmallFarmAnimal";
    case DrawSite::Event300:
        return "Event300";
    case DrawSite::Event750Gate:
        return "Event750Gate";
    case DrawSite::Event750Island:
        return "Event750Island";
    case DrawSite::CityRandomEvents:
        return "CityRandomEvents";
    case DrawSite::WildlifeSweep:
        return "WildlifeSweep";
    case DrawSite::Ai:
        return "Ai";
    case DrawSite::LinkRawMaterial:
        return "LinkRawMaterial";
    case DrawSite::LinkOre:
        return "LinkOre";
    case DrawSite::LinkTemplatePick:
        return "LinkTemplatePick";
    case DrawSite::LinkSpecialMark:
        return "LinkSpecialMark";
    case DrawSite::LinkTreasure:
        return "LinkTreasure";
    case DrawSite::LinkOrePlace:
        return "LinkOrePlace";
    case DrawSite::LinkNativeName:
        return "LinkNativeName";
    case DrawSite::LinkTreasureKind:
        return "LinkTreasureKind";
    case DrawSite::LinkTreasurePlace:
        return "LinkTreasurePlace";
    case DrawSite::ComputerName:
        return "ComputerName";
    case DrawSite::Count_:
        break;
    }
    return "";
}

/// The phase a site draws in (docs/sim/state.md §6): the scenario loader's sites (the
/// link selection, the placement, the computer names) at load, every other site
/// inside World::advance.
[[nodiscard]] constexpr DrawPhase draw_site_phase(DrawSite site) {
    switch (site) {
    case DrawSite::LinkRawMaterial:
    case DrawSite::LinkOre:
    case DrawSite::LinkTemplatePick:
    case DrawSite::LinkSpecialMark:
    case DrawSite::LinkTreasure:
    case DrawSite::LinkOrePlace:
    case DrawSite::LinkNativeName:
    case DrawSite::LinkTreasureKind:
    case DrawSite::LinkTreasurePlace:
    case DrawSite::ComputerName:
        return DrawPhase::Load;
    case DrawSite::SmallFarmAnimal:
    case DrawSite::Event300:
    case DrawSite::Event750Gate:
    case DrawSite::Event750Island:
    case DrawSite::CityRandomEvents:
    case DrawSite::WildlifeSweep:
    case DrawSite::Ai:
    case DrawSite::Count_:
        break;
    }
    return DrawPhase::Advance;
}

/// Whether `site` may draw in `phase`: a load-phase site never draws inside
/// World::advance and an advance site never while a scenario is loaded (tested in
/// island_links_test.cpp).
[[nodiscard]] constexpr bool may_draw(DrawSite site, DrawPhase phase) {
    return draw_site_phase(site) == phase;
}

/// Whether the sim reproduces this site's draws. The first four and the link
/// selection's exist; the rest are the original's sites the sim does not draw yet
/// (city random events 0x1002ba80, the fauna sweep 0x1009366f, the CompLogic AI), so
/// the cursor is short of the original's there (docs/sim/state.md §6). The replay-log header
/// records the names of the unported sites as the PRNG timeline.
[[nodiscard]] constexpr bool draw_site_ported(DrawSite site) {
    return site == DrawSite::SmallFarmAnimal || site == DrawSite::Event300 ||
           site == DrawSite::Event750Gate || site == DrawSite::Event750Island ||
           site == DrawSite::LinkRawMaterial || site == DrawSite::LinkOre ||
           site == DrawSite::LinkTemplatePick || site == DrawSite::LinkSpecialMark ||
           site == DrawSite::LinkTreasure;
}

/// A source of world draws. The world PRNG behind it is consumed in the
/// original's order, one cursor (docs/sim/state.md §6).
class DrawSource {
  public:
    virtual std::uint16_t next(DrawSite site) = 0;

  protected:
    ~DrawSource() = default;
};

/// The one world stream: forwards every site to the world PRNG.
struct GlobalDraw final : DrawSource {
    explicit GlobalDraw(WorldRng& r) : rng(r) {}
    std::uint16_t next(DrawSite /*site*/) override { return rng.next(); }
    WorldRng& rng;
};

/// Telemetry: draws per site in one advance. Not state, not hashed.
struct DrawRecord {
    DrawSite site = DrawSite::SmallFarmAnimal;
    std::uint32_t count = 0;
};

/// The number of draw sites (the length of a per-site count table).
inline constexpr std::size_t kDrawSiteCount = static_cast<std::size_t>(DrawSite::Count_);

/// A DrawSource that counts the draws per site and forwards them to another one
/// (docs/sim/state.md §4.2, §6: the per-site telemetry of SimOutput::draws and
/// Effects::draws, and the raw material of a later may_draw soundness check). The
/// counts are not state. `inner` must outlive the counter.
struct CountingDraw final : DrawSource {
    explicit CountingDraw(DrawSource& source) : inner(source) {}
    std::uint16_t next(DrawSite site) override {
        const auto i = static_cast<std::size_t>(site);
        if (i < kDrawSiteCount) {
            ++counts[i];
        }
        return inner.next(site);
    }
    DrawSource& inner;
    std::array<std::uint32_t, kDrawSiteCount> counts{};
};

/// The draws made between two readings of a counter, as records in site order
/// (sites without a draw are left out).
inline std::vector<DrawRecord>
draw_records_between(const std::array<std::uint32_t, kDrawSiteCount>& before,
                     const std::array<std::uint32_t, kDrawSiteCount>& after) {
    std::vector<DrawRecord> out;
    for (std::size_t i = 0; i < kDrawSiteCount; ++i) {
        if (after[i] != before[i]) {
            out.push_back(DrawRecord{static_cast<DrawSite>(i), after[i] - before[i]});
        }
    }
    return out;
}

/// A DrawSource bound to one site, with the `next()` shape DrawRef::of
/// expects: `SiteDraw d{source, DrawSite::SmallFarmAnimal}; DrawRef::of(d)`
/// forwards a production rule's draw to `source.next(site)` (`d` must
/// outlive the DrawRef).
struct SiteDraw {
    SiteDraw(DrawSource& s, DrawSite d) : source(s), site(d) {}
    std::uint16_t next() { return source.next(site); }
    DrawSource& source;
    DrawSite site;
};

} // namespace anno::sim
