// The INSELLNK resolver (spec: docs/sim/state.md §10 "As implemented in S14 stage 2";
// evidence: docs/re/annoscene.md §3–§5, D AnnoScene 0x10009550): which island template
// a scenario's link to a *DUMMY.scp becomes, which raw materials and ores it gets,
// which links are special and which hold a treasure.
//
// No file I/O: the caller hands in the candidate templates (their summaries, as the
// original's island cache holds them) and the draw source. Every draw names its site
// (draw.hpp, DrawPhase::Load); the result says which candidate each link takes, and
// the caller loads and places it (scenario_formats.hpp, place_link_island).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/draw.hpp>

namespace anno::sim {

/// One INSELLNK record whose path ends in DUMMY.scp (D AnnoScene 0x10009e0f–0x10009fd3,
/// docs/re/annoscene.md §3): the original appends such a link to its list in file order.
struct IslandLinkSpec {
    /// The head's u32 number; the island's table slot is `number & 0xFFFF`
    /// (D 0x10008503–0x1000850d, the third argument of world slot +0x88).
    std::uint32_t number = 0;
    /// The head's u16 x, y, width, height (sze.md "Island links").
    std::uint32_t x = 0, y = 0, w = 0, h = 0;
    /// From the file name's prefix (D 0x10009f38–0x10009f73, table 0x1003b328: po_ 0,
    /// t_ 1, n_ 2, pr_ 3, d_ 4, s_ 5, w_ 6; no match keeps 0).
    std::uint32_t climate = 0;
    /// The path as the record holds it.
    std::string path;
    /// Where the record is ("scene[12]") and its file offset, for warnings.
    std::string where;
    std::uint64_t offset = 0;
};

/// The scene records the selection reads (D AnnoScene 0x1000a076–0x1000a275,
/// docs/re/annoscene.md §5): what is left of them is consumed by the distribution.
struct LinkInputs {
    /// LinkTables::raw_kinds entries (this+0x51DC; ROHSTOFF bytes 0..9 are kinds 1, 2,
    /// 4..11).
    std::vector<std::uint16_t> raw_materials;
    /// LinkTables::ore_kinds entries (this+0x5292; ERZE bytes 0..3 are kinds 0..3,
    /// bytes 4, 5 kinds 5, 6).
    std::vector<std::uint16_t> ores;
    /// ERZE byte 6 (this+0x528A): how many links should get a special template.
    std::uint16_t special_count = 0;
    /// SCHATZ (this+0x150): how many links get a treasure.
    std::uint16_t treasures = 0;
};

/// One candidate template (§5 step 1): what GetAnnoInselFileData (AnnoWelt 0x100b25a0)
/// gives the cache, with the name the candidate order sorts by.
struct TemplateSummary {
    /// The path below Scenes/Islands, '/'-separated and upper-cased
    /// ("NORD/N_128X128_0001_1B.SCP").
    std::string name;
    /// Width and height from the INSEL5 (or INSEL4) head.
    std::uint32_t w = 0, h = 0;
    /// The INFO record (§2: [0] climate, [2] special, [4] + [5] capacity, [8], [9]
    /// must be 0).
    std::array<std::uint8_t, 12> info{};
};

/// Step 1's name filter (D 0x10007e16–0x10007e96): a file of the scan is a candidate
/// when its name ends in ".scp" (any case), and its upper-cased name contains no
/// "PIRAT" and does not end in "DUMMY.SCP". `file_name` is the name without folders.
[[nodiscard]] bool link_candidate_name(std::string_view file_name);

/// ASCII upper case, as the scan's `islower` / −0x20 (D 0x10007e41–0x10007e51).
[[nodiscard]] std::string ascii_upper(std::string_view s);

/// What the selection decided for one link (the link record, D §3 item 2).
struct ResolvedLink {
    /// Index into the caller's candidates (u32: a list longer than that is refused);
    /// nullopt when no candidate was left for the link (its name stays empty and the
    /// placement skips it, D 0x100084f4–0x100084fd).
    std::optional<std::uint32_t> candidate;
    /// Raw-material marks, LinkTables::raw_kinds entries of 0 or 1 (record +0x112).
    std::vector<std::uint8_t> raw_marks;
    /// Ore marks, LinkTables::ore_kinds entries of 0 or 1 (record +0x13A).
    std::vector<std::uint8_t> ore_marks;
    /// The chosen template's INFO[2] (record +0x14C).
    std::uint8_t special = 0;
    /// Record +0x14D.
    bool treasure = false;
    /// Candidates after step 2 (size, climate, INFO[8], INFO[9]) and after step 4
    /// (capacity).
    std::uint32_t matched = 0, kept = 0;
};

struct LinkResolution {
    /// One entry per link, in link order.
    std::vector<ResolvedLink> links;
    /// SCHATZ was more than the links could hold: the original's loop never ends
    /// (D 0x10009c5e–0x10009c9d); here it stops when every link holds a treasure.
    bool treasures_capped = false;
    /// The draws per site (telemetry, not state).
    std::array<std::uint32_t, kDrawSiteCount> draws{};
};

/// Steps 2–9 of the selection (D AnnoScene 0x10009550, docs/re/annoscene.md §5) over
/// `candidates` in the order `order` gives (Sorted: bytewise by name, ties by index).
/// No link: no draw and an empty result (D 0x10009562–0x10009576). Refused: tables
/// that do not validate, inputs whose lengths are not the tables' kind counts, more
/// links or candidates than a u32 counts, or the reserved order Cached.
formats::Result<LinkResolution, std::string>
resolve_island_links(std::span<const IslandLinkSpec> links, const LinkInputs& inputs,
                     std::span<const TemplateSummary> candidates, DrawSource& draw,
                     const LinkTables& tables, IslandCandidateOrder order);

} // namespace anno::sim
