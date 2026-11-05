#include <algorithm>
#include <bit>
#include <limits>
#include <set>
#include <utility>

#include <anno/formats/bigbrother.hpp>
#include <anno/sim/script.hpp>
namespace anno::sim {
namespace {
namespace bb = formats::bigbrother;
std::string upper(std::string_view s) {
    std::string out(s);
    for (auto& c : out)
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    return out;
}
struct Compiler {
    ScriptProgram p;
    const ScriptEnvironment& env;
    ScriptError error;
    std::uint32_t nodes = 0;
    bool fail(std::size_t at, std::string message) {
        if (error.message.empty())
            error = {static_cast<std::uint32_t>(at), std::move(message)};
        return false;
    }
    bool node(std::size_t at) {
        return ++nodes <= p.limits.nodes || fail(at, "program node limit");
    }
    std::optional<std::uint32_t> symbol(std::string_view name) const {
        for (std::size_t i = 0; i < p.symbols.size(); ++i)
            if (p.symbols[i] == name)
                return static_cast<std::uint32_t>(i);
        return std::nullopt;
    }
    std::optional<std::int32_t> number(std::string_view s, std::size_t at) {
        auto n = bb::to_integer(s);
        if (!n || *n < std::numeric_limits<std::int32_t>::min() ||
            *n > std::numeric_limits<std::int32_t>::max()) {
            fail(at, "expected signed32 integer");
            return std::nullopt;
        }
        return static_cast<std::int32_t>(*n);
    }
    std::optional<std::uint32_t> boolean(const bb::Value& v, std::size_t at) {
        if (v.kind != bb::ValueKind::Bool) {
            fail(at, "expected Boolean");
            return std::nullopt;
        }
        auto s = upper(v.text);
        if (s == "TRUE")
            return 1;
        if (s == "FALSE")
            return 0;
        fail(at, "expected TRUE/FALSE");
        return std::nullopt;
    }
    bool statement(const bb::Statement& st, std::vector<ScriptInstruction>& out) {
        const auto at = bb::offset_of(st);
        if (!node(at))
            return false;
        ScriptInstruction i;
        i.offset = static_cast<std::uint32_t>(at);
        if (const auto* a = std::get_if<bb::Assign>(&st)) {
            auto s = symbol(a->name);
            auto b = boolean(a->value, at);
            if (!s)
                return fail(at, "unknown signal: " + a->name);
            if (!b)
                return false;
            i.index = *s;
            i.value = *b;
        } else {
            const auto& c = std::get<bb::Call>(st);
            const auto name = upper(c.name);
            if (name == "SETCONSTRUCTION_DRAWSTATE" || name == "SETCONSTRUCTION_DRAWSTATE_ALL") {
                const bool all = name == "SETCONSTRUCTION_DRAWSTATE_ALL";
                if (c.args.size() != (all ? 2u : 3u) || c.args[0].kind != bb::ValueKind::String)
                    return fail(at, "permission action argument types/count");
                std::string lookup(c.args[0].contents());
                lookup.resize(lookup.find('@') == std::string::npos ? lookup.size()
                                                                    : lookup.find('@'));
                auto mapped =
                    std::find_if(p.mappings.begin(), p.mappings.end(), [&](const auto& m) {
                        return m.offset < at && upper(m.name) == upper(lookup);
                    });
                if (mapped == p.mappings.end())
                    return fail(at, "unknown player mapping: " + lookup);
                i.index = static_cast<std::uint32_t>(mapped - p.mappings.begin());
                i.action = all ? ScriptAction::PermissionAll : ScriptAction::Permission;
                if (!all) {
                    if (c.args[1].kind != bb::ValueKind::Number)
                        return fail(at, "expected type id");
                    auto n = number(c.args[1].text, at);
                    if (!n || *n < 0)
                        return fail(at, "invalid type id");
                    i.type = TypeId{static_cast<std::uint32_t>(*n)};
                    if (std::find(env.types.begin(), env.types.end(), i.type) == env.types.end())
                        return fail(at, "unknown construction type");
                }
                auto b = boolean(c.args.back(), at);
                if (!b)
                    return false;
                i.value = *b;
            } else if (name == "TIMER_SET" || name == "TIMER_START" || name == "TIMER_STOP") {
                const bool set = name == "TIMER_SET";
                if (c.args.size() != (set ? 2u : 1u) || c.args[0].kind != bb::ValueKind::Number)
                    return fail(at, "timer action argument types/count");
                auto n = number(c.args[0].text, at);
                if (!n || *n < 0 || static_cast<std::uint32_t>(*n) >= env.timer_count)
                    return fail(at, "timer index out of range");
                i.index = static_cast<std::uint32_t>(*n);
                i.action = set                     ? ScriptAction::TimerSet
                           : name == "TIMER_START" ? ScriptAction::TimerStart
                                                   : ScriptAction::TimerStop;
                if (set) {
                    if (c.args[1].kind != bb::ValueKind::Number)
                        return fail(at, "expected timer seconds");
                    auto seconds = number(c.args[1].text, at);
                    if (!seconds)
                        return false;
                    i.value = static_cast<std::uint32_t>(*seconds);
                }
            } else
                return fail(at, "unsupported action: " + name);
        }
        out.push_back(i);
        return true;
    }
    bool block(const std::vector<bb::Statement>& statements,
               const std::set<const bb::Statement*>& lost, std::vector<ScriptInstruction>& out) {
        for (const auto& s : statements)
            if (!lost.contains(&s) && !statement(s, out))
                return false;
        return true;
    }
    std::optional<std::uint32_t> expr(const bb::Grouped& g, std::uint32_t depth) {
        if (depth > p.limits.depth || !node(g.leaf ? g.leaf->offset : 0)) {
            fail(0, "expression depth/node limit");
            return std::nullopt;
        }
        ScriptExpr e;
        if (g.leaf) {
            const auto& l = *g.leaf;
            e.offset = static_cast<std::uint32_t>(l.offset);
            if (!l.subject.empty()) {
                fail(l.offset, "unsupported query: " + bb::describe(l));
                return std::nullopt;
            }
            const auto token = upper(l.name);
            const auto suffix = token.starts_with("TIMER_") ? std::string_view(token).substr(6)
                                                            : std::string_view{};
            const auto timer = bb::to_integer(suffix);
            const bool timer_builtin = timer && *timer >= 0 &&
                                       static_cast<std::uint64_t>(*timer) < env.timer_count &&
                                       std::to_string(*timer) == suffix;
            if (token == "PLAYTIME")
                e.query = ScriptQuery::Playtime;
            else if (timer_builtin) {
                e.query = ScriptQuery::Timer;
                e.index = static_cast<std::uint32_t>(*timer);
            } else if (auto signal = symbol(l.name))
                e.index = *signal;
            else {
                fail(l.offset, "unknown signal/query: " + l.name);
                return std::nullopt;
            }
            if (l.value) {
                if (e.query == ScriptQuery::Signal) {
                    if (l.op != "==") {
                        fail(l.offset, "Boolean comparison requires ==");
                        return std::nullopt;
                    }
                    auto b = boolean(*l.value, l.offset);
                    if (!b)
                        return std::nullopt;
                    e.value = static_cast<std::int32_t>(*b);
                } else {
                    if (l.value->kind != bb::ValueKind::Number) {
                        fail(l.offset, "numeric comparison required");
                        return std::nullopt;
                    }
                    auto n = number(l.value->text, l.offset);
                    if (!n)
                        return std::nullopt;
                    e.value = *n;
                }
                e.compare = l.op == "=="  ? ScriptCompare::Equal
                            : l.op == "<" ? ScriptCompare::Less
                                          : ScriptCompare::Greater;
            } else if (e.query != ScriptQuery::Signal) {
                fail(l.offset, "numeric query requires comparison");
                return std::nullopt;
            }
        } else {
            e.kind =
                g.connective == bb::Connective::And ? ScriptExpr::Kind::And : ScriptExpr::Kind::Or;
            auto a = expr(g.operands[0], depth + 1);
            if (!a)
                return std::nullopt;
            auto b = expr(g.operands[1], depth + 1);
            if (!b)
                return std::nullopt;
            e.left = *a;
            e.right = *b;
        }
        p.expressions.push_back(e);
        return static_cast<std::uint32_t>(p.expressions.size() - 1);
    }
};
struct Evaluation {
    bool truth = false, suppressed = false, ok = true;
};
Evaluation eval(const ScriptProgram& p, const ScriptState& s, std::uint32_t n, ScriptHost& host,
                ScriptWork& w) {
    if (!w.consume())
        return {false, false, false};
    const auto& e = p.expressions[n];
    if (e.kind != ScriptExpr::Kind::Leaf) {
        auto a = eval(p, s, e.left, host, w);
        if (!a.ok)
            return a;
        if (e.kind == ScriptExpr::Kind::And && !a.truth)
            return {false, true, true};
        auto b = eval(p, s, e.right, host, w);
        if (!b.ok)
            return b;
        return {e.kind == ScriptExpr::Kind::And ? a.truth && b.truth : a.truth || b.truth, false,
                true};
    }
    const std::int32_t value = e.query == ScriptQuery::Signal ? s.signals[e.index]
                               : e.query == ScriptQuery::Playtime
                                   ? std::bit_cast<std::int32_t>(s.last_arg) / 500
                                   : static_cast<std::int32_t>(host.timer_seconds(e.index));
    bool b = false;
    switch (e.compare) {
    case ScriptCompare::Truth:
        b = value != 0;
        break;
    case ScriptCompare::Equal:
        b = value == e.value;
        break;
    case ScriptCompare::Less:
        b = value < e.value;
        break;
    case ScriptCompare::Greater:
        b = value > e.value;
        break;
    }
    return {b, false, true};
}
} // namespace
formats::Result<ScriptProgram, ScriptError>
compile_script(std::string source, const ScriptEnvironment& env, ScriptLimits limits) {
    if (limits.source_bytes == 0 || limits.source_bytes > 1048576 ||
        source.size() > limits.source_bytes || limits.nodes == 0 || limits.nodes > 65536 ||
        limits.depth == 0 || limits.depth > 128 || limits.work == 0 || limits.work > 10000000)
        return ScriptError{0, "invalid script limits/source size"};
    // Normalize only grammar keywords, preserving symbol/string bytes and offsets.
    const auto bytes = [](const std::string& text) {
        return formats::Bytes(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
    };
    auto tokens = bb::lex(bytes(source));
    if (!tokens)
        return ScriptError{static_cast<std::uint32_t>(tokens.error().offset),
                           tokens.error().text()};
    std::string grammar = source;
    const std::set<std::string> keywords{"SET", "IF", "THEN", "ELSE", "INITBLOCK", "TRUE", "FALSE"};
    std::set<std::string> sections;
    for (const auto& token : tokens.value()) {
        auto name = upper(token.text);
        if (token.kind == bb::TokenKind::Section) {
            if (!sections.insert(name).second)
                return ScriptError{static_cast<std::uint32_t>(token.offset),
                                   "duplicate script section: " + name};
            grammar.replace(token.offset, token.text.size(), name);
        } else if (token.kind == bb::TokenKind::Name && keywords.contains(name))
            grammar.replace(token.offset, token.text.size(), name);
    }
    for (const auto& section : bb::known_sections())
        if (!sections.contains(std::string(section.name)))
            return ScriptError{0, "missing required script section: " + std::string(section.name)};
    auto ast = bb::parse(bytes(grammar), {.max_condition_depth = limits.depth});
    if (!ast)
        return ScriptError{static_cast<std::uint32_t>(ast.error().offset), ast.error().text()};
    Compiler c{{}, env, {}, 0};
    c.p.source = std::move(source);
    c.p.limits = limits;
    const auto& a = ast.value();
    for (const auto& section : a.sections)
        if (section.kind == bb::SectionKind::Unknown)
            c.fail(0, "unsupported section: " + section.name);
    if (!a.objects.empty() || !a.areas.empty())
        c.fail(0, "unsupported object/area definitions");
    c.p.symbols.push_back("SCHUETTIINITSIG");
    c.p.initial.push_back(1);
    for (const auto& m : a.players) {
        if (!c.node(m.offset))
            break;
        auto slot = c.number(m.slot, m.offset);
        if (!slot || *slot < 0 || static_cast<std::uint32_t>(*slot) >= env.player_slots) {
            c.fail(m.offset, "mapped player slot out of range");
            break;
        }
        c.p.mappings.push_back(
            {m.name, static_cast<std::uint32_t>(*slot), static_cast<std::uint32_t>(m.offset)});
    }
    std::set<const bb::Statement*> lost;
    for (const auto& r : bb::read_on(a)) {
        if (!r.end)
            c.fail(bb::offset_of(*r.statement), "unterminated read-on statement");
        lost.insert(r.lost.begin(), r.lost.end());
    }
    for (const auto& item : a.kernel) {
        if (!c.error.message.empty())
            break;
        if (const auto* set = std::get_if<bb::Assign>(&item)) {
            if (!c.node(set->offset))
                break;
            if (c.symbol(set->name)) {
                c.fail(set->offset, "duplicate signal: " + set->name);
                break;
            }
            auto value = c.boolean(set->value, set->offset);
            if (!value)
                break;
            c.p.symbols.push_back(set->name);
            c.p.initial.push_back(static_cast<std::uint8_t>(*value));
        } else if (const auto* init = std::get_if<bb::InitBlock>(&item)) {
            if (!c.block(init->body, lost, c.p.init))
                break;
        } else {
            const auto& iff = std::get<bb::If>(item);
            ScriptIf i;
            i.offset = static_cast<std::uint32_t>(iff.offset);
            auto condition = c.expr(bb::grouped(iff.condition), 1);
            if (!condition)
                break;
            i.condition = *condition;
            if (!c.node(iff.offset) || !c.block(iff.then_block, lost, i.yes) ||
                !c.block(iff.else_block, lost, i.no))
                break;
            c.p.branches.push_back(std::move(i));
        }
    }
    for (const auto& signal : a.signals) {
        auto s = c.symbol(signal.name);
        if (!s) {
            c.fail(signal.offset, "unknown restored signal: " + signal.name);
            break;
        }
        c.p.initial[*s] = signal.value ? 1 : 0;
    }
    if (!c.error.message.empty())
        return std::move(c.error);
    return std::move(c.p);
}
bool ScriptWork::consume() {
    if (remaining == 0) {
        exhausted = true;
        return false;
    }
    --remaining;
    ++used;
    return true;
}
ScriptState make_script_state(const ScriptProgram& p) {
    ScriptState s;
    s.context = ScriptContext::Pending;
    s.signals = p.initial;
    return s;
}
std::uint32_t script_visits(std::uint32_t arg, std::uint32_t count) {
    const auto n = std::bit_cast<std::int32_t>(arg * count + 125u);
    const auto q = n / 250;
    return q < 0 || static_cast<std::uint32_t>(q) > count ? count : static_cast<std::uint32_t>(q);
}
bool visit_script(const ScriptProgram& p, ScriptState& state, std::uint32_t arg, ScriptHost& host) {
    if (state.context == ScriptContext::Missing || state.context == ScriptContext::ExplicitNone)
        return true;
    if (state.context == ScriptContext::Fault || state.context == ScriptContext::Unsupported)
        return false;
    const bool pending = state.context == ScriptContext::Pending;
    auto invalid = [&] {
        state.context = ScriptContext::Fault;
        state.fault = ScriptFault::InvalidState;
        state.last_arg = arg;
        return false;
    };
    if (p.policy != kScriptPolicy || state.signals.size() != p.symbols.size() ||
        state.bindings.size() != p.mappings.size() ||
        (!p.branches.empty() && state.cursor >= p.branches.size()) ||
        (p.branches.empty() && state.cursor != 0) ||
        std::any_of(state.signals.begin(), state.signals.end(), [](auto b) { return b > 1; }))
        return invalid();
    const auto count = script_visits(arg, static_cast<std::uint32_t>(p.branches.size()));
    if (!pending && count == 0) {
        state.last_arg = arg;
        state.last_work = 0;
        return true;
    }
    auto staged = state;
    staged.last_arg = arg;
    ScriptWork work{p.limits.work, 0};
    ScriptFault fault = ScriptFault::Host;
    std::uint32_t offset = 0;
    if (!host.begin()) {
        host.rollback();
        state.last_arg = arg;
        state.last_work = 0;
        state.context = ScriptContext::Fault;
        state.fault = fault;
        return false;
    }
    auto block = [&](const auto& instructions) {
        for (const auto& i : instructions) {
            offset = i.offset;
            if (!work.consume()) {
                fault = ScriptFault::Budget;
                return false;
            }
            if (i.action == ScriptAction::Assign)
                staged.signals[i.index] = static_cast<std::uint8_t>(i.value);
            else if (i.action == ScriptAction::Permission ||
                     i.action == ScriptAction::PermissionAll) {
                if (!host.permission(i.index, i.type, i.value,
                                     i.action == ScriptAction::PermissionAll, work)) {
                    fault = work.exhausted ? ScriptFault::Budget : ScriptFault::Host;
                    return false;
                }
            } else if (!host.timer(i.action, i.index, i.value))
                return false;
        }
        return true;
    };
    bool ok = true;
    if (pending) {
        if (staged.signals[0] != 0)
            ok = block(p.init);
        staged.signals[0] = 0;
        staged.context = ScriptContext::Ready;
    }
    for (std::uint32_t k = 0; ok && k < count; ++k) {
        const auto& i = p.branches[staged.cursor];
        offset = i.offset;
        if (!work.consume()) {
            fault = ScriptFault::Budget;
            ok = false;
            break;
        }
        auto e = eval(p, staged, i.condition, host, work);
        if (!e.ok) {
            fault = ScriptFault::Budget;
            ok = false;
            break;
        }
        if (!e.suppressed)
            ok = block(e.truth ? i.yes : i.no);
        if (ok)
            staged.cursor = (staged.cursor + 1) % static_cast<std::uint32_t>(p.branches.size());
    }
    if (!ok) {
        host.rollback();
        state.context = ScriptContext::Fault;
        state.fault = fault;
        state.fault_offset = offset;
        state.last_arg = arg;
        state.last_work = work.used;
        return false;
    }
    host.commit();
    staged.last_work = work.used;
    state = std::move(staged);
    return true;
}
} // namespace anno::sim
