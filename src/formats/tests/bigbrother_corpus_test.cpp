// The scenario-script reader over the BIGBROTHER chunk of all 50 shipped .sze (needs
// ANNO1503_GAME_DIR). Figures: docs/formats/bigbrother.md §3.4. The output names files and
// counts only, never the scripts' text.
#include <algorithm>
#include <map>
#include <set>
#include <string>

#include <doctest/doctest.h>

#include <anno/formats/bigbrother.hpp>
#include <anno/test/game_dir.hpp>

namespace bb = anno::formats::bigbrother;

namespace {

struct Loaded {
    std::string file;
    bb::Script script;
};

/// Every shipped scenario's script, parsed; each must re-emit byte-identically.
std::vector<Loaded> load_all(const std::filesystem::path& game, std::size_t& identical) {
    std::vector<Loaded> out;
    identical = 0;
    for (const auto& f : anno::test::files_with_ext(game / "Scenes", "sze")) {
        const std::string name = f.filename().string();
        INFO(name);
        const auto buf = anno::test::read_file(f);
        auto payload = bb::find_payload(buf);
        REQUIRE(payload.ok());
        auto s = bb::parse(payload.value());
        if (!s) {
            // Offset and line only: the message would quote the script.
            INFO("refused at line " << s.error().line << ", offset " << s.error().offset);
            REQUIRE(s.ok());
        }
        const auto again = bb::emit(s.value());
        const bool same =
            std::equal(again.begin(), again.end(), payload.value().begin(), payload.value().end());
        CHECK(same);
        identical += same ? 1 : 0;
        out.push_back({name, std::move(s).value()});
    }
    return out;
}

/// The interpreter's value and C's reading (`&&` binds tighter), over the condition's atoms.
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

bool value_of(const bb::Grouped& g, const Truth& v) {
    if (g.operands.empty()) {
        return v.at(bb::describe(*g.leaf));
    }
    const bool a = value_of(g.operands[0], v);
    const bool b = value_of(g.operands[1], v);
    return g.connective == bb::Connective::And ? (a && b) : (a || b);
}

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

bool differs_from_c(const bb::Condition& c) {
    std::set<std::string> names;
    atoms(c, names);
    const std::vector<std::string> list(names.begin(), names.end());
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

void count_statements(const std::vector<bb::Statement>& body, std::map<std::string, int>& n,
                      std::set<std::string>& variables) {
    for (const auto& st : body) {
        if (const auto* a = std::get_if<bb::Assign>(&st)) {
            ++n["assign in a block"];
            variables.insert(a->name);
        } else {
            const auto& c = std::get<bb::Call>(st);
            ++n["call"];
            n["no ()"] += c.parens ? 0 : 1;
        }
        n["no ;"] += bb::has_semicolon(st) ? 0 : 1;
    }
}

bool is_timer(const std::string& name) {
    return name.size() > 6 && name.rfind("TIMER_", 0) == 0 &&
           std::all_of(name.begin() + 6, name.end(), [](char c) { return c >= '0' && c <= '9'; });
}

void count_primaries(const bb::Condition& c, std::map<std::string, int>& n) {
    for (const auto& t : c.terms) {
        if (!t.group.empty()) {
            count_primaries(t.group[0], n);
        } else if (t.primary.subject.empty()) {
            ++n[is_timer(t.primary.name) ? "timer" : "variable"];
        }
    }
}

} // namespace

TEST_CASE("bigbrother corpus: 50 of 50 parse and re-emit byte-identically, the counts of 3.4") {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("ANNO1503_GAME_DIR not set: skipped");
        return;
    }
    std::size_t identical = 0;
    const auto all = load_all(*game, identical);
    CHECK(all.size() == 50);
    CHECK(identical == 50);

    std::map<std::string, int> n;
    std::set<std::string> variables;
    std::size_t with_statements = 0;
    for (const auto& [file, s] : all) {
        INFO(file);
        // [SIGNALS] is SCHUETTIINITSIG = TRUE, then the SET variables in order (§3.5).
        std::vector<std::pair<std::string, bool>> want{{"SCHUETTIINITSIG", true}};
        for (const auto& it : s.kernel) {
            if (const auto* a = std::get_if<bb::Assign>(&it)) {
                want.emplace_back(a->name, a->value.text == "TRUE");
            }
        }
        std::vector<std::pair<std::string, bool>> got;
        for (const auto& g : s.signals) {
            got.emplace_back(g.name, g.value);
        }
        const bool signals_are_sets = got == want;
        CHECK(signals_are_sets);
        // Six known sections in order, no other.
        CHECK(s.sections.size() == 6);
        for (const auto& sec : s.sections) {
            n["comments"] += static_cast<int>(
                std::count_if(sec.tokens.begin(), sec.tokens.end(),
                              [](const bb::Token& t) { return t.kind == bb::TokenKind::Comment; }));
        }
        std::size_t statements = 0;
        for (const auto& it : s.kernel) {
            if (const auto* init = std::get_if<bb::InitBlock>(&it)) {
                ++n["INITBLOCK"];
                count_statements(init->body, n, variables);
                statements += init->body.size();
            } else if (const auto* a = std::get_if<bb::Assign>(&it)) {
                ++n["SET"];
                variables.insert(a->name);
                n["no ;"] += a->semicolon ? 0 : 1;
                ++statements;
            } else {
                const auto& c = std::get<bb::If>(it);
                ++n["IF"];
                n["ELSE"] += c.has_else ? 1 : 0;
                count_primaries(c.condition, n);
                count_statements(c.then_block, n, variables);
                count_statements(c.else_block, n, variables);
                statements += c.then_block.size() + c.else_block.size();
            }
        }
        with_statements += statements > 0 ? 1 : 0;
    }
    CHECK(n["INITBLOCK"] == 35);
    CHECK(n["SET"] == 271);
    CHECK(n["IF"] == 308);
    CHECK(n["ELSE"] == 3);
    CHECK(n["assign in a block"] == 382);
    CHECK(n["call"] == 1656);
    CHECK(n["no ;"] == 4);
    CHECK(n["no ()"] == 1);
    CHECK(n["timer"] == 24);
    CHECK(n["variable"] == 429);
    CHECK(n["comments"] == 1273);
    CHECK(variables.size() == 147);
    CHECK(with_statements == 37);
}

TEST_CASE("bigbrother corpus: the interpreter's reading of the shipped scripts") {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("ANNO1503_GAME_DIR not set: skipped");
        return;
    }
    std::size_t identical = 0;
    const auto all = load_all(*game, identical);
    REQUIRE(all.size() == 50);

    // 28 conditions in 10 files mix && and || without parentheses. Each has the shape
    // guard && ... && alt1 || alt2 ...: the interpreter reads it as guard && (...), C would not.
    std::map<std::string, int> mixed;
    int differ = 0, guarded = 0;
    // The block statements without ';' that read on to a ';' (§3.4): file, statement, lost.
    std::vector<std::tuple<std::string, std::string, std::vector<std::string>>> read_on;
    int refused = 0;
    for (const auto& [file, s] : all) {
        INFO(file);
        for (const auto& it : s.kernel) {
            const auto* c = std::get_if<bb::If>(&it);
            if (c == nullptr || !bb::is_mixed(c->condition)) {
                continue;
            }
            ++mixed[file];
            differ += differs_from_c(c->condition) ? 1 : 0;
            // The top connective is the first one, &&, with the first term (the guard) as its
            // left operand.
            const bb::Grouped g = bb::grouped(c->condition);
            const bool first_and = c->condition.connectives.front() == bb::Connective::And;
            const bool guard_left = g.operands.size() == 2 && g.connective == bb::Connective::And &&
                                    g.operands[0].leaf == &c->condition.terms[0].primary;
            guarded += first_and && guard_left ? 1 : 0;
        }
        for (const auto& r : bb::read_on(s)) {
            if (!r.end) {
                ++refused;
                continue;
            }
            std::vector<std::string> lost;
            for (const auto* x : r.lost) {
                lost.push_back(name_of(x));
            }
            read_on.emplace_back(file, name_of(r.statement), lost);
        }
    }
    int total = 0;
    for (const auto& [file, k] : mixed) {
        total += k;
    }
    CHECK(total == 28);
    CHECK(mixed.size() == 10);
    CHECK(differ == 28);
    CHECK(guarded == 28);

    CHECK(refused == 0);
    CHECK(read_on.size() == 3);
    std::sort(read_on.begin(), read_on.end());
    using Row = std::tuple<std::string, std::string, std::vector<std::string>>;
    const std::vector<Row> want{
        {"S01_02_01.sze", "SET_PROFILE", {}},
        {"S01_09_01.sze", "SET_DIPLOMACY", {}},
        {"S01_09_01.sze", "SET_FOR_BUY", {"MINIMAP_ALERT"}},
    };
    // A bool, so that a failure prints no script text.
    const bool as_in_the_spec = read_on == want;
    CHECK(as_in_the_spec);
    int lost = 0;
    for (const auto& row : read_on) {
        lost += static_cast<int>(std::get<2>(row).size());
    }
    CHECK(lost == 1);
}
