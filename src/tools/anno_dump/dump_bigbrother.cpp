// `bigbrother` (format in docs/formats/bigbrother.md §8, "Reference dump"; reference:
// tools/bigbrother.py dump).
#include <cstdio>
#include <map>

#include <anno/formats/bigbrother.hpp>

#include "dump.hpp"

namespace anno::dump {

namespace bb = formats::bigbrother;

namespace {

/// ASCII on every platform: printable ASCII except `\` as is, every other byte as \xHH.
std::string esc(std::string_view line) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    for (char ch : line) {
        const auto c = static_cast<std::uint8_t>(ch);
        if (c >= 0x20 && c <= 0x7e && c != '\\') {
            out.push_back(ch);
        } else {
            out += "\\x";
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 0xF]);
        }
    }
    return out;
}

std::string join_numbers(const std::vector<std::string>& numbers) {
    std::string s;
    for (const auto& n : numbers) {
        s += (s.empty() ? "" : ",") + bb::canonical_integer(n);
    }
    return s;
}

/// The read-on notes, by the statement's offset (unique per statement).
struct Notes {
    std::map<std::size_t, const bb::ReadOn*> swallowed;
    std::map<std::size_t, bool> lost;
};

void statement(const bb::Statement& st, std::string_view ind, const Notes& notes,
               std::vector<std::string>& out) {
    std::string line(ind);
    if (const auto* a = std::get_if<bb::Assign>(&st)) {
        line += (a->is_set ? "SET " : "") + a->name + " = " + a->value.text;
    } else {
        const auto& c = std::get<bb::Call>(st);
        line += "CALL " + c.name + "(";
        for (std::size_t i = 0; i < c.args.size(); ++i) {
            line += (i > 0 ? ", " : "") + c.args[i].text;
        }
        line += ")";
        line += c.parens ? "" : "  (no ())";
    }
    line += bb::has_semicolon(st) ? "" : "  (no ;)";
    if (const auto it = notes.swallowed.find(bb::offset_of(st)); it != notes.swallowed.end()) {
        const bb::ReadOn& r = *it->second;
        line += !r.end ? std::string("  (the interpreter refuses the kernel: no ';' follows)")
                       : "  (reads on to the ';' at offset " + std::to_string(*r.end) + "; loses " +
                             std::to_string(r.lost.size()) + " statement(s))";
    }
    if (notes.lost.count(bb::offset_of(st)) != 0) {
        line += "  (LOST: read into the statement before)";
    }
    out.push_back(std::move(line));
}

void statements(const std::vector<bb::Statement>& body, std::string_view ind, const Notes& notes,
                std::vector<std::string>& out) {
    for (const auto& st : body) {
        statement(st, ind, notes, out);
    }
}

std::vector<std::string> dump_lines(const bb::Script& s, bool full) {
    std::vector<std::string> out;
    std::size_t bytes = 0, tokens = 0, crlf = 0, lf = 0;
    for (const auto& sec : s.sections) {
        for (const auto& t : sec.tokens) {
            bytes += t.text.size();
            ++tokens;
            crlf += t.text == "\r\n" ? 1U : 0U;
            lf += t.text == "\n" ? 1U : 0U;
        }
    }
    out.push_back("SCRIPT bytes=" + std::to_string(bytes) + " tokens=" + std::to_string(tokens) +
                  " sections=" + std::to_string(s.sections.size()) +
                  " crlf=" + std::to_string(crlf) + " lf=" + std::to_string(lf));
    const auto reads = bb::read_on(s);
    Notes notes;
    for (const auto& r : reads) {
        notes.swallowed[bb::offset_of(*r.statement)] = &r;
        for (const auto* x : r.lost) {
            notes.lost[bb::offset_of(*x)] = true;
        }
    }
    for (const auto& sec : s.sections) {
        if (sec.kind == bb::SectionKind::Unknown) {
            out.push_back(sec.name + "  (not read: " + std::to_string(sec.tokens.size() - 1) +
                          " tokens kept)");
            continue;
        }
        out.push_back(sec.name);
        switch (sec.kind) {
        case bb::SectionKind::Objects:
            for (const auto& o : s.objects) {
                out.push_back("  " + o.kind + " \"" + o.name + "\" " + join_numbers(o.numbers));
            }
            break;
        case bb::SectionKind::Areas:
            for (const auto& a : s.areas) {
                out.push_back("  \"" + a.name + "\" " + join_numbers(a.corners));
            }
            break;
        case bb::SectionKind::Players:
            for (const auto& p : s.players) {
                out.push_back("  \"" + p.name + "\" " + bb::canonical_integer(p.slot));
            }
            break;
        case bb::SectionKind::Signals:
            for (const auto& g : s.signals) {
                out.push_back("  " + g.name + " = " + (g.value ? "TRUE" : "FALSE"));
            }
            break;
        case bb::SectionKind::Camera:
            out.push_back("  " + bb::canonical_integer(s.camera.x) + "," +
                          bb::canonical_integer(s.camera.y));
            break;
        case bb::SectionKind::Kernel:
            for (const auto& it : s.kernel) {
                if (const auto* init = std::get_if<bb::InitBlock>(&it)) {
                    out.emplace_back("  INITBLOCK");
                    statements(init->body, "    ", notes, out);
                } else if (const auto* set = std::get_if<bb::Assign>(&it)) {
                    // A top-level SET: the same line as a block statement, two spaces in.
                    statement(bb::Statement(*set), "  ", notes, out);
                } else {
                    const auto& cond = std::get<bb::If>(it);
                    out.push_back("  IF " + bb::describe(cond.condition) + " THEN");
                    if (bb::is_mixed(cond.condition)) {
                        out.push_back("     grouped by the interpreter: " +
                                      bb::describe(bb::grouped(cond.condition)));
                    }
                    statements(cond.then_block, "    ", notes, out);
                    if (cond.has_else) {
                        out.emplace_back("  ELSE");
                        statements(cond.else_block, "    ", notes, out);
                    }
                }
            }
            break;
        case bb::SectionKind::Unknown:
            break;
        }
    }
    if (full) {
        for (const auto& sec : s.sections) {
            for (const auto& t : sec.tokens) {
                out.push_back("TOKEN " + std::to_string(t.offset) + " " +
                              std::string(bb::kind_name(t.kind)) + " " + t.text);
            }
        }
    }
    return out;
}

} // namespace

bool dump_bigbrother(Bytes buf, bool full, std::string& out) {
    auto payload = bb::find_payload(buf);
    if (!payload) {
        std::fprintf(stderr, "NOPARSE: %s\n", payload.error().c_str());
        out += "NOPARSE\n";
        return false;
    }
    auto s = bb::parse(payload.value());
    if (!s) {
        std::fprintf(stderr, "NOPARSE: %s\n", s.error().text().c_str());
        out += "NOPARSE\n";
        return false;
    }
    for (const auto& line : dump_lines(s.value(), full)) {
        out += esc(line);
        out += '\n';
    }
    return true;
}

} // namespace anno::dump
