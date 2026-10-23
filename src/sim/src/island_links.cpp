// The INSELLNK resolver (docs/sim/state.md §10, stage S14 stage 2). Every step cites
// the instructions it follows (D AnnoScene.dll, docs/re/annoscene.md §5). The
// original's per-link counters are bytes and its lists hold byte indices; here they
// are full width, which differs only beyond 255 (a link count or a candidate list the
// original's fixed 512-byte arrays and byte indices could not hold either).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <anno/sim/island_links.hpp>

namespace anno::sim {

std::string ascii_upper(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - ('a' - 'A'));
        }
    }
    return out;
}

bool link_candidate_name(std::string_view file_name) {
    // D 0x10007e16: _stricmp of the last 4 characters with ".scp" (the original's
    // pointer is name + len - 4 also for a shorter name, which reads before the name;
    // such a name is no candidate here).
    const std::string up = ascii_upper(file_name);
    const auto ends_with = [&](std::string_view tail) {
        return up.size() >= tail.size() &&
               up.compare(up.size() - tail.size(), tail.size(), tail) == 0;
    };
    if (!ends_with(".SCP")) {
        return false;
    }
    // D 0x10007e60: strstr(name, "PIRAT") on the upper-cased name; 0x10007e82: _stricmp
    // of the last 9 characters with "DUMMY.scp".
    return up.find("PIRAT") == std::string::npos && !ends_with("DUMMY.SCP");
}

namespace {

/// One eligible link of a pick and its score (the 2-byte entries at esp+0x174 of
/// 0x10009170 and esp+0x178 of 0x10009330).
struct Entry {
    std::uint32_t link = 0;
    std::uint32_t score = 0;
};

/// The score of `width` in row `round` of a width rank table: the score of the last
/// position whose width equals it, 0 for none (0x10009215–0x10009224,
/// 0x10009406–0x10009429: a later match overwrites an earlier one).
std::uint32_t rank_score(const std::vector<std::uint32_t>& table,
                         const std::vector<std::uint32_t>& scores, std::uint32_t round,
                         std::uint32_t width) {
    std::uint32_t s = 0;
    const std::size_t row = static_cast<std::size_t>(round) * scores.size();
    for (std::size_t p = 0; p < scores.size(); ++p) {
        if (table[row + p] == width) {
            s = scores[p];
        }
    }
    return s;
}

/// The pick shared by the raw and the ore distribution (0x1000928a–0x10009317,
/// 0x100094b1–0x10009538): the best score is the maximum, starting from 0; the first
/// `cap` entries with it form the list; one draw picks from it. No entry: no draw.
std::optional<std::uint32_t> pick_best(const std::vector<Entry>& entries, std::uint32_t cap,
                                       DrawSource& draw, DrawSite site) {
    if (entries.empty()) {
        return std::nullopt; // 0x10009272 / 0x1000949c: return -1 before the draw
    }
    std::uint32_t best = 0;
    for (const Entry& e : entries) {
        best = std::max(best, e.score);
    }
    std::vector<std::uint32_t> list;
    for (const Entry& e : entries) {
        if (e.score == best) {
            list.push_back(e.link);
            if (list.size() >= cap) {
                break;
            }
        }
    }
    const std::uint32_t r = draw.next(site);
    return list[r % list.size()];
}

std::string check_inputs(const LinkInputs& in, const LinkTables& t) {
    if (in.raw_materials.size() != t.raw_kinds) {
        return "link inputs: " + std::to_string(in.raw_materials.size()) +
               " raw-material counts, the tables have " + std::to_string(t.raw_kinds) + " kinds";
    }
    if (in.ores.size() != t.ore_kinds) {
        return "link inputs: " + std::to_string(in.ores.size()) + " ore counts, the tables have " +
               std::to_string(t.ore_kinds) + " kinds";
    }
    return {};
}

} // namespace

formats::Result<LinkResolution, std::string>
resolve_island_links(std::span<const IslandLinkSpec> links, const LinkInputs& inputs,
                     std::span<const TemplateSummary> candidates, DrawSource& source,
                     const LinkTables& tables, IslandCandidateOrder order) {
    if (std::string e = tables.validate(); !e.empty()) {
        return e;
    }
    if (std::string e = check_inputs(inputs, tables); !e.empty()) {
        return e;
    }
    if (order != IslandCandidateOrder::Sorted) {
        return std::string("island candidate order: only Sorted is implemented (OQ-150)");
    }
    if (links.size() > std::numeric_limits<std::uint32_t>::max() ||
        candidates.size() > std::numeric_limits<std::uint32_t>::max()) {
        return std::string("link resolution: more links or candidates than a u32 counts");
    }
    LinkResolution res;
    // D 0x10009562–0x10009576: no link, nothing to do (and no draw).
    if (links.empty()) {
        return res;
    }
    CountingDraw draw(source);
    const auto n = static_cast<std::uint32_t>(links.size());
    const std::uint32_t raw_kinds = tables.raw_kinds;
    const std::uint32_t ore_kinds = tables.ore_kinds;
    res.links.resize(n);
    for (ResolvedLink& r : res.links) {
        r.raw_marks.assign(raw_kinds, 0);
        r.ore_marks.assign(ore_kinds, 0);
    }

    // Step 1's order (IslandCandidateOrder::Sorted): bytewise by name, ties by index.
    std::vector<std::uint32_t> by_order(candidates.size());
    std::iota(by_order.begin(), by_order.end(), std::uint32_t{0});
    // A total order (name, then index), so std::sort gives the stable result without
    // the temporary buffer std::stable_sort allocates.
    std::sort(by_order.begin(), by_order.end(), [&](std::uint32_t a, std::uint32_t b) {
        const int c = candidates[a].name.compare(candidates[b].name);
        return c != 0 ? c < 0 : a < b;
    });

    // Step 2 (0x1000964c–0x1000972b): per link the candidates of equal climate (INFO[0]),
    // width and height with INFO[8] = INFO[9] = 0, appended in candidate order.
    std::vector<std::vector<std::uint32_t>> lists(n);
    for (const std::uint32_t c : by_order) {
        const TemplateSummary& t = candidates[c];
        for (std::uint32_t i = 0; i < n; ++i) {
            const IslandLinkSpec& l = links[i];
            if (t.info[0] == l.climate && t.w == l.w && t.h == l.h && t.info[8] == 0 &&
                t.info[9] == 0) {
                lists[i].push_back(c);
            }
        }
    }
    for (std::uint32_t i = 0; i < n; ++i) {
        res.links[i].matched = static_cast<std::uint32_t>(lists[i].size());
    }

    // Step 3, raw materials (0x10009731–0x1000979c, pick 0x10009170).
    std::vector<std::uint16_t> raw = inputs.raw_materials;
    std::vector<std::uint32_t> raw_count(n, 0); // the byte array at esp+0x8d0
    std::uint32_t best_raw = 0;                 // byte [esp+0x10]
    for (std::uint32_t round = 0; round < tables.rounds; ++round) {
        for (std::uint32_t kind = 0; kind < raw_kinds; ++kind) {
            if (raw[kind] == 0) {
                continue;
            }
            std::vector<Entry> entries;
            for (std::uint32_t i = 0; i < n; ++i) {
                if (res.links[i].raw_marks[kind] != 0) { // 0x100091db
                    continue;
                }
                // 0x10009010: 1 << kind against the climate's mask; a climate past the
                // table (the jump table's `ja`) takes none.
                const std::uint32_t climate = links[i].climate;
                if (climate >= tables.raw_climate_mask.size() ||
                    ((tables.raw_climate_mask[climate] >> kind) & 1U) == 0) {
                    continue;
                }
                std::uint32_t score = rank_score(tables.raw_width_rank, tables.raw_rank_score,
                                                 round, links[i].w); // 0x100091fd–0x10009224
                if (raw_count[i] < round) {                          // 0x1000922f–0x10009236
                    ++score;
                }
                entries.push_back({i, score});
            }
            const std::optional<std::uint32_t> chosen =
                pick_best(entries, tables.best_list_cap, draw, DrawSite::LinkRawMaterial);
            if (!chosen) {
                continue;
            }
            res.links[*chosen].raw_marks[kind] = 1; // 0x10009317
            best_raw = std::max(best_raw, ++raw_count[*chosen]);
            --raw[kind]; // 0x1000978c
        }
    }

    // Step 3, ores (0x1000979e–0x10009803, pick 0x10009330).
    std::vector<std::uint16_t> ore = inputs.ores;
    std::vector<std::uint32_t> ore_count(n, 0); // the byte array at esp+0xad0
    for (std::uint32_t round = 0; round < tables.rounds; ++round) {
        for (std::uint32_t kind = 0; kind < ore_kinds; ++kind) {
            if (ore[kind] == 0) {
                continue;
            }
            std::vector<Entry> entries;
            for (std::uint32_t i = 0; i < n; ++i) {
                const ResolvedLink& r = res.links[i];
                if (r.ore_marks[kind] != 0) { // 0x100093ae
                    continue;
                }
                // 0x100090b0: kind 6 tests bit 0 (ore_mask_bit), the climate's byte mask.
                const std::uint32_t climate = links[i].climate;
                if (climate >= tables.ore_climate_mask.size() ||
                    ((tables.ore_climate_mask[climate] >> tables.ore_mask_bit[kind]) & 1U) == 0) {
                    continue;
                }
                // 0x10009130: no kind the link already has excludes this one.
                bool excluded = false;
                for (std::uint32_t e = 0; e < ore_kinds; ++e) {
                    if (r.ore_marks[e] != 0 &&
                        tables.ore_exclusion[static_cast<std::size_t>(kind) * ore_kinds + e] != 0) {
                        excluded = true;
                        break;
                    }
                }
                if (excluded) {
                    continue;
                }
                std::uint32_t score = rank_score(tables.ore_width_rank, tables.ore_rank_score,
                                                 round, links[i].w); // 0x100093e8–0x10009429
                if (ore_count[i] < round) {                          // 0x10009436–0x10009449
                    ++score;
                }
                // 0x1000944b–0x1000946c: + 1 when best_raw − the link's raw count > 2
                // (signed).
                if (static_cast<std::int64_t>(best_raw) - static_cast<std::int64_t>(raw_count[i]) >
                    2) {
                    ++score;
                }
                entries.push_back({i, score});
            }
            const std::optional<std::uint32_t> chosen =
                pick_best(entries, tables.best_list_cap, draw, DrawSite::LinkOre);
            if (!chosen) {
                continue;
            }
            res.links[*chosen].ore_marks[kind] = 1; // 0x10009538
            ++ore_count[*chosen];                   // 0x100097ef
            --ore[kind];                            // 0x100097f1
        }
    }

    // Step 4 (0x10009805–0x10009927): drop the candidates whose INFO[4] + INFO[5] is
    // below the link's ore marks; the kinds byte gets 1 for a special (INFO[2] != 0),
    // 2 for a normal candidate.
    std::vector<std::uint8_t> kinds(n, 0);
    for (std::uint32_t i = 0; i < n; ++i) {
        std::uint32_t marks = 0;
        for (const std::uint8_t m : res.links[i].ore_marks) {
            marks += m;
        }
        std::vector<std::uint32_t> kept;
        for (const std::uint32_t c : lists[i]) {
            const auto& info = candidates[c].info;
            if (static_cast<std::uint32_t>(info[4]) + info[5] < marks) {
                continue;
            }
            kept.push_back(c);
            kinds[i] = static_cast<std::uint8_t>(kinds[i] | (info[2] != 0 ? 1U : 2U));
        }
        lists[i] = std::move(kept);
        res.links[i].kept = static_cast<std::uint32_t>(lists[i].size());
    }

    // Step 5 (0x1000992d–0x10009a1d): links with one kind take candidate draw mod count.
    std::int64_t special = inputs.special_count; // dword [esp+0x24]
    const auto choose = [&](std::uint32_t i, std::uint32_t c) {
        const std::uint8_t sp = candidates[c].info[2];
        if (sp != 0) {
            --special; // 0x100099cc / 0x10009bdb
        }
        res.links[i].candidate = c;
        res.links[i].special = sp; // +0x14C
    };
    for (std::uint32_t i = 0; i < n; ++i) {
        if (lists[i].empty() || kinds[i] == 3) {
            continue;
        }
        const std::uint32_t r = draw.next(DrawSite::LinkTemplatePick);
        choose(i, lists[i][r % lists[i].size()]);
    }

    // Step 6 (0x10009a23–0x10009a91): `special` times a probe from draw mod n for an
    // unmarked link with both kinds; n + 1 probes, index d, then (d >= n ? 0 : d) + 1;
    // a hit within the first n probes is marked.
    std::vector<std::uint8_t> mark(n, 0); // the byte array at esp+0x4d0
    if (special < 0) {
        special = 0;
    }
    for (std::int64_t k = 0; k < special; ++k) {
        const std::uint32_t r = draw.next(DrawSite::LinkSpecialMark);
        std::uint32_t d = r % n;
        for (std::uint64_t probe = 0; probe <= n; ++probe) {
            // Index n (reached after the last link) never qualifies: the original's
            // arrays are zero past the links.
            if (d < n && kinds[d] == 3 && mark[d] == 0) {
                if (probe < n) {
                    mark[d] = 1;
                }
                break;
            }
            d = (d >= n ? 0 : d) + 1;
        }
    }

    // Step 7 (0x10009a93–0x10009c34): links with both kinds pick among the candidates
    // whose special flag agrees with the mark (at most pick_list_cap, in list order).
    for (std::uint32_t i = 0; i < n; ++i) {
        if (lists[i].empty() || kinds[i] != 3) {
            continue;
        }
        std::vector<std::uint32_t> list;
        for (const std::uint32_t c : lists[i]) {
            const bool sp = candidates[c].info[2] != 0;
            const bool take = special > 0 ? (sp ? mark[i] != 0 : mark[i] == 0)
                                          : (!sp && mark[i] == 0); // 0x10009b25–0x10009b56
            if (!take) {
                continue;
            }
            list.push_back(c);
            if (list.size() >= tables.pick_list_cap) { // 0x10009b64
                break;
            }
        }
        // The original divides by the list's length here; it cannot be 0 (at most
        // `special` links are marked and each marked link takes one special), so an
        // empty list leaves the link without a template and draws nothing.
        if (list.empty()) {
            continue;
        }
        const std::uint32_t r = draw.next(DrawSite::LinkTemplatePick);
        choose(i, list[r % list.size()]);
    }

    // Step 9 (0x10009c3a–0x10009c9d): one draw before the count is tested, then per
    // round a link at draw mod n gets the treasure flag if it has none yet.
    std::uint32_t treasures = inputs.treasures;
    std::uint32_t d = draw.next(DrawSite::LinkTreasure) % n;
    std::uint32_t flagged = 0;
    while (treasures != 0) {
        if (!res.links[d].treasure) {
            res.links[d].treasure = true;
            ++flagged;
            --treasures;
        }
        d = draw.next(DrawSite::LinkTreasure) % n;
        if (treasures != 0 && flagged == n) {
            res.treasures_capped = true; // the original loops for ever here
            break;
        }
    }
    res.draws = draw.counts;
    return res;
}

} // namespace anno::sim
