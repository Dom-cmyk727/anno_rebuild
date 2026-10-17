// `sze` (format in docs/formats/sze.md, "Reference dump"; reference:
// tools/sze.py dump).
#include <cstdio>

#include <anno/formats/sze.hpp>
#include <anno/formats/tagged.hpp>

#include "dump.hpp"

namespace anno::dump {

namespace sze = formats::sze;
namespace tagged = formats::tagged;

namespace {

std::string hex32(std::uint32_t v) {
    char b[12];
    std::snprintf(b, sizeof b, "%08x", v);
    return b;
}

std::string record_line(const tagged::Record& r) {
    return std::string(r.name) + " size=" + std::to_string(r.payload.size()) +
           " long=" + (r.long_form ? "1" : "0") + " hash=" + hex64(fnv64(r.payload));
}

/// --full: the nested sequence of record `name` (if any), indented by depth.
bool nested(std::string_view parent, const tagged::Record& r, std::size_t depth, std::string& out) {
    const auto k = sze::nested_prefix(parent, r.name);
    if (!k) {
        return true;
    }
    const std::string pad(depth * 2, ' ');
    if (r.payload.size() < *k) {
        out += pad + "NESTED BAD\n";
        return false;
    }
    auto kids = tagged::parse_records(r.payload.subspan(*k));
    if (!kids) {
        out += pad + "NESTED BAD\n";
        return false;
    }
    if (*k > 0) {
        out += pad + "PREFIX " + hex(r.payload.first(*k)) + "\n";
    }
    bool ok = true;
    for (const auto& kid : kids.value()) {
        out += pad + record_line(kid) + "\n";
        ok = nested(r.name, kid, depth + 1, out) && ok;
    }
    return ok;
}

} // namespace

bool dump_sze(Bytes buf, bool full, std::string& out) {
    auto s = sze::Scene::parse(buf);
    if (!s) {
        std::fprintf(stderr, "NOPARSE: %s\n", s.error().c_str());
        out += "NOPARSE\n";
        return false;
    }
    const auto& scene = s.value();
    const auto blocks = scene.chunks("SPIELER");
    out += std::string("SCENE edition=") +
           (scene.edition == sze::Edition::Addon ? "ANNOADDON1" : "ANNO") +
           " children=" + std::to_string(scene.children.size()) +
           " players=" + std::to_string(blocks.size()) + "\n";
    bool ok = true;
    for (const auto* c : scene.chunks("INSELLNK")) {
        auto l = sze::IslandLink::parse(c->payload);
        if (!l) {
            out += "LINK BAD\n";
            ok = false;
            continue;
        }
        const auto& v = l.value();
        out += "LINK x=" + std::to_string(v.x) + " y=" + std::to_string(v.y) +
               " width=" + std::to_string(v.width) + " height=" + std::to_string(v.height) +
               " number=" + std::to_string(v.number) + " path=" + quote_all(v.path) + "\n";
    }
    for (std::size_t k = 0; k < blocks.size(); ++k) {
        const Bytes p = blocks[k]->payload;
        if (p.size() < sze::kPlayerPrefixLen) {
            out += "PLAYER i=" + std::to_string(k) + " BAD\n";
            ok = false;
            continue;
        }
        auto player = sze::Player::parse(p, blocks[k]->offset + formats::chunk::kHeaderLen);
        if (!player) {
            std::fprintf(stderr, "NOPARSE: player %zu: %s\n", k, player.error().c_str());
            out += "PLAYER i=" + std::to_string(k) +
                   " index=" + std::to_string(formats::u32_le(p, 4)) +
                   " flag=" + std::to_string(formats::u32_le(p, 0)) + " BAD\n";
            ok = false;
            continue;
        }
        const auto& pl = player.value();
        out += "PLAYER i=" + std::to_string(k) + " index=" + std::to_string(pl.index) +
               " flag=" + std::to_string(pl.flag) +
               " records=" + std::to_string(pl.records.size()) + "\n";
        for (const auto& r : pl.records) {
            out += "  RECORD " + record_line(r) + "\n";
            if (r.name == "SCHIFF" || r.name == "SOLDAT") {
                auto u = sze::Unit::parse(r.payload);
                if (u) {
                    const auto& v = u.value();
                    std::string recs;
                    for (std::size_t i = 0; i < v.records.size(); ++i) {
                        recs += (i ? "," : "") + std::string(v.records[i].name);
                    }
                    out += "  UNIT " + std::string(r.name) + " id=" + std::to_string(v.id) +
                           " figure=" + std::to_string(v.figure) +
                           " owner=" + std::to_string(v.owner) + " attr=" + std::to_string(v.attr) +
                           " reserved=" + std::to_string(v.reserved) + " x=" + hex32(v.x_bits) +
                           " y=" + hex32(v.y_bits) + " records=" + recs + "\n";
                } else {
                    out += "  UNIT " + std::string(r.name) + " BAD\n";
                    ok = false;
                }
            }
            if (full) {
                ok = nested("SPIELER", r, 2, out) && ok;
            }
        }
    }
    return ok;
}

} // namespace anno::dump
