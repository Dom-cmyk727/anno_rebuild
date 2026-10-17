// `insel` (format in docs/formats/insel.md, "Reference dump"; reference:
// tools/insel.py dump).
#include <algorithm>
#include <cstdio>
#include <set>

#include <anno/formats/insel.hpp>

#include "dump.hpp"

namespace anno::dump {

namespace insel = formats::insel;

namespace {

constexpr std::string_view kBlockRecords[] = {"HEIGHT", "GFXID", "GELAENDE", "PROD2", "FLUESSE"};

bool is_block_record(std::string_view n) {
    return std::find(std::begin(kBlockRecords), std::end(kBlockRecords), n) !=
           std::end(kBlockRecords);
}

std::string hex16(std::uint16_t v) {
    char b[8];
    std::snprintf(b, sizeof b, "%04x", static_cast<unsigned>(v));
    return b;
}

} // namespace

bool dump_insel(Bytes buf, bool full, std::string& out) {
    auto is = insel::islands(buf);
    if (!is) {
        std::fprintf(stderr, "NOPARSE: %s\n", is.error().c_str());
        out += "NOPARSE\n";
        return false;
    }
    bool ok = true;
    out += "ISLANDS count=" + std::to_string(is.value().size()) + "\n";
    for (std::size_t k = 0; k < is.value().size(); ++k) {
        const auto& i = is.value()[k];
        std::string recs;
        for (std::size_t r = 0; r < i.records.size(); ++r) {
            recs += (r ? "," : "") + std::string(i.records[r].name());
        }
        out += "ISLAND i=" + std::to_string(k) +
               " version=" + (i.version == insel::Version::Insel4 ? "4" : "5") +
               " x=" + std::to_string(i.x) + " y=" + std::to_string(i.y) +
               " width=" + std::to_string(i.width) + " height=" + std::to_string(i.height) +
               " number=" + std::to_string(i.number) + " head=" + hex(i.head) + " records=" + recs +
               "\n";
        std::vector<std::string_view> seen;
        for (const auto& r : i.records) {
            if (!is_block_record(r.name())) {
                out += "  RECORD " + std::string(r.name()) +
                       " size=" + std::to_string(r.payload.size()) +
                       " hash=" + hex64(fnv64(r.payload)) + "\n";
            } else if (std::find(seen.begin(), seen.end(), r.name()) == seen.end()) {
                seen.push_back(r.name());
            }
        }
        auto hs = i.heights();
        auto gs = i.gfx_ids();
        if (hs && gs) {
            const auto& h = hs.value();
            const auto& g = gs.value();
            const auto lo = h.empty() ? 0 : *std::min_element(h.begin(), h.end());
            const auto hi = h.empty() ? 0 : *std::max_element(h.begin(), h.end());
            out += "  GRID HEIGHT min=" + std::to_string(lo) + " max=" + std::to_string(hi) +
                   " hash=" + hex64(fnv64(h)) + "\n";
            formats::ByteBuffer gb;
            for (auto v : g) {
                formats::put_u16_le(gb, v);
            }
            const std::set<std::uint16_t> distinct(g.begin(), g.end());
            out += "  GRID GFXID distinct=" + std::to_string(distinct.size()) +
                   " hash=" + hex64(fnv64(gb)) + "\n";
            std::set<std::uint8_t> textures;
            std::size_t water = 0;
            for (auto v : g) {
                const auto t = insel::GroundCell::from_gfx_id(v).texture;
                if (t == 0) {
                    ++water;
                } else {
                    textures.insert(t);
                }
            }
            std::string list;
            for (auto t : textures) {
                list += (list.empty() ? "" : ",") + std::to_string(t);
            }
            out += "  GROUND water=" + std::to_string(water) + " textures=" + list + "\n";
            if (full) {
                const std::size_t w = std::max<std::size_t>(i.width, 1);
                for (std::size_t y = 0; y * w < h.size(); ++y) {
                    out += "    H" + std::to_string(y) + " " +
                           hex(Bytes(h).subspan(y * w, std::min(w, h.size() - y * w))) + "\n";
                }
                for (std::size_t y = 0; y * w < g.size(); ++y) {
                    out += "    G" + std::to_string(y) + " ";
                    for (std::size_t x = y * w; x < std::min(g.size(), (y + 1) * w); ++x) {
                        out += hex16(g[x]);
                    }
                    out += "\n";
                }
            }
        } else {
            out += "  GRID BAD " + (!hs ? hs.error() : gs.error()) + "\n";
            ok = false;
        }
        for (std::string_view name : {"GELAENDE", "PROD2", "FLUESSE"}) {
            if (std::find(seen.begin(), seen.end(), name) == seen.end()) {
                continue;
            }
            auto bs = i.blocks(name);
            if (!bs) {
                out += "  BLOCKS " + std::string(name) + " BAD " + bs.error() + "\n";
                ok = false;
                continue;
            }
            std::size_t items = 0;
            formats::ByteBuffer bytes;
            for (const auto& b : bs.value()) {
                items += b.data.size() / 6;
                const auto hb = b.header_bytes();
                bytes.insert(bytes.end(), hb.begin(), hb.end());
                formats::put_bytes(bytes, b.data);
            }
            out += "  BLOCKS " + std::string(name) + " count=" + std::to_string(bs.value().size()) +
                   " items=" + std::to_string(items) + " hash=" + hex64(fnv64(bytes)) + "\n";
        }
    }
    return ok;
}

} // namespace anno::dump
