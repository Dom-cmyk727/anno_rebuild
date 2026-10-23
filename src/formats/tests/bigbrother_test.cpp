// The scenario-script reader on our own synthetic scripts (fixtures/synthetic/
// bigbrother_synthetic/, written by tools/bigbrother_fixtures.py; spec:
// docs/formats/bigbrother.md §1-§3; reference: tools/bigbrother.py).
#include <algorithm>
#include <filesystem>
#include <map>
#include <set>
#include <string>

#include <doctest/doctest.h>

#include <anno/formats/bigbrother.hpp>
#include <anno/test/game_dir.hpp>

namespace bb = anno::formats::bigbrother;
using anno::formats::ByteBuffer;
using anno::formats::Bytes;

namespace {

std::filesystem::path fixture_dir() {
    return std::filesystem::path(ANNO_FIXTURES_DIR) / "bigbrother_synthetic";
}

ByteBuffer to_bytes(std::string_view s) {
    return ByteBuffer(s.begin(), s.end());
}

std::string to_string(Bytes b) {
    return std::string(b.begin(), b.end());
}

/// The script of a fixture file.
std::string script_of(const std::string& name) {
    const auto file = anno::test::read_file(fixture_dir() / name);
    auto payload = bb::find_payload(file);
    REQUIRE(payload);
    return to_string(payload.value());
}

std::vector<std::filesystem::path> accepted_fixtures() {
    std::vector<std::filesystem::path> out;
    for (const auto& e : std::filesystem::directory_iterator(fixture_dir())) {
        if (e.is_regular_file() && e.path().filename().string().rfind("bad_", 0) != 0) {
            out.push_back(e.path());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

anno::formats::Result<bb::Script, bb::Error> parse(std::string_view text) {
    const ByteBuffer b = to_bytes(text);
    return bb::parse(b);
}

/// `text` with its one occurrence of `old` replaced.
std::string replaced(std::string text, std::string_view old, std::string_view now) {
    const auto at = text.find(old);
    REQUIRE(at != std::string::npos);
    REQUIRE(text.find(old, at + 1) == std::string::npos);
    return text.replace(at, old.size(), now);
}

std::string chunk(std::string_view name, std::string_view data) {
    std::string out(name);
    out.resize(12, '\0');
    const auto n = static_cast<std::uint32_t>(data.size());
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<char>((n >> shift) & 0xFF));
    }
    return out + std::string(data);
}

/// The grammar script with its first IF's condition replaced.
constexpr std::string_view kFirstCondition =
    R"(STARTED == TRUE && DONE == FALSE && PLAYER "ME" HOUSES "TOWN" > 49)";

bb::Condition condition_of(std::string_view text) {
    auto s = parse(replaced(script_of("grammar.sze"), kFirstCondition, text));
    INFO(text);
    REQUIRE(s);
    const auto* item = std::get_if<bb::If>(&s.value().kernel.at(3));
    REQUIRE(item != nullptr);
    return item->condition;
}

/// The atoms of a condition (its primaries as described), each a bit of `bits`.
using Truth = std::map<std::string, bool>;

void atoms(const bb::Condition& c, std::set<std::string>& out) {
    for (const auto& t : c.terms) {
        if (t.group.empty()) {
            out.insert(bb::describe(t.primary));
        } else {
            atoms(t.group[0], out);
        }
    }
}

/// The interpreter's value of a grouped condition (the values are pure here, so the skip of
/// `&&` does not show).
bool value_of(const bb::Grouped& g, const Truth& v) {
    if (g.operands.empty()) {
        return v.at(bb::describe(*g.leaf));
    }
    const bool a = value_of(g.operands[0], v);
    const bool b = value_of(g.operands[1], v);
    return g.connective == bb::Connective::And ? (a && b) : (a || b);
}

/// C's reading: `&&` binds tighter than `||`.
bool c_value(const bb::Condition& c, const Truth& v) {
    bool any = false;
    bool run = true;
    for (std::size_t i = 0; i < c.terms.size(); ++i) {
        const auto& t = c.terms[i];
        const bool x = t.group.empty() ? v.at(bb::describe(t.primary)) : c_value(t.group[0], v);
        run = run && x;
        if (i == c.connectives.size() || c.connectives[i] == bb::Connective::Or) {
            any = any || run;
            run = true;
        }
    }
    return any;
}

/// Whether the interpreter's grouping and C's reading differ for some truth values.
bool differs_from_c(const bb::Condition& c) {
    std::set<std::string> names;
    atoms(c, names);
    const std::vector<std::string> list(names.begin(), names.end());
    REQUIRE(list.size() < 24); // a truth table of at most 2^23 rows
    const bb::Grouped g = bb::grouped(c);
    for (std::size_t bits = 0; bits < (std::size_t{1} << list.size()); ++bits) {
        Truth v;
        for (std::size_t i = 0; i < list.size(); ++i) {
            v[list[i]] = ((bits >> i) & 1U) != 0;
        }
        if (value_of(g, v) != c_value(c, v)) {
            return true;
        }
    }
    return false;
}

std::string name_of(const bb::Statement* s) {
    return std::visit([](const auto& x) { return x.name; }, *s);
}

} // namespace

TEST_CASE("bigbrother: every synthetic script parses and re-emits byte-identically") {
    const auto files = accepted_fixtures();
    REQUIRE(files.size() == 8);
    for (const auto& f : files) {
        INFO(f.filename().string());
        const auto file = anno::test::read_file(f);
        auto payload = bb::find_payload(file);
        REQUIRE(payload);
        auto s = bb::parse(payload.value());
        if (!s) {
            INFO(s.error().text());
            REQUIRE(s);
        }
        const ByteBuffer again = bb::emit(s.value());
        CHECK(
            std::equal(again.begin(), again.end(), payload.value().begin(), payload.value().end()));
    }
}

TEST_CASE("bigbrother: every byte is one token, every token kind occurs") {
    const std::string text = script_of("grammar.sze");
    auto toks = bb::lex(to_bytes(text));
    REQUIRE(toks);
    std::string joined;
    std::set<bb::TokenKind> kinds;
    std::set<std::string> newlines;
    for (const auto& t : toks.value()) {
        CHECK(t.offset == joined.size());
        joined += t.text;
        kinds.insert(t.kind);
        if (t.kind == bb::TokenKind::Newline) {
            newlines.insert(t.text);
        }
    }
    CHECK(joined == text);
    CHECK(kinds.size() == 8);
    CHECK(newlines == std::set<std::string>{"\r\n", "\n"});
    // Touching tokens: AREA"COVE"REACHED_BY"RIVAL\xc4"||OBJECT
    const auto at = text.find("AREA\"COVE\"");
    REQUIRE(at != std::string::npos);
    const auto first = std::find_if(toks.value().begin(), toks.value().end(),
                                    [&](const bb::Token& t) { return t.offset == at; });
    REQUIRE(first != toks.value().end());
    std::vector<std::string> run;
    for (auto it = first; it != first + 6; ++it) {
        run.push_back(it->text);
    }
    CHECK(run == std::vector<std::string>{"AREA", "\"COVE\"", "REACHED_BY", "\"RIVAL\xc4\"", "||",
                                          "OBJECT"});
}

TEST_CASE("bigbrother: CRLF and LF are kept as read") {
    const std::string mixed = script_of("grammar.sze");
    const std::string lf = script_of("lf.sze");
    const std::string crlf = script_of("crlf.sze");
    CHECK(mixed.find("\r\n") != std::string::npos);
    CHECK(lf.find('\r') == std::string::npos);
    for (const std::string* text : {&mixed, &lf, &crlf}) {
        auto s = parse(*text);
        REQUIRE(s);
        CHECK(to_string(bb::emit(s.value())) == *text);
    }
    // The three differ only in their line ends.
    CHECK(mixed != lf);
    CHECK(lf != crlf);
}

TEST_CASE("bigbrother: Latin-1 bytes round trip") {
    std::string high;
    for (int c = 0x80; c <= 0xFF; ++c) {
        high.push_back(static_cast<char>(c));
    }
    std::string text = script_of("grammar.sze");
    text = replaced(text, "\"INTRO_1\"", "\"" + high + "\"");
    text = replaced(text, "$ an assignment in a block", "$" + high);
    auto s = parse(text);
    REQUIRE(s);
    CHECK(to_string(bb::emit(s.value())) == text);
}

TEST_CASE("bigbrother: the tree of the grammar script") {
    auto parsed = parse(script_of("grammar.sze"));
    REQUIRE(parsed);
    const bb::Script& s = parsed.value();
    REQUIRE(s.sections.size() == 6);
    for (std::size_t i = 0; i < 6; ++i) {
        CHECK(s.sections[i].name == bb::known_sections()[i].name);
        CHECK(s.sections[i].kind == bb::known_sections()[i].kind);
    }
    REQUIRE(s.objects.size() == 3);
    CHECK(s.objects[0].kind == "OBJECT_FIGUR");
    CHECK(s.objects[0].name == "SCOUT");
    CHECK(s.objects[0].numbers == std::vector<std::string>{"4"});
    CHECK(s.objects[2].kind == "OBJECT_ANLAGE");
    CHECK(s.objects[2].numbers == std::vector<std::string>{"12", "34", "5678"});
    REQUIRE(s.areas.size() == 2);
    CHECK(s.areas[1].name == "RIDGE");
    CHECK(s.areas[1].corners == std::vector<std::string>{"500", "60", "70", "800"});
    REQUIRE(s.players.size() == 3);
    CHECK(s.players[1].name == "RIVAL\xc4");
    CHECK(s.players[2].slot == "31");
    REQUIRE(s.signals.size() == 3);
    CHECK(s.signals[0].name == "SCHUETTIINITSIG");
    CHECK(s.signals[0].value);
    CHECK_FALSE(s.signals[1].value);
    CHECK(s.camera.x == "0");
    CHECK(s.camera.y == "0");

    REQUIRE(s.kernel.size() == 6);
    const auto& init = std::get<bb::InitBlock>(s.kernel[0]);
    REQUIRE(init.body.size() == 2);
    const auto& show = std::get<bb::Call>(init.body[0]);
    CHECK(show.name == "NOTE_SHOW");
    REQUIRE(show.args.size() == 3);
    CHECK(show.args[0].kind == bb::ValueKind::String);
    CHECK(show.args[0].contents() == "ME");
    CHECK(show.args[2].kind == bb::ValueKind::Bool);
    const auto& started = std::get<bb::Assign>(init.body[1]);
    CHECK((started.name == "STARTED" && !started.is_set && started.value.text == "TRUE"));
    const auto& set = std::get<bb::Assign>(s.kernel[1]);
    CHECK((set.is_set && set.semicolon && set.value.text == "FALSE"));

    const auto& first = std::get<bb::If>(s.kernel[3]);
    CHECK_FALSE(first.has_else);
    REQUIRE(first.then_block.size() == 3);
    const auto& pay = std::get<bb::Call>(first.then_block[1]);
    CHECK(pay.args[1].kind == bb::ValueKind::Number);
    CHECK(pay.args[1].text == "-250");
    const auto& refresh = std::get<bb::Call>(first.then_block[2]);
    CHECK((refresh.name == "REFRESH" && !refresh.parens && refresh.semicolon &&
           refresh.args.empty()));
    REQUIRE(first.condition.terms.size() == 3);
    const bb::Primary& houses = first.condition.terms[2].primary;
    CHECK(houses.subject == "PLAYER");
    CHECK(houses.name == "ME");
    CHECK(houses.query == "HOUSES");
    CHECK(houses.arg == std::optional<std::string>("TOWN"));
    CHECK(houses.op == ">");
    CHECK(houses.value->text == "49");
    const bb::Primary& var = first.condition.terms[0].primary;
    CHECK((var.subject.empty() && var.name == "STARTED" && var.op == "==" &&
           var.value->text == "TRUE"));

    const auto& second = std::get<bb::If>(s.kernel[4]);
    CHECK(second.has_else);
    REQUIRE(second.else_block.size() == 1);
    CHECK(bb::describe(second.condition) ==
          "(AREA \"COVE\" REACHED_BY \"RIVAL\xc4\" || OBJECT \"SCOUT\" GONE)");
    const auto& third = std::get<bb::If>(s.kernel[5]);
    REQUIRE(third.condition.terms.size() == 2);
    REQUIRE(third.condition.terms[0].group.size() == 1);
    CHECK(bb::describe(third.condition) ==
          "((TIMER_2 < 30 || TIMER_2 > 90) && OBJECT \"TOWER\" OWNER == \"ME\")");
}

TEST_CASE("bigbrother: a missing semicolon at the end of a line is kept") {
    auto s = parse(replaced(script_of("grammar.sze"), "SET DONE = FALSE;", "SET DONE = FALSE"));
    REQUIRE(s);
    CHECK_FALSE(std::get<bb::Assign>(s.value().kernel[2]).semicolon);
}

TEST_CASE("bigbrother: an unknown section is kept, not refused") {
    const std::string at_end = script_of("startup.sze");
    const std::string grammar = script_of("grammar.sze");
    const std::string between =
        replaced(grammar, "[SCRIPT_KERNEL]\n", "[STARTUP]\nSPEED = 2\r\n[SCRIPT_KERNEL]\n");
    const std::string first = "[STARTUP]\n\n" + grammar;
    for (const std::string* text : {&at_end, &between, &first}) {
        auto s = parse(*text);
        REQUIRE(s);
        CHECK(s.value().sections.size() == 7);
        const auto unknown = std::count_if(
            s.value().sections.begin(), s.value().sections.end(), [](const bb::Section& x) {
                return x.kind == bb::SectionKind::Unknown && x.name == "[STARTUP]";
            });
        CHECK(unknown == 1);
        CHECK(to_string(bb::emit(s.value())) == *text);
        CHECK(s.value().kernel.size() == 6);
    }
}

TEST_CASE("bigbrother: known sections out of order, missing or twice are refused") {
    const std::string text = script_of("grammar.sze");
    const auto signals = text.find("[SIGNALS]");
    const auto camera = text.find("[CAMARAPOS]");
    const std::string head = text.substr(0, signals);
    const std::string sig = text.substr(signals, camera - signals);
    const std::string cam = text.substr(camera);
    REQUIRE(parse(head + sig + cam));
    struct Case {
        std::string text;
        std::size_t line;
    };
    const std::vector<Case> cases{
        {head + cam + sig, 40},       // swapped: [CAMARAPOS] where [SIGNALS] belongs
        {head + sig, 45},             // [CAMARAPOS] missing: at the end
        {head + sig + cam + cam, 47}, // [CAMARAPOS] twice: at the second
        {replaced(text, "[AREA_DEFINE]", "[OBJECT_DEFINE]"), 5},
    };
    for (const auto& c : cases) {
        auto s = parse(c.text);
        REQUIRE_FALSE(s);
        INFO(s.error().text());
        CHECK(s.error().line == c.line);
    }
}

TEST_CASE("bigbrother: text outside the grammar is refused, naming its line") {
    struct Case {
        std::string_view old_text, new_text;
        std::size_t line;
    };
    const std::vector<Case> cases{
        {"SET STARTED = FALSE;", "SET STARTED = = FALSE;", 19},            // two operators
        {"PAY( \"ME\", -250 );", "PAY( \"ME\", -250 ;", 24},               // unclosed call
        {"PAY( \"ME\", -250 );", "PAY( \"ME\", -250, );", 24},             // trailing ','
        {"> 90)", "> 90", 35},                                             // unclosed group
        {"> 90)", "> +90)", 35},                                           // no '+' sign
        {"[SIGNALS]\n", "[SIGNALS]\nGO TRUE\r\n", 41},                     // signal without '='
        {"STARTED = FALSE\r\n", "STARTED = MAYBE\r\n", 42},                // signal value
        {"DONE = FALSE\r\n\n", "DONE = FALSE\r\nDONE = TRUE\r\n\n", 44},   // signal twice
        {"SCHUETTIINITSIG = TRUE", "IF = TRUE", 41},                       // keyword as a signal
        {" REFRESH;", " REFRESH; @", 25},                                  // a byte no token takes
        {" REFRESH;", " REFRESH;\r", 25},                                  // a CR alone
        {" REFRESH;", " REFRESH;;;", 25},                                  // empty statements
        {"SET DONE = FALSE;\r\n", "FOO\r\nSET DONE = FALSE;\r\n", 20},     // a stray name
        {" REFRESH;", " ELSE;", 25},                                       // keyword as a call
        {" REFRESH;", " IF DONE THEN { REFRESH; }", 25},                   // nested IF
        {" REFRESH;\r\n}", " REFRESH;\r\n FOO\r\n}", 27},                  // bare name in a block
        {"TIMER_RESET( 2, 0 );\r\n}\r\n", "TIMER_RESET( 2, 0 );\r\n", 39}, // block not closed
        {" DONE = TRUE;", " DONE = TRUE TRUE;", 23},                       // value then a value
        {" DONE = TRUE;", " DONE = TRUE REFRESH;", 23},                    // missing ';' mid-line
        {" DONE = TRUE;", " DONE = MAYBE;", 23},                           // a name as a value
        {"SET DONE = FALSE;", "SET IF = FALSE;", 20},                      // keyword as a variable
        {"SET DONE = FALSE;\r\n", "SET DONE = FALSE;\r\nFOO;\r\n", 21},    // unknown statement
        {"\"INTRO_1\"", "\"INTRO\r\n_1\"", 16},                            // string over two lines
        {kFirstCondition, "THEN", 21},                                     // keyword as a condition
        {kFirstCondition, "TRUE == FALSE", 21},                            // value as a condition
        {"OBJECT \"SCOUT\" GONE", "OBJECT \"SCOUT\" THEN", 27},            // keyword as a query
        {"HOUSES \"TOWN\" > 49", "HOUSES \"TOWN\" \"ALL\" > 49", 21},      // two query strings
        {"> 49", "!= 49", 21},                                             // unused comparisons
        {"> 49", "<= 49", 21},
        {"> 49", ">= 49", 21},
        {"> 49", "= 49", 21},
        {"DONE == FALSE &&", "DONE == FALSE ,", 21},                         // ',' as a connective
        {"> 49 THEN", "> 49", 22},                                           // no THEN
        {"ELSE\r\n{", "ELSE\r\nELSE\r\n{", 32},                              // ELSE twice
        {"[OBJECT_DEFINE]\nOBJECT_FIGUR", "[OBJECT_DEFINE]OBJECT_FIGUR", 1}, // header glued
        {"31\n[SCRIPT_KERNEL]", "31 [SCRIPT_KERNEL]", 11},                   // header mid-line
        {"OBJECT_FIGUR \"SCOUT\" 4", "OBJECT_FIGUR \"SCOUT\" 4,5,6", 2},     // numbers per kind
        {"12,34,5678", "12", 4},
        {"OBJECT_FIGUR \"SCOUT\"", "OBJECT_SCHIFF \"SCOUT\"", 2},    // unknown kind
        {"10, 20, 300, 400\n", "10, 20, 300, 400 $ c\n", 6},         // comment in a define
        {"\"RIDGE\" 500", "\"COVE\" 500", 7},                        // area twice
        {"\"RIDGE\" 500,60,70,800", "\"RIDGE\" 500,60", 7},          // area numbers
        {"\"ME\" 0\n", "\"ME\" 0, 1\n", 9},                          // one slot per name
        {"\"NOBODY\" 31", "\"ME\" 31", 11},                          // player twice
        {"[CAMARAPOS]\n0,0\r\n", "[CAMARAPOS]\n", 46},               // no position
        {"[CAMARAPOS]\n0,0\r\n", "[CAMARAPOS]\n0,0\r\n1,1\r\n", 47}, // two positions
    };
    const std::string text = script_of("grammar.sze");
    for (const auto& c : cases) {
        auto s = parse(replaced(text, c.old_text, c.new_text));
        INFO(c.new_text);
        REQUIRE_FALSE(s);
        INFO(s.error().text());
        CHECK(s.error().line == c.line);
        CHECK(s.error().text().rfind("line " + std::to_string(c.line) + ", offset ", 0) == 0);
        CHECK_FALSE(s.error().message.empty());
    }
}

TEST_CASE("bigbrother: the error offset is the offending byte") {
    const std::string text = replaced(script_of("grammar.sze"), " REFRESH;", " REFRESH; @");
    auto s = parse(text);
    REQUIRE_FALSE(s);
    CHECK(s.error().offset == text.find('@'));
}

TEST_CASE("bigbrother: text before the first section is refused") {
    for (std::string_view lead : {"X\n", "\n", " "}) {
        auto s = parse(std::string(lead) + script_of("grammar.sze"));
        INFO(lead);
        REQUIRE_FALSE(s);
        CHECK(s.error().line == 1);
    }
}

TEST_CASE("bigbrother: the payload is the one BIGBROTHER chunk") {
    const std::string script = script_of("grammar.sze");
    const std::string big = chunk("BIGBROTHER", script);
    struct Case {
        std::string file;
        bool ok;
    };
    const std::vector<Case> cases{
        {chunk("ANNOADDON1", chunk("SCENE", big)), true},
        {chunk("ANNO", chunk("SCENE", chunk("NOTES", "x") + big)), true},
        {chunk("ANNOADDON1", big), true}, // a savegame
        {chunk("ANNOADDON1", chunk("SCENE", big + big)), false},
        {chunk("ANNOADDON1", chunk("SCENE", chunk("OTHER", std::string(16, 'x')))), false},
        {chunk("OTHERROOT", chunk("SCENE", big)), false},
        {"not a chunk file", false},
    };
    for (const auto& c : cases) {
        const ByteBuffer file = to_bytes(c.file);
        auto p = bb::find_payload(file);
        CHECK(p.ok() == c.ok);
        if (p && c.ok) {
            CHECK(to_string(p.value()) == script);
        }
    }
}

TEST_CASE("bigbrother: the define numbers as integers") {
    CHECK(bb::to_integer("0") == std::optional<std::int64_t>(0));
    CHECK(bb::to_integer("-128") == std::optional<std::int64_t>(-128));
    CHECK(bb::to_integer("9223372036854775807") ==
          std::optional<std::int64_t>(9223372036854775807));
    CHECK(bb::to_integer("-9223372036854775808").has_value());
    CHECK_FALSE(bb::to_integer("9223372036854775808").has_value());
    CHECK_FALSE(bb::to_integer("-").has_value());
    CHECK_FALSE(bb::to_integer("1a").has_value());
    CHECK(bb::canonical_integer("007") == "7");
    CHECK(bb::canonical_integer("-0") == "0");
    CHECK(bb::canonical_integer("-00") == "0");
    CHECK(bb::canonical_integer("-012") == "-12");
    CHECK(bb::canonical_integer("123456789012345678901234567890") ==
          "123456789012345678901234567890");
}

TEST_CASE("bigbrother: connectives have equal rank and group to the right") {
    struct Case {
        std::string_view text, grouped;
    };
    const std::vector<Case> cases{
        {"A && B || C", "(A && (B || C))"},
        {"A || B && C", "(A || (B && C))"},
        {"A && B && C || D", "(A && (B && (C || D)))"},
        {"A || B || C", "(A || (B || C))"},
        {"(A || B) && C", "((A || B) && C)"},
        {"A && (B || C) && D", "(A && ((B || C) && D))"},
        {"(A)", "A"},
        {"((A))", "A"},
        {"A", "A"},
        {"A == FALSE && (B && C) || PLAYER \"P\" Q \"S\" > 9",
         "(A == FALSE && ((B && C) || PLAYER \"P\" Q \"S\" > 9))"},
    };
    for (const auto& c : cases) {
        INFO(c.text);
        CHECK(bb::describe(bb::grouped(condition_of(c.text))) == c.grouped);
    }
}

TEST_CASE("bigbrother: the grouping differs from C precedence where the connectives mix") {
    // A guard before alternatives covers all of them; C's reading would fire without it.
    const auto g = condition_of("A && B || C");
    const Truth fires_in_c{{"A", false}, {"B", false}, {"C", true}};
    CHECK(c_value(g, fires_in_c));
    CHECK_FALSE(value_of(bb::grouped(g), fires_in_c));
    for (std::string_view text :
         {"A && B || C", "A && B && C || D", "A == FALSE && (B && C) || D", "(A && B || C) && D"}) {
        INFO(text);
        CHECK(bb::is_mixed(condition_of(text)));
        CHECK(differs_from_c(condition_of(text)));
    }
    // Where they do not mix, both readings are the same function.
    for (std::string_view text : {"A && B && C", "A || B || C", "A && (B || C)", "(A || B) && C"}) {
        INFO(text);
        CHECK_FALSE(bb::is_mixed(condition_of(text)));
        CHECK_FALSE(differs_from_c(condition_of(text)));
    }
    // A || B && C means the same in both readings (C's && binds tighter, the interpreter groups
    // to the right), but it still mixes.
    CHECK(bb::is_mixed(condition_of("A || B && C")));
    CHECK_FALSE(differs_from_c(condition_of("A || B && C")));
}

TEST_CASE("bigbrother: the mixed fixture as the interpreter groups it") {
    auto s = parse(script_of("mixed.sze"));
    REQUIRE(s);
    std::vector<std::string> mixed;
    for (const auto& it : s.value().kernel) {
        if (const auto* c = std::get_if<bb::If>(&it); c != nullptr && bb::is_mixed(c->condition)) {
            mixed.push_back(bb::describe(bb::grouped(c->condition)));
        }
    }
    CHECK(mixed == std::vector<std::string>{
                       "(GUARD && (A || B))",
                       "(GUARD && (A && (B || (C || D))))",
                       "(A || (B && C))",
                       "(GUARD == FALSE && ((A && (B && C)) || PLAYER \"ME\" HOUSES \"ALL\" > 9))",
                       "((A || (B && C)) && D)",
                   });
}

TEST_CASE("bigbrother: the four kinds of statement without semicolon") {
    const std::string text = script_of("read_on.sze");
    auto parsed = parse(text);
    REQUIRE(parsed);
    const auto reads = bb::read_on(parsed.value());
    REQUIRE(reads.size() == 4);
    // The INITBLOCK's last statement reads on past its '}' to the next top-level SET's ';':
    // nothing of the block is lost.
    CHECK(name_of(reads[0].statement) == "LAST_ONE");
    const auto set_semi =
        text.find("SET FLAG_A = TRUE;") + std::string_view("SET FLAG_A = TRUE").size();
    CHECK(reads[0].end == std::optional<std::size_t>(set_semi));
    CHECK(*reads[0].end > text.find("}\r\nSET FLAG_A"));
    CHECK(reads[0].lost.empty());
    // A block statement reads on to the ';' of a commented-out line: nothing lost.
    CHECK(name_of(reads[1].statement) == "SLOW");
    CHECK(reads[1].end == std::optional<std::size_t>(text.find("( 4 );") + 5));
    CHECK(reads[1].lost.empty());
    // A block statement swallows the next statement of its block: that one is lost.
    CHECK(name_of(reads[2].statement) == "TRADE");
    CHECK(reads[2].end == std::optional<std::size_t>(text.find("BEEP( 7, 8 );") + 12));
    REQUIRE(reads[2].lost.size() == 1);
    CHECK(name_of(reads[2].lost[0]) == "BEEP");
    // An assignment does the same.
    CHECK(name_of(reads[3].statement) == "FLAG_A");
    REQUIRE(reads[3].lost.size() == 1);
    CHECK(name_of(reads[3].lost[0]) == "FLAG_B");
    // A top-level SET without ';' loses nothing and is not listed.
    const auto& set_b = std::get<bb::Assign>(parsed.value().kernel[2]);
    CHECK((set_b.name == "FLAG_B" && !set_b.semicolon));
    for (const auto& r : reads) {
        CHECK(r.statement != nullptr);
        CHECK(name_of(r.statement) != "FLAG_B");
    }
}

TEST_CASE("bigbrother: no semicolon after it makes the interpreter refuse the kernel") {
    auto s = parse(script_of("refuse.sze"));
    REQUIRE(s);
    const auto reads = bb::read_on(s.value());
    REQUIRE(reads.size() == 1);
    CHECK(name_of(reads[0].statement) == "THE_END");
    CHECK_FALSE(reads[0].end.has_value());
    CHECK(reads[0].lost.empty());
}

TEST_CASE("bigbrother: two statements without semicolon in a row, and the kernel's end") {
    const std::string block = " SLOW( \"ME\", \"PACE\", 3 )\r\n $OLD_CALL( 4 );\r\n NEXT( 5 );\r\n";
    const std::string text = script_of("read_on.sze");
    {
        // The second is swallowed by the first, and the third with it.
        auto s = parse(replaced(text, block, " SLOW( 1 )\r\n NEXT( 5 )\r\n MORE( 6 );\r\n"));
        REQUIRE(s);
        const auto reads = bb::read_on(s.value());
        REQUIRE(reads.size() == 4);
        CHECK(name_of(reads[1].statement) == "SLOW");
        REQUIRE(reads[1].lost.size() == 2);
        CHECK(name_of(reads[1].lost[0]) == "NEXT");
        CHECK(name_of(reads[1].lost[1]) == "MORE");
    }
    {
        // The kernel text ends at the next '[' (here an unknown section's): a ';' after it is
        // not found.
        const std::string refuse = script_of("refuse.sze");
        auto s = parse(replaced(refuse, "[SIGNALS]", "[STARTUP]\nLATER;\r\n[SIGNALS]"));
        REQUIRE(s);
        const auto reads = bb::read_on(s.value());
        REQUIRE(reads.size() == 1);
        CHECK_FALSE(reads[0].end.has_value());
        // ... also when the '[' stands in a comment of the kernel.
        const std::string_view stmt = " THE_END( \"ME\" )\r\n";
        auto c = parse(replaced(refuse, stmt, std::string(stmt) + " $ see [x];\r\n"));
        REQUIRE(c);
        const auto in_comment = bb::read_on(c.value());
        REQUIRE(in_comment.size() == 1);
        CHECK_FALSE(in_comment[0].end.has_value());
        // Without the '[', that ';' ends it.
        auto d = parse(replaced(refuse, stmt, std::string(stmt) + " $ see x;\r\n"));
        REQUIRE(d);
        const auto plain = bb::read_on(d.value());
        REQUIRE(plain.size() == 1);
        CHECK(plain[0].end.has_value());
    }
}

TEST_CASE("bigbrother: a condition nested deeper than the limit is refused, not a crash") {
    const std::string text = script_of("grammar.sze");
    const auto chain = [](std::size_t connectives, std::string_view op) {
        std::string s = "A0";
        for (std::size_t i = 1; i <= connectives; ++i) {
            s += std::string(" ") + std::string(op) + " A" + std::to_string(i);
        }
        return s;
    };
    const auto nest = [](std::size_t groups) {
        return std::string(groups, '(') + "A" + std::string(groups, ')');
    };
    const auto parses = [&](const std::string& cond, const bb::Options& options = {}) {
        const ByteBuffer b = to_bytes(replaced(text, kFirstCondition, cond));
        return bb::parse(b, options).ok();
    };
    CHECK(bb::Options{}.max_condition_depth == bb::kMaxConditionDepth);
    // Along a path: the connectives plus the parentheses.
    CHECK(parses(chain(bb::kMaxConditionDepth, "&&")));
    CHECK_FALSE(parses(chain(bb::kMaxConditionDepth + 1, "&&")));
    CHECK(parses(nest(bb::kMaxConditionDepth)));
    CHECK_FALSE(parses(nest(bb::kMaxConditionDepth + 1)));
    CHECK(parses("A && (" + nest(bb::kMaxConditionDepth - 2) + ")"));
    CHECK_FALSE(parses("A && (" + nest(bb::kMaxConditionDepth - 1) + ")"));
    // Far beyond it: an Error, no stack overflow.
    CHECK_FALSE(parses(nest(100000)));
    CHECK_FALSE(parses(chain(3000, "&&") + " || Z"));
    // The limit is an option.
    CHECK(parses(nest(400), bb::Options{400}));
    CHECK(parses(chain(400, "||"), bb::Options{400}));
    const ByteBuffer deep = to_bytes(replaced(text, kFirstCondition, nest(300)));
    auto e = bb::parse(deep);
    REQUIRE_FALSE(e);
    CHECK(e.error().line == 21);
    CHECK(e.error().offset == text.find(kFirstCondition) + 256);
}

TEST_CASE("bigbrother: the reader keeps no vocabulary") {
    // Any name is a call, a variable or a query: the names of §4-§6 are not compiled in.
    auto s = parse(replaced(script_of("grammar.sze"), " REFRESH;",
                            " ANY_NAME_AT_ALL( 1, \"x\", FALSE );\r\n Z9;"));
    REQUIRE(s);
    const auto& body = std::get<bb::If>(s.value().kernel[3]).then_block;
    REQUIRE(body.size() == 4);
    CHECK(std::get<bb::Call>(body[2]).name == "ANY_NAME_AT_ALL");
    CHECK(std::get<bb::Call>(body[3]).name == "Z9");
    CHECK(bb::known_sections().size() == 6);
    CHECK(bb::object_kinds().size() == 3);
    CHECK(bb::subjects().size() == 3);
}
