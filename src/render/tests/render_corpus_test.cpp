// Scene renders against the game files (ANNO1503_GAME_DIR; skips otherwise).
//
// Every 0200Single scenario and one island template, rendered with all
// layers: the whole scene at zoom group 2 (original water at 0 ms, enhanced
// water at 2,500 ms) and a 1600 × 900 region at the centre at group 1 (both
// styles at 700 ms), ships and soldiers included. Each line holds the
// FNV-1a 64 digest of the RGBA canvas and the draw counts; the expected
// lines are those of the Rust reference (crates/render, 2026-10-17), so the
// C++ renderer is pixel-identical to it (docs/render/iso.md, "Fidelity").
// Set ANNO_PRINT_DIGESTS=1 to print the lines.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/assets/figure.hpp>
#include <anno/assets/unit.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/formats/insel.hpp>
#include <anno/formats/sze.hpp>
#include <anno/formats/tex.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>
#include <anno/test/game_dir.hpp>

namespace render = anno::render;
namespace fig = anno::formats::figuren;

namespace {

std::uint64_t fnv(const std::vector<std::uint8_t>& b) {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (const std::uint8_t x : b) {
        h ^= x;
        h *= 0x100000001b3ULL;
    }
    return h;
}

std::string hex(std::uint64_t v) {
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
    return buf;
}

const char* style_name(render::WaterStyle s) {
    return s == render::WaterStyle::Original ? "Original" : "Enhanced";
}

std::vector<std::string> digest_lines(const std::filesystem::path& dir) {
    const auto felder_buf = anno::test::read_file(dir / "Textures" / "Felder.tex");
    const auto anl_buf = anno::test::read_file(dir / "Anlagen.dat");
    const auto hbuf = anno::test::read_file(dir / "FigurenHeader.dat");
    const auto header = fig::FigurenHeader::parse(hbuf).value();
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
    auto files = anno::test::files_with_ext(dir / "Scenes" / "0200Single", "sze");
    files.push_back(dir / "Scenes" / "Islands" / "Nord" / "N_064x064_0001_0B.scp");
    std::vector<std::string> out;
    for (const auto& p : files) {
        const auto buf = anno::test::read_file(p);
        auto anlagen = anno::formats::anlagen::AnlagenFile::parse(anl_buf).value();
        const auto felder = anno::formats::tex::TexFile::parse(felder_buf).value();
        auto textures = anno::assets::island::GroundTextures::load(felder).value();
        if (const auto c = render::water_colour(anlagen)) {
            textures.set_water(*c);
        }
        render::Scene scene("", anno::formats::insel::islands(buf).value(), std::move(textures),
                            std::move(anlagen), render::kHeightLift);
        if (auto s = anno::formats::sze::Scene::parse(buf)) {
            const anno::assets::unit::VolumeReader read =
                [&vols](std::uint32_t g, std::uint64_t o,
                        std::uint64_t l) -> std::optional<std::vector<std::uint8_t>> {
                if (g >= vols.size() || o + l > vols[g].size()) {
                    return std::nullopt;
                }
                const auto b = vols[g].begin() + static_cast<std::ptrdiff_t>(o);
                return std::vector<std::uint8_t>(b, b + static_cast<std::ptrdiff_t>(l));
            };
            const std::array kinds = {anno::assets::unit::UnitKind::Ship,
                                      anno::assets::unit::UnitKind::Soldier};
            auto units =
                anno::assets::unit::units(s.value(), header, kinds,
                                          anno::assets::figure::kOriginalPlayerColours, read)
                    .value();
            for (auto& u : units.units) {
                std::optional<float> z;
                if (u.kind == anno::assets::unit::UnitKind::Soldier) {
                    z = scene.ground_height(u.x, u.y);
                    if (!z) {
                        continue;
                    }
                }
                render::UnitSprite sprite{u.x, u.y, z, {}, {}};
                for (const auto& f : u.frames) {
                    sprite.frames.push_back(
                        f ? std::optional<render::UnitFrame>({f->frame, f->image}) : std::nullopt);
                }
                for (const auto& f : u.shadows) {
                    sprite.shadows.push_back(
                        f ? std::optional<render::UnitFrame>({f->frame, f->image}) : std::nullopt);
                }
                scene.units.push_back(std::move(sprite));
            }
        }
        const std::string name = p.filename().string();
        for (const auto& [time, style] :
             {std::pair{std::uint64_t{0}, render::WaterStyle::Original},
              std::pair{std::uint64_t{2500}, render::WaterStyle::Enhanced}}) {
            const auto rect = *scene.bounds(2);
            render::Layers layers = render::Layers::all();
            layers.time_ms = time;
            layers.water_style = style;
            const auto [c, st] = scene.render(rect, 2, layers);
            out.push_back(name + " g2 t" + std::to_string(time) + " " + style_name(style) + " " +
                          std::to_string(rect.w) + "x" + std::to_string(rect.h) + " " +
                          hex(fnv(c.pixels)) + " drawn=" + std::to_string(st.objects.drawn) +
                          " inv=" + std::to_string(st.objects.invisible) + " failed=" +
                          std::to_string(st.objects.failed) + " units=" + std::to_string(st.units));
        }
        const auto b = *scene.bounds(1);
        const render::Rect rect{b.x + b.w / 2 - 800, b.y + b.h / 2 - 450, 1600, 900};
        for (const auto style : {render::WaterStyle::Original, render::WaterStyle::Enhanced}) {
            render::Layers layers = render::Layers::all();
            layers.time_ms = 700;
            layers.water_style = style;
            const auto [c, st] = scene.render(rect, 1, layers);
            out.push_back(name + " g1 region " + style_name(style) + " " + hex(fnv(c.pixels)) +
                          " drawn=" + std::to_string(st.objects.drawn) +
                          " units=" + std::to_string(st.units));
        }
    }
    return out;
}

const std::vector<std::string> kExpected = {
    "Single_01.sze g2 t0 Original 16960x9170 2f0395814262894d drawn=69864 inv=11717 failed=0 "
    "units=5",
    "Single_01.sze g2 t2500 Enhanced 16960x9170 17b0c9717b25ab6b drawn=69297 inv=11717 failed=0 "
    "units=5",
    "Single_01.sze g1 region Original f3c82ff0f061887a drawn=24371 units=0",
    "Single_01.sze g1 region Enhanced 38fa6edcf261fb33 drawn=24115 units=0",
    "Single_05.sze g2 t0 Original 9040x3291 2ab607c8c9c0890b drawn=12628 inv=1240 failed=0 units=2",
    "Single_05.sze g2 t2500 Enhanced 9040x3291 84964184f8e64d2a drawn=12628 inv=1240 failed=0 "
    "units=2",
    "Single_05.sze g1 region Original c90b2060847e2a1f drawn=4603 units=0",
    "Single_05.sze g1 region Enhanced 7bc859ab6cbbe7d0 drawn=4603 units=0",
    "Single_06.sze g2 t0 Original 7280x4451 3f1ba536788d1526 drawn=13778 inv=3084 failed=0 units=3",
    "Single_06.sze g2 t2500 Enhanced 7280x4451 42c0e9aef5355085 drawn=13639 inv=3084 failed=0 "
    "units=3",
    "Single_06.sze g1 region Original 81d690c94bbb0fc5 drawn=599 units=0",
    "Single_06.sze g1 region Enhanced 27da1a1ded12c916 drawn=599 units=0",
    "Single_08.sze g2 t0 Original 4400x2211 d00b949e4837b656 drawn=7537 inv=1604 failed=0 units=1",
    "Single_08.sze g2 t2500 Enhanced 4400x2211 129a47786c8ec006 drawn=7537 inv=1604 failed=0 "
    "units=1",
    "Single_08.sze g1 region Original 7837282ad86bf737 drawn=6670 units=0",
    "Single_08.sze g1 region Enhanced 226bdaf64c2c1073 drawn=6670 units=0",
    "Single_10.sze g2 t0 Original 17440x9131 059c44e48f227c12 drawn=53212 inv=8756 failed=0 "
    "units=1",
    "Single_10.sze g2 t2500 Enhanced 17440x9131 2ab3fec630bb615a drawn=52890 inv=8756 failed=0 "
    "units=1",
    "Single_10.sze g1 region Original 13acfeb61e56be15 drawn=0 units=0",
    "Single_10.sze g1 region Enhanced 135943759028480b drawn=0 units=0",
    "Single_12.sze g2 t0 Original 3920x2491 9302ccbdc2a89d62 drawn=8342 inv=1282 failed=0 units=1",
    "Single_12.sze g2 t2500 Enhanced 3920x2491 936c1a3a4a47af2e drawn=8342 inv=1282 failed=0 "
    "units=1",
    "Single_12.sze g1 region Original ccecc1c9e8ce52c9 drawn=7295 units=0",
    "Single_12.sze g1 region Enhanced f75cd5ed42ab42b5 drawn=7295 units=0",
    "Single_14.sze g2 t0 Original 14080x9291 23b01ba36bfbcc8c drawn=47209 inv=9450 failed=0 "
    "units=3",
    "Single_14.sze g2 t2500 Enhanced 14080x9291 9c1d747545eab453 drawn=47077 inv=9450 failed=0 "
    "units=3",
    "Single_14.sze g1 region Original 323ce1b9064247a3 drawn=16735 units=1",
    "Single_14.sze g1 region Enhanced a5f9c1083372b79b drawn=16684 units=1",
    "Single_15.sze g2 t0 Original 9120x3611 5344dcbd6f6ded86 drawn=8216 inv=1082 failed=0 units=2",
    "Single_15.sze g2 t2500 Enhanced 9120x3611 8d72f483f5a43e1a drawn=8216 inv=1082 failed=0 "
    "units=2",
    "Single_15.sze g1 region Original 13acfeb61e56be15 drawn=0 units=0",
    "Single_15.sze g1 region Enhanced f6d3a04f6785983f drawn=0 units=0",
    "Single_16.sze g2 t0 Original 7200x4051 75e8330f63ced1f2 drawn=8743 inv=239 failed=0 units=3",
    "Single_16.sze g2 t2500 Enhanced 7200x4051 98c476dc599b849f drawn=8743 inv=239 failed=0 "
    "units=3",
    "Single_16.sze g1 region Original 7a3fa2adca976f53 drawn=1384 units=0",
    "Single_16.sze g1 region Enhanced 8a3caa97f52c354a drawn=1384 units=0",
    "Single_24.sze g2 t0 Original 7200x2811 768dbcffaa6380e2 drawn=24577 inv=2880 failed=0 "
    "units=541",
    "Single_24.sze g2 t2500 Enhanced 7200x2811 3bb67eb647dd2d9d drawn=24411 inv=2880 failed=0 "
    "units=541",
    "Single_24.sze g1 region Original 610c80610617b85a drawn=23697 units=0",
    "Single_24.sze g1 region Enhanced 22b256621848c823 drawn=23531 units=0",
    "N_064x064_0001_0B.scp g2 t0 Original 1280x891 d73b254ebe87486f drawn=949 inv=338 failed=0 "
    "units=0",
    "N_064x064_0001_0B.scp g2 t2500 Enhanced 1280x891 5118c7e5cf5c0b0f drawn=930 inv=338 failed=0 "
    "units=0",
    "N_064x064_0001_0B.scp g1 region Original 95cafb25426a2a9d drawn=949 units=0",
    "N_064x064_0001_0B.scp g1 region Enhanced f24622877f58bc18 drawn=930 units=0",
};

} // namespace

TEST_CASE("scene renders match the Rust reference") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto lines = digest_lines(*dir);
    if (std::getenv("ANNO_PRINT_DIGESTS") != nullptr) {
        for (const auto& l : lines) {
            std::printf("%s\n", l.c_str());
        }
    }
    REQUIRE(lines.size() == kExpected.size());
    for (std::size_t i = 0; i < lines.size(); ++i) {
        CHECK(lines[i] == kExpected[i]);
    }
}

TEST_CASE("frames (chunk cache and composition) match the Rust reference") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // FrameRenderer with 4 threads, chunks built in the frame; a 1280 × 720
    // view at the centre of each zoom group, 0RGB words hashed as LE bytes.
    // Expected: Rust reference (crates/render, 2026-10-17).
    const std::vector<std::string> expected = {
        "Single_01.sze g2 t0 Original f435559f2e90bf6c",
        "Single_01.sze g2 t900 Original 56d4608c02dec8b8",
        "Single_01.sze g2 t900 Enhanced 5f5038c7a0eb3691",
        "Single_01.sze g1 t0 Original 09a628d338b317de",
        "Single_01.sze g1 t900 Original b1981bfd551d3f3e",
        "Single_01.sze g1 t900 Enhanced a5627197b93411f5",
        "Single_01.sze g0 t0 Original 8755589201feb24a",
        "Single_01.sze g0 t900 Original 8433a14744329f0f",
        "Single_01.sze g0 t900 Enhanced 5298e2b2d0786b1f",
        "Single_24.sze g2 t0 Original e7894f5a1560f7d2",
        "Single_24.sze g2 t900 Original 154c9a87f81f5ff5",
        "Single_24.sze g2 t900 Enhanced 349052c9e9383e0a",
        "Single_24.sze g1 t0 Original a17a423d76e40c10",
        "Single_24.sze g1 t900 Original 9c02fbfb3767584d",
        "Single_24.sze g1 t900 Enhanced cb6ad0fe841fc554",
        "Single_24.sze g0 t0 Original 84d6d908bfd83e6a",
        "Single_24.sze g0 t900 Original 84d6d908bfd83e6a",
        "Single_24.sze g0 t900 Enhanced 84d6d908bfd83e6a",
    };
    const auto felder_buf = anno::test::read_file(*dir / "Textures" / "Felder.tex");
    const auto anl_buf = anno::test::read_file(*dir / "Anlagen.dat");
    std::vector<std::string> lines;
    for (const char* name : {"Single_01.sze", "Single_24.sze"}) {
        const auto buf = anno::test::read_file(*dir / "Scenes" / "0200Single" / name);
        auto anlagen = anno::formats::anlagen::AnlagenFile::parse(anl_buf).value();
        const auto felder = anno::formats::tex::TexFile::parse(felder_buf).value();
        auto textures = anno::assets::island::GroundTextures::load(felder).value();
        if (const auto c = render::water_colour(anlagen)) {
            textures.set_water(*c);
        }
        const render::Scene scene("", anno::formats::insel::islands(buf).value(),
                                  std::move(textures), std::move(anlagen), render::kHeightLift);
        render::FrameRenderer fr(4);
        for (const std::size_t group : {std::size_t{2}, std::size_t{1}, std::size_t{0}}) {
            const auto b = *scene.bounds(group);
            const render::Rect view{b.x + b.w / 2 - 640, b.y + b.h / 2 - 360, 1280, 720};
            for (const auto& [time, style] :
                 {std::pair{std::uint64_t{0}, render::WaterStyle::Original},
                  std::pair{std::uint64_t{900}, render::WaterStyle::Original},
                  std::pair{std::uint64_t{900}, render::WaterStyle::Enhanced}}) {
                std::vector<std::uint32_t> out(1280 * 720);
                fr.render(scene, view, group, time, style, out.data());
                std::vector<std::uint8_t> bytes;
                bytes.reserve(out.size() * 4);
                for (const std::uint32_t px : out) {
                    for (int k = 0; k < 4; ++k) {
                        bytes.push_back(static_cast<std::uint8_t>(px >> (8 * k)));
                    }
                }
                lines.push_back(std::string(name) + " g" + std::to_string(group) + " t" +
                                std::to_string(time) + " " + style_name(style) + " " +
                                hex(fnv(bytes)));
            }
        }
    }
    REQUIRE(lines.size() == expected.size());
    for (std::size_t i = 0; i < lines.size(); ++i) {
        CHECK(lines[i] == expected[i]);
    }
}
