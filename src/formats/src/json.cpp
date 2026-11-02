// A strict JSON reader and the canonical writer (see json.hpp).
#include <algorithm>
#include <limits>

#include <anno/formats/json.hpp>

namespace anno::formats::json {

const Value* Value::find(std::string_view key) const {
    if (kind != Kind::Object) {
        return nullptr;
    }
    for (const auto& member : members) {
        if (member.first == key) {
            return &member.second;
        }
    }
    return nullptr;
}

Value Value::make_integer(std::int64_t v) {
    Value out;
    out.kind = Kind::Integer;
    out.integer = v;
    return out;
}

Value Value::make_string(std::string s) {
    Value out;
    out.kind = Kind::String;
    out.text = std::move(s);
    return out;
}

Value Value::make_array() {
    Value out;
    out.kind = Kind::Array;
    return out;
}

Value Value::make_object() {
    Value out;
    out.kind = Kind::Object;
    return out;
}

namespace {

class Reader {
  public:
    Reader(Bytes text, Limits limits) : text_(text), limits_(limits) {}

    Result<Value, Error> run() {
        skip_space();
        Value root;
        if (!value(root, 1)) {
            return error_;
        }
        skip_space();
        if (pos_ != text_.size()) {
            return fail_error("unexpected data after the value");
        }
        return root;
    }

  private:
    Bytes text_;
    Limits limits_;
    std::size_t pos_ = 0;
    Error error_;

    bool fail(const char* message) {
        if (error_.message.empty()) {
            error_.offset = pos_;
            error_.message = message;
        }
        return false;
    }
    Error fail_error(const char* message) {
        fail(message);
        return error_;
    }

    [[nodiscard]] bool at_end() const { return pos_ >= text_.size(); }
    [[nodiscard]] std::uint8_t peek() const { return text_[pos_]; }

    void skip_space() {
        while (!at_end() && (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r')) {
            ++pos_;
        }
    }

    bool literal(std::string_view word) {
        if (text_.size() - pos_ < word.size()) {
            return false;
        }
        for (std::size_t i = 0; i < word.size(); ++i) {
            if (text_[pos_ + i] != static_cast<std::uint8_t>(word[i])) {
                return false;
            }
        }
        pos_ += word.size();
        return true;
    }

    bool value(Value& out, std::uint32_t depth) {
        if (at_end()) {
            return fail("unexpected end of input");
        }
        const std::uint8_t c = peek();
        if (c == '{') {
            return object(out, depth);
        }
        if (c == '[') {
            return array(out, depth);
        }
        if (c == '"') {
            out.kind = Kind::String;
            return string(out.text);
        }
        if (c == 't' || c == 'f' || c == 'n') {
            return word(out);
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            return number(out);
        }
        return fail("unexpected character");
    }

    bool word(Value& out) {
        if (literal("true")) {
            out.kind = Kind::Bool;
            out.boolean = true;
            return true;
        }
        if (literal("false")) {
            out.kind = Kind::Bool;
            out.boolean = false;
            return true;
        }
        if (literal("null")) {
            out.kind = Kind::Null;
            return true;
        }
        return fail("invalid literal");
    }

    bool digits(std::size_t& count) {
        count = 0;
        while (!at_end() && peek() >= '0' && peek() <= '9') {
            ++pos_;
            ++count;
        }
        return count > 0;
    }

    bool number(Value& out) {
        const std::size_t start = pos_;
        bool negative = false;
        if (peek() == '-') {
            negative = true;
            ++pos_;
        }
        if (at_end()) {
            return fail("a number needs digits");
        }
        const std::size_t int_start = pos_;
        if (peek() == '0') {
            ++pos_;
            if (!at_end() && peek() >= '0' && peek() <= '9') {
                return fail("a number has a leading zero");
            }
        } else {
            std::size_t n = 0;
            if (!digits(n)) {
                return fail("a number needs digits");
            }
        }
        const std::size_t int_end = pos_;
        bool real = false;
        if (!at_end() && peek() == '.') {
            real = true;
            ++pos_;
            std::size_t n = 0;
            if (!digits(n)) {
                return fail("a fraction needs digits");
            }
        }
        if (!at_end() && (peek() == 'e' || peek() == 'E')) {
            real = true;
            ++pos_;
            if (!at_end() && (peek() == '+' || peek() == '-')) {
                ++pos_;
            }
            std::size_t n = 0;
            if (!digits(n)) {
                return fail("an exponent needs digits");
            }
        }
        if (real) {
            out.kind = Kind::Real;
            out.text.assign(text_.begin() + static_cast<std::ptrdiff_t>(start),
                            text_.begin() + static_cast<std::ptrdiff_t>(pos_));
            return true;
        }
        // An integer: accumulate as a negative magnitude so that int64 min fits.
        std::int64_t acc = 0;
        for (std::size_t i = int_start; i < int_end; ++i) {
            const std::int64_t digit = text_[i] - '0';
            if (acc < (std::numeric_limits<std::int64_t>::min() + digit) / 10) {
                pos_ = start;
                return fail("an integer does not fit int64");
            }
            acc = acc * 10 - digit;
        }
        if (!negative) {
            if (acc == std::numeric_limits<std::int64_t>::min()) {
                pos_ = start;
                return fail("an integer does not fit int64");
            }
            acc = -acc;
        }
        out.kind = Kind::Integer;
        out.integer = acc;
        return true;
    }

    static void put_utf8(std::string& out, std::uint32_t cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool hex4(std::uint32_t& out) {
        if (text_.size() - pos_ < 4) {
            return fail("a \\u escape needs four hex digits");
        }
        out = 0;
        for (int i = 0; i < 4; ++i) {
            const std::uint8_t c = text_[pos_ + static_cast<std::size_t>(i)];
            std::uint32_t d = 0;
            if (c >= '0' && c <= '9') {
                d = static_cast<std::uint32_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                d = static_cast<std::uint32_t>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                d = static_cast<std::uint32_t>(c - 'A' + 10);
            } else {
                return fail("a \\u escape needs four hex digits");
            }
            out = out * 16 + d;
        }
        pos_ += 4;
        return true;
    }

    /// One UTF-8 sequence at pos_ (a lead byte >= 0x80), validated and copied.
    bool utf8(std::string& out) {
        const std::uint8_t lead = peek();
        std::size_t len = 0;
        std::uint32_t cp = 0;
        std::uint32_t min = 0;
        if (lead >= 0xC2 && lead <= 0xDF) {
            len = 2;
            cp = lead & 0x1FU;
            min = 0x80;
        } else if (lead >= 0xE0 && lead <= 0xEF) {
            len = 3;
            cp = lead & 0x0FU;
            min = 0x800;
        } else if (lead >= 0xF0 && lead <= 0xF4) {
            len = 4;
            cp = lead & 0x07U;
            min = 0x10000;
        } else {
            return fail("invalid UTF-8");
        }
        if (text_.size() - pos_ < len) {
            return fail("invalid UTF-8");
        }
        for (std::size_t i = 1; i < len; ++i) {
            const std::uint8_t c = text_[pos_ + i];
            if ((c & 0xC0U) != 0x80U) {
                return fail("invalid UTF-8");
            }
            cp = (cp << 6) | (c & 0x3FU);
        }
        if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
            return fail("invalid UTF-8");
        }
        for (std::size_t i = 0; i < len; ++i) {
            out.push_back(static_cast<char>(text_[pos_ + i]));
        }
        pos_ += len;
        return true;
    }

    bool string(std::string& out) {
        ++pos_; // the opening quote
        out.clear();
        for (;;) {
            if (at_end()) {
                return fail("unterminated string");
            }
            const std::uint8_t c = peek();
            if (c == '"') {
                ++pos_;
                return true;
            }
            if (c < 0x20) {
                return fail("a control character in a string");
            }
            if (c >= 0x80) {
                if (!utf8(out)) {
                    return false;
                }
                continue;
            }
            if (c != '\\') {
                out.push_back(static_cast<char>(c));
                ++pos_;
                continue;
            }
            ++pos_;
            if (at_end()) {
                return fail("unterminated string");
            }
            const std::uint8_t e = peek();
            ++pos_;
            switch (e) {
            case '"':
                out.push_back('"');
                break;
            case '\\':
                out.push_back('\\');
                break;
            case '/':
                out.push_back('/');
                break;
            case 'b':
                out.push_back('\b');
                break;
            case 'f':
                out.push_back('\f');
                break;
            case 'n':
                out.push_back('\n');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case 't':
                out.push_back('\t');
                break;
            case 'u': {
                std::uint32_t cp = 0;
                if (!hex4(cp)) {
                    return false;
                }
                if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    return fail("a lone low surrogate escape");
                }
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    std::uint32_t low = 0;
                    if (text_.size() - pos_ < 2 || text_[pos_] != '\\' || text_[pos_ + 1] != 'u') {
                        return fail("a lone high surrogate escape");
                    }
                    pos_ += 2;
                    if (!hex4(low)) {
                        return false;
                    }
                    if (low < 0xDC00 || low > 0xDFFF) {
                        return fail("a lone high surrogate escape");
                    }
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                }
                put_utf8(out, cp);
                break;
            }
            default:
                --pos_;
                return fail("an invalid escape");
            }
        }
    }

    bool array(Value& out, std::uint32_t depth) {
        if (depth > limits_.max_depth) {
            return fail("nested too deeply");
        }
        out.kind = Kind::Array;
        ++pos_;
        skip_space();
        if (!at_end() && peek() == ']') {
            ++pos_;
            return true;
        }
        for (;;) {
            skip_space();
            out.items.emplace_back();
            if (!value(out.items.back(), depth + 1)) {
                return false;
            }
            skip_space();
            if (at_end()) {
                return fail("unterminated array");
            }
            if (peek() == ',') {
                ++pos_;
                continue;
            }
            if (peek() == ']') {
                ++pos_;
                return true;
            }
            return fail("expected ',' or ']'");
        }
    }

    bool object(Value& out, std::uint32_t depth) {
        if (depth > limits_.max_depth) {
            return fail("nested too deeply");
        }
        out.kind = Kind::Object;
        ++pos_;
        skip_space();
        if (!at_end() && peek() == '}') {
            ++pos_;
            return true;
        }
        for (;;) {
            skip_space();
            if (at_end() || peek() != '"') {
                return fail("expected a string key");
            }
            std::string key;
            if (!string(key)) {
                return false;
            }
            for (const auto& member : out.members) {
                if (member.first == key) {
                    return fail("duplicate key");
                }
            }
            skip_space();
            if (at_end() || peek() != ':') {
                return fail("expected ':'");
            }
            ++pos_;
            skip_space();
            out.members.emplace_back(std::move(key), Value{});
            if (!value(out.members.back().second, depth + 1)) {
                return false;
            }
            skip_space();
            if (at_end()) {
                return fail("unterminated object");
            }
            if (peek() == ',') {
                ++pos_;
                continue;
            }
            if (peek() == '}') {
                ++pos_;
                return true;
            }
            return fail("expected ',' or '}'");
        }
    }
};

void write_escaped(std::string& out, const std::string& s) {
    static constexpr char kHex[] = "0123456789abcdef";
    const auto u4 = [&](std::uint32_t v) {
        out += "\\u";
        out.push_back(kHex[(v >> 12) & 0xFU]);
        out.push_back(kHex[(v >> 8) & 0xFU]);
        out.push_back(kHex[(v >> 4) & 0xFU]);
        out.push_back(kHex[v & 0xFU]);
    };
    out.push_back('"');
    for (std::size_t i = 0; i < s.size();) {
        const auto c = static_cast<std::uint8_t>(s[i]);
        if (c < 0x80) {
            ++i;
            switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (c < 0x20 || c == 0x7F) {
                    u4(c);
                } else {
                    out.push_back(static_cast<char>(c));
                }
            }
            continue;
        }
        // The reader guarantees valid UTF-8.
        std::size_t len = c >= 0xF0 ? 4 : (c >= 0xE0 ? 3 : 2);
        std::uint32_t cp = c & (len == 4 ? 0x07U : (len == 3 ? 0x0FU : 0x1FU));
        for (std::size_t k = 1; k < len && i + k < s.size(); ++k) {
            cp = (cp << 6) | (static_cast<std::uint8_t>(s[i + k]) & 0x3FU);
        }
        i += len;
        if (cp >= 0x10000) {
            cp -= 0x10000;
            u4(0xD800 + (cp >> 10));
            u4(0xDC00 + (cp & 0x3FFU));
        } else {
            u4(cp);
        }
    }
    out.push_back('"');
}

void write_value(std::string& out, const Value& v) {
    switch (v.kind) {
    case Kind::Null:
        out += "null";
        break;
    case Kind::Bool:
        out += v.boolean ? "true" : "false";
        break;
    case Kind::Integer:
        out += std::to_string(v.integer);
        break;
    case Kind::Real:
        out += v.text;
        break;
    case Kind::String:
        write_escaped(out, v.text);
        break;
    case Kind::Array: {
        out.push_back('[');
        for (std::size_t i = 0; i < v.items.size(); ++i) {
            if (i != 0) {
                out.push_back(',');
            }
            write_value(out, v.items[i]);
        }
        out.push_back(']');
        break;
    }
    case Kind::Object: {
        std::vector<const std::pair<std::string, Value>*> sorted;
        sorted.reserve(v.members.size());
        for (const auto& member : v.members) {
            sorted.push_back(&member);
        }
        // Code-point order is byte order of UTF-8.
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto* a, const auto* b) { return a->first < b->first; });
        out.push_back('{');
        for (std::size_t i = 0; i < sorted.size(); ++i) {
            if (i != 0) {
                out.push_back(',');
            }
            write_escaped(out, sorted[i]->first);
            out.push_back(':');
            write_value(out, sorted[i]->second);
        }
        out.push_back('}');
        break;
    }
    }
}

} // namespace

Result<Value, Error> parse(Bytes text, Limits limits) {
    return Reader(text, limits).run();
}

std::string canonical(const Value& value) {
    std::string out;
    write_value(out, value);
    return out;
}

} // namespace anno::formats::json
