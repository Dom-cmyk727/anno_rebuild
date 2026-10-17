// anno_assets against the game files (ANNO1503_GAME_DIR; skips otherwise).
//
// Digests: FNV-1a 64 over every decoded image in a fixed order (per image
// u16 width LE, u16 height LE, the RGBA bytes; a failed image adds
// FF FF FF FF). The expected values are those of the Rust reference
// implementation (crates/assets, 2026-10-17), so the port is pixel-identical
// on the whole corpus; docs/render/assets.md lists the order and the counts.
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/assets/building.hpp>
#include <anno/assets/figure.hpp>
#include <anno/assets/island.hpp>
#include <anno/assets/texture.hpp>
#include <anno/assets/unit.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/formats/insel.hpp>
#include <anno/formats/sze.hpp>
#include <anno/formats/tex.hpp>
#include <anno/test/game_dir.hpp>

namespace assets = anno::assets;
namespace fig = anno::formats::figuren;
namespace sze = anno::formats::sze;
using anno::formats::Bytes;

namespace {

struct Fnv {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    std::size_t ok = 0, failed = 0;

    void bytes(Bytes b) {
        for (const std::uint8_t x : b) {
            h ^= x;
            h *= 0x100000001b3ULL;
        }
    }
    void u16(std::uint16_t v) {
        const std::array<std::uint8_t, 2> b = {static_cast<std::uint8_t>(v & 0xFF),
                                               static_cast<std::uint8_t>(v >> 8)};
        bytes(b);
    }
    void u32(std::uint32_t v) {
        std::array<std::uint8_t, 4> b{};
        for (std::size_t i = 0; i < 4; ++i) {
            b[i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF);
        }
        bytes(b);
    }
    void text(std::string_view s) {
        bytes(Bytes(reinterpret_cast<const std::uint8_t*>(s.data()), s.size()));
    }
    void img(const assets::Rgba* r) {
        if (r != nullptr) {
            ++ok;
            u16(r->width);
            u16(r->height);
            bytes(r->pixels);
        } else {
            ++failed;
            const std::array<std::uint8_t, 4> marker = {0xFF, 0xFF, 0xFF, 0xFF};
            bytes(marker);
        }
    }
    template <class R> void result(const R& r) { img(r ? &r.value() : nullptr); }
};

std::vector<const fig::GfxSet*> with_shadow(const fig::GfxSet& g) {
    std::vector<const fig::GfxSet*> v = {&g};
    if (g.shadow) {
        v.push_back(g.shadow.get());
    }
    return v;
}

std::uint32_t bits(float f) {
    std::uint32_t u = 0;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

/// The concatenated volume of each zoom group 0–2.
std::vector<std::vector<std::uint8_t>> figure_volumes(const std::filesystem::path& dir) {
    std::vector<std::vector<std::uint8_t>> vols(3);
    for (std::uint32_t g = 0; g < 3; ++g) {
        for (std::uint32_t part = 0;; ++part) {
            const auto p = dir / fig::volume_file_name(g, part);
            if (!std::filesystem::exists(p)) {
                break;
            }
            const auto d = anno::test::read_file(p);
            vols[g].insert(vols[g].end(), d.begin(), d.end());
        }
    }
    return vols;
}

} // namespace

TEST_CASE("textures, buildings, figures, units and ground match the Rust reference") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // Every texture of every .tex file.
    Fnv t;
    for (const auto& p : anno::test::files_with_ext(*dir, "tex")) {
        const auto buf = anno::test::read_file(p);
        auto f = anno::formats::tex::TexFile::parse(buf);
        if (!f) {
            t.text("NOPARSE");
            continue;
        }
        for (const auto& x : f.value().textures) {
            t.result(assets::to_rgba(f.value(), x));
        }
    }
    CHECK(t.ok == 5176);
    CHECK(t.failed == 0);
    CHECK(t.h == 0xd4bba222e9fbf012ULL);

    // Every frame of every Anlagen.dat sprite set and its shadow.
    const auto abuf = anno::test::read_file(*dir / "Anlagen.dat");
    auto a = anno::formats::anlagen::AnlagenFile::parse(abuf);
    REQUIRE(a);
    Fnv b;
    for (const auto& e : a.value().entries) {
        for (const auto& g : e.gfx) {
            for (const fig::GfxSet* s : with_shadow(g)) {
                for (const auto& fs : s->frame_sets) {
                    for (const auto& f : fs.frames) {
                        b.result(assets::building::to_rgba(a.value(), e, fs.group, f));
                    }
                }
            }
        }
    }
    CHECK(b.ok == 28899);
    CHECK(b.failed == 0);
    CHECK(b.h == 0x4d53061488caa2b0ULL);

    // Every figure frame; frames of player-coloured types also in colour 2.
    const auto hbuf = anno::test::read_file(*dir / "FigurenHeader.dat");
    auto h = fig::FigurenHeader::parse(hbuf);
    REQUIRE(h);
    const auto vols = figure_volumes(*dir);
    const auto red = assets::figure::kOriginalPlayerColours[1];
    Fnv fg;
    std::size_t coloured_entries = 0;
    for (const auto& e : h.value().entries) {
        const bool coloured = assets::figure::player_coloured(e);
        coloured_entries += coloured ? 1 : 0;
        for (const auto& g : e.gfx) {
            for (const fig::GfxSet* s : with_shadow(g)) {
                for (const auto& fs : s->frame_sets) {
                    const Bytes vol = vols.at(fs.group);
                    for (const auto& f : fs.frames) {
                        fg.result(assets::figure::to_rgba(e, f, vol));
                        if (coloured) {
                            const auto data = assets::figure::frame_data(f, vol);
                            if (data) {
                                fg.result(assets::figure::decode(e, f, *data, red));
                            } else {
                                fg.img(nullptr);
                            }
                        }
                    }
                }
            }
        }
    }
    CHECK(coloured_entries == 69);
    CHECK(fg.ok == 943446);
    CHECK(fg.failed == 0);
    CHECK(fg.h == 0x7ea22e33731cbd26ULL);

    // Ships and soldiers of every scenario.
    Fnv u;
    assets::unit::UnitStats total;
    const std::array kinds = {assets::unit::UnitKind::Ship, assets::unit::UnitKind::Soldier};
    const assets::unit::VolumeReader read =
        [&vols](std::uint32_t g, std::uint64_t o,
                std::uint64_t l) -> std::optional<std::vector<std::uint8_t>> {
        if (g >= vols.size() || o + l > vols[g].size()) {
            return std::nullopt;
        }
        const auto begin = vols[g].begin() + static_cast<std::ptrdiff_t>(o);
        return std::vector<std::uint8_t>(begin, begin + static_cast<std::ptrdiff_t>(l));
    };
    for (const auto& p : anno::test::files_with_ext(*dir / "Scenes", "sze")) {
        const auto buf = anno::test::read_file(p);
        auto scene = sze::Scene::parse(buf);
        REQUIRE(scene);
        auto list = assets::unit::units(scene.value(), h.value(), kinds,
                                        assets::figure::kOriginalPlayerColours, read);
        REQUIRE(list);
        total.units += list.value().stats.units;
        total.missing += list.value().stats.missing;
        total.failed_frames += list.value().stats.failed_frames;
        for (const auto& x : list.value().units) {
            const std::array<std::uint8_t, 1> kind = {
                static_cast<std::uint8_t>(x.kind == assets::unit::UnitKind::Ship ? 0 : 1)};
            u.bytes(kind);
            u.u16(x.figure);
            u.u32(bits(x.x));
            u.u32(bits(x.y));
            for (const auto* frames : {&x.frames, &x.shadows}) {
                for (const auto& fr : *frames) {
                    if (fr) {
                        u.u32(fr->frame.offset);
                        u.img(fr->image.get());
                    } else {
                        u.text("none");
                    }
                }
            }
            u.text("|");
        }
    }
    CHECK(total == assets::unit::UnitStats{2644, 0, 0});
    CHECK(u.ok == 14883);
    CHECK(u.failed == 0);
    CHECK(u.h == 0xbb8c007fcecc115cULL);

    // Top-down ground of every island of the 0200Single scenarios, 4 px per tile.
    const auto fbuf = anno::test::read_file(*dir / "Textures" / "Felder.tex");
    auto felder = anno::formats::tex::TexFile::parse(fbuf);
    REQUIRE(felder);
    auto ground = assets::island::GroundTextures::load(felder.value());
    REQUIRE(ground);
    Fnv gd;
    for (const auto& p : anno::test::files_with_ext(*dir / "Scenes" / "0200Single", "sze")) {
        const auto buf = anno::test::read_file(p);
        auto islands = anno::formats::insel::islands(buf);
        REQUIRE(islands);
        for (const auto& i : islands.value()) {
            gd.result(assets::island::ground_top_down(i, ground.value(), 4));
        }
    }
    CHECK(gd.ok == 99);
    CHECK(gd.failed == 0);
    CHECK(gd.h == 0xf3d27f9beafdc758ULL);
}

namespace {

/// Palette colours that the player variant changes.
int hits(const anno::formats::tex::Palette& p) {
    int n = 0;
    for (unsigned i = 0; i < 256; ++i) {
        const auto c = p.rgb(static_cast<std::uint8_t>(i));
        n += assets::figure::recolour(c, assets::figure::kOriginalPlayerColours[0]) != c ? 1 : 0;
    }
    return n;
}

} // namespace

TEST_CASE("player colours: coloured figures and green-test hits") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto buf = anno::test::read_file(*dir / "FigurenHeader.dat");
    auto header = fig::FigurenHeader::parse(buf);
    REQUIRE(header);
    std::vector<std::uint32_t> coloured;
    for (const auto& e : header.value().entries) {
        if (assets::figure::player_coloured(e)) {
            coloured.push_back(e.id);
        }
    }
    CHECK(coloured.size() == 69);
    auto has = [&coloured](std::uint32_t id) {
        return std::find(coloured.begin(), coloured.end(), id) != coloured.end();
    };
    CHECK((has(701) && has(718) && has(1543)));
    // No ship type, and the thief (723) keeps its colours.
    for (std::uint32_t id = 1501; id <= 1511; ++id) {
        CHECK_FALSE(has(id));
    }
    CHECK_FALSE(has(723));
    const fig::Entry* archer = header.value().entry(701);
    REQUIRE(archer != nullptr);
    CHECK(hits(archer->palettes.at(0).palette) == 48);

    const auto abuf = anno::test::read_file(*dir / "Anlagen.dat");
    auto anlagen = anno::formats::anlagen::AnlagenFile::parse(abuf);
    REQUIRE(anlagen);
    const anno::formats::tex::Palette* flag = nullptr;
    for (const auto& t : anlagen.value().colors) {
        if (t.palette.id == 109) {
            flag = &t.palette;
            break;
        }
    }
    REQUIRE(flag != nullptr);
    CHECK(hits(*flag) == 30);
}

TEST_CASE("player colours at game start over all scenarios") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::size_t removed = 0, coloured = 0, without_colour = 0;
    for (const auto& p : anno::test::files_with_ext(*dir / "Scenes", "sze")) {
        const auto buf = anno::test::read_file(p);
        auto scene = sze::Scene::parse(buf);
        REQUIRE(scene);
        auto start = assets::unit::start_colours(scene.value());
        REQUIRE(start);
        if (p.filename() == "Single_24.sze") {
            // All eight preset in the scenario (player 0 = colour 2).
            const std::map<std::uint32_t, std::uint8_t> expect = {{0, 2}, {1, 1}, {2, 6}, {3, 3},
                                                                  {4, 4}, {5, 5}, {6, 8}, {7, 7}};
            CHECK(start.value().slots == expect);
        }
        removed += start.value().removed.size();
        coloured += start.value().slots.size();
        // Every human or computer player that is not removed has a colour,
        // unless the pool ran out.
        for (const auto& pl : scene.value().players().value()) {
            const auto hd = pl.head();
            if (hd && pl.index != 31 && (hd->is_human() || hd->is_computer()) &&
                !start.value().removed.contains(pl.index) &&
                !start.value().slots.contains(pl.index) && hd->colour != 0) {
                ++without_colour;
            }
        }
    }
    CHECK(coloured == 130);
    CHECK(removed == 0);
    CHECK(without_colour == 0);
}
