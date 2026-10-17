// `figuren`, `anlagen`, `frames` (formats in docs/formats/figuren.md and
// docs/formats/anlagen.md; reference: tools/figuren.py).
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

#include <anno/formats/anlagen.hpp>
#include <anno/formats/figuren.hpp>

#include "dump.hpp"

namespace anno::dump {

namespace fig = formats::figuren;

namespace {

std::string names(const std::vector<formats::chunk::Chunk>& chunks) {
    std::string s;
    for (std::size_t i = 0; i < chunks.size(); ++i) {
        s += (i ? "," : "") + std::string(chunks[i].name());
    }
    return s;
}

void dump_gfx_set(std::string& out, const std::string& label, const std::string& indent,
                  const fig::GfxSet& g, bool full) {
    formats::ByteBuffer other;
    for (const auto& r : g.records) {
        if (r.name() != "TEXTURES" && r.name() != "SHADOW") {
            formats::put_bytes(other, r.payload);
        }
    }
    out += indent + label + " prefix=" + hex(g.prefix) + " records=" + names(g.records) +
           " other=" + hex64(fnv64(other)) + "\n";
    for (const auto& fs : g.frame_sets) {
        out += indent + "  TEXTURES group=" + std::to_string(fs.group) +
               " frames=" + std::to_string(fs.frames.size()) +
               " hash=" + hex64(fnv64(fs.to_bytes())) + "\n";
        if (!full) {
            continue;
        }
        for (std::size_t k = 0; k < fs.frames.size(); ++k) {
            const auto& f = fs.frames[k];
            out += indent + "    FRAME k=" + std::to_string(k) +
                   " width=" + std::to_string(f.width) + " height=" + std::to_string(f.height) +
                   " palette=" + std::to_string(f.palette) +
                   " u06=" + std::to_string(f.unknown_06) + " format=" + quote(f.format_raw) +
                   " anchor=" + std::to_string(f.anchor_x) + "," + std::to_string(f.anchor_y) +
                   " offset=" + std::to_string(f.offset) + " length=" + std::to_string(f.length) +
                   " u20=" + std::to_string(f.unknown_20) + "\n";
        }
    }
    if (g.shadow) {
        dump_gfx_set(out, "SHADOW", indent + "  ", *g.shadow, full);
    }
}

/// The ENTRY block shared by `figuren` and `anlagen`.
void dump_entry(std::string& out, const fig::Entry& e, bool full) {
    out += "ENTRY id=" + std::to_string(e.id) + " flag=" + std::to_string(e.flag) +
           " records=" + names(e.records) + "\n";
    for (const auto& r : e.records) {
        if (r.name() == "COLORS" || r.name() == "HAEUSER") {
            continue;
        }
        out += "  RECORD " + std::string(r.name()) + " size=" + std::to_string(r.payload.size()) +
               " hash=" + hex64(fnv64(r.payload)) + "\n";
    }
    if (!e.palette_ids.empty()) {
        out += "  COLORIDS ids=";
        for (std::size_t i = 0; i < e.palette_ids.size(); ++i) {
            out += (i ? "," : "") + std::to_string(e.palette_ids[i]);
        }
        out += "\n";
    }
    for (std::size_t i = 0; i < e.palettes.size(); ++i) {
        const auto& p = e.palettes[i].palette;
        out += "  COLORTABLE i=" + std::to_string(i) + " id=" + std::to_string(p.id) +
               " flag=" + std::to_string(p.flag) + " rgb=" + palette_hash(p) +
               " name=" + quote(e.palettes[i].name) + "\n";
    }
    for (std::size_t i = 0; i < e.gfx.size(); ++i) {
        dump_gfx_set(out, "SET i=" + std::to_string(i), "  ", e.gfx[i], full);
    }
}

std::optional<formats::ByteBuffer> read_all(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return formats::ByteBuffer(std::istreambuf_iterator<char>(in), {});
}

/// Concatenated Figuren<g><nn>.dat of one zoom group (empty if none).
formats::ByteBuffer read_volume(const std::filesystem::path& dir, std::uint32_t group) {
    formats::ByteBuffer data;
    for (std::uint32_t part = 0;; ++part) {
        auto b = read_all(dir / fig::volume_file_name(group, part));
        if (!b) {
            break;
        }
        data.insert(data.end(), b->begin(), b->end());
    }
    return data;
}

/// `width= height= format= opaque= pixels=`; pixels = FNV-1a 64 over 0 per
/// transparent pixel and 1, index, z, level per covered one, or BAD.
bool frame_line(const std::vector<formats::ByteBuffer>& vols, std::uint32_t group,
                const fig::Frame& f, std::string& out) {
    std::optional<fig::Pixels> px;
    const auto fmt = f.format();
    if (fmt && group < vols.size() && std::uint64_t{f.offset} + f.length <= vols[group].size()) {
        const Bytes data(vols[group].data() + f.offset, f.length);
        auto d = fig::decode_frame(*fmt, data, f.width, f.height);
        if (d) {
            px = std::move(d).value();
        }
    }
    std::size_t opaque = 0;
    std::string pixels = "BAD";
    if (px) {
        formats::ByteBuffer flat;
        for (const auto& p : *px) {
            if (p) {
                flat.insert(flat.end(), {1, p->index, p->z, p->level});
                ++opaque;
            } else {
                flat.push_back(0);
            }
        }
        pixels = hex64(fnv64(flat));
    }
    out += "width=" + std::to_string(f.width) + " height=" + std::to_string(f.height) +
           " format=" + quote(f.format_raw) + " opaque=" + std::to_string(opaque) +
           " pixels=" + pixels + "\n";
    return px.has_value();
}

} // namespace

bool dump_figuren(Bytes buf, bool full, std::string& out) {
    auto h = fig::FigurenHeader::parse(buf);
    if (!h) {
        std::fprintf(stderr, "NOPARSE: %s\n", h.error().c_str());
        out += "NOPARSE\n";
        return false;
    }
    out += "FIGUREN entries=" + std::to_string(h.value().entries.size()) +
           " volumes=" + std::to_string(h.value().volumes.size()) + "\n";
    for (const auto& v : h.value().volumes) {
        out += "VOLUME group=" + std::to_string(v.group) + " size=" + std::to_string(v.size) +
               " frames=" + std::to_string(v.frames) + "\n";
    }
    for (const auto& e : h.value().entries) {
        dump_entry(out, e, full);
    }
    return true;
}

bool dump_anlagen(Bytes buf, bool full, std::string& out) {
    auto f = formats::anlagen::AnlagenFile::parse(buf);
    if (!f) {
        std::fprintf(stderr, "NOPARSE: %s\n", f.error().c_str());
        out += "NOPARSE\n";
        return false;
    }
    out += "ANLAGEN entries=" + std::to_string(f.value().entries.size()) +
           " volumes=" + std::to_string(f.value().volumes.size()) + "\n";
    for (const auto& v : f.value().volumes) {
        out += "VOLUME group=" + std::to_string(v.group) + " frames=" + std::to_string(v.frames) +
               " bytes=" + std::to_string(v.data.size()) + "\n";
    }
    for (const auto& e : f.value().entries) {
        dump_entry(out, e, full);
    }
    return true;
}

int dump_frames(const std::string& dir_arg, const std::vector<std::string>& ids) {
    const std::filesystem::path dir(dir_arg);
    auto buf = read_all(dir / "FigurenHeader.dat");
    if (!buf) {
        std::fprintf(stderr, "%s: cannot read FigurenHeader.dat\n", dir_arg.c_str());
        return 1;
    }
    auto h = fig::FigurenHeader::parse(*buf);
    if (!h) {
        std::fprintf(stderr, "%s: %s\n", dir_arg.c_str(), h.error().c_str());
        return 1;
    }
    std::vector<formats::ByteBuffer> vols;
    for (std::uint32_t g = 0; g < 3; ++g) {
        vols.push_back(read_volume(dir, g));
    }
    std::string out;
    bool ok = true;
    for (const auto& id : ids) {
        const fig::Entry* e = nullptr;
        for (const auto& x : h.value().entries) {
            if (std::to_string(x.id) == id) {
                e = &x;
                break;
            }
        }
        if (e == nullptr) {
            out += "ENTRY id=" + id + " MISSING\n";
            ok = false;
            continue;
        }
        out += "ENTRY id=" + id + " sets=" + std::to_string(e->gfx.size()) + "\n";
        for (std::size_t i = 0; i < e->gfx.size(); ++i) {
            const fig::GfxSet* sets[2] = {&e->gfx[i], e->gfx[i].shadow.get()};
            for (int shadow = 0; shadow < 2; ++shadow) {
                if (sets[shadow] == nullptr) {
                    continue;
                }
                for (const auto& fs : sets[shadow]->frame_sets) {
                    for (std::size_t k = 0; k < fs.frames.size(); ++k) {
                        out +=
                            "FRAME set=" + std::to_string(i) + " shadow=" + std::to_string(shadow) +
                            " group=" + std::to_string(fs.group) + " k=" + std::to_string(k) + " ";
                        ok = frame_line(vols, fs.group, fs.frames[k], out) && ok;
                    }
                }
            }
        }
    }
    std::fwrite(out.data(), 1, out.size(), stdout);
    return ok ? 0 : 1;
}

} // namespace anno::dump
