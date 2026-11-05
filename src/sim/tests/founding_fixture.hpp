// Reader of the committed founding fixtures (fixtures/synthetic/founding/*.txt). It checks the
// header's "body fnv64" line against the non-comment lines before anything else is parsed, so
// a hand-edited fixture fails loudly instead of passing silently.
#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

namespace founding_fixture {

inline std::uint64_t fnv64(const std::string& data) {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (const char c : data) {
        h = (h ^ static_cast<std::uint8_t>(c)) * 0x100000001b3ULL;
    }
    return h;
}

/// The non-comment lines of fixtures/synthetic/founding/<name>, after the integrity check.
inline std::vector<std::string> read_lines(const std::string& name) {
    const std::filesystem::path p = std::filesystem::path(ANNO_FIXTURES_DIR) / "founding" / name;
    std::ifstream in(p);
    REQUIRE_MESSAGE(in.good(), "missing fixture " << p.string());
    std::vector<std::string> lines;
    std::string body, line, want;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back(); // a CRLF checkout (core.autocrlf) must hash the same as LF
        }
        const std::string tag = "# body fnv64: ";
        if (line.rfind(tag, 0) == 0) {
            want = line.substr(tag.size(), 16);
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        body += line + "\n";
        lines.push_back(line);
    }
    REQUIRE_MESSAGE(want.size() == 16, name << ": no body fnv64 line");
    std::ostringstream got;
    got << std::hex;
    got.width(16);
    got.fill('0');
    got << fnv64(body);
    CHECK_MESSAGE(got.str() == want, name << ": body does not match its recorded hash");
    return lines;
}

} // namespace founding_fixture
