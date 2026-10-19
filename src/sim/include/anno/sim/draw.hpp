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
