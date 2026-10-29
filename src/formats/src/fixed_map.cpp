#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/fixed_map.hpp>

namespace anno::formats::fixed_map {
namespace {

constexpr std::uint64_t kFnvOffset = 0xcbf29ce484222325ULL;
constexpr std::uint64_t kFnvPrime = 0x100000001b3ULL;

std::uint64_t fnv64(const std::uint8_t* p, std::size_t n) {
    std::uint64_t h = kFnvOffset;
    for (std::size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= kFnvPrime;
    }
    return h;
}

void put_str(ByteBuffer& out, std::string_view s) {
    put_u32_le(out, static_cast<std::uint32_t>(s.size()));
    out.insert(out.end(), s.begin(), s.end());
}

void put_i32_le(ByteBuffer& out, std::int32_t v) {
    put_u32_le(out, static_cast<std::uint32_t>(v));
}

std::string hex16(std::uint64_t v) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string s(16, '0');
    for (int i = 15; i >= 0; --i) {
        s[static_cast<std::size_t>(i)] = kDigits[v & 0xf];
        v >>= 4;
    }
    return s;
}

struct Line {
    std::uint32_t number = 1;
    std::string_view text;
};

std::vector<Line> lines_of(Bytes bytes) {
    std::vector<Line> out;
    std::uint32_t number = 1;
    std::size_t i = 0;
    while (i < bytes.size()) {
        std::size_t end = i;
        while (end < bytes.size() && bytes[end] != '\n') {
            ++end;
        }
        std::size_t last = end;
        if (last > i && bytes[last - 1] == '\r') {
            --last;
        }
        out.push_back(Line{
            number, std::string_view(reinterpret_cast<const char*>(bytes.data() + i), last - i)});
        i = end < bytes.size() ? end + 1 : end;
        ++number;
    }
    return out;
}

bool ascii_line(std::string_view s) {
    for (unsigned char c : s) {
        if (c != '\t' && (c < 0x20 || c > 0x7e)) {
            return false;
        }
    }
    return true;
}

std::string_view trim_left(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
        s.remove_prefix(1);
    }
    return s;
}

std::vector<std::string_view> fields_of(std::string_view s) {
    std::vector<std::string_view> out;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
            ++i;
        }
        if (i >= s.size()) {
            break;
        }
        const std::size_t start = i;
        while (i < s.size() && s[i] != ' ' && s[i] != '\t') {
            ++i;
        }
        out.push_back(s.substr(start, i - start));
    }
    return out;
}

std::string normalise_key(std::string_view key) {
    std::string out;
    out.reserve(key.size());
    for (unsigned char c : key) {
        if (c == '\\') {
            out.push_back('/');
        } else if (c >= 'a' && c <= 'z') {
            out.push_back(static_cast<char>(c - 'a' + 'A'));
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

Error err(std::uint32_t line, std::string message) {
    return Error{line, std::move(message)};
}

template <typename T>
Result<T, Error> parse_int(std::string_view token, std::uint32_t line, std::string_view record,
                           int field, T lo, T hi) {
    if (token.empty()) {
        return err(line, "field " + std::to_string(field) + " of '" + std::string(record) +
                             "' is not an integer");
    }
    const bool neg = token.front() == '-';
    if (neg && !std::is_signed_v<T>) {
        return err(line, "field " + std::to_string(field) + " of '" + std::string(record) +
                             "' is not an integer");
    }
    std::string_view digits = token;
    if (neg) {
        digits.remove_prefix(1);
    }
    if (digits.empty() || digits.front() == '+') {
        return err(line, "field " + std::to_string(field) + " of '" + std::string(record) +
                             "' is not an integer");
    }
    for (unsigned char c : digits) {
        if (c < '0' || c > '9') {
            return err(line, "field " + std::to_string(field) + " of '" + std::string(record) +
                                 "' is not an integer");
        }
    }
    if constexpr (std::is_signed_v<T>) {
        long long v = 0;
        const auto [ptr, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), v);
        if (ec != std::errc{} || ptr != digits.data() + digits.size()) {
            return err(line, "field " + std::to_string(field) + " of '" + std::string(record) +
                                 "' is out of range");
        }
        if (neg) {
            if (v > static_cast<long long>(std::numeric_limits<T>::max()) + 1) {
                return err(line, "field " + std::to_string(field) + " of '" + std::string(record) +
                                     "' is out of range");
            }
            v = -v;
        }
        if (v < static_cast<long long>(lo) || v > static_cast<long long>(hi)) {
            return err(line, "field " + std::to_string(field) + " of '" + std::string(record) +
                                 "' is out of range");
        }
        return static_cast<T>(v);
    } else {
        unsigned long long v = 0;
        const auto [ptr, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), v);
        if (ec != std::errc{} || ptr != digits.data() + digits.size() ||
            v > static_cast<unsigned long long>(std::numeric_limits<T>::max()) ||
            v < static_cast<unsigned long long>(lo) || v > static_cast<unsigned long long>(hi)) {
            return err(line, "field " + std::to_string(field) + " of '" + std::string(record) +
                                 "' is out of range");
        }
        return static_cast<T>(v);
    }
}

Result<bool, Error> need(const std::vector<std::string_view>& f, std::size_t n, std::uint32_t line,
                         std::string_view record) {
    if (f.size() != n) {
        const std::size_t need_n = n - 1;
        const char* word = need_n == 1 ? "field" : "fields";
        return err(line, "record '" + std::string(record) + "' needs " + std::to_string(need_n) +
                             " " + word + ", has " + std::to_string(f.size() - 1));
    }
    return true;
}

} // namespace

std::string error_line(const Error& error) {
    return "error: line " + std::to_string(error.line) + ": " + error.message;
}

Result<FixedMap, Error> parse(Bytes bytes) {
    const std::vector<Line> rows = lines_of(bytes);
    bool saw_version = false;
    bool saw_name = false;
    bool saw_edge = false;
    bool saw_base = false;
    bool saw_seed = false;
    bool saw_money = false;
    FixedMap map;
    std::vector<Island> islands;
    std::vector<Start> starts;
    struct Extra {
        std::uint32_t slot = 0;
        std::vector<std::uint32_t> values;
        std::uint32_t line = 0;
        bool raw = false;
    };
    std::vector<Extra> extras;

    auto take_once = [&](bool& flag, std::string_view kind,
                         std::uint32_t line) -> Result<bool, Error> {
        if (flag) {
            return err(line, "duplicate record '" + std::string(kind) + "'");
        }
        flag = true;
        return true;
    };

    for (const Line& row : rows) {
        if (!ascii_line(row.text)) {
            return err(row.number, "line is not ASCII");
        }
        const std::string_view body = trim_left(row.text);
        if (body.empty() || body.front() == '#') {
            continue;
        }
        const std::vector<std::string_view> f = fields_of(body);
        const std::string_view kind = f[0];
        if (!saw_version) {
            if (kind != "anno1503-fixed-map" || f.size() != 2 || f[1] != "1") {
                return err(row.number, "version line must be 'anno1503-fixed-map 1'");
            }
            saw_version = true;
            continue;
        }
        if (kind == "name") {
            if (auto bad = take_once(saw_name, kind, row.number); !bad) {
                return bad.error();
            }
            if (auto bad = need(f, 2, row.number, kind); !bad) {
                return bad.error();
            }
            map.name = std::string(f[1]);
        } else if (kind == "edge") {
            if (auto bad = take_once(saw_edge, kind, row.number); !bad) {
                return bad.error();
            }
            if (auto bad = need(f, 3, row.number, kind); !bad) {
                return bad.error();
            }
            auto w = parse_int<std::uint32_t>(f[1], row.number, kind, 1, 1, kEdgeBound);
            if (!w) {
                return w.error();
            }
            auto h = parse_int<std::uint32_t>(f[2], row.number, kind, 2, 1, kEdgeBound);
            if (!h) {
                return h.error();
            }
            map.width = w.value();
            map.height = h.value();
        } else if (kind == "base") {
            if (auto bad = take_once(saw_base, kind, row.number); !bad) {
                return bad.error();
            }
            if (auto bad = need(f, 2, row.number, kind); !bad) {
                return bad.error();
            }
            map.base = std::string(f[1]);
        } else if (kind == "seed") {
            if (auto bad = take_once(saw_seed, kind, row.number); !bad) {
                return bad.error();
            }
            if (auto bad = need(f, 2, row.number, kind); !bad) {
                return bad.error();
            }
            auto s = parse_int<std::uint32_t>(f[1], row.number, kind, 1, 0,
                                              std::numeric_limits<std::uint32_t>::max());
            if (!s) {
                return s.error();
            }
            map.seed = s.value();
        } else if (kind == "money") {
            if (auto bad = take_once(saw_money, kind, row.number); !bad) {
                return bad.error();
            }
            if (auto bad = need(f, 2, row.number, kind); !bad) {
                return bad.error();
            }
            auto m = parse_int<std::int32_t>(f[1], row.number, kind, 1,
                                             std::numeric_limits<std::int32_t>::min(),
                                             std::numeric_limits<std::int32_t>::max());
            if (!m) {
                return m.error();
            }
            map.money = m.value();
        } else if (kind == "island") {
            if (auto bad = need(f, 5, row.number, kind); !bad) {
                return bad.error();
            }
            if (islands.size() >= kMaxRecords) {
                return err(row.number, "too many island records");
            }
            auto slot = parse_int<std::uint32_t>(f[1], row.number, kind, 1, 0,
                                                 std::numeric_limits<std::uint32_t>::max());
            if (!slot) {
                return slot.error();
            }
            auto x = parse_int<std::int32_t>(f[2], row.number, kind, 2,
                                             std::numeric_limits<std::int32_t>::min(),
                                             std::numeric_limits<std::int32_t>::max());
            if (!x) {
                return x.error();
            }
            auto y = parse_int<std::int32_t>(f[3], row.number, kind, 3,
                                             std::numeric_limits<std::int32_t>::min(),
                                             std::numeric_limits<std::int32_t>::max());
            if (!y) {
                return y.error();
            }
            Island island;
            island.slot = slot.value();
            island.x = x.value();
            island.y = y.value();
            island.key = normalise_key(f[4]);
            island.line = row.number;
            islands.push_back(std::move(island));
        } else if (kind == "start") {
            if (auto bad = need(f, 6, row.number, kind); !bad) {
                return bad.error();
            }
            if (starts.size() >= kMaxRecords) {
                return err(row.number, "too many start records");
            }
            auto seat = parse_int<std::uint32_t>(f[1], row.number, kind, 1, 0,
                                                 std::numeric_limits<std::uint32_t>::max());
            if (!seat) {
                return seat.error();
            }
            auto island = parse_int<std::uint32_t>(f[2], row.number, kind, 2, 0,
                                                   std::numeric_limits<std::uint32_t>::max());
            if (!island) {
                return island.error();
            }
            auto x = parse_int<std::int32_t>(f[3], row.number, kind, 3,
                                             std::numeric_limits<std::int32_t>::min(),
                                             std::numeric_limits<std::int32_t>::max());
            if (!x) {
                return x.error();
            }
            auto y = parse_int<std::int32_t>(f[4], row.number, kind, 4,
                                             std::numeric_limits<std::int32_t>::min(),
                                             std::numeric_limits<std::int32_t>::max());
            if (!y) {
                return y.error();
            }
            auto rot = parse_int<std::uint32_t>(f[5], row.number, kind, 5, 0, 3);
            if (!rot) {
                return rot.error();
            }
            Start start;
            start.seat = seat.value();
            start.island = island.value();
            start.x = x.value();
            start.y = y.value();
            start.rotation = rot.value();
            start.line = row.number;
            starts.push_back(std::move(start));
        } else if (kind == "raw" || kind == "ore") {
            if (f.size() < 2) {
                return err(row.number, "record '" + std::string(kind) + "' needs 1 field, has 0");
            }
            if (f.size() - 2 > kMaxValues) {
                return err(row.number, "record '" + std::string(kind) + "' needs at most " +
                                           std::to_string(kMaxValues) + " values, has " +
                                           std::to_string(f.size() - 2));
            }
            auto slot = parse_int<std::uint32_t>(f[1], row.number, kind, 1, 0,
                                                 std::numeric_limits<std::uint32_t>::max());
            if (!slot) {
                return slot.error();
            }
            Extra extra;
            extra.slot = slot.value();
            extra.line = row.number;
            extra.raw = kind == "raw";
            for (std::size_t i = 2; i < f.size(); ++i) {
                auto v = parse_int<std::uint32_t>(f[i], row.number, kind, static_cast<int>(i), 0,
                                                  std::numeric_limits<std::uint32_t>::max());
                if (!v) {
                    return v.error();
                }
                extra.values.push_back(v.value());
            }
            extras.push_back(std::move(extra));
        } else {
            return err(row.number, "unknown record '" + std::string(kind) + "'");
        }
    }
    if (!saw_version) {
        return err(1, "missing version line");
    }
    if (!saw_name || !saw_edge || !saw_base || !saw_seed) {
        std::string which = !saw_name ? "name" : !saw_edge ? "edge" : !saw_base ? "base" : "seed";
        return err(1, "missing record '" + which + "'");
    }
    std::stable_sort(islands.begin(), islands.end(),
                     [](const Island& a, const Island& b) { return a.slot < b.slot; });
    for (std::size_t i = 1; i < islands.size(); ++i) {
        if (islands[i].slot == islands[i - 1].slot) {
            return err(islands[i].line, "duplicate island slot " + std::to_string(islands[i].slot));
        }
    }
    std::stable_sort(starts.begin(), starts.end(),
                     [](const Start& a, const Start& b) { return a.seat < b.seat; });
    for (std::size_t i = 1; i < starts.size(); ++i) {
        if (starts[i].seat == starts[i - 1].seat) {
            return err(starts[i].line, "duplicate start seat " + std::to_string(starts[i].seat));
        }
    }
    std::vector<std::uint8_t> raw_set(islands.size(), 0);
    std::vector<std::uint8_t> ore_set(islands.size(), 0);
    for (const Extra& extra : extras) {
        std::size_t index = islands.size();
        for (std::size_t i = 0; i < islands.size(); ++i) {
            if (islands[i].slot == extra.slot) {
                index = i;
                break;
            }
        }
        if (index == islands.size()) {
            return err(extra.line, std::string(extra.raw ? "raw" : "ore") + " names island slot " +
                                       std::to_string(extra.slot) + ", which is not in the layout");
        }
        std::vector<std::uint8_t>& seen = extra.raw ? raw_set : ore_set;
        if (seen[index] != 0) {
            return err(extra.line, std::string("duplicate ") + (extra.raw ? "raw" : "ore") +
                                       " for island slot " + std::to_string(extra.slot));
        }
        seen[index] = 1;
        if (extra.raw) {
            islands[index].raw = extra.values;
        } else {
            islands[index].ore = extra.values;
        }
    }
    map.islands = std::move(islands);
    map.starts = std::move(starts);
    return map;
}

ByteBuffer canonical(const FixedMap& map) {
    ByteBuffer out;
    put_u32_le(out, map.version);
    put_str(out, map.name);
    put_u32_le(out, map.width);
    put_u32_le(out, map.height);
    put_str(out, map.base);
    put_u32_le(out, map.seed);
    out.push_back(map.money ? std::uint8_t{1} : std::uint8_t{0});
    put_i32_le(out, map.money.value_or(0));
    put_u32_le(out, static_cast<std::uint32_t>(map.islands.size()));
    for (const Island& island : map.islands) {
        put_u32_le(out, island.slot);
        put_i32_le(out, island.x);
        put_i32_le(out, island.y);
        put_str(out, island.key);
        put_u32_le(out, static_cast<std::uint32_t>(island.raw.size()));
        for (std::uint32_t v : island.raw) {
            put_u32_le(out, v);
        }
        put_u32_le(out, static_cast<std::uint32_t>(island.ore.size()));
        for (std::uint32_t v : island.ore) {
            put_u32_le(out, v);
        }
    }
    put_u32_le(out, static_cast<std::uint32_t>(map.starts.size()));
    for (const Start& start : map.starts) {
        put_u32_le(out, start.seat);
        put_u32_le(out, start.island);
        put_i32_le(out, start.x);
        put_i32_le(out, start.y);
        put_u32_le(out, start.rotation);
    }
    return out;
}

std::uint64_t digest(const FixedMap& map) {
    const ByteBuffer bytes = canonical(map);
    return fnv64(bytes.data(), bytes.size());
}

std::string summary(const FixedMap& map) {
    std::string out;
    out += "fixed-map " + std::to_string(map.version) + "\n";
    out += "name " + map.name + "\n";
    out += "edge " + std::to_string(map.width) + " " + std::to_string(map.height) + "\n";
    out += "base " + map.base + "\n";
    out += "seed " + std::to_string(map.seed) + "\n";
    if (map.money) {
        out += "money " + std::to_string(*map.money) + "\n";
    } else {
        out += "money default\n";
    }
    for (const Island& island : map.islands) {
        out += "island " + std::to_string(island.slot) + " " + std::to_string(island.x) + " " +
               std::to_string(island.y) + " " + island.key + "\n";
        if (!island.raw.empty()) {
            out += "raw " + std::to_string(island.slot);
            for (std::uint32_t v : island.raw) {
                out += " " + std::to_string(v);
            }
            out += "\n";
        }
        if (!island.ore.empty()) {
            out += "ore " + std::to_string(island.slot);
            for (std::uint32_t v : island.ore) {
                out += " " + std::to_string(v);
            }
            out += "\n";
        }
    }
    for (const Start& start : map.starts) {
        out += "start " + std::to_string(start.seat) + " " + std::to_string(start.island) + " " +
               std::to_string(start.x) + " " + std::to_string(start.y) + " " +
               std::to_string(start.rotation) + "\n";
    }
    out += "digest " + hex16(digest(map)) + "\n";
    return out;
}

} // namespace anno::formats::fixed_map
