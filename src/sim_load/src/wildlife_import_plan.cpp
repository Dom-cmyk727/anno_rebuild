#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
#include <utility>

#include <anno/formats/sha256.hpp>
#include <anno/formats/tagged.hpp>
#include <anno/sim_load/wildlife_import_plan.hpp>

namespace anno::sim_load {
namespace {
auto key(const PrefixSource& s) {
    return std::pair{s.header, s.record};
}
bool same_source(const PrefixSource& a, const PrefixSource& b) {
    return std::tie(a.island, a.header, a.record, a.header_offset, a.slot) ==
           std::tie(b.island, b.header, b.record, b.header_offset, b.slot);
}
struct ImportEvent {
    PrefixSource source;
    std::uint64_t chunk = 0, index = 0, ordinal = 0;
    std::uint32_t category = 0;
};
} // namespace
formats::Result<WildlifeImportedChunk, WildlifeImportFailure>
reconstruct_wildlife_import(const WildlifeFirstCallResult& prepared, formats::Bytes scene,
                            std::uint32_t cx, std::uint32_t cy, const WildlifeImportFlags& flags) {
    using S = WildlifeImportStage;
    const auto fail = [&](S stage, std::string reason, std::optional<PrefixSource> source = {}) {
        return WildlifeImportFailure{stage, std::move(reason), flags, prepared.fill.prefix_consumed,
                                     source};
    };
    // Proven represented categories, not all native cache/flag meanings.
    constexpr std::uint16_t presence_mask = 0x43;
    if (!flags.non_presence || flags.provenance.empty() ||
        (*flags.non_presence & presence_mask) != 0)
        return fail(S::Flags, "import plan: non-presence flags/provenance required and disjoint");
    const auto& is = prepared.fill.island;
    const auto& tiles = is.resource.tiles;
    const auto parsed = formats::insel::islands(scene);
    if (!parsed || parsed.value().size() != 1 || prepared.ordinal != 0 ||
        formats::sha256_hex(scene) != prepared.source_sha256 || !is.complete_local_records ||
        prepared.supplied.chunk_edge != 8 || tiles.chunk_edge() != 8 || tiles.x0() % 8 != 0 ||
        tiles.y0() % 8 != 0)
        return fail(S::Source, "import plan: completed aligned source profile required");
    const auto& raw = parsed.value()[0];
    if (raw.version != formats::insel::Version::Insel5 || raw.number != is.slot ||
        raw.x != tiles.x0() || raw.y != tiles.y0() || raw.width != tiles.width() ||
        raw.height != tiles.height() || std::uint64_t{cx} * 8 + 8 > raw.width ||
        std::uint64_t{cy} * 8 + 8 > raw.height || is.headers.size() != raw.records.size() ||
        is.chunk_heads.size() != tiles.chunk_count() ||
        (!is.resource_list_chunks.empty() && is.resource_list_chunks.size() != tiles.chunk_count()))
        return fail(S::Source, "import plan: source geometry/projection size mismatch");
    const auto source_valid = [&](const PrefixSource& s, std::string_view tag) {
        return s.island == prepared.ordinal && s.slot == raw.number &&
               s.header < raw.records.size() && s.header_offset == raw.records[s.header].offset &&
               raw.records[s.header].name() == tag;
    };
    std::vector<std::vector<formats::insel::HirschRecord>> spawn_raw(raw.records.size());
    std::vector<std::vector<formats::tagged::Record>> tagged(raw.records.size());
    std::vector<std::uint64_t> expected(raw.records.size()), represented(raw.records.size());
    for (std::size_t h = 0; h < raw.records.size(); ++h) {
        const auto& header = is.headers[h];
        const auto tag = raw.records[h].name();
        if (!source_valid(header.source, tag) || header.source.header != h ||
            header.source.record != 0 || header.tag != tag)
            return fail(S::Chronology, "import plan: header source mismatch", header.source);
        if (tag == "HIRSCH") {
            const auto decoded = formats::insel::decode_hirsch(raw.records[h].payload);
            if (!decoded)
                return fail(S::Source, "import plan: malformed HIRSCH", header.source);
            spawn_raw[h] = decoded.value();
            expected[h] = decoded.value().size();
        } else if (tag == "BERGWERK2" || tag == "OCCUR") {
            const auto decoded = formats::tagged::parse_records(raw.records[h].payload);
            if (!decoded)
                return fail(S::Source, "import plan: malformed resource header", header.source);
            tagged[h] = decoded.value();
            const auto item = tag == "BERGWERK2" ? "ENTRY" : "ENTRY2";
            for (const auto& r : tagged[h])
                expected[h] += r.name == item ? 1U : 0U;
        } else
            continue;
        if (header.records != expected[h])
            return fail(S::Chronology, "import plan: completed header count mismatch",
                        header.source);
    }
    std::vector<ImportEvent> events;
    std::set<std::pair<std::uint64_t, std::uint64_t>> spawn_keys, berg_keys, load_keys;
    for (std::size_t i = 0; i < is.spawns.size(); ++i) {
        const auto& p = is.spawns[i];
        if (!source_valid(p.source, "HIRSCH") ||
            p.source.record >= spawn_raw[p.source.header].size() ||
            !spawn_keys.insert(key(p.source)).second)
            return fail(S::Chronology, "import plan: spawn source missing or duplicate", p.source);
        if (p.raw != spawn_raw[p.source.header][p.source.record] || p.raw.x >= raw.width ||
            p.raw.y >= raw.height ||
            p.chunk != (std::uint64_t{p.raw.y} / 8) * tiles.chunks_x() + p.raw.x / 8 ||
            p.goods > std::numeric_limits<std::uint16_t>::max() ||
            p.draw_before > prepared.fill.prefix_consumed)
            return fail(S::Payload, "import plan: spawn payload/chunk mismatch", p.source);
        ++represented[p.source.header];
        events.push_back({p.source, p.chunk, i, 0, 0});
    }
    std::vector<std::vector<std::uint64_t>> berg_nodes(is.bergwerk_entries.size());
    std::vector<std::vector<std::uint64_t>> ore_nodes(is.occurrences.size());
    for (std::size_t i = 0; i < is.resource_list_nodes.size(); ++i) {
        const auto& n = is.resource_list_nodes[i];
        if (n.chunk >= tiles.chunk_count() || is.resource_list_chunks.empty())
            return fail(S::Payload, "import plan: resource chunk outside matrix");
        if (n.kind == 1 && n.entry < berg_nodes.size())
            berg_nodes[n.entry].push_back(i);
        else if (n.kind == 6 && n.entry < ore_nodes.size())
            ore_nodes[n.entry].push_back(i);
        else
            return fail(S::Payload, "import plan: unsupported category or corrupt resource index");
    }
    std::vector<std::uint64_t> header_nodes(raw.records.size()), header_created(raw.records.size());
    for (std::size_t i = 0; i < is.bergwerk_entries.size(); ++i) {
        const auto& e = is.bergwerk_entries[i];
        if (!source_valid(e.source, "BERGWERK2") ||
            e.source.record >= tagged[e.source.header].size() ||
            !berg_keys.insert(key(e.source)).second)
            return fail(S::Chronology, "import plan: BERGWERK source missing or duplicate",
                        e.source);
        const auto& r = tagged[e.source.header][e.source.record];
        if (r.name != "ENTRY" || r.payload.size() != 4)
            return fail(S::Payload, "import plan: wrong BERGWERK record", e.source);
        const auto word = formats::u32_le(r.payload, 0);
        if (e.local_x != (word & 0xfffU) || e.local_y != ((word >> 12) & 0xfffU) ||
            e.rotation != ((word >> 24) & 3U) ||
            e.world_x != static_cast<std::uint16_t>(e.local_x + raw.x) ||
            e.world_y != static_cast<std::uint16_t>(e.local_y + raw.y))
            return fail(S::Payload, "import plan: BERGWERK source geometry mismatch", e.source);
        const auto width = e.rotation & 1U ? 4U : 3U, height = e.rotation & 1U ? 3U : 4U;
        std::size_t ordinal = 0;
        for (auto y = e.world_y / 8 * 8; y < e.world_y + height; y += 8)
            for (auto x = e.world_x / 8 * 8; x < e.world_x + width; x += 8) {
                const auto local_x = std::int64_t{x} - raw.x, local_y = std::int64_t{y} - raw.y;
                if (local_x < 0 || local_y < 0 || local_x / 8 >= tiles.chunks_x() ||
                    local_y / 8 >= tiles.chunks_y() || ordinal >= berg_nodes[i].size())
                    return fail(S::Payload, "import plan: BERGWERK footprint mismatch", e.source);
                const auto index = berg_nodes[i][ordinal];
                const auto& n = is.resource_list_nodes[index];
                const auto chunk = static_cast<std::uint64_t>(local_y / 8) * tiles.chunks_x() +
                                   static_cast<std::uint64_t>(local_x / 8);
                const auto offsets = ((e.world_x - x) & 15U) | (((e.world_y - y) & 15U) << 4);
                if (n.chunk != chunk || n.offsets != offsets || n.rotation != e.rotation)
                    return fail(S::Payload, "import plan: resource insertion ordinal mismatch",
                                e.source);
                events.push_back({e.source, chunk, index, ordinal++, 1});
                ++header_nodes[e.source.header];
            }
        if (ordinal != berg_nodes[i].size())
            return fail(S::Payload, "import plan: duplicate BERGWERK footprint node", e.source);
        ++represented[e.source.header];
    }
    std::vector<std::uint64_t> created(is.occurrences.size());
    std::vector<std::vector<PrefixOccurrenceLoad>> loads(is.occurrences.size());
    std::optional<std::pair<std::uint64_t, std::uint64_t>> last_load;
    for (const auto& load : is.occurrence_loads) {
        if (last_load && *last_load >= key(load.source))
            return fail(S::Chronology, "import plan: occurrence load order mismatch", load.source);
        last_load = key(load.source);
        if (!source_valid(load.source, "OCCUR") ||
            load.source.record >= tagged[load.source.header].size() ||
            !load_keys.insert(key(load.source)).second || load.occurrence >= is.occurrences.size())
            return fail(S::Chronology, "import plan: corrupt occurrence load/source", load.source);
        const auto& r = tagged[load.source.header][load.source.record];
        const auto& ore = is.occurrences[load.occurrence];
        if (r.name != "ENTRY2" || r.payload.size() < 16 || key(load.source) < key(ore.source))
            return fail(S::Payload, "import plan: occurrence load before creation", load.source);
        if (load.created) {
            if (++created[load.occurrence] != 1 || !same_source(load.source, ore.source) ||
                ore.type != ((formats::u32_le(r.payload, 0) >> 14) & 0xfffU))
                return fail(S::Chronology, "import plan: duplicate occurrence creation",
                            load.source);
            ++header_created[load.source.header];
        }
        loads[load.occurrence].push_back(load);
        ++represented[load.source.header];
    }
    for (std::size_t i = 0; i < is.occurrences.size(); ++i) {
        const auto& ore = is.occurrences[i];
        if (!source_valid(ore.source, "OCCUR") || created[i] != 1 || ore_nodes[i].size() != 1 ||
            ore.tile_x < raw.x || ore.tile_y < raw.y ||
            std::int64_t{ore.tile_x} >= raw.x + raw.width ||
            std::int64_t{ore.tile_y} >= raw.y + raw.height)
            return fail(S::Payload, "import plan: occurrence node/geometry mismatch", ore.source);
        const auto x = static_cast<std::uint32_t>(ore.tile_x - raw.x);
        const auto y = static_cast<std::uint32_t>(ore.tile_y - raw.y);
        const auto index = ore_nodes[i][0];
        const auto& n = is.resource_list_nodes[index];
        const auto chunk = std::uint64_t{y / 8} * tiles.chunks_x() + x / 8;
        if (n.chunk != chunk || n.x_offset != x % 8 || n.y_offset != y % 8)
            return fail(S::Payload, "import plan: occurrence chunk/offset mismatch", ore.source);
        events.push_back({ore.source, chunk, index, 0, 6});
    }
    for (std::size_t h = 0; h < raw.records.size(); ++h) {
        const auto& header = is.headers[h];
        if ((header.tag == "HIRSCH" || header.tag == "BERGWERK2" || header.tag == "OCCUR") &&
            represented[h] != expected[h])
            return fail(S::Chronology, "import plan: omitted source record", header.source);
        if ((header.tag == "BERGWERK2" && header.bergwerk_nodes_added != header_nodes[h]) ||
            (header.tag == "OCCUR" && header.occurrences_added != header_created[h]))
            return fail(S::Chronology, "import plan: creation completion count mismatch",
                        header.source);
    }
    std::sort(events.begin(), events.end(), [](const auto& a, const auto& b) {
        return std::tie(a.source.header, a.source.record, a.ordinal) <
               std::tie(b.source.header, b.source.record, b.ordinal);
    });
    std::vector<std::optional<std::uint64_t>> spawn_heads(tiles.chunk_count()),
        resource_heads(tiles.chunk_count());
    std::vector<std::uint8_t> resource_flags(tiles.chunk_count());
    std::uint64_t spawn_index = 0, resource_index = 0;
    std::optional<std::tuple<std::uint64_t, std::uint64_t, std::uint64_t>> last;
    for (const auto& e : events) {
        const auto id = std::tuple{e.source.header, e.source.record, e.ordinal};
        if (last == id)
            return fail(S::Chronology, "import plan: duplicate token chronology", e.source);
        last = id;
        if (e.category == 0) {
            if (e.index != spawn_index++ ||
                is.spawns[e.index].next_in_chunk != spawn_heads[e.chunk])
                return fail(S::SpawnLinks, "import plan: spawn projection order/link mismatch",
                            e.source);
            spawn_heads[e.chunk] = e.index;
        } else {
            if (e.index != resource_index++ ||
                is.resource_list_nodes[e.index].next != resource_heads[e.chunk])
                return fail(S::ResourceLinks,
                            "import plan: resource projection order/link mismatch", e.source);
            resource_heads[e.chunk] = e.index;
            resource_flags[e.chunk] |= e.category == 1 ? std::uint8_t{2} : std::uint8_t{0x40};
        }
    }
    if (spawn_heads != is.chunk_heads)
        return fail(S::SpawnLinks, "import plan: spawn head/omitted node mismatch");
    for (std::size_t c = 0; c < resource_heads.size(); ++c) {
        if (is.resource_list_chunks.empty()) {
            if (resource_heads[c])
                return fail(S::ResourceLinks, "import plan: missing resource matrix");
        } else if (is.resource_list_chunks[c].head != resource_heads[c] ||
                   is.resource_list_chunks[c].resource_flags != resource_flags[c])
            return fail(S::ResourceLinks, "import plan: resource head/presence mismatch");
    }
    WildlifeImportedChunk out;
    out.supplied_flags = flags;
    out.prefix_draws = prepared.fill.prefix_consumed;
    out.source_token_count = events.size();
    out.chunk_x = cx;
    out.chunk_y = cy;
    out.scene_sha256 = prepared.source_sha256;
    const auto chunk = std::uint64_t{cy} * tiles.chunks_x() + cx;
    // Tokens are preparation-local chronological insertion ordinals, not native IDs.
    for (std::size_t i = events.size(); i > 0; --i) {
        const auto& e = events[i - 1];
        if (e.chunk != chunk)
            continue;
        const auto token = static_cast<std::uint64_t>(i - 1);
        WildlifeChunkRecord record;
        record.token = token;
        record.category = e.category;
        if (e.category == 0) {
            const auto& p = is.spawns[e.index];
            record.position = static_cast<std::uint8_t>((p.raw.x % 8) + 8 * (p.raw.y % 8));
            record.occupied = false;
            record.timestamp = p.timestamp;
            record.goods = static_cast<std::uint16_t>(p.goods);
            out.derived_presence |= std::uint16_t{1};
        } else if (e.category == 1) {
            const auto& n = is.resource_list_nodes[e.index];
            out.sidecars.emplace(token, WildlifeBergwerkSidecar{is.bergwerk_entries[n.entry], n});
            out.derived_presence |= std::uint16_t{2};
        } else {
            const auto& n = is.resource_list_nodes[e.index];
            out.sidecars.emplace(
                token, WildlifeOccurrenceSidecar{is.occurrences[n.entry], n, loads[n.entry]});
            out.derived_presence |= std::uint16_t{0x40};
        }
        out.snapshot.records.push_back(record);
        out.order.push_back({token, e.category, e.source, e.index, e.ordinal});
    }
    out.snapshot.flags = static_cast<std::uint16_t>(*flags.non_presence | out.derived_presence);
    return out;
}
formats::Result<WildlifeImportPlan, WildlifeImportPlanFailure> prepare_wildlife_import_plan(
    const WildlifeFirstCallResult& prepared, formats::Bytes scene, std::uint32_t cx,
    std::uint32_t cy, const WildlifeImportRuntime& runtime, GroundDescriptorProfile profile,
    std::span<const formats::figuren::Entry> entries, formats::Bytes game,
    const sim::TypeRegistry& registry, const WildlifeSpeciesSource& species,
    std::span<const std::uint32_t> maintenance_draws, std::span<const std::uint32_t> selector_draws,
    std::span<const WildlifeCaveFact> caves) {
    using S = WildlifeImportPlanStage;
    WildlifeImportPlanFailure work;
    work.supplied = runtime;
    work.draws.imported_prefix = prepared.fill.prefix_consumed;
    work.unused_maintenance.assign(maintenance_draws.begin(), maintenance_draws.end());
    const auto fail = [&](S stage, std::string reason) {
        work.stage = stage;
        work.reason = std::move(reason);
        return work;
    };
    if (runtime.provenance.empty())
        return fail(S::Runtime, "import plan: runtime context provenance required");
    auto imported = reconstruct_wildlife_import(prepared, scene, cx, cy, runtime.flags);
    if (!imported) {
        work.import_error = imported.error();
        return fail(S::Import, imported.error().reason);
    }
    work.imported = std::move(imported.value());
    for (const auto token : runtime.fresh_tokens)
        if (token < work.imported->source_token_count)
            return fail(S::Runtime, "import plan: fresh token overlaps source namespace " +
                                        std::to_string(token));
    auto habitat =
        bind_wildlife_habitat_chunk(prepared, scene, cx, cy, profile, entries, game, registry);
    if (!habitat)
        return fail(S::Habitat, habitat.error());
    work.habitat = std::move(habitat.value());
    auto maintained = prepare_wildlife_habitat_maintenance(*work.habitat, work.imported->snapshot,
                                                           runtime.time, runtime.allocation,
                                                           runtime.fresh_tokens, maintenance_draws);
    work.draws.maintenance = maintained ? maintained.value().completed.consumed
                                        : maintained.error().diagnostic_only.consumed;
    if (work.draws.maintenance > maintenance_draws.size())
        return fail(S::Maintenance, "import plan: invalid maintenance draw accounting");
    const auto unused = maintenance_draws.subspan(work.draws.maintenance);
    work.unused_maintenance.assign(unused.begin(), unused.end());
    if (!maintained) {
        work.maintenance_error = maintained.error();
        return fail(S::Maintenance, maintained.error().reason);
    }
    work.maintenance = std::move(maintained.value());
    auto final = bind_wildlife_final_chunk(prepared, cx, cy, profile, entries, game, registry);
    if (!final)
        return fail(S::FinalFacts, final.error());
    work.final_facts = std::move(final.value());
    work.final_facts->facts.caves.assign(caves.begin(), caves.end());
    if (species.climate != prepared.fill.island.climate || species.game.size() != game.size() ||
        (species.game.data() != game.data() &&
         !std::equal(species.game.begin(), species.game.end(), game.begin())))
        return fail(S::Materialization, "import plan: species climate/Game source mismatch");
    auto selected = prepare_wildlife_materialization(
        *work.maintenance, runtime.time, work.final_facts->facts, species, selector_draws);
    work.draws.selector_only = selected ? selected.value().completed.selector_consumed
                                        : selected.error().diagnostic_only.selector_consumed;
    if (!selected) {
        work.materialization_error = std::move(selected.error());
        return fail(S::Materialization, work.materialization_error->reason);
    }
    return WildlifeImportPlan{runtime,
                              std::move(*work.imported),
                              std::move(*work.habitat),
                              std::move(*work.final_facts),
                              std::move(*work.maintenance),
                              std::move(selected.value()),
                              work.draws,
                              std::move(work.unused_maintenance)};
}
} // namespace anno::sim_load
