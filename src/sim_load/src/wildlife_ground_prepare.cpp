#include <algorithm>
#include <limits>

#include <anno/sim_load/wildlife_ground_prepare.hpp>

namespace anno::sim_load {
namespace {
std::string failure(const char* reason) {
    return std::string("wildlife ground: ") + reason;
}
} // namespace

formats::Result<std::vector<std::uint8_t>, std::string>
prepare_wildlife_ground(const WildlifeGroundInput& in) {
    const auto w = in.island.w, h = in.island.h;
    if (w == 0 || h == 0)
        return failure("positive geometry required");
    // TileRect uses u32 extents and signed tile coordinates. Widen before adding;
    // u32*u32 fits u64, and the last tile (not the exclusive end) must fit TileXY.
    if (std::int64_t{in.island.x0} + w - 1 > std::numeric_limits<std::int32_t>::max() ||
        std::int64_t{in.island.y0} + h - 1 > std::numeric_limits<std::int32_t>::max())
        return failure("geometry exceeds tile coordinates");
    const auto count = std::uint64_t{w} * h;
    if (count > std::vector<std::uint8_t>{}.max_size())
        return failure("plane size not representable");
    if (in.gfx.size() != count || in.objects.size() != count || in.ground.size() != count)
        return failure("plane length mismatch");
    // Original fixed 5x5 kernel (Welt 0x10084670–0x10084698), not a content cap.
    if (in.kernel.size() != 25)
        return failure("kernel needs 25 bytes");
    for (const auto graphic : in.gfx)
        if ((graphic >> 2U) >= in.lookup.size())
            return failure("lookup coverage missing");
    for (const auto& fact : in.facts) {
        switch (fact.property) {
        case GroundProperty::NullDescriptor:
            if (fact.kind != 0 || fact.value != 0)
                return failure("malformed null descriptor");
            break;
        case GroundProperty::Absent:
        case GroundProperty::Unsuccessful:
            if (fact.value != 0)
                return failure("malformed absent/unsuccessful property");
            break;
        case GroundProperty::Value:
            break;
        default:
            return failure("unknown property state");
        }
    }
    std::vector<GroundObjectFact> facts(in.facts.begin(), in.facts.end());
    std::sort(facts.begin(), facts.end(),
              [](const auto& a, const auto& b) { return a.object < b.object; });
    for (std::size_t i = 1; i < facts.size(); ++i)
        if (facts[i - 1].object == facts[i].object)
            return failure("duplicate object fact");
    const auto find = [&](std::uint32_t word) {
        return std::lower_bound(facts.begin(), facts.end(), word,
                                [](const auto& fact, auto object) { return fact.object < object; });
    };
    for (const auto word : in.objects) {
        const auto fact = find(word);
        if (fact == facts.end() || fact->object != word)
            return failure("missing object fact");
    }
    // All supplied evidence validated before numerical work or output publication.
    std::vector<std::uint8_t> scratch(static_cast<std::size_t>(count));
    for (std::size_t i = 0; i < scratch.size(); ++i) {
        std::uint32_t p = in.lookup[in.gfx[i] >> 2U];
        const auto& fact = *find(in.objects[i]);
        // Welt 0x10084541–0x1008458b: whole-word lookup, kind group, byte property.
        if (fact.property == GroundProperty::Value && fact.kind >= 225 && fact.kind <= 256 &&
            fact.value != 0) {
            const std::uint32_t v = fact.value;
            p = p < v ? (p + 3U * v) / 4U : (v + 3U * p) / 4U;
        }
        scratch[i] = static_cast<std::uint8_t>((p + 5U) / 10U);
    }
    std::vector<std::uint8_t> output(in.ground.begin(), in.ground.end());
    for (std::uint64_t y = 0; y < h; ++y)
        for (std::uint64_t x = 0; x < w; ++x) {
            const auto i = static_cast<std::size_t>(y * w + x);
            if (scratch[i] == 0) // Welt 0x10084661: retain old LOW nibble too.
                continue;
            std::uint32_t sum = 0;
            for (std::int32_t dy = -2; dy <= 2; ++dy)
                for (std::int32_t dx = -2; dx <= 2; ++dx) {
                    const auto nx = static_cast<std::int64_t>(x) + dx;
                    const auto ny = static_cast<std::int64_t>(y) + dy;
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                        continue;
                    const auto index =
                        static_cast<std::size_t>(std::uint64_t(ny) * w + std::uint64_t(nx));
                    const auto k = static_cast<std::size_t>((dy + 2) * 5 + dx + 2);
                    // Per-term division before accumulation, Welt 0x10084721–0x10084741.
                    // Max sum 25*(26*255/12) fits u32; never byte-width product.
                    sum += (std::uint32_t{scratch[index]} * in.kernel[k]) / 12U;
                }
            const auto value = std::min(sum, 10U);
            output[i] = static_cast<std::uint8_t>((in.ground[i] & 0xf0U) | value);
        }
    return output;
}
} // namespace anno::sim_load
