#include <algorithm>

#include <anno/formats/objectpos.hpp>

namespace anno::formats::objectpos {

namespace {
constexpr std::array<std::uint8_t, kNameLen> kConpo2 = {'C', 'O', 'N', 'P', 'O', '2'};
} // namespace

std::string SubRecord::name_str() const {
    const auto end = std::find(name.begin(), name.end(), std::uint8_t{0});
    return {name.begin(), end};
}

Result<ObjectPos, std::string> ObjectPos::parse(Bytes record) {
    ObjectPos out;
    std::size_t at = 0;
    while (at < record.size()) {
        if (record.size() - at < kHeaderLen) {
            return "OBJECTPOS: sub-record header at " + std::to_string(at) + " runs past the end";
        }
        SubRecord sub;
        std::copy_n(record.begin() + static_cast<std::ptrdiff_t>(at), kNameLen, sub.name.begin());
        const std::size_t size = u16_le(record, at + kNameLen);
        at += kHeaderLen;
        if (record.size() - at < size) {
            return "OBJECTPOS: sub-record '" + sub.name_str() + "' of " + std::to_string(size) +
                   " bytes runs past the end";
        }
        if (sub.name == kConpo2 && size % kArrowLen != 0) {
            return "OBJECTPOS: CONPO2 size " + std::to_string(size) + " is not a multiple of 3";
        }
        const auto first = record.begin() + static_cast<std::ptrdiff_t>(at);
        sub.data.assign(first, first + static_cast<std::ptrdiff_t>(size));
        at += size;
        out.records.push_back(std::move(sub));
    }
    return out;
}

ByteBuffer ObjectPos::to_bytes() const {
    ByteBuffer out;
    for (const SubRecord& sub : records) {
        out.insert(out.end(), sub.name.begin(), sub.name.end());
        put_u16_le(out, static_cast<std::uint16_t>(sub.data.size()));
        out.insert(out.end(), sub.data.begin(), sub.data.end());
    }
    return out;
}

std::vector<Arrow> ObjectPos::conpo2() const {
    std::vector<Arrow> out;
    for (const SubRecord& sub : records) {
        if (sub.name != kConpo2)
            continue;
        out.clear();
        for (std::size_t i = 0; i + kArrowLen <= sub.data.size(); i += kArrowLen)
            out.push_back(Arrow{sub.data[i], sub.data[i + 1], sub.data[i + 2]});
    }
    return out;
}

} // namespace anno::formats::objectpos
