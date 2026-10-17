#include "dump.hpp"

#include <cstdio>

namespace anno::dump {

std::uint64_t fnv64(Bytes bytes) {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (std::uint8_t b : bytes) {
        h = (h ^ b) * 0x100000001b3ULL;
    }
    return h;
}

std::string hex64(std::uint64_t v) {
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
    return buf;
}

std::string hex(Bytes bytes) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string s;
    for (std::uint8_t b : bytes) {
        s.push_back(kDigits[b >> 4]);
        s.push_back(kDigits[b & 15]);
    }
    return s;
}

std::string quote_all(Bytes raw) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string s = "\"";
    for (std::uint8_t b : raw) {
        if (b >= 0x20 && b <= 0x7e && b != '"' && b != '\\') {
            s.push_back(static_cast<char>(b));
        } else {
            s += "\\x";
            s.push_back(kDigits[b >> 4]);
            s.push_back(kDigits[b & 15]);
        }
    }
    s.push_back('"');
    return s;
}

std::string quote(Bytes raw) {
    std::size_t len = 0;
    while (len < raw.size() && raw[len] != 0) {
        ++len;
    }
    return quote_all(raw.first(len));
}

std::string palette_hash(const formats::tex::Palette& p) {
    formats::ByteBuffer rgb;
    for (unsigned i = 0; i < formats::tex::kPaletteLen; ++i) {
        const auto c = p.rgb(static_cast<std::uint8_t>(i));
        rgb.insert(rgb.end(), c.begin(), c.end());
    }
    return hex64(fnv64(rgb));
}

void palette_rows(const formats::tex::Palette& p, std::string& out) {
    for (unsigned row = 0; row < 16; ++row) {
        out += "  P" + std::to_string(row);
        for (unsigned i = 0; i < 16; ++i) {
            const auto c = p.rgb(static_cast<std::uint8_t>(row * 16 + i));
            out += " " + hex(c);
        }
        out += "\n";
    }
}

} // namespace anno::dump
