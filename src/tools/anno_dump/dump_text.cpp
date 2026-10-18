// `keyed`, `texte` (docs/formats/data-text.md, "Reference dump"; reference:
// tools/text.py) and `zei` (docs/formats/zei.md; reference: tools/zei.py).
#include <cstdio>

#include <anno/formats/text.hpp>
#include <anno/formats/zei.hpp>

#include "dump.hpp"

namespace anno::dump {

namespace text = formats::text;

namespace {

std::string summary(const std::vector<text::Line>& ls) {
    std::size_t crlf = 0;
    for (const auto& l : ls) {
        crlf += l.ending == text::Ending::CrLf ? 1U : 0U;
    }
    const bool final_nl = !ls.empty() && ls.back().text.empty();
    return "LINES count=" + std::to_string(ls.size()) + " crlf=" + std::to_string(crlf) +
           " final=" + (final_nl ? "1" : "0") + "\n";
}

} // namespace

bool dump_keyed(Bytes buf, std::string& out) {
    out += summary(text::lines(buf));
    for (const auto& [l, k] : text::keyed(buf)) {
        const std::string n = std::to_string(l.number);
        switch (k.tag) {
        case text::KeyedKindTag::Blank:
            break;
        case text::KeyedKindTag::Section:
            out += "SECTION line=" + n + " name=" + quote_all(k.name) + "\n";
            break;
        case text::KeyedKindTag::Entry:
            out += "ENTRY line=" + n + " key=" + quote_all(k.name) +
                   " fields=" + std::to_string(text::fields(k.value).size()) +
                   " value=" + quote_all(k.value) + "\n";
            break;
        case text::KeyedKindTag::Other:
            out += "OTHER line=" + n + " text=" + quote_all(k.name) + "\n";
            break;
        }
    }
    return true;
}

bool dump_texte(Bytes buf, bool full, std::string& out) {
    out += summary(text::lines(buf));
    const auto t = text::Texte::parse(buf);
    out += "TEXTE sections=" + std::to_string(t.sections.size()) +
           " preamble=" + std::to_string(t.preamble.size()) + "\n";
    bool ok = true;
    for (const auto& s : t.sections) {
        formats::ByteBuffer joined;
        for (const auto& l : s.lines) {
            formats::put_bytes(joined, l);
            joined.push_back('\n');
        }
        out += "SECTION line=" + std::to_string(s.line) + " name=" + quote_all(s.name) +
               " lines=" + std::to_string(s.lines.size()) + " hash=" + hex64(fnv64(joined)) + "\n";
        if (text::equals(s.name, "ANLAGEN")) {
            for (const auto& l : s.lines) {
                if (auto it = text::id_text(l)) {
                    out += "  ID id=" + std::to_string(it->first) +
                           " text=" + quote_all(it->second) + "\n";
                } else {
                    out += "  ID BAD text=" + quote_all(l) + "\n";
                    ok = false;
                }
            }
        }
        if (full) {
            for (const auto& l : s.lines) {
                out += "  L " + quote_all(l) + "\n";
            }
        }
    }
    return ok;
}

bool dump_zei(Bytes buf, bool full, std::string& out) {
    auto cs = formats::zei::CharSet::parse(buf);
    if (!cs) {
        std::fprintf(stderr, "NOPARSE: %s\n", cs.error().c_str());
        out += "NOPARSE\n";
        return false;
    }
    std::size_t have = 0;
    for (const auto& g : cs.value().glyphs) {
        have += g.data.empty() ? 0U : 1U;
    }
    out += "CHARSET size_a=" + std::to_string(cs.value().size_a) +
           " size_b=" + std::to_string(cs.value().size_b) +
           " reserved=" + hex(cs.value().reserved) + " glyphs=" + std::to_string(have) + "\n";
    bool ok = true;
    for (const auto& g : cs.value().glyphs) {
        if (g.data.empty()) {
            continue;
        }
        const std::string head = "GLYPH code=" + std::to_string(g.code) +
                                 " width=" + std::to_string(g.width) +
                                 " length=" + std::to_string(g.data.size());
        auto img = g.decode();
        if (!img) {
            out += head + " BAD\n";
            ok = false;
            continue;
        }
        const auto& im = img.value();
        out += head + " image=" + std::to_string(im.width) + "x" + std::to_string(im.height) +
               " hash=" + hex64(fnv64(g.data)) + "\n";
        if (full) {
            for (std::size_t y = 0; y < im.height; ++y) {
                out += "  ";
                for (std::size_t x = 0; x < im.width; ++x) {
                    const auto& v = im.levels[y * im.width + x];
                    out += !v ? '.' : *v < 10 ? static_cast<char>('0' + *v) : '#';
                }
                out += "\n";
            }
        }
    }
    return ok;
}

} // namespace anno::dump
