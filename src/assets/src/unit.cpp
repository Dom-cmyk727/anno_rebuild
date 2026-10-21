#include <algorithm>
#include <tuple>

#include <anno/assets/unit.hpp>

namespace anno::assets::unit {

namespace fig = formats::figuren;
namespace sze = formats::sze;

std::string_view record_name(UnitKind kind) {
    return kind == UnitKind::Ship ? "SCHIFF" : "SOLDAT";
}

Result<StartColours, std::string> start_colours(const sze::Scene& scene, std::uint32_t colours) {
    // 0xFF is the "assign at game start" marker, never a slot.
    colours = std::min<std::uint32_t>(colours, 0xFE);
    auto players = scene.players();
    if (!players) {
        return players.error();
    }
    std::vector<std::pair<std::uint32_t, sze::PlayerHead>> heads;
    for (const auto& p : players.value()) {
        // Player 31 is skipped by every game-start loop (D: docs/re/annoscene.md
        // §6–§9; OQ-027 b).
        if (p.index == 31) {
            continue;
        }
        if (auto h = p.head()) {
            heads.emplace_back(p.index, *h);
        }
    }
    auto in_range = [colours](std::uint8_t c) { return c >= 1 && c <= colours; };
    StartColours out;
    std::set<std::uint8_t> pool;
    for (unsigned c = 1; c <= colours; ++c) {
        pool.insert(static_cast<std::uint8_t>(c));
    }
    auto take_lowest = [&pool]() -> std::optional<std::uint8_t> {
        if (pool.empty()) {
            return std::nullopt;
        }
        const std::uint8_t c = *pool.begin();
        pool.erase(pool.begin());
        return c;
    };
    std::optional<std::uint32_t> local;
    for (const auto& [index, h] : heads) {
        if (h.is_human()) {
            local = index;
            break;
        }
    }
    for (const auto& [index, h] : heads) {
        if (in_range(h.colour)) {
            out.slots.insert_or_assign(index, h.colour);
            if (local == index) {
                pool.erase(h.colour);
            }
        }
    }
    if (local && !out.slots.contains(*local)) {
        if (auto c = take_lowest()) {
            out.slots.insert_or_assign(*local, *c);
        }
    }
    for (const auto& [index, h] : heads) {
        if (h.colour != 0xFF || local == index) {
            continue;
        }
        if (h.is_human()) {
            out.removed.insert(index);
        } else if (h.is_computer()) {
            if (auto c = take_lowest()) {
                out.slots.insert_or_assign(index, *c);
            }
        }
    }
    return out;
}

namespace {

/// Decoded frame per (figure, shadow, group, frame index, colour), shared by
/// units of the same type, facing and colour.
using CacheKey =
    std::tuple<std::uint16_t, bool, std::uint32_t, std::size_t, std::optional<figure::Rgb>>;
using FrameCache = std::map<CacheKey, std::optional<DecodedFrame>>;

struct Decoder {
    const VolumeReader& read;
    FrameCache cache;
    UnitStats& stats;

    std::vector<std::optional<DecodedFrame>> frames(const fig::Entry& entry,
                                                    const std::vector<fig::FrameSet>& sets,
                                                    const sze::Unit& unit, bool shadow,
                                                    std::optional<figure::Rgb> colour) {
        std::vector<std::optional<DecodedFrame>> out;
        if (sets.empty()) {
            return out;
        }
        std::uint32_t groups = 0;
        for (const auto& fs : sets) {
            groups = std::max(groups, fs.group + 1);
        }
        // Shadows are coverage masks: no colours.
        if (shadow) {
            colour = std::nullopt;
        }
        for (std::uint32_t g = 0; g < groups; ++g) {
            const auto fs = std::find_if(sets.begin(), sets.end(),
                                         [g](const fig::FrameSet& s) { return s.group == g; });
            if (fs == sets.end() || fs->frames.empty()) {
                out.emplace_back();
                continue;
            }
            const std::size_t k = unit.attr % fs->frames.size();
            const CacheKey key{unit.figure, shadow, g, k, colour};
            auto it = cache.find(key);
            if (it == cache.end()) {
                const fig::Frame& f = fs->frames[k];
                std::optional<DecodedFrame> decoded;
                if (auto data = read(g, f.offset, f.length)) {
                    if (auto img = figure::decode(entry, f, *data, colour)) {
                        decoded =
                            DecodedFrame{f, std::make_shared<const Rgba>(std::move(img).value()),
                                         static_cast<std::uint32_t>(k), colour};
                    }
                }
                if (!decoded) {
                    ++stats.failed_frames;
                }
                it = cache.emplace(key, std::move(decoded)).first;
            }
            out.push_back(it->second);
        }
        return out;
    }
};

} // namespace

Result<Units, std::string> units(const sze::Scene& scene, const fig::FigurenHeader& header,
                                 std::span<const UnitKind> kinds,
                                 std::span<const figure::Rgb> palette, const VolumeReader& read) {
    Units out;
    auto start = start_colours(
        scene, static_cast<std::uint32_t>(std::min<std::size_t>(palette.size(), 0xFE)));
    if (!start) {
        return start.error();
    }
    auto players = scene.players();
    if (!players) {
        return players.error();
    }
    Decoder decoder{read, {}, out.stats};
    for (const auto& player : players.value()) {
        if (start.value().removed.contains(player.index)) {
            continue;
        }
        std::optional<figure::Rgb> player_colour;
        if (const auto slot = start.value().slots.find(player.index);
            slot != start.value().slots.end() && slot->second >= 1 &&
            slot->second <= palette.size()) {
            player_colour = palette[slot->second - 1U];
        }
        for (const UnitKind kind : kinds) {
            auto list = player.units(record_name(kind));
            if (!list) {
                return list.error();
            }
            for (const auto& unit : list.value()) {
                ++out.stats.units;
                const fig::Entry* entry = header.entry(unit.figure);
                if (entry == nullptr || entry->gfx.empty()) {
                    ++out.stats.missing;
                    continue;
                }
                const fig::GfxSet& set = entry->gfx.front();
                const auto colour = figure::player_coloured(*entry) ? player_colour : std::nullopt;
                UnitFrames u{kind, unit.figure, unit.x(), unit.y(), {}, {}};
                u.frames = decoder.frames(*entry, set.frame_sets, unit, false, colour);
                if (set.shadow) {
                    u.shadows = decoder.frames(*entry, set.shadow->frame_sets, unit, true, colour);
                }
                out.units.push_back(std::move(u));
            }
        }
    }
    return out;
}

} // namespace anno::assets::unit
