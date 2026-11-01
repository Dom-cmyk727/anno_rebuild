#include <anno/sim/figure_veto.hpp>

namespace anno::sim::veto {

namespace {

/// Coordinates are clamped to +-kFar tiles before arithmetic. Figure positions are Q24.8 in an
/// i32, so |position| < 2^23 tiles; a rectangle edge beyond 2^40 is indistinguishable from
/// one at 2^40 for every comparison below (footprint sizes are u32, margins u32), and the clamp
/// keeps every product inside i64.
constexpr std::int64_t kFar = std::int64_t{1} << 40;

constexpr std::int64_t sat(std::int64_t v) {
    return v < -kFar ? -kFar : (v > kFar ? kFar : v);
}

constexpr bool in_range(std::uint32_t v, std::uint32_t lo, std::uint32_t hi) {
    return v >= lo && v <= hi;
}

struct Rect {
    std::int64_t x0, y0, x1, y1;
};

enum class Outcome : std::uint8_t { Clear, Veto, Missing };

struct Decision {
    Outcome outcome = Outcome::Clear;
    Reason reason = Reason::None;
    Missing missing = Missing::None;
};

constexpr Decision clear() {
    return {};
}
constexpr Decision vetoed(Reason r) {
    return {Outcome::Veto, r, Missing::None};
}
constexpr Decision missing(Missing m) {
    return {Outcome::Missing, Reason::None, m};
}

/// C++ integer division truncates toward zero, like the original's __ftol of the f32 value.
constexpr std::int64_t trunc_tiles(std::int64_t raw) {
    return raw / 256;
}

/// Path A: distance squared to one integer corner <= range squared. Positions are Q24.8, the
/// corner is scaled by 256. dx and dy are checked against the range first, so every square
/// below is at most range^2 < 2^64 and the comparison cannot overflow.
bool within_range_of_corner(FixedPos p, std::int64_t cx, std::int64_t cy, std::uint64_t range) {
    const std::int64_t dx = std::int64_t{p.x} - cx * 256;
    const std::int64_t dy = std::int64_t{p.y} - cy * 256;
    const std::uint64_t adx = static_cast<std::uint64_t>(dx < 0 ? -dx : dx);
    const std::uint64_t ady = static_cast<std::uint64_t>(dy < 0 ? -dy : dy);
    if (adx > range || ady > range) {
        return false;
    }
    return adx * adx <= range * range - ady * ady;
}

Decision hostile_branch(const NearbyFigure& f, const Rect& r) {
    if (!f.range_88) {
        return missing(Missing::Range);
    }
    const std::uint64_t range = *f.range_88;
    const std::int64_t xs[2] = {r.x0, r.x1};
    const std::int64_t ys[2] = {r.y0, r.y1};
    for (const std::int64_t cy : ys) {
        for (const std::int64_t cx : xs) {
            if (within_range_of_corner(f.pos, cx, cy, range)) {
                return vetoed(Reason::HostileInRange);
            }
        }
    }
    return clear();
}

Decision plain_branch(const NearbyFigure& f, const Rect& r, const Rules& rules) {
    // Half-open rectangle against the position (x0 <= X < x1, y0 <= Z < y1).
    if (std::int64_t{f.pos.x} >= r.x0 * 256 && std::int64_t{f.pos.x} < r.x1 * 256 &&
        std::int64_t{f.pos.y} >= r.y0 * 256 && std::int64_t{f.pos.y} < r.y1 * 256) {
        return vetoed(Reason::PointInFootprint);
    }
    if (!in_range(f.kind, rules.hull_kind_min, rules.hull_kind_max)) {
        return clear();
    }
    if (!f.hull_half_88) {
        return missing(Missing::HullHalfExtent);
    }
    // Any int64 is accepted from a direct caller: saturate before the arithmetic (not after an
    // overflow). |h| <= 2^60 and |pos| < 2^31 keep every sum inside int64; a hull that large
    // already covers or inverts every rectangle (edges <= 2^48 raw units), so the decision is the
    // one the exact value would give.
    constexpr std::int64_t kHullSat = std::int64_t{1} << 60;
    const std::int64_t h = *f.hull_half_88 < -kHullSat  ? -kHullSat
                           : *f.hull_half_88 > kHullSat ? kHullSat
                                                        : *f.hull_half_88;
    const std::int64_t xmin = trunc_tiles(std::int64_t{f.pos.x} - h);
    const std::int64_t xmax = trunc_tiles(std::int64_t{f.pos.x} + h);
    const std::int64_t ymin = trunc_tiles(std::int64_t{f.pos.y} - h);
    const std::int64_t ymax = trunc_tiles(std::int64_t{f.pos.y} + h);
    if (xmin < r.x1 && xmax > r.x0 && ymin < r.y1 && ymax > r.y0) {
        return vetoed(Reason::HullOverlap);
    }
    return clear();
}

Decision decide(PlayerId placing, const NearbyFigure& f, const Rect& rect, const Rect& grown,
                const Rules& rules) {
    // The registry query. Conditions on facts the struct always carries are evaluated first: a
    // figure those conditions exclude is not returned whatever its registration is.
    if (!in_range(f.kind, rules.query_kind_min, rules.query_kind_max)) {
        return clear();
    }
    if ((f.state_word & rules.query_excluded_state) != 0) {
        return clear();
    }
    const std::int64_t tx = trunc_tiles(f.pos.x);
    const std::int64_t ty = trunc_tiles(f.pos.y);
    if (tx < grown.x0 || tx >= grown.x1 || ty < grown.y0 || ty >= grown.y1) {
        return clear();
    }
    switch (f.registration) {
    case Registration::Embarked:
        return clear();
    case Registration::Unknown:
        return missing(Missing::Registration);
    case Registration::SectorCell:
        break;
    }

    // usable = world slot +0x178.
    bool usable = (f.state_word & rules.unusable_state) == 0;
    if (usable && in_range(f.kind, rules.crew_kind_min, rules.crew_kind_max)) {
        if (f.crew_occupied == Fact::Unknown) {
            return missing(Missing::CrewOccupied);
        }
        usable = f.crew_occupied == Fact::True;
    }
    if (!usable) {
        return plain_branch(f, rect, rules);
    }
    // owner = world slot +0x158.
    if (f.owner.present == Fact::Unknown) {
        return missing(Missing::OwnerPresence);
    }
    if (f.owner.present == Fact::False) {
        return plain_branch(f, rect, rules);
    }
    if (!f.owner.player.valid()) {
        return missing(Missing::OwnerPlayer);
    }
    // hostile = world slot +0x154: a different player and a non-zero relation of kind 2.
    if (f.owner.player == placing) {
        return plain_branch(f, rect, rules);
    }
    if (!f.relation_war) {
        return missing(Missing::RelationWar);
    }
    if (*f.relation_war == 0) {
        return plain_branch(f, rect, rules);
    }
    if (!f.owner_property_1c) {
        return missing(Missing::OwnerProperty1c);
    }
    if (*f.owner_property_1c == rules.exempt_owner_property) {
        // 0x10045233 cmp [..], 0x11; je 0x10045494: the loop increment. The figure is SKIPPED,
        // it does not fall to the point/hull test.
        return clear();
    }
    return hostile_branch(f, rect);
}

} // namespace

Result nearby_figures(PlayerId placing, site::Origin origin, std::uint32_t w, std::uint32_t h,
                      std::span<const NearbyFigure> figures, const Rules& rules) {
    Rect rect{};
    rect.x0 = sat(origin.x);
    rect.y0 = sat(origin.y);
    rect.x1 = rect.x0 + std::int64_t{w};
    rect.y1 = rect.y0 + std::int64_t{h};
    const std::int64_t m = rules.query_margin;
    const Rect grown{rect.x0 - m, rect.y0 - m, rect.x1 + m, rect.y1 + m};

    Result out;
    bool have_missing = false;
    for (std::size_t i = 0; i < figures.size(); ++i) {
        const Decision d = decide(placing, figures[i], rect, grown, rules);
        if (d.outcome == Outcome::Veto) {
            return Result{Verdict::Vetoed, d.reason, Missing::None, i};
        }
        if (d.outcome == Outcome::Missing && !have_missing) {
            have_missing = true;
            out = Result{Verdict::MissingFact, Reason::None, d.missing, i};
        }
    }
    return out;
}

bool selected_ship_state_allows(std::uint32_t state_word, const Rules& rules) {
    return (state_word & rules.selected_ship_reject_bit) == 0;
}

} // namespace anno::sim::veto
