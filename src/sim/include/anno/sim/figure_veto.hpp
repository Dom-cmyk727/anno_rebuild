// The nearby-figure veto of the ship-founding placement as a pure, headless predicate
// (task MAP-2, package 2; research: docs/re/ship-founding.md sections 11.4, 12.2 and 14;
// contract: docs/sim/ship-founding.md P1b and P12).
//
// This is NOT "founding allowed". It answers one question: given the footprint and the
// facts of the figures near it, does a figure stand in the way. It reads no World, mutates
// nothing and draws no random number. The caller supplies, per candidate figure, every fact
// the original reads; a fact the caller does not know is Unknown, and a candidate whose
// verdict depends on an Unknown fact makes the result MissingFact, never Clear. There is no
// permissive default: absence of data does not mean "allowed".
//
// Original (AnnoFrame/AnnoGame 0x10045080 and 0x100454d0, AnnoWelt.dll registry query
// 0x100985c0 and world slots +0x154/+0x158/+0x178; every address is traced in the research
// note):
//  1. QUERY. The figure registry returns the units whose truncated position lies in the
//     footprint rectangle grown by 10 tiles (half-open), whose BEWOHNTYPE is in the queried
//     class groups (97..128) and whose state word has none of the bits 0x0a. The original
//     fills a 256-entry buffer and silently drops the rest; this predicate does not inherit
//     that cap.
//  2. PER FIGURE. usable = the state word has none of the bits 0x200e, and a figure of
//     BEWOHNTYPE 108..112 (the original's war machines) has at least one occupied crew slot.
//     Path A applies when the figure is usable, has an owner, is hostile to the placing
//     player (different player and the placing player's relation byte of kind 2 towards the
//     owner is non-zero): when the owner's property 0x1c is 0x11 the figure is skipped (no
//     veto, AnnoFrame.dll 0x10045233 je 0x10045494); otherwise it vetoes when the
//     figure's position is within the type's range of ANY of the four integer corners of the
//     footprint (distance squared <= range squared, equality is a hit) and never falls to
//     path B. A figure that is not usable, has no owner or is not hostile takes path B: it
//     vetoes when the position lies in the half-open footprint rectangle, or, for BEWOHNTYPE
//     113..128, when its hull box (the position +- the type's hull half-extent, each bound
//     truncated toward zero) overlaps the rectangle.
//  3. The selected ship is NOT excluded from the scan. Its own rejection (state-word bit
//     0x02, AnnoFrame.dll 0x10048d54) is a separate predicate, selected_ship_state_allows.
//
// Representation (contract C2). Positions are Q24.8 (FixedPos), the original's are f32. The
// type's range and hull half-extent are Q24.8 raw values supplied by the caller (the original
// constants 16, 7 are exact; the hull half-extent 1.55 of ship 1502 is f32 0x3fc66666 and is
// not representable: the caller rounds it, the choice is the caller's and the boundary
// difference is limited to positions within 1/256 tile of a truncation boundary). With
// positions that are multiples of 1/256 and a hull half-extent that is exact in both
// representations, the comparisons are exactly the original's. Path A rounds dx^2 and r^2 to
// f32 (x87, AnnoFrame.dll 0x10045242-0x100452bc); the integer comparison equals it when
// range <= 16 tiles and |position| < 2^16 tiles (then every square is exact in f32). Outside
// that domain, and wherever the x87 precision control differs (unknown), results at the
// boundary are NOT claimed to equal the original's.
//
// Complexity: one linear pass over the candidates, early return only when nothing remains to
// learn (a veto), no sort, no pairwise scan, no allocation. Iteration order is the span's
// order; the verdict does not depend on it except for WHICH candidate is reported.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include <anno/sim/figure.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/site_checks.hpp>

namespace anno::sim::veto {

/// A fact the caller may not know. Unknown is never read as False.
enum class Fact : std::uint8_t { Unknown, False, True };

/// Whether the figure is visible to the registry's spatial query.
enum class Registration : std::uint8_t {
    Unknown,
    /// Registered in the sector cells at a position (a ship loaded from a scenario).
    SectorCell,
    /// Created without a position and carried (the embarked scout): the query cannot return it.
    Embarked,
};

/// The owner object of a figure (world slot +0x158): none, or a player.
struct OwnerFact {
    Fact present = Fact::Unknown;
    PlayerId player; ///< meaningful only with present == True
};

/// Everything the predicate reads about one candidate figure.
struct NearbyFigure {
    FigureId id; ///< reported back, never interpreted
    Registration registration = Registration::Unknown;
    FixedPos pos;                 ///< Q24.8 tile units
    std::uint32_t kind = 0;       ///< BEWOHNTYPE
    std::uint32_t state_word = 0; ///< the figure's state word (slot +0x8c(0))
    OwnerFact owner;
    /// Crew slots occupied: required only for the kinds the rules name (war machines).
    Fact crew_occupied = Fact::Unknown;
    /// The placing player's relation byte of kind 2 towards the owner
    /// ([player + 0x1ac + 6 * owner + 2]); nullopt = not supplied. Required only when the
    /// figure is usable and owned by a player other than the placing one.
    std::optional<std::uint8_t> relation_war;
    /// The owner's property 0x1c; nullopt = not supplied. Required only on the hostile branch.
    std::optional<std::uint32_t> owner_property_1c;
    /// The type's range in 1/256 tile, already resolved (the original reads LOGIC[0x25] / 8 and
    /// substitutes 7 for 0); nullopt = not supplied. Required only on the hostile branch.
    std::optional<std::uint32_t> range_88;
    /// The type's hull half-extent in 1/256 tile; nullopt = not supplied. Required only for
    /// kinds with a hull on the non-hostile branch when the point test did not already veto.
    std::optional<std::uint32_t> hull_half_88;
};

/// The original's constants, as data (CLAUDE.md: no hard-coded limits). Defaults are the
/// shipped values; a mod or another rule set changes the struct, not the predicate.
struct Rules {
    /// Rectangle margin of the registry query, in tiles (0x10045080: 10).
    std::uint32_t query_margin = 10;
    /// BEWOHNTYPE range handled by the query's class groups 6 and 7 (mask 0xc0).
    std::uint32_t query_kind_min = 97, query_kind_max = 128;
    /// Query excludes a figure with any of these state bits.
    std::uint32_t query_excluded_state = 0x0a;
    /// A figure with any of these bits is not usable (world slot +0x178).
    std::uint32_t unusable_state = 0x200e;
    /// Kinds that need an occupied crew slot to be usable.
    std::uint32_t crew_kind_min = 108, crew_kind_max = 112;
    /// Kinds with a hull box on the non-hostile branch (class group 0x71).
    std::uint32_t hull_kind_min = 113, hull_kind_max = 128;
    /// The owner property value (0x1c == 0x11) for which a hostile figure is skipped outright.
    /// Its meaning is not established (docs/re/ship-founding.md section 14); only the comparison
    /// is.
    std::uint32_t exempt_owner_property = 0x11;
    /// The selected ship's state-word bit that rejects it (AnnoFrame.dll 0x10048d54); unnamed.
    std::uint32_t selected_ship_reject_bit = 0x02;
};

enum class Verdict : std::uint8_t {
    Clear,       ///< no candidate vetoes and every candidate's verdict was decidable
    Vetoed,      ///< some candidate vetoes (a veto stands even if other facts are Unknown)
    MissingFact, ///< no veto found, but some candidate could not be decided
};

enum class Reason : std::uint8_t {
    None,
    HostileInRange,   ///< path A: within range of a footprint corner
    PointInFootprint, ///< path B: position inside the half-open rectangle
    HullOverlap,      ///< path B: hull box overlaps the rectangle
};

/// What was missing when the verdict is MissingFact.
enum class Missing : std::uint8_t {
    None,
    Registration,
    OwnerPresence,
    OwnerPlayer,
    CrewOccupied,
    RelationWar,
    OwnerProperty1c,
    Range,
    HullHalfExtent,
};

struct Result {
    Verdict verdict = Verdict::Clear;
    Reason reason = Reason::None;
    Missing missing = Missing::None;
    /// Index in the input of the deciding candidate (first veto, else first undecidable).
    std::uint64_t index = 0;
    friend bool operator==(const Result&, const Result&) = default;
};

/// The width and height of the rectangle the original hands to the nearby-figure scan
/// (AnnoFrame.dll 0x100450bc-0x100450f9): A = the type's first dimension, B = the second;
/// for rotation 1 or 3 BOTH extents are B (`mov ebp, eax` at 0x100450d5), for 0 and 2 they are
/// (A, B). This is the original's behaviour and NOT the swap the placement validator
/// (AnnoWelt.dll 0x100414d0) does; the two agree for square types such as the 4x4 warehouse.
/// Reproduced, not corrected, so that a non-square type is scanned over the same area.
struct Extents {
    std::uint32_t w = 0, h = 0;
    friend bool operator==(const Extents&, const Extents&) = default;
};
[[nodiscard]] constexpr Extents scan_extents(std::uint32_t type_w, std::uint32_t type_h,
                                             std::uint8_t rotation) {
    return (rotation == 1 || rotation == 3) ? Extents{type_h, type_h} : Extents{type_w, type_h};
}

/// The footprint is the rotation-adjusted rectangle [origin, origin + (w, h)). `placing` is the
/// placing player. Candidates need not be pre-filtered: the query conditions (registration,
/// kind, state bits, grown rectangle) are evaluated here, so a caller can pass every figure of
/// a region or every figure of the world. Verdict precedence: Vetoed over MissingFact over Clear.
[[nodiscard]] Result nearby_figures(PlayerId placing, site::Origin origin, std::uint32_t w,
                                    std::uint32_t h, std::span<const NearbyFigure> figures,
                                    const Rules& rules = {});

/// The selected ship's own rejection: true when the state word does NOT have the reject bit.
/// Independent of nearby_figures (the same bit is part of the query's exclusion mask, which
/// says nothing about whether the ship may found).
[[nodiscard]] bool selected_ship_state_allows(std::uint32_t state_word, const Rules& rules = {});

} // namespace anno::sim::veto
