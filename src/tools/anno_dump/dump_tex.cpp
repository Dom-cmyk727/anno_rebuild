// `tex`, `inc`, `colors` (formats in docs/formats/tex.md, "Reference dump",
// and docs/formats/palette.md; reference: tools/tex.py).
#include <cstdio>

#include <anno/formats/colors.hpp>
#include <anno/formats/inc.hpp>
#include <anno/formats/tex.hpp>

#include "dump.hpp"

namespace anno::dump {

namespace tex = formats::tex;

namespace {

void dump_texture(const tex::Texture& t, bool full, std::string& out) {
    auto px = t.decode();
    std::size_t opaque = 0;
    std::string pixels = "BAD";
    if (px) {
        formats::ByteBuffer flat;
        for (const auto& p : px.value()) {
            flat.push_back(p ? 1 : 0);
            flat.push_back(p ? *p : 0);
            opaque += p ? 1U : 0U;
        }
        pixels = hex64(fnv64(flat));
    } else {
        std::fprintf(stderr, "texture %u: %s\n", t.id, px.error().c_str());
    }
    const auto& h = t.head;
    out += "TEXTURE id=" + std::to_string(t.id) + " flag=" + std::to_string(t.flag) +
           " width=" + std::to_string(h.width) + " height=" + std::to_string(h.height) +
           " palette=" + std::to_string(h.palette_id) + " u06=" + std::to_string(h.unknown_06) +
           " u08=" + std::to_string(h.unknown_08) + " format=" + quote(h.format_raw) +
           " key=" + hex(h.color_key) + " u18=" + std::to_string(h.unknown_18) +
           " u1a=" + std::to_string(h.unknown_1a) + " u1c=" + hex(h.unknown_1c) +
           " body=" + std::to_string(t.body.size()) +
           " name=" + (t.name ? quote(*t.name) : std::string("-")) +
           " opaque=" + std::to_string(opaque) + " pixels=" + pixels + "\n";
    if (full && px) {
        const std::size_t w = h.width;
        for (std::size_t y = 0; y < h.height; ++y) {
            out += "  R" + std::to_string(y) + " ";
            for (std::size_t x = 0; x < w; ++x) {
                const auto& p = px.value()[y * w + x];
                if (p) {
                    const std::uint8_t b = *p;
                    out += hex(Bytes(&b, 1));
                } else {
                    out += "..";
                }
            }
            out += "\n";
        }
    }
}

} // namespace

bool dump_tex(Bytes buf, bool full, std::string& out) {
    auto f = tex::TexFile::parse(buf);
    if (!f) {
        std::fprintf(stderr, "NOPARSE: %s\n", f.error().c_str());
        out += "NOPARSE\n";
        return false;
    }
    const auto& file = f.value();
    out += "TEX palettes=" + std::to_string(file.palettes.size()) +
           " textures=" + std::to_string(file.textures.size()) + "\n";
    for (const auto& [is_palette, i] : file.order) {
        if (is_palette) {
            const auto& p = file.palettes[i];
            out += "PALETTE id=" + std::to_string(p.id) + " flag=" + std::to_string(p.flag) +
                   " rgb=" + palette_hash(p) + "\n";
            if (full) {
                palette_rows(p, out);
            }
        } else {
            dump_texture(file.textures[i], full, out);
        }
    }
    return true;
}

bool dump_inc(Bytes buf, std::string& out) {
    auto recs = formats::inc::parse_access(buf);
    if (!recs) {
        out += "NOPARSE\n";
        return false;
    }
    out += "INC records=" + std::to_string(recs->size()) + "\n";
    for (const auto& r : *recs) {
        out += "ACCESS source=" + quote(r.source) + " number=" + std::to_string(r.number) + "\n";
    }
    return true;
}

bool dump_colors(Bytes buf, bool full, std::string& out) {
    auto ts = formats::colors::parse_anlagen(buf);
    if (!ts) {
        std::fprintf(stderr, "NOPARSE: %s\n", ts.error().c_str());
        out += "NOPARSE\n";
        return false;
    }
    out += "COLORS entries=" + std::to_string(ts.value().size()) + "\n";
    for (const auto& t : ts.value()) {
        out += "COLORTABLE id=" + std::to_string(t.palette.id) +
               " flag=" + std::to_string(t.palette.flag) + " rgb=" + palette_hash(t.palette) +
               " name=" + quote(t.name) + "\n";
        if (full) {
            palette_rows(t.palette, out);
        }
    }
    return true;
}

} // namespace anno::dump
