#include <algorithm>
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
            prepared.headers.push_back({source(setup_end), std::string(name), 0, {}, {}, {}});
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
            prepared.headers.push_back({source(h), std::string(name), 0, {}, {}, {}});
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
                if (prepared.bergwerk_chunks.empty())
                    prepared.bergwerk_chunks.resize(static_cast<std::size_t>(nx * ny));
                const auto before = prepared.bergwerk_nodes.size();
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
                        auto& target = prepared.bergwerk_chunks[chunk];
                        const auto index =
                            static_cast<std::uint64_t>(prepared.bergwerk_nodes.size());
                        prepared.bergwerk_nodes.push_back(
                            {entry, chunk, offsets, rotation, 0x10, 1, target.head});
                        target.head = index;
                        target.resource_flags |= 2;
                    }
                    ++prepared.headers.back().records;
                }
                prepared.headers.back().bergwerk_nodes_added =
                    prepared.bergwerk_nodes.size() - before;
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
