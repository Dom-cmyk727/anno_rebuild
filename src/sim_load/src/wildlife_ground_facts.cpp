#include <algorithm>
#include <array>
#include <map>
#include <set>

#include <anno/sim_load/wildlife_ground_facts.hpp>

namespace anno::sim_load {
formats::Result<std::vector<GroundObjectFact>, std::string>
bind_wildlife_ground_facts(GroundDescriptorProfile profile, std::span<const std::uint32_t> words,
                           std::span<const formats::figuren::Entry> entries,
                           const sim::TypeRegistry& types) {
    if (profile != GroundDescriptorProfile::GogAnlagen)
        return std::string("ground facts: unsupported descriptor profile");
    const std::set<std::uint32_t> requested(words.begin(), words.end());
    std::map<std::uint32_t, const formats::figuren::Entry*> indexed;
    for (const auto& entry : entries)
        if (requested.contains(entry.id) && !indexed.emplace(entry.id, &entry).second)
            return "ground facts object " + std::to_string(entry.id) + ": duplicate source entry";
    std::vector<GroundObjectFact> out;
    out.reserve(requested.size());
    for (const auto word : requested) {
        const auto prefix = "ground facts object " + std::to_string(word) + ": ";
        const auto it = indexed.find(word);
        const auto* type = types.find(sim::TypeId{word});
        // Fresh catalogue lookup Game 0x100037e0 and zero-init 0x10003834.
        // Missing nonzero metadata is NOT turned into an invented null fact.
        if (word == 0 && it == indexed.end() && !type) {
            out.push_back({word, GroundProperty::NullDescriptor, 0, 0});
            continue;
        }
        if (it == indexed.end())
            return prefix + "missing source entry";
        if (!type || type->id.v != word)
            return prefix + "missing registry type";
        const auto& entry = *it->second;
        const auto count = [&](std::string_view name) {
            return std::count_if(entry.records.begin(), entry.records.end(),
                                 [name](const auto& r) { return r.name() == name; });
        };
        if (count("HEAD") != 1 || count("PRODTYPE") != 1)
            return prefix + "unique HEAD and PRODTYPE required";
        const auto head = entry.record("HEAD"), kind_record = entry.record("PRODTYPE");
        // Game 0x10009348 zeroes 56 bytes; smaller PRESENT records zero-extend.
        // The original unbounded larger copy is not a supported safe profile.
        std::array<std::uint8_t, 56> normalized{};
        if (!head || head->size() > normalized.size())
            return prefix + "unsupported HEAD length";
        std::copy(head->begin(), head->end(), normalized.begin());
        if (!kind_record)
            return prefix + "missing PRODTYPE";
        const auto end = std::find(kind_record->begin(), kind_record->end(), std::uint8_t{0});
        if (end == kind_record->end() || end == kind_record->begin())
            return prefix + "unterminated or empty PRODTYPE";
        const auto kind = sim::prodtype_by_name(std::string(kind_record->begin(), end));
        if (!kind)
            return prefix + "unsupported PRODTYPE";
        if (type->prodtype != *kind)
            return prefix + "raw kind/registry mismatch";
        if (type->head_raw.size() != normalized.size() ||
            !std::equal(normalized.begin(), normalized.end(), type->head_raw.begin()) ||
            type->width != normalized[0] || type->height != normalized[1] ||
            type->flags_word != formats::u16_le(normalized, 0x1c))
            return prefix + "HEAD/registry mismatch";
        // Actual derived +0x48 forwards 0x2e to common Game 0x1000a979:
        // byte +0x77 from HEAD[0x19], output DWORD, success 1 EVEN FOR ZERO.
        // Out-of-group facts are pure data too; numerical leaf short-circuits.
        out.push_back({word, GroundProperty::Value, *kind, normalized[0x19]});
    }
    return out;
}
} // namespace anno::sim_load
