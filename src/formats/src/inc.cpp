#include <algorithm>

#include <anno/formats/chunk.hpp>
#include <anno/formats/inc.hpp>

namespace anno::formats::inc {

std::optional<std::vector<AccessEntry>> parse_access(Bytes buf) {
    auto f = chunk::parse_file(buf);
    if (!f || !f.value().trailing.empty()) {
        return std::nullopt;
    }
    const chunk::Chunk& root = f.value().chunks.front();
    if (!root.is_container || root.children.size() != 1 || root.children[0].name() != "ACCESS") {
        return std::nullopt;
    }
    return parse_records(root.children[0].payload);
}

std::optional<std::vector<AccessEntry>> parse_records(Bytes payload) {
    std::vector<AccessEntry> out;
    Bytes rest = payload;
    while (!rest.empty()) {
        const auto nul = std::find(rest.begin(), rest.end(), std::uint8_t{0});
        if (nul == rest.end()) {
            return std::nullopt;
        }
        const Bytes record = rest.first(static_cast<std::size_t>(nul - rest.begin()));
        rest = rest.subspan(record.size() + 1);
        const auto comma = std::find(record.rbegin(), record.rend(), std::uint8_t{','});
        if (comma == record.rend()) {
            return std::nullopt;
        }
        const std::size_t at =
            record.size() - 1 - static_cast<std::size_t>(comma - record.rbegin());
        const Bytes digits = record.subspan(at + 1);
        if (digits.empty()) {
            return std::nullopt;
        }
        std::uint64_t n = 0;
        for (std::uint8_t d : digits) {
            if (d < '0' || d > '9') {
                return std::nullopt;
            }
            n = n * 10 + (d - '0');
            if (n > 0xFFFFFFFFu) {
                return std::nullopt;
            }
        }
        out.push_back({record.first(at), static_cast<std::uint32_t>(n)});
    }
    return out;
}

} // namespace anno::formats::inc
