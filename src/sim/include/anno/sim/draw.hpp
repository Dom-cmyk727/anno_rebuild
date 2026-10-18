// The only door to the world PRNG (spec: docs/sim/state.md §4.2, §6).
//
// Every draw site names itself. The table of sites is the single list the
// per-site telemetry, the "PRNG timeline" manifest and, later, the parallel
// island executor are derived from; a new site is a new enumerator, and
// -Wswitch-enum makes a predicate table over the sites incomplete at compile
// time. Production rules do not include the generator: they take a DrawRef
// (production.hpp), which the driver builds over a SiteDraw.
#pragma once

#include <cstdint>

#include <anno/sim/rng.hpp>

namespace anno::sim {

/// Where in the original a draw takes place (docs/sim/state.md §6). The first
/// four are implemented; the rest are declared so the manifest of unported
/// sites is complete and are never drawn yet.
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
    /// Number of sites (not a site).
    Count_
};

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

/// Telemetry: draws per site in one advance. Not state, not hashed.
struct DrawRecord {
    DrawSite site = DrawSite::SmallFarmAnimal;
    std::uint32_t count = 0;
};

} // namespace anno::sim
