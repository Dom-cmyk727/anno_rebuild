#include <array>

#include <anno/formats/tagged.hpp>

namespace anno::formats::tagged {

namespace {

/// The name in a 6-byte field if it matches [A-Z][A-Z0-9_]{0,5} and is
/// padded with zeros only; empty otherwise.
std::string_view name_of(Bytes field) {
    std::size_t len = 0;
    while (len < kNameLen && field[len] != 0) {
        ++len;
    }
    if (len == 0 || field[0] < 'A' || field[0] > 'Z') {
        return {};
    }
    for (std::size_t i = 1; i < len; ++i) {
        const std::uint8_t c = field[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
            return {};
        }
    }
    for (std::size_t i = len; i < kNameLen; ++i) {
        if (field[i] != 0) {
            return {};
        }
    }
    return {reinterpret_cast<const char*>(field.data()), len};
}

} // namespace

Result<std::vector<Record>, std::string> parse_records(Bytes data, std::size_t base) {
    std::vector<Record> out;
    std::size_t off = 0;
    while (off < data.size()) {
        const std::size_t at = base + off;
        if (data.size() - off < kHeaderLen) {
            return "truncated record header at " + std::to_string(at);
        }
        const std::string_view name = name_of(data.subspan(off, kNameLen));
        if (name.empty()) {
            return "invalid record name at " + std::to_string(at);
        }
        const std::uint16_t field = u16_le(data, off + kNameLen);
        std::size_t start = off + kHeaderLen;
        const bool long_form = field >= kLongMarker;
        std::uint32_t length = field;
        if (long_form) {
            if (data.size() - start < 2) {
                return "truncated record header at " + std::to_string(at);
            }
            length = static_cast<std::uint32_t>(field & 0xFFu) << 16 | u16_le(data, start);
            start += 2;
        }
        if (length > data.size() - start) {
            return "record at " + std::to_string(at) + " declares length " +
                   std::to_string(length) + " past end of data";
        }
        out.push_back({name, at, long_form, data.subspan(start, length)});
        off = start + length;
    }
    return out;
}

bool write_record(std::string_view name, bool long_form, Bytes payload, ByteBuffer& out) {
    if (name.size() > kNameLen) {
        return false;
    }
    std::array<std::uint8_t, kNameLen> field{};
    for (std::size_t i = 0; i < name.size(); ++i) {
        field[i] = static_cast<std::uint8_t>(name[i]);
    }
    if (name_of(field) != name) {
        return false;
    }
    if (payload.size() > kMaxLongLength || (!long_form && payload.size() >= kLongMarker)) {
        return false;
    }
    const auto length = static_cast<std::uint32_t>(payload.size());
    out.insert(out.end(), field.begin(), field.end());
    if (long_form) {
        put_u16_le(out, static_cast<std::uint16_t>(kLongMarker | (length >> 16)));
    }
    put_u16_le(out, static_cast<std::uint16_t>(length & 0xFFFF));
    put_bytes(out, payload);
    return true;
}

bool write_records(const std::vector<Record>& records, ByteBuffer& out) {
    for (const auto& r : records) {
        if (!write_record(r.name, r.long_form, r.payload, out)) {
            return false;
        }
    }
    return true;
}

const Record* find(const std::vector<Record>& records, std::string_view name) {
    for (const auto& r : records) {
        if (r.name == name) {
            return &r;
        }
    }
    return nullptr;
}

} // namespace anno::formats::tagged
