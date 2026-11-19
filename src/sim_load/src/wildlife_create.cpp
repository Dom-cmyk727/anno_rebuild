#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <map>
#include <set>

#include <anno/formats/annogame.hpp>
#include <anno/formats/sha256.hpp>
#include <anno/sim_load/wildlife_create.hpp>

namespace anno::sim_load {
namespace {
formats::Result<std::vector<WildlifeSpeciesFact>, std::string>
read_species_facts(WildlifeDescriptorProfile profile, std::span<const sim::TypeId> requested,
                   std::span<const formats::figuren::Entry> entries, formats::Bytes game,
                   bool new_only) {
    if (profile != WildlifeDescriptorProfile::Gog)
        return std::string("creation descriptor: unsupported profile");
    if (requested.empty())
        return std::string("creation descriptor: no requested types");
    const auto mapping = formats::annogame::read_wildlife_names(game);
    if (!mapping)
        return mapping.error();
    std::set<std::uint32_t> ids;
    for (auto id : requested)
        ids.insert(id.v);
    std::map<std::uint32_t, const formats::figuren::Entry*> indexed;
    for (const auto& entry : entries)
        if (ids.contains(entry.id) && !indexed.emplace(entry.id, &entry).second)
            return std::string("creation descriptor: duplicate entry id");
    std::vector<WildlifeSpeciesFact> out;
    for (auto id : ids) {
        const auto found = indexed.find(id);
        if (found == indexed.end())
            return std::string("creation descriptor: missing entry");
        const auto& entry = *found->second;
        if (entry.flag != 0)
            return std::string("creation descriptor: unsupported entry flag");
        const auto count = [&](std::string_view name) {
            return std::count_if(entry.records.begin(), entry.records.end(),
                                 [name](const auto& r) { return r.name() == name; });
        };
        if (count("BEWOHNTYPE") != 1 || count("LOGIC") > 1 || count("WARE") > 1)
            return std::string("creation descriptor: unique needed records required");
        const auto raw_kind = entry.record("BEWOHNTYPE");
        // Game lookup input ends before property-buffer index at stack +0x84.
        const auto terminated = [](formats::Bytes bytes, std::size_t maximum) {
            return bytes.size() <= maximum && !bytes.empty() &&
                   std::find(bytes.begin(), bytes.end(), std::uint8_t{0}) != bytes.end();
        };
        if (!raw_kind || !terminated(*raw_kind, 84))
            return std::string("creation descriptor: unsafe BEWOHNTYPE name");
        const auto text = [](formats::Bytes bytes) {
            return std::string(bytes.begin(),
                               std::find(bytes.begin(), bytes.end(), std::uint8_t{0}));
        };
        const auto name = text(*raw_kind);
        const auto kind =
            std::find(mapping.value().kinds.begin(), mapping.value().kinds.end(), name);
        if (kind == mapping.value().kinds.end())
            return std::string("creation descriptor: unknown BEWOHNTYPE");
        const auto number = static_cast<std::uint32_t>(kind - mapping.value().kinds.begin());
        if (new_only && number != 49 && number != 51)
            return std::string("creation descriptor: unsupported animal kind");
        const auto logic = entry.record("LOGIC"), ware = entry.record("WARE");
        std::uint16_t hp = 5; // Game constructor 0x10011249; absent is not present zero.
        if (logic) {
            std::array<std::uint8_t, 44> buffer{}; // Game 0x100101b9, fixed file-reader buffer.
            if (logic->size() > buffer.size())
                return std::string("creation descriptor: unsafe LOGIC length");
            std::copy(logic->begin(), logic->end(), buffer.begin());
            hp = formats::u16_le(buffer, 0x1e);
        }
        sim::GoodsId yield = 0; // Game ctor 0x10011259, absent WARE preserves zero.
        bool ware_matched = false;
        if (ware) {
            if (!terminated(*ware, 256)) // Original stack input buffer, not a content limit.
                return std::string("creation descriptor: unsafe WARE name");
            const auto goods = text(*ware);
            const auto matched =
                std::find(mapping.value().goods.begin(), mapping.value().goods.end(), goods);
            if (matched == mapping.value().goods.end()) {
                if (new_only || number == 49 || number == 51)
                    return std::string("creation descriptor: unknown WARE");
                // Fresh non-New facts: no match returns without +0x100 store
                // (Game 0x1001042d–0x10010445), preserving constructor zero.
            } else {
                ware_matched = true;
                yield = static_cast<sim::GoodsId>(matched - mapping.value().goods.begin());
            }
        }
        // Identity of used parsed fields, not authenticity or a whole file hash.
        formats::ByteBuffer identity;
        formats::put_u32_le(identity, id);
        formats::put_u32_le(identity, entry.flag);
        for (const auto& record : entry.records)
            if (record.name() == "BEWOHNTYPE" || record.name() == "LOGIC" ||
                record.name() == "WARE") {
                identity.insert(identity.end(), record.name_field.begin(), record.name_field.end());
                formats::put_u32_le(identity, static_cast<std::uint32_t>(record.payload.size()));
                identity.insert(identity.end(), record.payload.begin(), record.payload.end());
            }
        out.push_back({sim::TypeId{id}, number, yield, hp, mapping.value().source_sha256,
                       formats::sha256_hex(identity), logic.has_value(), ware.has_value(),
                       ware_matched});
    }
    return out;
}
} // namespace
formats::Result<std::vector<WildlifeSpeciesFact>, std::string>
bind_wildlife_species_facts(WildlifeDescriptorProfile profile,
                            std::span<const sim::TypeId> requested,
                            std::span<const formats::figuren::Entry> entries, formats::Bytes game) {
    return read_species_facts(profile, requested, entries, game, false);
}
formats::Result<std::vector<WildlifeCreationDescriptor>, std::string>
bind_wildlife_creation_descriptors(WildlifeDescriptorProfile profile,
                                   std::span<const sim::TypeId> requested,
                                   std::span<const formats::figuren::Entry> entries,
                                   formats::Bytes game) {
    const auto facts = read_species_facts(profile, requested, entries, game, true);
    if (!facts)
        return facts.error();
    std::vector<WildlifeCreationDescriptor> out;
    for (const auto& fact : facts.value()) {
        if (fact.kind != 49 && fact.kind != 51)
            return std::string("creation descriptor: unsupported animal kind");
        out.push_back(WildlifeCreationDescriptor(fact.id, fact.kind, fact.hp, fact.yield,
                                                 fact.image_sha256, fact.source_sha256,
                                                 fact.logic_present, fact.ware_present));
    }
    return out;
}
namespace {
formats::Result<std::uint32_t, std::string> truncate_coordinate(std::uint32_t bits) {
    const auto exponent = (bits >> 23) & 255U;
    if (exponent == 255 || ((bits >> 31) != 0 && (bits & 0x7fffffffU) != 0))
        return std::string("creation effects: unsupported coordinate");
    if (exponent < 127)
        return std::uint32_t{0}; // Includes subnormals and either signed zero.
    const auto shift = exponent - 127;
    if (shift > 30)
        return std::string("creation effects: coordinate outside integer projection");
    const auto significand = 0x800000U | (bits & 0x7fffffU);
    return shift >= 23 ? significand << (shift - 23) : significand >> (23 - shift);
}
} // namespace
formats::Result<WildlifeCreationEffects, std::string>
project_wildlife_creation(const WildlifeCreationDescriptor& descriptor,
                          const WildlifeCreationInputs& input) {
    if (input.manager != WildlifeManagerOutcome::Success &&
        input.manager != WildlifeManagerOutcome::Null)
        return std::string("creation effects: unsupported manager outcome");
    const auto& g = input.geometry;
    if (g.place != WildlifeCreationPlace::Ordinary &&
        g.place != WildlifeCreationPlace::ResolvedCaveEntrance)
        return std::string("creation effects: unsupported geometry profile");
    for (auto bits : input.position_words)
        if (((bits >> 23) & 255U) == 255)
            return std::string("creation effects: nonfinite position");
    const auto x = truncate_coordinate(input.position_words[0]);
    const auto z = truncate_coordinate(input.position_words[2]);
    if (!x)
        return x.error();
    if (!z)
        return z.error();
    const auto inside = [&](std::uint32_t tx, std::uint32_t tz) {
        return g.area.w > 0 && g.area.h > 0 && std::int64_t{tx} >= g.area.x0 &&
               std::int64_t{tz} >= g.area.y0 &&
               std::int64_t{tx} < std::int64_t{g.area.x0} + g.area.w &&
               std::int64_t{tz} < std::int64_t{g.area.y0} + g.area.h;
    };
    if (!inside(x.value(), z.value()) || !inside(g.home_x, g.home_z) ||
        g.home_x > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
        g.home_z > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
        return std::string("creation effects: outside supplied geometry");
    if (g.place == WildlifeCreationPlace::Ordinary &&
        (g.home_x != x.value() || g.home_z != z.value()))
        return std::string("creation effects: inconsistent ordinary home");
    if (g.place == WildlifeCreationPlace::ResolvedCaveEntrance && g.entrance_count == 0)
        return std::string("creation effects: OQ-574 cave without entrance");
    const auto exact_binary32_integer = [](std::uint32_t value) {
        const auto width = std::bit_width(value);
        return width <= 24 || (value & ((1U << (width - 24)) - 1U)) == 0;
    };
    // Cave helper FILD ->FSTP at 0x10092f84. Refuse unproved rounding,
    // rather than choosing an FP mode or imposing an engine coordinate cap.
    if (g.place == WildlifeCreationPlace::ResolvedCaveEntrance &&
        (!exact_binary32_integer(g.home_x) || !exact_binary32_integer(g.home_z)))
        return std::string("creation effects: cave target needs unproved rounding");
    if (input.assigned_id > std::numeric_limits<std::uint16_t>::max())
        return std::string("creation effects: id outside original projection");
    WildlifeCreationEffects out;
    out.spawn = input.prior;
    out.helper_consumed = input.helper_consumed;
    using S = WildlifeCreationStage;
    if (input.manager == WildlifeManagerOutcome::Null) {
        out.stages = {S::ManagerNull};
        return out; // No manager rollback is claimed by this projection.
    }
    WildlifeCreatedProjection animal;
    animal.type = descriptor.id();
    animal.engine_id = input.assigned_id;
    animal.original_id = static_cast<std::uint16_t>(input.assigned_id);
    animal.full_creation_time = input.creation_time;
    animal.original_times.fill(static_cast<std::uint16_t>(input.creation_time));
    animal.hp = descriptor.hp();
    animal.quantity = 0x200;
    const auto id = descriptor.id().v;
    if (id == 128 || id == 129 || id == 529 || id == 1206)
        animal.quantity += 0x600; // Welt 0x10052a6c–0x10052a93; not a species cap.
    animal.carried_goods = 0x45;
    animal.yield = descriptor.yield();
    animal.position_words = input.position_words;
    animal.second_position_words = input.position_words;
    animal.home = {g.home_x, g.home_z};
    animal.original_home = {static_cast<std::uint16_t>(g.home_x),
                            static_cast<std::uint16_t>(g.home_z)};
    animal.original_current_tile = animal.original_home;
    animal.descriptor_source_sha256 = descriptor.source_sha256();
    out.spawn.id = animal.original_id;
    out.spawn.timestamp = static_cast<std::uint8_t>(input.sweep_time / 1000U + 60U);
    out.spawn.packed = static_cast<std::uint8_t>(out.spawn.packed | 0x40U);
    out.spawn.goods = static_cast<std::uint16_t>(descriptor.yield());
    out.stages = {S::ConstructorQuantity,
                  S::AttachType,
                  S::AttachWorld,
                  S::SuppliedRegistration,
                  S::SetTime,
                  S::CopyPosition,
                  S::SuppliedSpatialRegistration,
                  S::SetHome,
                  S::SetCargoGoods,
                  S::AttachIsland,
                  S::QueryYield,
                  S::SaveId,
                  S::SaveTimestamp,
                  S::MarkOccupied,
                  S::SaveYield};
    out.animal = std::move(animal);
    return out;
}
} // namespace anno::sim_load
