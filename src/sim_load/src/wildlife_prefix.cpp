#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <set>

#include <anno/formats/sha256.hpp>
#include <anno/formats/tagged.hpp>
#include <anno/sim_load/wildlife_prefix.hpp>
namespace anno::sim_load {
namespace ri = sim::resource_import;
formats::Result<std::vector<ri::TypeFacts>, std::string>
resource_metadata(std::span<const formats::figuren::Entry> entries,
                  const sim::TypeRegistry& types) {
    std::map<std::string, sim::GoodsId> names;
    names.emplace("UNUSED", 0); // Explicit parsed token only, never an absent-field fallback.
    const auto goods = types.goods_table();
    for (std::uint32_t i = 0; i < goods.size(); ++i)
        if (!goods[i].name.empty())
            names.emplace(goods[i].name, i);
    std::map<std::uint32_t, ri::TypeFacts> indexed;
    for (const auto& entry : entries) {
        const auto count = [&](std::string_view name) {
            return std::count_if(entry.records.begin(), entry.records.end(),
                                 [name](const auto& r) { return r.name() == name; });
        };
        if (count("WARE") != 1 || count("HEAD") != 1)
            return "resource metadata entry " + std::to_string(entry.id) +
                   ": missing/repeated WARE or HEAD";
        const auto ware = entry.record("WARE"), head = entry.record("HEAD");
        const auto prefix = "resource metadata entry " + std::to_string(entry.id) + ": ";
        if (!ware || ware->empty())
            return prefix + "missing WARE";
        const auto end = std::find(ware->begin(), ware->end(), std::uint8_t{0});
        if (end == ware->end() || end == ware->begin())
            return prefix + "unterminated/empty WARE";
        const std::string name(ware->begin(), end);
        const auto found = names.find(name);
        if (found == names.end() || found->second > 255)
            return prefix + "unknown or non-byte WARE";
        if (!head || head->size() < 0x1c)
            return prefix + "truncated/missing HEAD selector tag";
        if (!indexed
                 .emplace(entry.id, ri::TypeFacts{sim::TypeId{entry.id},
                                                  static_cast<std::uint8_t>(found->second),
                                                  formats::u16_le(*head, 0x1a)})
                 .second)
            return prefix + "duplicate id";
    }
    std::vector<ri::TypeFacts> out;
    for (const auto& [id, fact] : indexed) {
        (void)id;
        out.push_back(fact);
    }
    return out;
}
namespace {
ri::ClimateFacts climate_facts(const formats::annogame::ResourceTables& raw) {
    const auto widen = [](const auto& row) {
        std::vector<sim::TypeId> out;
        for (const auto id : row)
            out.emplace_back(id);
        return out;
    };
    return {widen(raw.slots), widen(raw.tag104), widen(raw.tag201), widen(raw.tag404)};
}
bool setup_tag(std::string_view name) {
    return name == "INFO" || name == "HEIGHT" || name == "GFXID";
}
// OCCUR's reached ore profile only; contract: docs/sim/wildlife-occur.md.
std::uint64_t resource_columns(const formats::insel::Island& raw) {
    return (std::uint64_t{raw.width} + 7) / 8;
}
void materialize_resource_chunks(PreparedIsland& out, const formats::insel::Island& raw) {
    if (out.resource_list_chunks.empty())
        out.resource_list_chunks.resize(static_cast<std::size_t>(
            resource_columns(raw) * ((std::uint64_t{raw.height} + 7) / 8)));
}
std::optional<std::uint64_t> occurrence_candidate(const PreparedIsland& out,
                                                  const formats::insel::Island& raw, std::int32_t x,
                                                  std::int32_t y) {
    const auto nx = resource_columns(raw);
    const auto first_x = static_cast<std::uint64_t>(x - raw.x) / 8;
    const auto first_y = static_cast<std::uint64_t>(y - raw.y) / 8;
    const auto ny = (std::uint64_t{raw.height} + 7) / 8;
    // 0x10095770: first occupied chunk at/after this position, then its FIRST kind 6.
    for (auto cy = first_y; cy < ny; ++cy)
        for (auto cx = cy == first_y ? first_x : 0; cx < nx; ++cx)
            for (auto n = out.resource_list_chunks[cy * nx + cx].head; n;
                 n = out.resource_list_nodes[*n].next)
                if (out.resource_list_nodes[*n].kind == 6)
                    return out.resource_list_nodes[*n].entry;
    return {};
}
std::vector<std::uint64_t> query_occurrences(const PreparedIsland& out,
                                             const formats::insel::Island& raw, std::uint16_t x,
                                             std::uint16_t y, std::uint32_t w, std::uint32_t h) {
    const auto left = std::max(std::int64_t{x}, std::int64_t{raw.x});
    const auto top = std::max(std::int64_t{y}, std::int64_t{raw.y});
    const auto right = std::min(std::int64_t{x} + w, std::int64_t{raw.x} + raw.width);
    const auto bottom = std::min(std::int64_t{y} + h, std::int64_t{raw.y} + raw.height);
    std::vector<std::uint64_t> found;
    if (left >= right || top >= bottom)
        return found;
    const auto nx = resource_columns(raw);
    // 0x10095480: world-aligned chunk visits; local matrix division truncates.
    for (auto by = top / 8 * 8; by < bottom; by += 8)
        for (auto bx = left / 8 * 8; bx < right; bx += 8) {
            const auto cx = static_cast<std::uint64_t>((bx - raw.x) / 8);
            const auto cy = static_cast<std::uint64_t>((by - raw.y) / 8);
            const auto chunk = cy * nx + cx;
            for (auto n = out.resource_list_chunks[chunk].head; n;
                 n = out.resource_list_nodes[*n].next) {
                const auto& node = out.resource_list_nodes[*n];
                if (node.kind != 6)
                    continue;
                // Original chunk coordinates are u8 fields, 0x1008c3ea/0x1008c3f6.
                const auto px = std::int64_t{raw.x} + std::uint8_t(cx) * 8 + node.x_offset;
                const auto py = std::int64_t{raw.y} + std::uint8_t(cy) * 8 + node.y_offset;
                if (px >= x && px < std::int64_t{x} + w && py >= y && py < std::int64_t{y} + h)
                    found.push_back(node.entry);
            }
        }
    return found;
}
std::int32_t signed_nibble(std::uint8_t bits) {
    const auto value = static_cast<std::int32_t>(bits & 15U);
    return value >= 8 ? value - 16 : value;
}
std::uint64_t refresh_ore_masks(PreparedIsland& out, const formats::insel::Island& raw,
                                std::int32_t tile_x, std::int32_t tile_y) {
    // Reader 0x10096124..0x10096171: u16 subtraction, not a signed edge clamp.
    const auto x = static_cast<std::uint16_t>(tile_x - 7);
    const auto y = static_cast<std::uint16_t>(tile_y - 7);
    const auto left = std::max(std::int64_t{x} - 3 - raw.x, std::int64_t{0});
    const auto top = std::max(std::int64_t{y} - 3 - raw.y, std::int64_t{0});
    const auto right = std::min(std::int64_t{x} + 15 - raw.x, std::int64_t{raw.width});
    const auto bottom = std::min(std::int64_t{y} + 15 - raw.y, std::int64_t{raw.height});
    const auto nx = resource_columns(raw);
    std::uint64_t changed = 0;
    for (auto by = top / 8 * 8; by < bottom; by += 8)
        for (auto bx = left / 8 * 8; bx < right; bx += 8) {
            const auto chunk =
                static_cast<std::uint64_t>(by / 8) * nx + static_cast<std::uint64_t>(bx / 8);
            for (auto n = out.resource_list_chunks[chunk].head; n;
                 n = out.resource_list_nodes[*n].next) {
                auto& node = out.resource_list_nodes[*n];
                if (node.kind != 1)
                    continue;
                const auto ax = raw.x + bx + signed_nibble(node.offsets);
                const auto ay =
                    raw.y + by + signed_nibble(static_cast<std::uint8_t>(node.offsets >> 4));
                if (ax < x || ax >= std::int64_t{x} + 15 || ay < y || ay >= std::int64_t{y} + 15)
                    continue;
                const std::uint32_t width = node.rotation & 1U ? 4 : 3;
                const std::uint32_t height = node.rotation & 1U ? 3 : 4;
                const auto sources =
                    query_occurrences(out, raw, static_cast<std::uint16_t>(ax - 7),
                                      static_cast<std::uint16_t>(ay - 7), width + 14, height + 14);
                std::uint8_t mask = 0x10;
                // Radius 7 row bounds generated at 0x1007c0de..0x1007c115.
                constexpr std::array<std::int64_t, 8> row_bounds{7, 7, 7, 6, 6, 5, 4, 2};
                for (const auto index : sources) {
                    const auto& ore = out.occurrences[index];
                    auto dy = ay - ore.tile_y;
                    if (dy < 0)
                        dy = 1 - dy - height;
                    if (dy > 7)
                        continue;
                    dy = std::max(dy, std::int64_t{0});
                    auto dx = ax - ore.tile_x;
                    if (dx < 0)
                        dx = 1 - dx - width;
                    if (dx <= row_bounds[static_cast<std::size_t>(dy)])
                        mask = static_cast<std::uint8_t>(mask | (1U << (ore.goods - 1)));
                }
                changed += node.mask != mask;
                node.mask = mask;
            }
        }
    return changed;
}
formats::Result<PrefixOccurrenceLoad, std::string> load_ore(PreparedIsland& out,
                                                            const formats::insel::Island& raw,
                                                            const formats::tagged::Record& record,
                                                            PrefixSource source,
                                                            const sim::TypeRegistry& types) {
    if (record.payload.size() < 16)
        return std::string("ENTRY2 needs its 16-byte base");
    const auto type = (formats::u32_le(record.payload, 0) >> 14) & 0xfffU;
    const auto* facts = types.find_figure_type(type);
    if (!facts || facts->bewohntype != 145)
        return std::string("unsupported occurrence figure class/type ") + std::to_string(type);
    const auto goods = types.goods_table();
    const auto ware = std::find_if(goods.begin(), goods.end(),
                                   [&](const auto& g) { return g.name == facts->ware; });
    if (ware == goods.end() || ware - goods.begin() < 1 || ware - goods.begin() > 8)
        return std::string("unsupported ore goods");
    const auto local_x = std::bit_cast<float>(formats::u32_le(record.payload, 4));
    const auto local_y = std::bit_cast<float>(formats::u32_le(record.payload, 8));
    const float world_x = local_x + static_cast<float>(raw.x);
    const float world_y = local_y + static_cast<float>(raw.y);
    if (!std::isfinite(world_x) || !std::isfinite(world_y) ||
        static_cast<double>(world_x) < std::numeric_limits<std::int32_t>::min() ||
        static_cast<double>(world_x) > std::numeric_limits<std::int32_t>::max() ||
        static_cast<double>(world_y) < std::numeric_limits<std::int32_t>::min() ||
        static_cast<double>(world_y) > std::numeric_limits<std::int32_t>::max())
        return std::string("unsafe occurrence position");
    const auto x = static_cast<std::int32_t>(world_x), y = static_cast<std::int32_t>(world_y);
    if (x < raw.x || y < raw.y || std::int64_t{x} >= std::int64_t{raw.x} + raw.width ||
        std::int64_t{y} >= std::int64_t{raw.y} + raw.height)
        return std::string("occurrence position outside island matrix");
    materialize_resource_chunks(out, raw);
    const auto candidate = occurrence_candidate(out, raw, x, y);
    if (candidate &&
        static_cast<std::uint16_t>(out.occurrences[*candidate].tile_x) ==
            static_cast<std::uint16_t>(x) &&
        static_cast<std::uint16_t>(out.occurrences[*candidate].tile_y) ==
            static_cast<std::uint16_t>(y))
        return PrefixOccurrenceLoad{source, *candidate, 0, false};
    PrefixOccurrence ore;
    ore.source = source;
    ore.type = type;
    ore.goods = static_cast<std::uint32_t>(ware - goods.begin());
    ore.world_x_bits = std::bit_cast<std::uint32_t>(world_x);
    ore.world_y_bits = std::bit_cast<std::uint32_t>(world_y);
    ore.height_bits = formats::u32_le(record.payload, 12);
    ore.tile_x = x;
    ore.tile_y = y;
    // Original descriptor-id exception in attach, 0x1006377d..0x1006378b.
    ore.reserve = type == 1661 ? 0 : 1;
    ore.quantity_88 = ore.reserve ? 512000 : 102400;
    if (record.payload.size() - 16 == 8) {
        ore.file_tons = formats::u16_le(record.payload, 16);
        ore.quantity_88 = std::uint32_t{*ore.file_tons} << 8;
        if (ore.reserve != 0 && ore.quantity_88 < 409600)
            ore.quantity_88 = 512000;
        ore.discovery = formats::u32_le(record.payload, 20);
    }
    const auto index = static_cast<std::uint64_t>(out.occurrences.size());
    out.occurrences.push_back(ore);
    const auto lx = static_cast<std::uint32_t>(x - raw.x),
               ly = static_cast<std::uint32_t>(y - raw.y);
    const auto chunk = std::uint64_t{ly / 8} * resource_columns(raw) + lx / 8;
    auto& target = out.resource_list_chunks[chunk];
    const auto node = static_cast<std::uint64_t>(out.resource_list_nodes.size());
    out.resource_list_nodes.push_back({index, chunk, 0, 0, 0, 6, target.head,
                                       static_cast<std::uint8_t>(lx % 8),
                                       static_cast<std::uint8_t>(ly % 8)});
    target.head = node;
    target.resource_flags |= 0x40;
    return PrefixOccurrenceLoad{source, index, refresh_ore_masks(out, raw, x, y), true};
}
} // namespace
formats::Result<PreparedPrefix, PrefixFailure>
prepare_wildlife_prefix(formats::Bytes scene, std::span<const formats::figuren::Entry> entries,
                        formats::Bytes annogame, const sim::TypeRegistry& types,
                        PrefixInputs inputs, std::span<const std::uint32_t> draws) {
    if (inputs.mode_bit > 1 || inputs.stamp_provenance.empty() || inputs.mode_provenance.empty() ||
        inputs.chunk_edge == 0)
        return PrefixFailure{
            "prefix: explicit mode/stamp provenance and nonzero chunk geometry required", {}};
    const auto raw_islands = formats::insel::islands(scene);
    if (!raw_islands)
        return PrefixFailure{raw_islands.error(), {}};
    if (inputs.sampled_stamps.size() != raw_islands.value().size())
        return PrefixFailure{"prefix: one explicit stamp per source island required", {}};
    const auto facts = resource_metadata(entries, types);
    if (!facts)
        return PrefixFailure{facts.error(), {}};
    PreparedPrefix out;
    out.supplied = std::move(inputs);
    out.source_sha256 = formats::sha256_hex(scene);
    std::set<std::uint32_t> seen_slots;
    std::map<std::uint32_t, ri::ClimateFacts> climates;
    std::optional<formats::annogame::AnbauNames> anbau_names;
    for (std::uint64_t ordinal = 0; ordinal < raw_islands.value().size(); ++ordinal) {
        const auto& raw = raw_islands.value()[ordinal];
        PreparedIsland prepared;
        prepared.slot = raw.number;
        sim::Island backing;
        if (!backing.tiles.reset(raw.x, raw.y, raw.width, raw.height, out.supplied.chunk_edge))
            return PrefixFailure{"prefix: invalid island geometry", {}};
        if (!seen_slots.insert(raw.number).second ||
            raw.version != formats::insel::Version::Insel5) {
            out.boundary = PrefixBoundary{
                PrefixSource{ordinal, std::numeric_limits<std::uint64_t>::max(), 0, 0, raw.number},
                "INSEL5", "fresh embedded unique-slot domain required"};
            prepared.resource = ri::Output{std::move(backing.tiles), {}, {}, 0};
            out.islands.push_back(std::move(prepared));
            return PrefixFailure{out.boundary->reason, std::move(out)};
        }
        std::vector<std::uint8_t> heights(backing.tiles.tile_count(), 0);
        std::vector<std::uint16_t> graphics(backing.tiles.tile_count(), 0);
        std::uint64_t setup_end = 0;
        bool has_info = false;
        std::optional<PrefixBoundary> setup_error;
        const auto source = [&](std::uint64_t h, std::uint64_t r = 0) {
            return PrefixSource{ordinal, h, r, h < raw.records.size() ? raw.records[h].offset : 0,
                                raw.number};
        };
        for (; setup_end < raw.records.size() && setup_tag(raw.records[setup_end].name());
             ++setup_end) {
            const auto& header = raw.records[setup_end];
            const auto name = header.name();
            prepared.headers.push_back(
                {source(setup_end), std::string(name), 0, {}, {}, {}, {}, {}, {}});
            if (name == "INFO") {
                if (header.payload.size() != 12) {
                    setup_error = PrefixBoundary{source(setup_end), "INFO", "strict INFO length"};
                    break;
                }
                has_info = true;
                prepared.climate = header.payload[0];
                continue;
            }
            const auto block = formats::insel::Block::parse(header.payload);
            if (!block) {
                setup_error = PrefixBoundary{source(setup_end), std::string(name), block.error()};
                break;
            }
            const auto& b = block.value();
            const auto count = std::uint64_t{b.width} * b.height;
            const std::uint64_t cell_size = name == "HEIGHT" ? 1U : 2U;
            if (b.x > raw.width || b.y > raw.height || b.width > raw.width - b.x ||
                b.height > raw.height - b.y || b.data.size() != count * cell_size) {
                setup_error = PrefixBoundary{source(setup_end), std::string(name),
                                             "invalid terrain block geometry/length"};
                break;
            }
            for (std::uint32_t y = 0; y < b.height; ++y)
                for (std::uint32_t x = 0; x < b.width; ++x) {
                    const auto from = std::uint64_t{y} * b.width + x;
                    const auto to = std::uint64_t{b.y + y} * raw.width + b.x + x;
                    if (name == "HEIGHT")
                        heights[to] = b.data[from];
                    else
                        graphics[to] = formats::u16_le(b.data, from * 2U);
                }
        }
        backing.terrain = sim::make_terrain(std::move(heights), std::move(graphics), {});
        prepared.terrain = backing.terrain;
        const auto stop_before = [&](PrefixBoundary boundary) {
            out.boundary = std::move(boundary);
            prepared.resource = ri::Output{
                backing.tiles, std::vector<std::uint8_t>(backing.tiles.tile_count(), 0), {}, 0};
            out.islands.push_back(std::move(prepared));
        };
        if (!has_info && !setup_error)
            setup_error =
                PrefixBoundary{source(0), "INFO", "explicit leading INFO climate required"};
        if (setup_error) {
            stop_before(*setup_error);
            return PrefixFailure{out.boundary->reason, std::move(out)};
        }
        auto climate = climates.find(prepared.climate);
        if (climate == climates.end()) {
            const auto table = formats::annogame::read_resource_tables(annogame, prepared.climate);
            if (!table)
                return PrefixFailure{table.error(), {}};
            climate = climates.emplace(prepared.climate, climate_facts(table.value())).first;
        }
        const std::vector<std::uint8_t> variation(backing.tiles.tile_count(), 0);
        auto started = ri::Session::start(backing.tiles, sim::VertexGrid::of(backing), variation,
                                          types, facts.value(), climate->second);
        if (!started)
            return PrefixFailure{"prefix: resource session initialization refused", {}};
        auto& session = started.value();
        std::optional<std::vector<std::uint8_t>> fertility;
        const auto finalize_terrain = [&] {
            // Keep backing alive for Session's borrowed HEIGHT view. Only extraction
            // copies the other columns; no map-sized copy per source record/header.
            if (fertility)
                prepared.terrain = sim::make_terrain(backing.terrain->height, backing.terrain->gfx,
                                                     std::move(*fertility));
        };
        prepared.chunk_heads.resize(backing.tiles.chunk_count());
        const auto stop = [&](PrefixBoundary boundary) {
            out.boundary = std::move(boundary);
            prepared.resource = std::move(session).diagnostic();
            finalize_terrain();
            out.islands.push_back(std::move(prepared));
        };
        for (std::uint64_t h = setup_end; h < raw.records.size(); ++h) {
            const auto& header = raw.records[h];
            const auto name = header.name();
            prepared.headers.push_back({source(h), std::string(name), 0, {}, {}, {}, {}, {}, {}});
            if (name == "GELAENDE") {
                const auto block = formats::insel::Block::parse(header.payload);
                if (!block || block.value().data.size() % 6 != 0) {
                    stop({source(h), "GELAENDE", "invalid original item block length"});
                    return PrefixFailure{out.boundary->reason, std::move(out)};
                }
                const auto& b = block.value();
                for (std::uint64_t i = 0; i < b.data.size() / 6; ++i) {
                    const auto item = formats::insel::decode_import_object(b.data.subspan(i * 6, 6),
                                                                           b, raw.x, raw.y);
                    if (!item) {
                        stop({source(h, i), "GELAENDE", item.error()});
                        return PrefixFailure{out.boundary->reason, std::move(out)};
                    }
                    const auto& o = item.value();
                    ri::Record record{sim::TypeId{o.type}, o.x, o.y, o.rotation, o.state};
                    const auto before = out.consumed, resource_before = session.consumed();
                    const auto effect = session.apply(
                        record, draws.subspan(static_cast<std::size_t>(out.consumed)));
                    if (!effect) {
                        out.consumed += effect.error().consumed - resource_before;
                        stop({source(h, i), "GELAENDE",
                              "resource refusal kind " +
                                  std::to_string(static_cast<unsigned>(effect.error().kind)) +
                                  " type " + std::to_string(o.type)});
                        return PrefixFailure{out.boundary->reason, std::move(out)};
                    }
                    out.consumed += effect.value().draws;
                    prepared.resources.push_back({source(h, i), record, effect.value(), before});
                    ++prepared.headers.back().records;
                }
                // Header callback runs even for zero items, after the record loop.
                const auto closed = session.close_import_block(
                    static_cast<std::int32_t>(std::uint32_t{raw.x} + b.x),
                    static_cast<std::int32_t>(std::uint32_t{raw.y} + b.y), b.width, b.height);
                if (!closed) {
                    stop({source(h, static_cast<std::uint64_t>(b.data.size() / 6)), "GELAENDE",
                          "poisoned header epilogue"});
                    return PrefixFailure{out.boundary->reason, std::move(out)};
                }
                prepared.headers.back().water_mask_updates = closed.value();
            } else if (name == "DISCOVER") {
                // AnnoWelt 0x100899ab/0x100899b6; contract: wildlife-discover.md.
                // Exact framing is defensive; the original arm ignores short-read failure.
                if (header.payload.size() != 4) {
                    stop({source(h), "DISCOVER", "expected four-byte DISCOVER payload"});
                    return PrefixFailure{out.boundary->reason, std::move(out)};
                }
                const std::uint32_t mask = formats::u16_le(header.payload, 0);
                prepared.discover_mask = mask;
                prepared.headers.back().discover = mask;
                prepared.headers.back().records = 1;
            } else if (name == "ANBAU") {
                const std::uint32_t seed = prepared.climate == 0 ? 0 : 0xf009;
                if (header.payload.empty()) {
                    // Original seed-only path returns without consulting any name.
                    prepared.anbau_mask = seed;
                    prepared.headers.back().anbau = PrefixAnbauEffect{seed, seed, 0};
                    continue;
                }
                if (!anbau_names) {
                    const auto names = formats::annogame::read_anbau_names(annogame);
                    if (!names) {
                        stop({source(h), "ANBAU", names.error()});
                        return PrefixFailure{out.boundary->reason, std::move(out)};
                    }
                    anbau_names = names.value();
                }
                const auto& names = *anbau_names;
                std::vector<std::string> tokens;
                std::size_t at = 0;
                // The supported original reader's 13-token bound equals this name
                // window, 0x100898dc..0x1008995f. Tail bytes are not another token.
                while (at < header.payload.size() && tokens.size() < names.names.size()) {
                    const auto rest = header.payload.subspan(at);
                    const auto end = std::find(rest.begin(), rest.end(), std::uint8_t{0});
                    if (end == rest.end()) {
                        stop({source(h, tokens.size()), "ANBAU", "unterminated processed token"});
                        return PrefixFailure{out.boundary->reason, std::move(out)};
                    }
                    tokens.emplace_back(rest.begin(), end);
                    at += tokens.back().size() + 1;
                }
                auto mask = seed;
                for (std::uint64_t i = 0; i < tokens.size(); ++i) {
                    const auto match = std::find(names.names.begin(), names.names.end(), tokens[i]);
                    std::optional<std::uint32_t> goods;
                    if (match != names.names.end()) {
                        const auto offset = static_cast<std::uint32_t>(match - names.names.begin());
                        goods = names.first_goods + offset;
                        mask |= std::uint32_t{1} << offset;
                    }
                    prepared.anbau_tokens.push_back({source(h, i), std::move(tokens[i]), goods});
                    ++prepared.headers.back().records;
                }
                prepared.anbau_mask = mask;
                prepared.headers.back().anbau =
                    PrefixAnbauEffect{seed, mask, header.payload.size() - at};
            } else if (name == "OCCUR") {
                const auto records = formats::tagged::parse_records(header.payload);
                if (!records) {
                    stop({source(h), "OCCUR", records.error()});
                    return PrefixFailure{out.boundary->reason, std::move(out)};
                }
                const auto before = prepared.occurrences.size();
                for (std::uint64_t r = 0; r < records.value().size(); ++r) {
                    if (records.value()[r].name != "ENTRY2")
                        continue;
                    const auto loaded =
                        load_ore(prepared, raw, records.value()[r], source(h, r), types);
                    if (!loaded) {
                        stop({source(h, r), "OCCUR", loaded.error()});
                        return PrefixFailure{out.boundary->reason, std::move(out)};
                    }
                    prepared.occurrence_loads.push_back(loaded.value());
                    ++prepared.headers.back().records;
                }
                prepared.headers.back().occurrences_added = prepared.occurrences.size() - before;
            } else if (name == "BERGWERK2") {
                const auto records = formats::tagged::parse_records(header.payload);
                if (!records) {
                    stop({source(h), "BERGWERK2", records.error()});
                    return PrefixFailure{out.boundary->reason, std::move(out)};
                }
                // Original matrix, AnnoWelt 0x1008c2ed..0x1008c39e; not spawn-head geometry.
                constexpr std::int64_t edge = 8;
                const auto nx = (std::int64_t{raw.width} + edge - 1) / edge;
                const auto ny = (std::int64_t{raw.height} + edge - 1) / edge;
                if (prepared.resource_list_chunks.empty())
                    prepared.resource_list_chunks.resize(static_cast<std::size_t>(nx * ny));
                const auto before = prepared.resource_list_nodes.size();
                for (std::uint64_t r = 0; r < records.value().size(); ++r) {
                    const auto& record = records.value()[r];
                    if (record.name != "ENTRY")
                        continue; // Original 0x10078a36 skips unknown tagged payloads.
                    if (record.payload.size() != 4) {
                        stop({source(h, r), "BERGWERK2", "ENTRY must contain one u32"});
                        return PrefixFailure{out.boundary->reason, std::move(out)};
                    }
                    const auto word = formats::u32_le(record.payload, 0);
                    const auto lx = word & 0xfffU;
                    const auto ly = (word >> 12) & 0xfffU;
                    const auto rotation = static_cast<std::uint8_t>((word >> 24) & 3U);
                    const auto x = static_cast<std::uint16_t>(lx + raw.x);
                    const auto y = static_cast<std::uint16_t>(ly + raw.y);
                    // Original 0x10077e63..0x10077e93: 3x4 or 4x3, not type metadata.
                    const std::int64_t w = (rotation & 1U) ? 4 : 3;
                    const std::int64_t height = (rotation & 1U) ? 3 : 4;
                    std::vector<std::pair<std::uint64_t, std::uint8_t>> nodes;
                    for (auto by = y / edge * edge; by < y + height; by += edge)
                        for (auto bx = x / edge * edge; bx < x + w; bx += edge) {
                            // C truncating division matches the original signed idiom.
                            const auto cx = (bx - raw.x) / edge;
                            const auto cy = (by - raw.y) / edge;
                            if (cx < 0 || cy < 0 || cx >= nx || cy >= ny) {
                                stop({source(h, r), "BERGWERK2",
                                      "resource chunk outside island matrix"});
                                return PrefixFailure{out.boundary->reason, std::move(out)};
                            }
                            const auto dx = static_cast<std::uint8_t>(x - bx);
                            const auto dy = static_cast<std::uint8_t>(y - by);
                            nodes.emplace_back(
                                static_cast<std::uint64_t>(cy * nx + cx),
                                static_cast<std::uint8_t>((dx & 15U) | ((dy & 15U) << 4)));
                        }
                    const auto entry = static_cast<std::uint64_t>(prepared.bergwerk_entries.size());
                    prepared.bergwerk_entries.push_back({source(h, r), lx, ly, x, y, rotation});
                    for (const auto& [chunk, offsets] : nodes) {
                        auto& target = prepared.resource_list_chunks[chunk];
                        const auto index =
                            static_cast<std::uint64_t>(prepared.resource_list_nodes.size());
                        prepared.resource_list_nodes.push_back(
                            {entry, chunk, offsets, rotation, 0x10, 1, target.head});
                        target.head = index;
                        target.resource_flags |= 2;
                    }
                    ++prepared.headers.back().records;
                }
                prepared.headers.back().bergwerk_nodes_added =
                    prepared.resource_list_nodes.size() - before;
            } else if (name == "FRUCHTBAR") {
                const auto effect = session.apply_fertility(header.payload);
                if (!effect) {
                    stop({source(h), "FRUCHTBAR", "invalid fertility length or poisoned session"});
                    return PrefixFailure{out.boundary->reason, std::move(out)};
                }
                fertility.emplace(header.payload.begin(), header.payload.end());
                prepared.headers.back().fertility_updates = effect.value();
            } else if (name == "HIRSCH") {
                const auto records = formats::insel::decode_hirsch(header.payload);
                if (!records) {
                    stop({source(h), "HIRSCH", records.error()});
                    return PrefixFailure{out.boundary->reason, std::move(out)};
                }
                for (std::uint64_t i = 0; i < records.value().size(); ++i) {
                    const auto& r = records.value()[i];
                    if (r.x >= raw.width || r.y >= raw.height) {
                        stop({source(h, i), "HIRSCH", "spawn coordinate outside island"});
                        return PrefixFailure{out.boundary->reason, std::move(out)};
                    }
                    const auto before = out.consumed;
                    const auto loaded = sim::wildlife_load(
                        r.flags, sim::wildlife_load_base(out.supplied.sampled_stamps[ordinal]),
                        out.supplied.mode_bit, r.goods,
                        draws.subspan(static_cast<std::size_t>(out.consumed)));
                    if (!loaded) {
                        out.consumed += loaded.error().consumed;
                        stop({source(h, i), "HIRSCH", "wildlife load draw/input refusal"});
                        return PrefixFailure{out.boundary->reason, std::move(out)};
                    }
                    out.consumed += loaded.value().consumed;
                    const auto chunk =
                        std::uint64_t{r.y / out.supplied.chunk_edge} * backing.tiles.chunks_x() +
                        r.x / out.supplied.chunk_edge;
                    const auto n = static_cast<std::uint64_t>(prepared.spawns.size());
                    prepared.spawns.push_back({source(h, i), r, loaded.value().value.saved,
                                               loaded.value().value.timestamp, chunk, before,
                                               prepared.chunk_heads[chunk]});
                    prepared.chunk_heads[chunk] = n;
                    ++prepared.headers.back().records;
                }
            } else {
                stop({source(h), std::string(name),
                      "unsupported intervening header or late terrain/setup callback"});
                return PrefixFailure{out.boundary->reason, std::move(out)};
            }
        }
        auto finished = session.finish();
        if (!finished)
            return PrefixFailure{"prefix: poisoned session cannot complete", {}};
        prepared.resource = std::move(finished.value());
        finalize_terrain();
        prepared.complete_local_records = true;
        out.islands.push_back(std::move(prepared));
    }
    out.complete_local_records = true;
    return out;
}
} // namespace anno::sim_load
