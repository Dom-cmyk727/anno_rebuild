// Scenario script reader (docs/formats/bigbrother.md; reference: tools/bigbrother.py, whose
// grammar, error cases and interpreter's reading this mirrors).
#include <algorithm>
#include <array>
#include <limits>

#include <anno/formats/bigbrother.hpp>
#include <anno/formats/chunk.hpp>

namespace anno::formats::bigbrother {

namespace {

// The six sections in the order every shipped script has them (P, 50 of 50). The reader also
// knows STARTUP (S: AnnoScene.dll 0x1003e628), which no shipped script has: kept as unknown.
constexpr std::array<KnownSection, 6> kKnownSections{{
    {"[OBJECT_DEFINE]", SectionKind::Objects},
    {"[AREA_DEFINE]", SectionKind::Areas},
    {"[PLAYERMAPPER_DEFINE]", SectionKind::Players},
    {"[SCRIPT_KERNEL]", SectionKind::Kernel},
    {"[SIGNALS]", SectionKind::Signals},
    {"[CAMARAPOS]", SectionKind::Camera},
}};

// The define lines of [OBJECT_DEFINE] and their numbers (P: 133, 80, 37 lines).
constexpr std::array<ObjectKind, 3> kObjectKinds{{
    {"OBJECT_FIGUR", 1},
    {"OBJECT_STADT", 1},
    {"OBJECT_ANLAGE", 3},
}};

constexpr std::array<std::string_view, 3> kSubjects{"PLAYER", "OBJECT", "AREA"};
// The comparisons the scripts use (P); != <= >= lex but never occur and are refused.
constexpr std::array<std::string_view, 3> kCompare{"==", "<", ">"};
// Never a variable, signal, call or query name (P): keywords and truth values.
constexpr std::array<std::string_view, 7> kReserved{"INITBLOCK", "SET",  "IF",   "THEN",
                                                    "ELSE",      "TRUE", "FALSE"};
// Operators in the reference lexer's order (two-byte ones before their prefixes).
constexpr std::array<std::string_view, 16> kOperators{"&&", "||", "==", "!=", "<=", ">=", "<", ">",
                                                      "=",  "!",  "(",  ")",  "{",  "}",  ",", ";"};

template <class C> bool contains(const C& c, std::string_view v) {
    return std::find(c.begin(), c.end(), v) != c.end();
}

bool is_reserved(std::string_view v) {
    return contains(kReserved, v);
}

bool is_digit(std::uint8_t c) {
    return c >= '0' && c <= '9';
}
bool is_alpha(std::uint8_t c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}
bool is_alnum(std::uint8_t c) {
    return is_alpha(c) || is_digit(c);
}

std::string bytes_text(Bytes b, std::size_t from, std::size_t to) {
    std::string s;
    s.reserve(to - from);
    for (std::size_t i = from; i < to; ++i) {
        s.push_back(static_cast<char>(b[i]));
    }
    return s;
}

/// 1 + the LF bytes before `offset`.
std::size_t line_at(Bytes text, std::size_t offset) {
    const auto end = text.begin() + static_cast<std::ptrdiff_t>(std::min(offset, text.size()));
    return 1 + static_cast<std::size_t>(std::count(text.begin(), end, std::uint8_t{'\n'}));
}

/// Printable ASCII as is, every other byte as \xHH (for error messages).
std::string shown(std::string_view s) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out = "'";
    for (char ch : s) {
        const auto c = static_cast<std::uint8_t>(ch);
        if (c >= 0x20 && c <= 0x7e && c != '\\' && c != '\'') {
            out.push_back(ch);
        } else {
            out += "\\x";
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 0xF]);
        }
    }
    return out + "'";
}

/// The token's length at `pos`, or 0 if no token starts there (the reference's ordered
/// alternatives: the first that matches wins).
std::pair<TokenKind, std::size_t> match_token(Bytes t, std::size_t pos) {
    const std::size_t n = t.size();
    const auto at = [&](std::size_t i) -> std::uint8_t { return i < n ? t[i] : std::uint8_t{0}; };
    const std::uint8_t c = t[pos];
    if (c == '\n') {
        return {TokenKind::Newline, 1};
    }
    if (c == '\r' && at(pos + 1) == '\n' && pos + 1 < n) {
        return {TokenKind::Newline, 2};
    }
    if (c == ' ' || c == '\t') {
        std::size_t e = pos;
        while (e < n && (t[e] == ' ' || t[e] == '\t')) {
            ++e;
        }
        return {TokenKind::Blank, e - pos};
    }
    if (c == '$') {
        std::size_t e = pos + 1;
        while (e < n && t[e] != '\r' && t[e] != '\n') {
            ++e;
        }
        return {TokenKind::Comment, e - pos};
    }
    if (c == '[') {
        std::size_t e = pos + 1;
        while (e < n && ((t[e] >= 'A' && t[e] <= 'Z') || t[e] == '_')) {
            ++e;
        }
        if (e > pos + 1 && e < n && t[e] == ']') {
            return {TokenKind::Section, e + 1 - pos};
        }
        return {TokenKind::Section, 0};
    }
    if (c == '"') {
        std::size_t e = pos + 1;
        while (e < n && t[e] != '"' && t[e] != '\r' && t[e] != '\n') {
            ++e;
        }
        if (e < n && t[e] == '"') {
            return {TokenKind::String, e + 1 - pos};
        }
        return {TokenKind::String, 0};
    }
    if (is_digit(c) || (c == '-' && pos + 1 < n && is_digit(t[pos + 1]))) {
        std::size_t e = pos + 1;
        while (e < n && is_digit(t[e])) {
            ++e;
        }
        return {TokenKind::Number, e - pos};
    }
    if (is_alpha(c)) {
        std::size_t e = pos + 1;
        while (e < n && is_alnum(t[e])) {
            ++e;
        }
        return {TokenKind::Name, e - pos};
    }
    for (std::string_view op : kOperators) {
        if (pos + op.size() <= n &&
            std::equal(op.begin(), op.end(), t.begin() + static_cast<std::ptrdiff_t>(pos),
                       [](char a, std::uint8_t b) { return static_cast<std::uint8_t>(a) == b; })) {
            return {TokenKind::Operator, op.size()};
        }
    }
    return {TokenKind::Blank, 0};
}

bool is_trivia(TokenKind k) {
    return k == TokenKind::Newline || k == TokenKind::Blank || k == TokenKind::Comment;
}

/// The significant tokens of a kernel, each with whether a line break lies before it.
class Cursor {
  public:
    Cursor(Bytes text, const std::vector<Token>& toks, std::size_t first, std::size_t last,
           std::string_view where)
        : text_(text), where_(where) {
        bool gap = false;
        for (std::size_t i = first; i < last; ++i) {
            const Token& x = toks[i];
            if (x.kind == TokenKind::Newline) {
                gap = true;
            } else if (!is_trivia(x.kind)) {
                t_.push_back(&x);
                nl_.push_back(gap);
                gap = false;
            }
        }
        end_offset_ = last < toks.size() ? toks[last].offset : text.size();
    }

    [[nodiscard]] bool done() const { return i_ >= t_.size(); }
    [[nodiscard]] const Token* peek(std::size_t k = 0) const {
        return i_ + k < t_.size() ? t_[i_ + k] : nullptr;
    }
    /// Nothing else follows on the current line.
    [[nodiscard]] bool at_line_end() const { return done() || nl_[i_]; }
    [[nodiscard]] bool is(TokenKind kind, std::string_view text = {}) const {
        const Token* t = peek();
        return t != nullptr && t->kind == kind && (text.empty() || t->text == text);
    }
    [[nodiscard]] bool is_op(std::string_view text) const { return is(TokenKind::Operator, text); }

    [[nodiscard]] Error error_at(std::size_t off, std::string message) const {
        return Error{off, line_at(text_, off), std::move(message)};
    }

    [[nodiscard]] Error fail(std::string_view what) const {
        const Token* t = peek();
        const std::size_t off = t != nullptr ? t->offset : end_offset_;
        std::string got = t != nullptr ? shown(t->text) : std::string("the end of the section");
        return Error{off, line_at(text_, off),
                     std::string(where_) + ": " + std::string(what) + ", got " + got};
    }

    /// The next token if it is of `kind` (and `text`), else nullptr (then `fail` explains).
    const Token* take(TokenKind kind, std::string_view text = {}) {
        if (!is(kind, text)) {
            return nullptr;
        }
        return t_[i_++];
    }
    const Token* take_any() { return done() ? nullptr : t_[i_++]; }
    /// A name that is not reserved.
    const Token* name() {
        const Token* t = peek();
        if (t == nullptr || t->kind != TokenKind::Name || is_reserved(t->text)) {
            return nullptr;
        }
        ++i_;
        return t;
    }

  private:
    Bytes text_;
    std::string_view where_;
    std::vector<const Token*> t_;
    std::vector<bool> nl_;
    std::size_t i_ = 0;
    std::size_t end_offset_ = 0;
};

template <class T> using R = Result<T, Error>;

R<Value> value(Cursor& c) {
    const Token* t = c.peek();
    if (t != nullptr && t->kind == TokenKind::Name && (t->text == "TRUE" || t->text == "FALSE")) {
        c.take_any();
        return Value{ValueKind::Bool, t->text};
    }
    if (t != nullptr && t->kind == TokenKind::Number) {
        c.take_any();
        return Value{ValueKind::Number, t->text};
    }
    if (t != nullptr && t->kind == TokenKind::String) {
        c.take_any();
        return Value{ValueKind::String, t->text};
    }
    return c.fail("expected a value");
}

std::string unquote(const std::string& s) {
    return s.substr(1, s.size() - 2);
}

R<Primary> primary(Cursor& c) {
    const Token* t = c.name();
    if (t == nullptr) {
        return c.fail("expected a name");
    }
    Primary p;
    p.offset = t->offset;
    if (contains(kSubjects, t->text)) {
        const Token* s = c.take(TokenKind::String);
        if (s == nullptr) {
            return c.fail("expected STRING");
        }
        const Token* q = c.name();
        if (q == nullptr) {
            return c.fail("expected a name");
        }
        p.subject = t->text;
        p.name = unquote(s->text);
        p.query = q->text;
        if (const Token* a = c.take(TokenKind::String)) {
            p.arg = unquote(a->text);
        }
    } else {
        p.name = t->text;
    }
    const Token* o = c.peek();
    if (o != nullptr && o->kind == TokenKind::Operator && contains(kCompare, o->text)) {
        c.take_any();
        p.op = o->text;
        auto v = value(c);
        if (!v) {
            return v.error();
        }
        p.value = std::move(v).value();
    }
    return p;
}

/// The depth Options::max_condition_depth bounds: along any path of the condition as written,
/// the connectives `grouped` nests it under plus the parentheses. Term `i` of `k` connectives
/// lies under `i + 1` of them, the last one under `k`.
std::size_t written_depth(const Condition& g) {
    const std::size_t k = g.connectives.size();
    std::size_t d = 0;
    for (std::size_t i = 0; i < g.terms.size(); ++i) {
        const std::size_t pos = i < k ? i + 1 : k;
        const Term& t = g.terms[i];
        d = std::max(d, pos + (t.group.empty() ? 0 : 1 + written_depth(t.group[0])));
    }
    return d;
}

/// `base`: a lower bound of this condition's depth (written_depth). Checking it before each term
/// bounds the recursion while parsing; the exact depth is checked when the condition is read.
R<Condition> condition(Cursor& c, std::size_t base, std::size_t max_depth) {
    Condition g;
    while (true) {
        Term term;
        const std::size_t level = base + g.connectives.size();
        const bool group = c.is_op("(");
        if (level + (group ? 1U : 0U) > max_depth) {
            return c.fail("condition nested deeper than " + std::to_string(max_depth) +
                          " levels (Options::max_condition_depth)");
        }
        if (c.take(TokenKind::Operator, "(") != nullptr) {
            auto inner = condition(c, level + 1, max_depth);
            if (!inner) {
                return inner.error();
            }
            if (c.take(TokenKind::Operator, ")") == nullptr) {
                return c.fail("expected )");
            }
            term.group.push_back(std::move(inner).value());
        } else {
            auto p = primary(c);
            if (!p) {
                return p.error();
            }
            term.primary = std::move(p).value();
        }
        g.terms.push_back(std::move(term));
        if (c.take(TokenKind::Operator, "&&") != nullptr) {
            g.connectives.push_back(Connective::And);
        } else if (c.take(TokenKind::Operator, "||") != nullptr) {
            g.connectives.push_back(Connective::Or);
        } else {
            return g;
        }
    }
}

R<std::vector<Value>> call_args(Cursor& c) {
    std::vector<Value> args;
    if (c.take(TokenKind::Operator, "(") == nullptr) {
        return c.fail("expected (");
    }
    if (!c.is_op(")")) {
        while (true) {
            auto v = value(c);
            if (!v) {
                return v.error();
            }
            args.push_back(std::move(v).value());
            if (c.take(TokenKind::Operator, ",") == nullptr) {
                break;
            }
        }
    }
    if (c.take(TokenKind::Operator, ")") == nullptr) {
        return c.fail("expected )");
    }
    return args;
}

/// A statement's `;`. Four shipped statements end without one (S01_02_01, S01_08_01, S01_09_01
/// twice); each ends its line, so a missing `;` is accepted only at the end of a line.
R<bool> end(Cursor& c) {
    if (c.take(TokenKind::Operator, ";") != nullptr) {
        return true;
    }
    if (!c.at_line_end()) {
        return c.fail("expected ';'");
    }
    return false;
}

R<std::vector<Statement>> block(Cursor& c) {
    if (c.take(TokenKind::Operator, "{") == nullptr) {
        return c.fail("expected {");
    }
    std::vector<Statement> body;
    while (!c.is_op("}")) {
        if (c.peek() == nullptr || c.peek()->kind != TokenKind::Name) {
            return c.fail("expected IDENT");
        }
        const Token* t = c.name();
        if (t == nullptr) {
            return c.fail("expected a name");
        }
        if (c.take(TokenKind::Operator, "=") != nullptr) {
            auto v = value(c);
            if (!v) {
                return v.error();
            }
            auto semi = end(c);
            if (!semi) {
                return semi.error();
            }
            body.emplace_back(
                Assign{t->text, std::move(v).value(), false, semi.value(), t->offset});
        } else if (c.is_op("(")) {
            auto args = call_args(c);
            if (!args) {
                return args.error();
            }
            auto semi = end(c);
            if (!semi) {
                return semi.error();
            }
            body.emplace_back(
                Call{t->text, std::move(args).value(), true, semi.value(), t->offset});
        } else if (c.take(TokenKind::Operator, ";") != nullptr) {
            body.emplace_back(Call{t->text, {}, false, true, t->offset});
        } else {
            return c.fail("expected '=', '(' or ';' after " + t->text);
        }
    }
    c.take_any();
    return body;
}

R<std::vector<KernelItem>> parse_kernel(Cursor c, const Options& options) {
    std::vector<KernelItem> items;
    while (!c.done()) {
        const Token* t = c.take(TokenKind::Name);
        if (t == nullptr) {
            return c.fail("expected IDENT");
        }
        if (t->text == "INITBLOCK") {
            if (c.take(TokenKind::Operator, "(") == nullptr) {
                return c.fail("expected (");
            }
            if (c.take(TokenKind::Operator, ")") == nullptr) {
                return c.fail("expected )");
            }
            auto body = block(c);
            if (!body) {
                return body.error();
            }
            items.emplace_back(InitBlock{std::move(body).value(), t->offset});
        } else if (t->text == "SET") {
            const Token* n = c.name();
            if (n == nullptr) {
                return c.fail("expected a name");
            }
            if (c.take(TokenKind::Operator, "=") == nullptr) {
                return c.fail("expected =");
            }
            auto v = value(c);
            if (!v) {
                return v.error();
            }
            auto semi = end(c);
            if (!semi) {
                return semi.error();
            }
            items.emplace_back(
                Assign{n->text, std::move(v).value(), true, semi.value(), t->offset});
        } else if (t->text == "IF") {
            auto cond = condition(c, 0, options.max_condition_depth);
            if (!cond) {
                return cond.error();
            }
            if (written_depth(cond.value()) > options.max_condition_depth) {
                return c.error_at(t->offset, "[SCRIPT_KERNEL]: the condition of the IF at offset " +
                                                 std::to_string(t->offset) + " nests deeper than " +
                                                 std::to_string(options.max_condition_depth) +
                                                 " levels (Options::max_condition_depth)");
            }
            if (c.take(TokenKind::Name, "THEN") == nullptr) {
                return c.fail("expected THEN for the IF at offset " + std::to_string(t->offset));
            }
            If item;
            item.offset = t->offset;
            item.condition = std::move(cond).value();
            auto then_block = block(c);
            if (!then_block) {
                return then_block.error();
            }
            item.then_block = std::move(then_block).value();
            if (c.take(TokenKind::Name, "ELSE") != nullptr) {
                auto else_block = block(c);
                if (!else_block) {
                    return else_block.error();
                }
                item.has_else = true;
                item.else_block = std::move(else_block).value();
            }
            items.emplace_back(std::move(item));
        } else {
            return c.error_at(t->offset, "[SCRIPT_KERNEL]: unexpected " + shown(t->text) +
                                             " at offset " + std::to_string(t->offset));
        }
    }
    return items;
}

/// The significant tokens of each line of a define section.
std::vector<std::vector<const Token*>> lines(const std::vector<Token>& toks, std::size_t first,
                                             std::size_t last) {
    std::vector<std::vector<const Token*>> out;
    std::vector<const Token*> cur;
    for (std::size_t i = first; i < last; ++i) {
        const Token& t = toks[i];
        if (t.kind == TokenKind::Newline) {
            if (!cur.empty()) {
                out.push_back(std::move(cur));
            }
            cur.clear();
        } else if (t.kind != TokenKind::Blank && t.kind != TokenKind::Comment) {
            cur.push_back(&t);
        }
    }
    if (!cur.empty()) {
        out.push_back(std::move(cur));
    }
    return out;
}

/// The line's shape as the reference writes it: operators by their text, the rest by kind.
std::string shape(const std::vector<const Token*>& ln) {
    std::string s;
    for (const Token* t : ln) {
        if (!s.empty()) {
            s.push_back(' ');
        }
        s += t->kind == TokenKind::Operator ? t->text : std::string(kind_name(t->kind));
    }
    return s;
}

std::string object_shape(std::size_t numbers) {
    std::string s = "IDENT STRING NUMBER";
    for (std::size_t i = 1; i < numbers; ++i) {
        s += " , NUMBER";
    }
    return s;
}

std::vector<std::string> numbers_of(const std::vector<const Token*>& ln) {
    std::vector<std::string> out;
    for (const Token* t : ln) {
        if (t->kind == TokenKind::Number) {
            out.push_back(t->text);
        }
    }
    return out;
}

R<bool> parse_defines(Bytes text, const std::vector<Token>& toks, std::size_t first,
                      std::size_t last, const KnownSection& sec, Script& out) {
    const std::string where(sec.name);
    const std::size_t end_offset = last < toks.size() ? toks[last].offset : text.size();
    const auto error_at = [&](std::size_t off, std::string msg) {
        return Error{off, line_at(text, off), where + ": " + std::move(msg)};
    };
    for (std::size_t i = first; i < last; ++i) {
        if (toks[i].kind == TokenKind::Comment) { // all comments are in the kernel (P)
            return error_at(toks[i].offset,
                            "a comment at offset " + std::to_string(toks[i].offset));
        }
    }
    std::vector<std::pair<std::string, std::size_t>> names;
    std::size_t camera_lines = 0;
    for (const auto& ln : lines(toks, first, last)) {
        const std::string s = shape(ln);
        std::string joined;
        for (const Token* t : ln) {
            joined += (joined.empty() ? "" : " ") + t->text;
        }
        const Error bad =
            error_at(ln[0]->offset, "unexpected line " + shown(joined) + " at offset " +
                                        std::to_string(ln[0]->offset));
        switch (sec.kind) {
        case SectionKind::Objects: {
            // OBJECT_FIGUR "N" id | OBJECT_STADT "N" id | OBJECT_ANLAGE "N" x,y,type (P)
            const auto k = std::find_if(kObjectKinds.begin(), kObjectKinds.end(),
                                        [&](const ObjectKind& o) { return o.name == ln[0]->text; });
            if (ln[0]->kind != TokenKind::Name || k == kObjectKinds.end() ||
                s != object_shape(k->numbers)) {
                return bad;
            }
            out.objects.push_back(
                {ln[0]->text, unquote(ln[1]->text), numbers_of(ln), ln[0]->offset});
            names.emplace_back(out.objects.back().name, ln[0]->offset);
            break;
        }
        case SectionKind::Areas:
            if (s != "STRING NUMBER , NUMBER , NUMBER , NUMBER") {
                return bad;
            }
            out.areas.push_back({unquote(ln[0]->text), numbers_of(ln), ln[0]->offset});
            names.emplace_back(out.areas.back().name, ln[0]->offset);
            break;
        case SectionKind::Players:
            if (s != "STRING NUMBER") {
                return bad;
            }
            out.players.push_back({unquote(ln[0]->text), ln[1]->text, ln[0]->offset});
            names.emplace_back(out.players.back().name, ln[0]->offset);
            break;
        case SectionKind::Signals:
            if (s != "IDENT = IDENT" || is_reserved(ln[0]->text) ||
                (ln[2]->text != "TRUE" && ln[2]->text != "FALSE")) {
                return bad;
            }
            out.signals.push_back({ln[0]->text, ln[2]->text == "TRUE", ln[0]->offset});
            names.emplace_back(out.signals.back().name, ln[0]->offset);
            break;
        case SectionKind::Camera:
            if (s != "NUMBER , NUMBER" || camera_lines > 0) { // one line (P: 50 of 50)
                return bad;
            }
            out.camera = {ln[0]->text, ln[2]->text, ln[0]->offset};
            ++camera_lines;
            break;
        case SectionKind::Kernel:
        case SectionKind::Unknown:
            return error_at(ln[0]->offset, "no reader");
        }
    }
    if (sec.kind == SectionKind::Camera && camera_lines == 0) {
        return error_at(end_offset, "no position");
    }
    // Every name once (P: 50 of 50). Named as the reference does: the first name that occurs
    // twice, at its second occurrence.
    for (std::size_t i = 0; i < names.size(); ++i) {
        for (std::size_t j = i + 1; j < names.size(); ++j) {
            if (names[j].first == names[i].first) {
                return error_at(names[j].second, shown(names[i].first) + " defined twice");
            }
        }
    }
    return true;
}

template <class F> void for_each_block(const Script& s, F&& f) {
    for (const auto& it : s.kernel) {
        if (const auto* init = std::get_if<InitBlock>(&it)) {
            f(init->body);
        } else if (const auto* cond = std::get_if<If>(&it)) {
            f(cond->then_block);
            if (cond->has_else) {
                f(cond->else_block);
            }
        }
    }
}

} // namespace

std::string Error::text() const {
    return "line " + std::to_string(line) + ", offset " + std::to_string(offset) + ": " + message;
}

std::string_view kind_name(TokenKind kind) {
    switch (kind) {
    case TokenKind::Newline:
        return "NEWLINE";
    case TokenKind::Blank:
        return "WS";
    case TokenKind::Comment:
        return "COMMENT";
    case TokenKind::Section:
        return "SECTION";
    case TokenKind::String:
        return "STRING";
    case TokenKind::Number:
        return "NUMBER";
    case TokenKind::Name:
        return "IDENT";
    case TokenKind::Operator:
        return "OP";
    }
    return "?";
}

Result<std::vector<Token>, Error> lex(Bytes text) {
    std::vector<Token> out;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const auto [kind, len] = match_token(text, pos);
        if (len == 0) {
            return Error{pos, line_at(text, pos),
                         "unexpected " + shown(bytes_text(text, pos, pos + 1)) + " at offset " +
                             std::to_string(pos)};
        }
        out.push_back(Token{kind, pos, bytes_text(text, pos, pos + len)});
        pos += len;
    }
    return out;
}

std::string_view Value::contents() const {
    const std::string_view t(text);
    return kind == ValueKind::String && t.size() >= 2 ? t.substr(1, t.size() - 2) : t;
}

std::size_t offset_of(const Statement& s) {
    return std::visit([](const auto& x) { return x.offset; }, s);
}

bool has_semicolon(const Statement& s) {
    return std::visit([](const auto& x) { return x.semicolon; }, s);
}

std::span<const KnownSection> known_sections() {
    return kKnownSections;
}

std::span<const ObjectKind> object_kinds() {
    return kObjectKinds;
}

std::span<const std::string_view> subjects() {
    return kSubjects;
}

Result<Script, Error> parse(Bytes text, const Options& options) {
    auto lexed = lex(text);
    if (!lexed) {
        return lexed.error();
    }
    const std::vector<Token> toks = std::move(lexed).value();
    std::vector<std::size_t> heads;
    for (std::size_t i = 0; i < toks.size(); ++i) {
        if (toks[i].kind == TokenKind::Section) {
            heads.push_back(i);
        }
    }
    // The six, each once and in order; any other name is kept.
    std::size_t next = 0;
    for (std::size_t h : heads) {
        const auto k = std::find_if(kKnownSections.begin(), kKnownSections.end(),
                                    [&](const KnownSection& s) { return s.name == toks[h].text; });
        if (k == kKnownSections.end()) {
            continue;
        }
        if (next >= kKnownSections.size() || k->name != kKnownSections[next].name) {
            const std::string want = next < kKnownSections.size()
                                         ? std::string(kKnownSections[next].name)
                                         : std::string("no further known section");
            return Error{toks[h].offset, line_at(text, toks[h].offset),
                         "section " + toks[h].text + " where " + want + " belongs"};
        }
        ++next;
    }
    if (next < kKnownSections.size()) {
        return Error{text.size(), line_at(text, text.size()),
                     "section " + std::string(kKnownSections[next].name) + " missing"};
    }
    if (heads.front() != 0) {
        return Error{0, 1, "text before the first section"};
    }
    for (std::size_t h : heads) { // a header is a line of its own (P: 300 of 300)
        if ((h > 0 && toks[h - 1].kind != TokenKind::Newline) || h + 1 == toks.size() ||
            toks[h + 1].kind != TokenKind::Newline) {
            return Error{toks[h].offset, line_at(text, toks[h].offset),
                         toks[h].text + " at offset " + std::to_string(toks[h].offset) +
                             " is not a line of its own"};
        }
    }
    Script script;
    for (std::size_t k = 0; k < heads.size(); ++k) {
        const std::size_t first = heads[k];
        const std::size_t last = k + 1 < heads.size() ? heads[k + 1] : toks.size();
        Section sec;
        sec.name = toks[first].text;
        const auto known = std::find_if(kKnownSections.begin(), kKnownSections.end(),
                                        [&](const KnownSection& s) { return s.name == sec.name; });
        sec.kind = known == kKnownSections.end() ? SectionKind::Unknown : known->kind;
        if (sec.kind == SectionKind::Kernel) {
            auto items = parse_kernel(Cursor(text, toks, first + 1, last, sec.name), options);
            if (!items) {
                return items.error();
            }
            script.kernel = std::move(items).value();
        } else if (sec.kind != SectionKind::Unknown) {
            auto ok = parse_defines(text, toks, first + 1, last, *known, script);
            if (!ok) {
                return ok.error();
            }
        }
        sec.tokens.assign(toks.begin() + static_cast<std::ptrdiff_t>(first),
                          toks.begin() + static_cast<std::ptrdiff_t>(last));
        script.sections.push_back(std::move(sec));
    }
    return script;
}

ByteBuffer emit(const Script& script) {
    ByteBuffer out;
    for (const auto& sec : script.sections) {
        for (const auto& t : sec.tokens) {
            for (char c : t.text) {
                out.push_back(static_cast<std::uint8_t>(c));
            }
        }
    }
    return out;
}

Result<Bytes, std::string> find_payload(Bytes file) {
    auto parsed = chunk::parse_file(file);
    if (!parsed || parsed.value().chunks.empty() ||
        (parsed.value().chunks[0].name() != "ANNO" &&
         parsed.value().chunks[0].name() != "ANNOADDON1") ||
        !parsed.value().chunks[0].is_container) {
        return std::string("no ANNO or ANNOADDON1 container");
    }
    const chunk::Chunk& root = parsed.value().chunks[0];
    const std::vector<chunk::Chunk>* kids = &root.children;
    for (const auto& c : root.children) {
        if (c.name() == "SCENE" && c.is_container) {
            kids = &c.children;
            break;
        }
    }
    std::vector<Bytes> found;
    for (const auto& c : *kids) {
        if (c.name() == "BIGBROTHER") {
            found.push_back(c.payload);
        }
    }
    if (found.size() != 1) {
        return std::to_string(found.size()) + " BIGBROTHER chunks";
    }
    return found[0];
}

std::optional<std::int64_t> to_integer(std::string_view number) {
    const bool neg = !number.empty() && number[0] == '-';
    const std::string_view digits = neg ? number.substr(1) : number;
    if (digits.empty()) {
        return std::nullopt;
    }
    // Accumulate negatively so that the most negative value fits.
    std::int64_t v = 0;
    constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
    for (char ch : digits) {
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
        const std::int64_t d = ch - '0';
        if (v < (kMin + d) / 10) {
            return std::nullopt;
        }
        v = v * 10 - d;
    }
    if (neg) {
        return v;
    }
    if (v == kMin) {
        return std::nullopt;
    }
    return -v;
}

std::string canonical_integer(std::string_view number) {
    const bool neg = !number.empty() && number[0] == '-';
    std::string_view digits = neg ? number.substr(1) : number;
    while (digits.size() > 1 && digits[0] == '0') {
        digits.remove_prefix(1);
    }
    if (digits == "0") {
        return "0";
    }
    return (neg ? "-" : "") + std::string(digits);
}

Grouped grouped(const Condition& condition) {
    if (condition.terms.empty()) { // not from parse: an empty leaf
        return Grouped{};
    }
    std::vector<Grouped> items;
    items.reserve(condition.terms.size());
    for (const auto& t : condition.terms) {
        if (!t.group.empty()) {
            items.push_back(grouped(t.group[0]));
        } else {
            Grouped leaf;
            leaf.leaf = &t.primary;
            items.push_back(std::move(leaf));
        }
    }
    // Right to left: the last term, then each connective with the term before it.
    Grouped node = std::move(items.back());
    for (std::size_t i = condition.connectives.size(); i-- > 0;) {
        Grouped parent;
        parent.connective = condition.connectives[i];
        parent.operands.push_back(std::move(items[i]));
        parent.operands.push_back(std::move(node));
        node = std::move(parent);
    }
    return node;
}

bool is_mixed(const Condition& condition) {
    const auto& c = condition.connectives;
    if (!c.empty() && std::any_of(c.begin(), c.end(), [&](Connective x) { return x != c[0]; })) {
        return true;
    }
    return std::any_of(condition.terms.begin(), condition.terms.end(),
                       [](const Term& t) { return !t.group.empty() && is_mixed(t.group[0]); });
}

namespace {
std::string_view connective_text(Connective c) {
    return c == Connective::And ? "&&" : "||";
}
} // namespace

std::string describe(const Primary& p) {
    std::string s = p.subject.empty() ? p.name : p.subject + " \"" + p.name + "\" " + p.query;
    if (p.arg) {
        s += " \"" + *p.arg + "\"";
    }
    if (!p.op.empty() && p.value) {
        s += " " + p.op + " " + p.value->text;
    }
    return s;
}

std::string describe(const Condition& condition) {
    std::string s = "(";
    for (std::size_t i = 0; i < condition.terms.size(); ++i) {
        if (i > 0) {
            s += " ";
            s += connective_text(condition.connectives[i - 1]);
            s += " ";
        }
        const Term& t = condition.terms[i];
        s += t.group.empty() ? describe(t.primary) : describe(t.group[0]);
    }
    return s + ")";
}

std::string describe(const Grouped& g) {
    if (g.operands.size() != 2) {
        return g.leaf != nullptr ? describe(*g.leaf) : std::string();
    }
    return "(" + describe(g.operands[0]) + " " + std::string(connective_text(g.connective)) + " " +
           describe(g.operands[1]) + ")";
}

std::vector<ReadOn> read_on(const Script& script) {
    std::string text;
    for (const auto& sec : script.sections) {
        for (const auto& t : sec.tokens) {
            text += t.text;
        }
    }
    std::vector<ReadOn> out;
    const auto kernel =
        std::find_if(script.sections.begin(), script.sections.end(),
                     [](const Section& s) { return s.kind == SectionKind::Kernel; });
    if (kernel == script.sections.end()) {
        return out;
    }
    const Token& head = kernel->tokens.front();
    const std::size_t start = head.offset + head.text.size();
    // The kernel text ends at the next '[' (AnnoScene 0x1000b3f0, call at 0x10016f76).
    const std::size_t stop = std::min(text.find('[', start), text.size());
    for_each_block(script, [&](const std::vector<Statement>& body) {
        std::vector<const Statement*> gone;
        for (const Statement& st : body) {
            if (has_semicolon(st) || std::find(gone.begin(), gone.end(), &st) != gone.end()) {
                continue;
            }
            const std::size_t from = offset_of(st);
            const std::size_t semi = from < stop
                                         ? std::string_view(text).substr(0, stop).find(';', from)
                                         : std::string_view::npos;
            ReadOn r;
            r.statement = &st;
            if (semi != std::string_view::npos) {
                r.end = semi;
                for (const Statement& s : body) {
                    if (from < offset_of(s) && offset_of(s) < semi) {
                        r.lost.push_back(&s);
                    }
                }
                gone.insert(gone.end(), r.lost.begin(), r.lost.end());
            }
            out.push_back(std::move(r));
        }
    });
    return out;
}

} // namespace anno::formats::bigbrother
