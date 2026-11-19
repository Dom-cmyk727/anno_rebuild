#include <algorithm>
#include <array>
#include <limits>
#include <utility>

#include <anno/formats/sha256.hpp>
#include <anno/sim/fisher.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim_load/wildlife_postload_fill.hpp>

namespace anno::sim_load {
namespace {
std::string failure(const char* reason) {
    return std::string("wildlife fill: ") + reason;
}
} // namespace

formats::Result<PrecomputedWildlifeFill, std::string>
prepare_wildlife_postload_fill(const PreparedPrefix& prefix, std::uint64_t ordinal,
                               formats::Bytes scene,
                               std::span<const formats::figuren::Entry> entries,
                               const sim::TypeRegistry& types, WildlifeFillRoles roles) {
    if (!prefix.complete_local_records || prefix.boundary || ordinal >= prefix.islands.size())
        return failure("complete local headers required");
    const auto& source = prefix.islands[ordinal];
    if (!source.complete_local_records)
        return failure("complete local headers required");
    if (prefix.source_sha256 != formats::sha256_hex(scene))
        return failure("source scene identity mismatch");
    const auto parsed = formats::insel::islands(scene);
    if (!parsed)
        return failure("source scene does not parse");
    if (parsed.value().size() != prefix.islands.size())
        return failure("source island sequence mismatch");
    const auto& raw = parsed.value()[ordinal];
    const auto& tiles = source.resource.tiles;
    const auto w = tiles.width(), h = tiles.height();
    if (raw.version != formats::insel::Version::Insel5 || w == 0 || h == 0 ||
        raw.number != source.slot || raw.x != tiles.x0() || raw.y != tiles.y0() || raw.width != w ||
        raw.height != h || tiles.chunk_edge() != prefix.supplied.chunk_edge)
        return failure("source island geometry mismatch");
    const auto count = std::uint64_t{w} * h;
    if (!source.terrain || source.terrain->height.size() != count ||
        source.terrain->gfx.size() != count || source.terrain->fertility.size() != count ||
        source.resource.variation.size() != count)
        return failure("terrain/ground plane shape mismatch");
    if (!sim::terrain_matches(*source.terrain))
        return failure("terrain digest mismatch");
    if (source.headers.size() != raw.records.size())
        return failure("header sequence mismatch");
    std::optional<formats::Bytes> last_ground;
    for (std::uint64_t i = 0; i < raw.records.size(); ++i) {
        const auto& header = source.headers[i];
        const auto& record = raw.records[i];
        if (header.source.island != ordinal || header.source.slot != raw.number ||
            header.source.header != i || header.source.header_offset != record.offset ||
            header.tag != record.name())
            return failure("header provenance mismatch");
        if (header.tag == "FRUCHTBAR") {
            if (!header.fertility_updates || record.payload.size() != count)
                return failure("completed FRUCHTBAR required");
            last_ground = record.payload;
        } else if (header.fertility_updates) {
            return failure("FRUCHTBAR completion on another header");
        }
    }
    if (!last_ground)
        return failure("completed FRUCHTBAR required");
    if (!std::equal(last_ground->begin(), last_ground->end(), source.terrain->fertility.begin()))
        return failure("last FRUCHTBAR payload mismatch");
    for (std::uint64_t i = 0; i < count; ++i)
        if ((source.resource.variation[i] & 15U) != ((*last_ground)[i] & 15U))
            return failure("effective ground low nibble mismatch");
    // Reuse strict format accessors rather than a second terrain loader. Reject
    // incomplete/overlapping source blocks; do not invent missing source samples.
    const auto heights = raw.heights();
    const auto graphics = raw.gfx_ids();
    if (!heights || !graphics)
        return failure("source terrain blocks incomplete or overlapping");
    if (source.terrain->height != heights.value() || source.terrain->gfx != graphics.value())
        return failure("source terrain provenance mismatch");
    if (roles.grass.v == 0 || roles.fish.v == 0 || roles.grass == roles.fish)
        return failure("distinct nonzero role ids required");
    const auto role =
        [&](sim::TypeId id) -> formats::Result<PrecomputedWildlifeFill::Role, std::string> {
        const formats::figuren::Entry* entry = nullptr;
        for (const auto& candidate : entries)
            if (candidate.id == id.v) {
                if (entry)
                    return failure("duplicate role entry");
                entry = &candidate;
            }
        if (!entry)
            return failure("missing role entry");
        const auto* type = types.find(id);
        if (!type || type->id != id)
            return failure("missing registry role");
        if (std::count_if(entry->records.begin(), entry->records.end(),
                          [](const auto& record) { return record.name() == "HEAD"; }) != 1)
            return failure("unique role HEAD required");
        const auto head = entry->record("HEAD");
        // 56 is the original HEAD buffer width (Game 0x10009351), not a type cap.
        std::array<std::uint8_t, 56> normalized{};
        if (!head || head->size() < 0x1e || head->size() > normalized.size())
            return failure("unsupported role HEAD length");
        std::copy(head->begin(), head->end(), normalized.begin());
        if (type->head_raw.size() != normalized.size() ||
            !std::equal(normalized.begin(), normalized.end(), type->head_raw.begin()) ||
            type->flags_word != formats::u16_le(*head, 0x1c) || type->width != normalized[0] ||
            type->height != normalized[1])
            return failure("role HEAD/registry mismatch");
        if ((type->flags_word & 3U) != 1U)
            return failure("unsupported role height profile");
        const auto code = sim::walk_code(*type, 0); // Never walk_class or literal 2.
        if (code > std::numeric_limits<std::uint8_t>::max())
            return failure("unsupported role walk byte");
        return PrecomputedWildlifeFill::Role{id, static_cast<std::uint8_t>(code)};
    };
    const auto grass = role(roles.grass);
    if (!grass)
        return grass.error();
    const auto fish = role(roles.fish);
    if (!fish)
        return fish.error();
    return PrecomputedWildlifeFill(std::make_unique<PreparedIsland>(source), grass.value(),
                                   fish.value(), prefix.consumed);
}

formats::Result<WildlifeFillResult, std::string>
apply_wildlife_postload_fill(PrecomputedWildlifeFill capability, sim::TileRect area) {
    if (!capability.source_)
        return failure("capability already consumed");
    WildlifeFillResult result;
    result.island = std::move(*capability.source_);
    capability.source_.reset();
    result.prefix_consumed = capability.prefix_consumed_;
    auto& tiles = result.island.resource.tiles;
    const auto w = tiles.width(), h = tiles.height();
    const auto& heights = result.island.terrain->height;
    const auto x0 = std::max(std::int64_t{tiles.x0()}, std::int64_t{area.x0});
    const auto y0 = std::max(std::int64_t{tiles.y0()}, std::int64_t{area.y0});
    const auto x1 = std::min(std::int64_t{tiles.x0()} + w, std::int64_t{area.x0} + area.w);
    const auto y1 = std::min(std::int64_t{tiles.y0()} + h, std::int64_t{area.y0} + area.h);
    for (auto y = y0; y < y1; ++y)
        for (auto x = x0; x < x1; ++x) {
            const auto lx = static_cast<std::uint32_t>(x - tiles.x0());
            const auto ly = static_cast<std::uint32_t>(y - tiles.y0());
            const auto i = std::uint64_t{ly} * w + lx;
            if (tiles.object(i) != 0) // WHOLE word, Welt 0x10092976; no id mask.
                continue;
            const auto mean = static_cast<std::uint8_t>(
                (std::uint32_t{*sim::fisher_corner(heights, w, h, lx, ly)} +
                 *sim::fisher_corner(heights, w, h, lx + 1, ly) +
                 *sim::fisher_corner(heights, w, h, lx, ly + 1) +
                 *sim::fisher_corner(heights, w, h, lx + 1, ly + 1)) /
                4U);
            if (mean < 16) { // Bound original low threshold; postload §2/§10.
                tiles.set_object(i, capability.fish_.id.v);
                tiles.set_item_height(i, mean); // Preserve code/mask/high aux (92acb).
                ++result.fish;
            } else if (mean >= 32 && (result.island.resource.variation[i] & 15U) * 10U > 50U) {
                tiles.set_object(i, capability.grass_.id.v);
                // Original fe0000ff mask; code/mask are separate engine columns (§11).
                tiles.set_aux(i, (tiles.aux(i) & 0xfe000000U) | mean);
                tiles.set_walk_code(i, capability.grass_.code);
                tiles.set_walk_mask(i, 0);
                ++result.grass;
            } else {
                continue;
            }
            tiles.set_foot_dx(i, 0);
            tiles.set_foot_dy(i, 0);
            tiles.set_foot_rotation(i, 0);
            tiles.set_phase(i, 0); // Original byte 3 &=0x80: separate anchor retained.
        }
    return result;
}
} // namespace anno::sim_load
