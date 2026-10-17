#include <algorithm>

#include <anno/formats/text.hpp>

namespace anno::formats::text {

namespace {
bool space(std::uint8_t c) {
    return c == ' ' || c == '\t';
}
Bytes trim_start(Bytes s) {
    while (!s.empty() && space(s.front())) {
        s = s.subspan(1);
    }
    return s;
}
Bytes trim(Bytes s) {
    s = trim_start(s);
    while (!s.empty() && space(s.back())) {
        s = s.first(s.size() - 1);
    }
    return s;
}
bool alpha(std::uint8_t c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
bool digit(std::uint8_t c) {
    return c >= '0' && c <= '9';
}
} // namespace

bool equals(Bytes b, std::string_view s) {
    return b.size() == s.size() &&
           std::equal(b.begin(), b.end(), s.begin(),
                      [](std::uint8_t x, char y) { return x == static_cast<std::uint8_t>(y); });
}

std::vector<Line> lines(Bytes buf) {
    std::vector<Line> out;
    Bytes rest = buf;
    while (true) {
        const auto nl = std::find(rest.begin(), rest.end(), std::uint8_t{'\n'});
        if (nl == rest.end()) {
            break;
        }
        Bytes text = rest.first(static_cast<std::size_t>(nl - rest.begin()));
        Ending e = Ending::Lf;
        if (!text.empty() && text.back() == '\r') {
            text = text.first(text.size() - 1);
            e = Ending::CrLf;
        }
        out.push_back({out.size() + 1, text, e});
        rest = rest.subspan(static_cast<std::size_t>(nl - rest.begin()) + 1);
    }
    out.push_back({out.size() + 1, rest, Ending::None});
    return out;
}

ByteBuffer write_lines(const std::vector<Line>& ls) {
    ByteBuffer out;
    for (const auto& l : ls) {
        put_bytes(out, l.text);
        if (l.ending == Ending::CrLf) {
            out.push_back('\r');
        }
        if (l.ending != Ending::None) {
            out.push_back('\n');
        }
    }
    return out;
}

Bytes strip_comment(Bytes text) {
    bool q = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '"') {
            q = !q;
        } else if (text[i] == ';' && !q) {
            return text.first(i);
        }
    }
    return text;
}

KeyedKind keyed_kind(Bytes text) {
    const Bytes code = trim(strip_comment(text));
    if (code.empty()) {
        return {};
    }
    if (code.size() > 2 && code.front() == '[' && code.back() == ']') {
        const Bytes name = code.subspan(1, code.size() - 2);
        if (std::find(name.begin(), name.end(), std::uint8_t{']'}) == name.end()) {
            return {KeyedKindTag::Section, name, {}};
        }
    }
    std::size_t key_len = 0;
    while (key_len < code.size() &&
           (alpha(code[key_len]) || digit(code[key_len]) || code[key_len] == '_')) {
        ++key_len;
    }
    if ((alpha(code[0]) || code[0] == '_')) {
        const Bytes rest = trim_start(code.subspan(key_len));
        if (!rest.empty() && rest.front() == ':') {
            return {KeyedKindTag::Entry, code.first(key_len), trim(rest.subspan(1))};
        }
    }
    return {KeyedKindTag::Other, code, {}};
}

std::vector<std::pair<Line, KeyedKind>> keyed(Bytes buf) {
    std::vector<std::pair<Line, KeyedKind>> out;
    for (const auto& l : lines(buf)) {
        out.emplace_back(l, keyed_kind(l.text));
    }
    return out;
}

std::vector<Bytes> fields(Bytes value) {
    std::vector<Bytes> out;
    if (value.empty()) {
        return out;
    }
    std::size_t start = 0;
    bool q = false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '"') {
            q = !q;
        } else if (value[i] == ',' && !q) {
            out.push_back(trim(value.subspan(start, i - start)));
            start = i + 1;
        }
    }
    out.push_back(trim(value.subspan(start)));
    return out;
}

std::optional<std::int64_t> to_int(Bytes f) {
    bool neg = false;
    if (!f.empty() && f.front() == '-') {
        neg = true;
        f = f.subspan(1);
    }
    if (f.empty() || f.size() > 18) {
        return std::nullopt;
    }
    std::int64_t v = 0;
    for (std::uint8_t c : f) {
        if (!digit(c)) {
            return std::nullopt;
        }
        v = v * 10 + (c - '0');
    }
    return neg ? -v : v;
}

std::optional<std::vector<std::int64_t>> ints(Bytes value) {
    std::vector<std::int64_t> out;
    std::size_t i = 0;
    while (i < value.size()) {
        while (i < value.size() && (value[i] == ',' || space(value[i]))) {
            ++i;
        }
        std::size_t j = i;
        while (j < value.size() && value[j] != ',' && !space(value[j])) {
            ++j;
        }
        if (j > i) {
            auto v = to_int(value.subspan(i, j - i));
            if (!v) {
                return std::nullopt;
            }
            out.push_back(*v);
        }
        i = j;
    }
    return out;
}

std::optional<Bytes> quoted(Bytes f) {
    if (f.size() >= 2 && f.front() == '"' && f.back() == '"') {
        return f.subspan(1, f.size() - 2);
    }
    return std::nullopt;
}

std::optional<std::int64_t> fixed(Bytes field, unsigned digits) {
    bool neg = false;
    Bytes body = field;
    if (!body.empty() && body.front() == '-') {
        neg = true;
        body = body.subspan(1);
    }
    const auto dot = std::find(body.begin(), body.end(), std::uint8_t{'.'});
    const Bytes ip = body.first(static_cast<std::size_t>(dot - body.begin()));
    const bool has_dot = dot != body.end();
    const Bytes frac = has_dot ? body.subspan(ip.size() + 1) : Bytes();
    auto all_digits = [](Bytes s) { return std::all_of(s.begin(), s.end(), digit); };
    if (ip.empty() || !all_digits(ip) || !all_digits(frac) || (has_dot && frac.empty()) ||
        frac.size() > digits || digits > 9 || ip.size() > 9) {
        return std::nullopt;
    }
    std::int64_t scale = 1;
    for (unsigned k = 0; k < digits; ++k) {
        scale *= 10;
    }
    std::int64_t whole = 0, part = 0;
    for (auto c : ip) {
        whole = whole * 10 + (c - '0');
    }
    for (auto c : frac) {
        part = part * 10 + (c - '0');
    }
    for (std::size_t k = frac.size(); k < digits; ++k) {
        part *= 10;
    }
    const std::int64_t v = whole * scale + part;
    return neg ? -v : v;
}

std::vector<Entry> section_entries(Bytes buf, std::optional<std::string_view> name) {
    std::optional<Bytes> current;
    std::vector<Entry> out;
    for (const auto& [l, k] : keyed(buf)) {
        if (k.tag == KeyedKindTag::Section) {
            current = k.name;
        } else if (k.tag == KeyedKindTag::Entry) {
            const bool match = name ? (current && equals(*current, *name)) : !current.has_value();
            if (match) {
                out.push_back({l.number, k.name, k.value});
            }
        }
    }
    return out;
}

std::pair<std::vector<Entry>, std::vector<std::vector<Entry>>>
split_records(const std::vector<Entry>& entries, std::string_view start) {
    std::vector<Entry> head;
    std::vector<std::vector<Entry>> recs;
    for (const auto& e : entries) {
        if (equals(e.key, start)) {
            recs.emplace_back();
        }
        (recs.empty() ? head : recs.back()).push_back(e);
    }
    return {head, recs};
}

Texte Texte::parse(Bytes buf) {
    auto all = lines(buf);
    if (!all.empty() && all.back().text.empty() && all.back().ending == Ending::None) {
        all.pop_back();
    }
    Texte out;
    for (const auto& l : all) {
        const Bytes t = l.text;
        const bool header = t.size() > 2 && t.front() == '[' && t.back() == ']' &&
                            std::find(t.begin() + 1, t.end() - 1, std::uint8_t{']'}) == t.end() - 1;
        if (header) {
            out.sections.push_back({t.subspan(1, t.size() - 2), l.number, {}});
        } else if (!out.sections.empty()) {
            out.sections.back().lines.push_back(t);
        } else {
            out.preamble.push_back(t);
        }
    }
    return out;
}

const TextSection* Texte::section(std::string_view name) const {
    for (const auto& s : sections) {
        if (equals(s.name, name)) {
            return &s;
        }
    }
    return nullptr;
}

std::optional<std::pair<std::uint32_t, Bytes>> id_text(Bytes line) {
    const auto colon = std::find(line.begin(), line.end(), std::uint8_t{':'});
    if (colon == line.end()) {
        return std::nullopt;
    }
    const Bytes id = line.first(static_cast<std::size_t>(colon - line.begin()));
    if (id.empty() || id.size() > 9 || !std::all_of(id.begin(), id.end(), digit)) {
        return std::nullopt;
    }
    std::uint32_t v = 0;
    for (auto c : id) {
        v = v * 10 + static_cast<std::uint32_t>(c - '0');
    }
    return std::pair{v, line.subspan(id.size() + 1)};
}

} // namespace anno::formats::text
