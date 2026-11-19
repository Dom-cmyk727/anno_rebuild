#include <algorithm>
#include <array>
#include <limits>
#include <optional>

#include <anno/formats/sha256.hpp>
#include <anno/formats/text.hpp>
#include <anno/sim_load/wildlife_growth_load.hpp>

namespace anno::sim_load {
namespace {
using formats::Bytes;
// Original scratch capacity, Game 0x10006b4b–0x10006b6b: 0x100 DWORDs,
// not an engine content limit. Larger unsafe source profiles explicitly refuse.
constexpr std::size_t kStagingWords = 256;
// 16-bit GFX packing minus two rotation bits, Game 0x10006d19–0x10006db1.
constexpr std::size_t kPackedBytes = 1U << 14U;
bool space(std::uint8_t c) {
    return c == ' ' || c == '\t';
}
bool digit(std::uint8_t c) {
    return c >= '0' && c <= '9';
}
struct NumberRow {
    std::vector<std::int32_t> values;
    bool stopped = false;
};
formats::Result<NumberRow, std::string> numbers(Bytes s, bool allow_stop = false) {
    NumberRow out;
    std::size_t i = 0;
    while (true) {
        while (i < s.size() && space(s[i]))
            ++i;
        bool negative = false;
        if (i < s.size() && (s[i] == '-' || s[i] == '+')) {
            negative = s[i] == '-';
            ++i;
        }
        if (i == s.size() || !digit(s[i]))
            return std::string("canonical decimal integer required");
        if (s[i] == '0' && i + 1 < s.size() && digit(s[i + 1]))
            return std::string("unsupported numeric syntax");
        const std::uint64_t limit = negative ? 2147483648ULL : 2147483647ULL;
        std::uint64_t n = 0;
        while (i < s.size() && digit(s[i])) {
            const auto d = static_cast<std::uint64_t>(s[i] - std::uint8_t{'0'});
            if (n > (limit - d) / 10U)
                return std::string("signed integer out of range");
            n = n * 10U + d;
            ++i;
        }
        const auto signed_n =
            negative ? -static_cast<std::int64_t>(n) : static_cast<std::int64_t>(n);
        out.values.push_back(static_cast<std::int32_t>(signed_n));
        if (i < s.size() && !space(s[i]) && s[i] != ',')
            return std::string("unsupported numeric syntax");
        while (i < s.size() && space(s[i]))
            ++i;
        if (i == s.size())
            return out;
        if (s[i] == ',') {
            ++i;
            continue;
        }
        if (!allow_stop || !numbers(s.subspan(i)))
            return std::string("unsupported numeric tail");
        out.stopped = true; // Game 0x10016381–0x100163ad; tail discarded by 16210.
        return out;
    }
}
bool climate_tokens(Bytes s) {
    std::uint32_t tokens = 0;
    std::size_t i = 0;
    while (true) {
        while (i < s.size() && space(s[i]))
            ++i;
        const auto start = i;
        while (i < s.size() && ((s[i] >= 'A' && s[i] <= 'Z') || s[i] == '_'))
            ++i;
        if (i == start || ++tokens > 2)
            return false;
        while (i < s.size() && space(s[i]))
            ++i;
        if (i == s.size())
            return true;
        if (s[i++] != ',')
            return false;
    }
}
} // namespace

formats::Result<GrowthLookup, std::string> load_wildlife_growth(Bytes source) {
    if (source.empty() || source.back() != '\n' ||
        std::find(source.begin(), source.end(), std::uint8_t{0}) != source.end())
        return std::string(
            "growth lookup: nonempty newline-terminated source without NUL required");
    GrowthLookup result;
    std::array<std::int32_t, kStagingWords> stage{};
    std::uint32_t w = 6, h = 6, row = 0, bridge_row = 0;
    std::optional<std::int32_t> id;
    std::uint64_t id_line = 0;
    bool active = false, ended = false, complete = false, size_seen = false;
    const auto incomplete = [&] { return id && (!complete || bridge_row != 0); };
    for (const auto& line : formats::text::lines(source)) {
        const auto fail = [&](const std::string& why) {
            return "growth lookup line " + std::to_string(line.number) + ": " + why;
        };
        const auto semi = std::find(line.text.begin(), line.text.end(), std::uint8_t{';'});
        if (std::find(semi, line.text.end(), std::uint8_t{'['}) != line.text.end())
            return fail("bracket in comment unsupported");
        for (auto it = line.text.begin(); it != semi; ++it)
            if ((*it < 32 && *it != '\t') || *it > 126 || *it == '"')
                return fail("non-ASCII, control or quoted code unsupported");
        const auto entry = formats::text::keyed_kind(line.text);
        using Tag = formats::text::KeyedKindTag;
        if (entry.tag == Tag::Blank)
            continue;
        if (ended)
            return fail("content after END");
        const auto is = [&](const char* key) { return formats::text::equals(entry.name, key); };
        if (entry.tag == Tag::Section && is("WACHSTUM")) {
            if (incomplete())
                return fail("incomplete record or Bridge batch");
            active = true;
            ++result.sections;
            stage.fill(0); // Only section reset, NOT an Id reset (Game 0x10006b4b).
            w = h = 6;
            row = bridge_row = 0;
            id.reset();
            complete = size_seen = false;
            continue;
        }
        if (entry.tag == Tag::Section && is("END")) {
            if (!active)
                return fail("WACHSTUM required before END");
            if (incomplete())
                return fail("incomplete record or Bridge batch");
            ended = true;
            continue;
        }
        if (!active)
            return fail("WACHSTUM required");
        if (entry.tag != Tag::Entry)
            return fail("supported key: value required");
        if (is("Size")) {
            if (id || size_seen)
                return fail("Size only once before Id");
            const auto parsed = numbers(entry.value);
            if (!parsed)
                return fail(parsed.error());
            const auto& dims = parsed.value().values;
            if (dims.size() != 2 || dims[0] <= 0 || dims[1] <= 0 ||
                std::uint64_t(dims[0]) * std::uint64_t(dims[1]) > kStagingWords)
                return fail("dimensions exceed proved staging domain");
            w = static_cast<std::uint32_t>(dims[0]);
            h = static_cast<std::uint32_t>(dims[1]);
            size_seen = true;
        } else if (is("Id")) {
            if (incomplete())
                return fail("incomplete record or Bridge batch");
            const auto parsed = numbers(entry.value);
            if (!parsed)
                return fail(parsed.error());
            if (parsed.value().values.size() != 1)
                return fail("one Id required");
            id = parsed.value().values[0];
            id_line = line.number;
            row = bridge_row = 0;
            complete = false;
        } else if (is("Proz") || is("Bridge")) {
            const auto primary = is("Proz");
            if (!id || (primary && complete))
                return fail("Id required or extra Proz row");
            const auto parsed = numbers(entry.value, true);
            if (!parsed)
                return fail(parsed.error());
            const auto& vals = parsed.value().values;
            if (vals.size() > w)
                return fail("too many row values");
            if (!primary) {
                ++result.bridge_rows;
                bridge_row = (bridge_row + 1) % h;
                continue; // Independent secondary output is not produced here.
            }
            if (vals.size() < w)
                result.short_rows.push_back({line.number, *id, row,
                                             static_cast<std::uint32_t>(vals.size()), w,
                                             parsed.value().stopped});
            std::copy(vals.begin(), vals.end(),
                      stage.begin() + static_cast<std::ptrdiff_t>(row * w));
            if (++row == h) {
                if (result.values.empty())
                    result.values.assign(kPackedBytes, 0);
                for (std::uint32_t y = 0; y < h; ++y)
                    for (std::uint32_t x = 0; x < w; ++x) {
                        const auto index = ((static_cast<std::uint32_t>(*id) & 255U) << 6U) |
                                           ((x & 7U) << 3U) | (y & 7U);
                        result.values[index] = static_cast<std::uint8_t>(stage[y * w + x]);
                    }
                result.records.push_back({*id, w, h, id_line});
                complete = true;
            }
        } else if (is("Klima")) {
            if (!id || !climate_tokens(entry.value))
                return fail("unsupported Klima sidecar");
            ++result.climate_rows;
        } else {
            return fail("unknown key or section");
        }
    }
    if (!ended || result.records.empty())
        return std::string("growth lookup: END and at least one complete record required");
    result.source_sha256 = formats::sha256_hex(source);
    return result;
}
} // namespace anno::sim_load
