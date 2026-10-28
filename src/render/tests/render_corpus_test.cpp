// Scene renders against the game files (ANNO1503_GAME_DIR; skips otherwise).
//
// Every 0200Single scenario and one island template, rendered with all
// layers: the whole scene at zoom group 2 (original water at 0 ms, enhanced
// water at 2,500 ms) and a 1600 × 900 region at the centre at group 1 (both
// styles at 700 ms), ships and soldiers included. Each line holds the
// FNV-1a 64 digest of the RGBA canvas and the draw counts; the expected
// lines are those of the Rust reference (crates/render/examples/digest_scene.rs
// `scenes` and `frames`, 2026-10-17/18), so the
// C++ renderer is pixel-identical to it (docs/render/iso.md, "Fidelity").
// Exception since 2026-10-18: the Enhanced lines of scenes with rivers come
// from the C++ renderer itself, because enhanced rivers now run through the
// waterfalls and mouths into the sea (docs/render/iso.md, "Rivers into the
// sea"; the Rust reference gets no new features). Every Original line still
// equals the Rust reference.
// Second exception since 2026-10-19 (river gaps, docs/render/iso.md, "River
// gaps"): the enhanced river water now covers the painted riverbed, so the
// Enhanced lines of every scene with river pieces changed: Single_01, _06,
// _10, _14 and _24 at group 2, Single_24's group 1 region and the Single_24
// group 2 / group 1 frames. Unchanged: the group 1 regions of Single_01, _10
// and _14 (`river_qa --rect` finds no river sprite pixel in them; Single_24's
// holds 34,889), Single_05, _08, _12, _15, _16 and N_064x064_0001_0B (no river
// pieces), and the Single_01 frames.
// Third exception since 2026-10-19 (river flow shading, docs/render/rivers.md
// section 15, stage S5): enhanced rivers are shaded by flow-map advection along
// the solved flow (river_flow_shade) instead of the legacy shimmer that slides
// along fixed axes, so the Enhanced lines that contain river pixels changed,
// once, and only those: Single_01 g2 (the 4 river islands, mouths), Single_06
// g2, Single_10 g2, Single_14 g2, Single_24 g2 and Single_24's g1 region
// (34,889 river sprite pixels), and the Single_24 g2 and g1 frames (g0's view
// centre has no river). Unchanged and still checked: every Original line
// (the river path is enhanced-only), the Enhanced lines of scenes or views
// without river pixels (Single_01's g1 region and frames, Single_05, _08,
// _12, _15, _16, N_064x064_0001_0B, Single_24's g0 frame), and a run with
// --river-flow legacy / Layers::river_flow = Legacy reproduces the previous
// digests (test "legacy river shading reproduces the shimmer"). The lines
// differ only in river pixels (checked by pixel comparison against the legacy
// render in the river flow corpus test: no non-river pixel changes).
// Set ANNO_PRINT_DIGESTS=1 to print the lines.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/assets/figure.hpp>
#include <anno/assets/unit.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/formats/insel.hpp>
#include <anno/formats/sze.hpp>
#include <anno/formats/tex.hpp>
#include <anno/render/chunk_reduce.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/river.hpp>
#include <anno/render/scene.hpp>
#include <anno/test/budget.hpp>
#include <anno/test/game_dir.hpp>
#include <anno/test/parallel.hpp>

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

/// The digest lines of the corpus, four per scenario in file order; `only`:
/// just the scenarios with these file names (empty: all); `river_flow`: how
/// enhanced rivers are shaded.
std::vector<std::string>
digest_lines(const std::filesystem::path& dir, const std::vector<std::string_view>& only = {},
             render::RiverFlowMode river_flow = render::RiverFlowMode::Flow) {
    // Felder, Anlagen and the figure volumes are hundreds of megabytes. Loaded
    // on the way into the test, four heavy processes pin them before any
    // picture is admitted and the full map no longer fits. Load them once,
    // after this process holds a reservation.
    struct Catalog {
        std::vector<std::uint8_t> felder_buf;
        std::vector<std::uint8_t> anl_buf;
        std::optional<fig::FigurenHeader> header;
        std::vector<std::vector<std::uint8_t>> vols{3};
    };
    std::shared_ptr<Catalog> catalog;
    std::once_flag catalog_once;
    auto ensure_catalog = [&]() {
        std::call_once(catalog_once, [&] {
            auto loaded = std::make_shared<Catalog>();
            loaded->felder_buf = anno::test::read_file(dir / "Textures" / "Felder.tex");
            loaded->anl_buf = anno::test::read_file(dir / "Anlagen.dat");
            const auto hbuf = anno::test::read_file(dir / "FigurenHeader.dat");
            loaded->header = fig::FigurenHeader::parse(hbuf).value();
            for (std::uint32_t g = 0; g < 3; ++g) {
                for (std::uint32_t part = 0;; ++part) {
                    const auto path = dir / fig::volume_file_name(g, part);
                    if (!std::filesystem::exists(path)) {
                        break;
                    }
                    const auto data = anno::test::read_file(path);
                    loaded->vols[g].insert(loaded->vols[g].end(), data.begin(), data.end());
                }
            }
            catalog = std::move(loaded);
        });
    };
    auto files = anno::test::files_with_ext(dir / "Scenes" / "0200Single", "sze");
    files.push_back(dir / "Scenes" / "Islands" / "Nord" / "N_064x064_0001_0B.scp");
    if (!only.empty()) {
        std::erase_if(files, [&](const auto& f) {
            return std::find(only.begin(), only.end(), f.filename().string()) == only.end();
        });
    }
    // One render per task, four per scenario (whole map at group 2 in both
    // water styles, a group 1 region in both): a few whole-map renders of the
    // largest scenarios take most of the time, so splitting by scenario left
    // most threads idle. Each task builds its own scene; the lines keep the
    // file order.
    constexpr std::size_t kCases = 4;
    // budget 0 here: each item reserves its measured render bytes itself, so a
    // group-2 map is not admitted on the 1 GiB default.
    auto opt = anno::test::parallel_options();
    opt.budget = 0;
    const auto per_case = anno::test::parallel_indexed(
        files.size() * kCases,
        [&](std::size_t ti) {
            const std::size_t fi = ti / kCases;
            const std::size_t ci = ti % kCases;
            const auto& p = files[fi];
            const auto buf = anno::test::read_file(p);
            // One reservation covers setup and the picture, taken before the
            // scene exists. A waiter that also held the setup reservation
            // kept that byte count pending, and the picture was then refused
            // as "exceeds capacity" while every worker still held its setup.
            // Island layout is a lower bound: unit sprites extend the union
            // (Single_01 group 2 is 16960×9170, past the island boxes). When
            // the built scene is larger, drop it and reserve the real
            // rectangle while holding nothing.
            std::uint64_t pixels = 1600ULL * 900ULL;
            if (ci < 2) {
                const auto islands = anno::formats::insel::islands(buf).value();
                std::vector<render::IslandBox> boxes;
                boxes.reserve(islands.size());
                for (const auto& island : islands) {
                    boxes.push_back(render::IslandBox::of(island));
                }
                const auto placed = render::layout(boxes, 2);
                const auto island_bounds = render::scene_bounds(placed);
                if (!island_bounds) {
                    throw std::runtime_error(p.filename().string() + " has no group-2 bounds");
                }
                pixels = static_cast<std::uint64_t>(island_bounds->w) *
                         static_cast<std::uint64_t>(island_bounds->h);
            }
            auto reserve_picture = [](std::uint64_t px) {
                anno::test::Admit info;
                const std::uint64_t bytes = anno::test::cpu_render_setup_bytes() +
                                            anno::test::cpu_render_additional_bytes(px);
                auto hold = anno::test::Budget::process().reserve(bytes, &info);
                if (!hold) {
                    throw std::runtime_error(
                        "[anno-test memory] refused estimate=" + std::to_string(bytes) +
                        " reason=" + info.reason + " available=" + std::to_string(info.available));
                }
                return hold;
            };
            auto picture = reserve_picture(pixels);
            ensure_catalog();
            auto render_cpu = anno::test::Budget::process().acquire_cpu();
            std::unique_ptr<render::Scene> scene;
            auto build_scene = [&]() {
                auto islands = anno::formats::insel::islands(buf).value();
                auto anlagen = anno::formats::anlagen::AnlagenFile::parse(catalog->anl_buf).value();
                const auto felder = anno::formats::tex::TexFile::parse(catalog->felder_buf).value();
                auto textures = anno::assets::island::GroundTextures::load(felder).value();
                if (const auto c = render::water_colour(anlagen)) {
                    textures.set_water(*c);
                }
                scene = std::make_unique<render::Scene>("", std::move(islands), std::move(textures),
                                                        std::move(anlagen), render::kHeightLift);
                if (auto s = anno::formats::sze::Scene::parse(buf)) {
                    const anno::assets::unit::VolumeReader read =
                        [&](std::uint32_t g, std::uint64_t o,
                            std::uint64_t l) -> std::optional<std::vector<std::uint8_t>> {
                        const auto& vols = catalog->vols;
                        if (g >= vols.size() || o + l > vols[g].size()) {
                            return std::nullopt;
                        }
                        const auto b = vols[g].begin() + static_cast<std::ptrdiff_t>(o);
                        return std::vector<std::uint8_t>(b, b + static_cast<std::ptrdiff_t>(l));
                    };
                    const std::array kinds = {anno::assets::unit::UnitKind::Ship,
                                              anno::assets::unit::UnitKind::Soldier};
                    auto units = anno::assets::unit::units(
                                     s.value(), *catalog->header, kinds,
                                     anno::assets::figure::kOriginalPlayerColours, read)
                                     .value();
                    for (auto& u : units.units) {
                        std::optional<float> z;
                        if (u.kind == anno::assets::unit::UnitKind::Soldier) {
                            z = scene->ground_height(u.x, u.y);
                            if (!z) {
                                continue;
                            }
                        }
                        render::UnitSprite sprite{u.x, u.y, z, {}, {}};
                        for (const auto& f : u.frames) {
                            sprite.frames.push_back(
                                f ? std::optional<render::UnitFrame>(
                                        render::to_unit_frame(*f, u.figure, false))
                                  : std::nullopt);
                        }
                        for (const auto& f : u.shadows) {
                            sprite.shadows.push_back(
                                f ? std::optional<render::UnitFrame>(
                                        render::to_unit_frame(*f, u.figure, true))
                                  : std::nullopt);
                        }
                        scene->units.push_back(std::move(sprite));
                    }
                }
            };
            build_scene();
            if (ci < 2) {
                const auto full = *scene->bounds(2);
                const std::uint64_t actual =
                    static_cast<std::uint64_t>(full.w) * static_cast<std::uint64_t>(full.h);
                if (actual > pixels) {
                    scene.reset();
                    picture.release();
                    render_cpu.release();
                    pixels = actual;
                    picture = reserve_picture(pixels);
                    ensure_catalog();
                    render_cpu = anno::test::Budget::process().acquire_cpu();
                    build_scene();
                }
            }
            (void)picture;
            (void)render_cpu;
            const std::string name = p.filename().string();
            const render::WaterStyle style =
                ci % 2 == 0 ? render::WaterStyle::Original : render::WaterStyle::Enhanced;
            render::Layers layers = render::Layers::all();
            layers.water_style = style;
            layers.river_flow = river_flow;
            if (ci < 2) {
                const std::uint64_t time = ci == 0 ? 0 : 2500;
                const auto rect = *scene->bounds(2);
                layers.time_ms = time;
                const auto [c, st] = scene->render(rect, 2, layers);
                return name + " g2 t" + std::to_string(time) + " " + style_name(style) + " " +
                       std::to_string(rect.w) + "x" + std::to_string(rect.h) + " " +
                       hex(fnv(c.pixels)) + " drawn=" + std::to_string(st.objects.drawn) +
                       " inv=" + std::to_string(st.objects.invisible) +
                       " failed=" + std::to_string(st.objects.failed) +
                       " units=" + std::to_string(st.units);
            }
            const auto b = *scene->bounds(1);
            const render::Rect rect{b.x + b.w / 2 - 800, b.y + b.h / 2 - 450, 1600, 900};
            layers.time_ms = 700;
            const auto [c, st] = scene->render(rect, 1, layers);
            return name + " g1 region " + style_name(style) + " " + hex(fnv(c.pixels)) +
                   " drawn=" + std::to_string(st.objects.drawn) +
                   " units=" + std::to_string(st.units);
        },
        opt);
    return per_case;
}

const std::vector<std::string> kExpected = {
    "Single_01.sze g2 t0 Original 16960x9170 2f0395814262894d drawn=69864 inv=11717 failed=0 "
    "units=5",
    "Single_01.sze g2 t2500 Enhanced 16960x9170 c173c978c6ae696b drawn=69256 inv=11717 failed=0 "
    "units=5",
    "Single_01.sze g1 region Original f3c82ff0f061887a drawn=24371 units=0",
    "Single_01.sze g1 region Enhanced 38fa6edcf261fb33 drawn=24110 units=0",
    "Single_05.sze g2 t0 Original 9040x3291 2ab607c8c9c0890b drawn=12628 inv=1240 failed=0 units=2",
    "Single_05.sze g2 t2500 Enhanced 9040x3291 84964184f8e64d2a drawn=12628 inv=1240 failed=0 "
    "units=2",
    "Single_05.sze g1 region Original c90b2060847e2a1f drawn=4603 units=0",
    "Single_05.sze g1 region Enhanced 7bc859ab6cbbe7d0 drawn=4603 units=0",
    "Single_06.sze g2 t0 Original 7280x4451 3f1ba536788d1526 drawn=13778 inv=3084 failed=0 units=3",
    "Single_06.sze g2 t2500 Enhanced 7280x4451 0eda75c6891a3e3d drawn=13626 inv=3084 failed=0 "
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
    "Single_10.sze g2 t2500 Enhanced 17440x9131 7151216672105309 drawn=52883 inv=8756 failed=0 "
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
    "Single_14.sze g2 t2500 Enhanced 14080x9291 fe84bb91f57b406e drawn=47064 inv=9450 failed=0 "
    "units=3",
    "Single_14.sze g1 region Original 323ce1b9064247a3 drawn=16735 units=1",
    "Single_14.sze g1 region Enhanced a5f9c1083372b79b drawn=16681 units=1",
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
    "Single_24.sze g2 t2500 Enhanced 7200x2811 8cd51f3c175b9611 drawn=24406 inv=2880 failed=0 "
    "units=541",
    "Single_24.sze g1 region Original 610c80610617b85a drawn=23697 units=0",
    "Single_24.sze g1 region Enhanced 5f8671f1b63761f6 drawn=23526 units=0",
    "N_064x064_0001_0B.scp g2 t0 Original 1280x891 d73b254ebe87486f drawn=949 inv=338 failed=0 "
    "units=0",
    "N_064x064_0001_0B.scp g2 t2500 Enhanced 1280x891 5118c7e5cf5c0b0f drawn=930 inv=338 failed=0 "
    "units=0",
    "N_064x064_0001_0B.scp g1 region Original 95cafb25426a2a9d drawn=949 units=0",
    "N_064x064_0001_0B.scp g1 region Enhanced f24622877f58bc18 drawn=930 units=0",
};

// Lines that differ on MSVC's UCRT: the enhanced water calls std::exp and
// std::pow (iso.cpp), which UCRT rounds differently in the last bit from
// Apple's libm and glibc (as for the wave solver, fluid_test.cpp). Measured
// 2026-10-18 on Windows 11 26H2 with MSVC 19.44; the Rust reference on the
// same machine (Rust 1.99.1, x86_64-pc-windows-msvc, `digest_scene scenes`)
// prints the same line, so C++ = Rust holds on UCRT too. Every other line,
// including the enhanced lines of scenes with rivers, is identical there.
// Fallback if more libms diverge: libm-free exp/pow in the enhanced water
// (docs/render/iso.md, "Fidelity").
const std::vector<std::pair<std::size_t, std::string>> kUcrt = {
    {33, "Single_16.sze g2 t2500 Enhanced 7200x4051 b744196ec08d7b3e drawn=8743 inv=239 failed=0 "
         "units=3"},
};

std::vector<std::string> expected_lines() {
    std::vector<std::string> out = kExpected;
#if defined(_MSC_VER)
    for (const auto& [i, line] : kUcrt) {
        out.at(i) = line;
    }
#endif
    return out;
}

} // namespace

// The suite "heavy" marks a case that needs well over 1 GB with ANNO_TEST_THREADS
// workers (this one: about 35 GB at 16 threads, measured alone): CTest runs it
// alone (anno_add_doctest in the top CMakeLists.txt).
TEST_CASE("scene renders match the recorded digests (the Rust reference's, except the enhanced "
          "river lines)" *
          doctest::test_suite("heavy")) {
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
    const std::vector<std::string> expected = expected_lines();
    REQUIRE(lines.size() == expected.size());
    for (std::size_t i = 0; i < lines.size(); ++i) {
        CHECK(lines[i] == expected[i]);
    }
}

TEST_CASE("legacy river shading reproduces the previous enhanced digests (game files)" *
          doctest::test_suite("heavy")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // --river-flow legacy keeps river_shade bit for bit (rivers.md 6, 8): the
    // Enhanced digests of every scenario with river pixels before the flow
    // shading (the former expected lines, which were the Rust reference's
    // before the river gap fix and this C++ renderer's after it), with the
    // units drawn. [1] is the group 2 render, [3] the group 1 region; where the
    // region has no river pixels its line did not change.
    struct Old {
        const char* file;
        const char* g2;
        const char* g1_region; ///< null: unchanged (the current expected line)
    };
    const std::vector<Old> old = {
        {"Single_01.sze",
         "Single_01.sze g2 t2500 Enhanced 16960x9170 ba0b1101d9e6229f drawn=69256 inv=11717 "
         "failed=0 units=5",
         nullptr},
        {"Single_06.sze",
         "Single_06.sze g2 t2500 Enhanced 7280x4451 e3a111d08b078b4b drawn=13626 inv=3084 "
         "failed=0 units=3",
         nullptr},
        {"Single_10.sze",
         "Single_10.sze g2 t2500 Enhanced 17440x9131 ec3566e92ecbcb5e drawn=52883 inv=8756 "
         "failed=0 units=1",
         nullptr},
        {"Single_14.sze",
         "Single_14.sze g2 t2500 Enhanced 14080x9291 e78f231176506a9d drawn=47064 inv=9450 "
         "failed=0 units=3",
         nullptr},
        {"Single_24.sze",
         "Single_24.sze g2 t2500 Enhanced 7200x2811 de09ab1e97a17170 drawn=24406 inv=2880 "
         "failed=0 units=541",
         "Single_24.sze g1 region Enhanced febd87d511c3ec25 drawn=23526 units=0"},
    };
    const std::vector<std::string> now = expected_lines();
    // All five scenarios in one call per mode (20 renders on the test threads):
    // one call per scenario kept 4 renders in flight and ran this case alone on
    // the machine (it is "heavy") for minutes.
    std::vector<std::string_view> files;
    for (const Old& o : old) {
        files.emplace_back(o.file);
    }
    const auto all_legacy = digest_lines(*dir, files, render::RiverFlowMode::Legacy);
    const auto all_flow = digest_lines(*dir, files);
    REQUIRE(all_legacy.size() == 4 * old.size());
    REQUIRE(all_flow.size() == 4 * old.size());
    // The four lines of one scenario; every line starts with its file name.
    auto lines_of = [](const std::vector<std::string>& all, const char* file) {
        std::vector<std::string> out;
        for (const std::string& line : all) {
            if (line.rfind(std::string(file) + " ", 0) == 0) {
                out.push_back(line);
            }
        }
        return out;
    };
    for (const Old& o : old) {
        INFO(o.file);
        const auto legacy = lines_of(all_legacy, o.file);
        const auto flow = lines_of(all_flow, o.file);
        REQUIRE(legacy.size() == 4);
        REQUIRE(flow.size() == 4);
        CHECK(legacy[1] == o.g2);
        const auto at = std::find_if(now.begin(), now.end(), [&](const std::string& line) {
            return line.rfind(std::string(o.file) + " g1 region Enhanced", 0) == 0;
        });
        REQUIRE(at != now.end());
        CHECK(legacy[3] == (o.g1_region != nullptr ? std::string(o.g1_region) : *at));
        // The flow shading differs where rivers are in the view.
        CHECK(flow[1] != legacy[1]);
        CHECK((o.g1_region != nullptr) == (flow[3] != legacy[3]));
        // Original water ignores the river mode.
        CHECK(flow[0] == legacy[0]);
        CHECK(flow[2] == legacy[2]);
    }
}

TEST_CASE("frames (chunk cache and composition) match the recorded digests (the Rust reference's, "
          "except the enhanced river frames)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // FrameRenderer with 4 threads, chunks built in the frame; a 1280 × 720
    // view at the centre of each zoom group, 0RGB words hashed as LE bytes.
    // Expected: crates/render/examples/digest_scene.rs frames.
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
        "Single_24.sze g2 t900 Enhanced 1c4dd9c8aab9b746",
        "Single_24.sze g1 t0 Original a17a423d76e40c10",
        "Single_24.sze g1 t900 Original 9c02fbfb3767584d",
        "Single_24.sze g1 t900 Enhanced c98bde27b8efb888",
        "Single_24.sze g0 t0 Original 6b07c0d4d77df178",
        "Single_24.sze g0 t900 Original b9a7aaa3f213ac65",
        "Single_24.sze g0 t900 Enhanced 56f35854554ef7ce",
    };
    // With the legacy river shading the two Single_24 river frames are the
    // previous enhanced digests (the flow shading changed those only; the
    // Single_01 views and Single_24's group 0 view hold no river pixels).
    std::vector<std::string> expected_legacy = expected;
    expected_legacy[11] = "Single_24.sze g2 t900 Enhanced 3ac692d3b0a5f43b";
    expected_legacy[14] = "Single_24.sze g1 t900 Enhanced 4d4a2f8226e38ad1";
    const auto felder_buf = anno::test::read_file(*dir / "Textures" / "Felder.tex");
    const auto anl_buf = anno::test::read_file(*dir / "Anlagen.dat");
    for (const render::RiverFlowMode mode :
         {render::RiverFlowMode::Flow, render::RiverFlowMode::Legacy}) {
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
            fr.river_flow = mode;
            for (const std::size_t group : {std::size_t{2}, std::size_t{1}, std::size_t{0}}) {
                const auto b = *scene.bounds(group);
                // Single_24's centre at group 0 is all land: move to a coast.
                const int dx = std::string_view(name) == "Single_24.sze" && group == 0 ? 2000 : 0;
                const render::Rect view{b.x + b.w / 2 - 640 + dx, b.y + b.h / 2 - 360, 1280, 720};
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
        const auto& want = mode == render::RiverFlowMode::Flow ? expected : expected_legacy;
        REQUIRE(lines.size() == want.size());
        for (std::size_t i = 0; i < lines.size(); ++i) {
            CHECK(lines[i] == want[i]);
        }
    }
}

namespace {

/// The scene of one file without units (the river tests need islands only).
render::Scene load_islands(const std::filesystem::path& dir, const std::filesystem::path& file) {
    // The parsed files refer to their buffers, which must outlive the scene: keep them for
    // the test run (a deque does not move its elements).
    static std::deque<std::vector<std::uint8_t>> keep;
    static std::mutex keep_mutex; // the corpus cases load scenes from several threads
    const std::lock_guard lock(keep_mutex);
    keep.push_back(anno::test::read_file(dir / "Textures" / "Felder.tex"));
    const auto felder = anno::formats::tex::TexFile::parse(keep.back()).value();
    keep.push_back(anno::test::read_file(dir / "Anlagen.dat"));
    auto anlagen = anno::formats::anlagen::AnlagenFile::parse(keep.back()).value();
    auto textures = anno::assets::island::GroundTextures::load(felder).value();
    if (const auto c = render::water_colour(anlagen)) {
        textures.set_water(*c);
    }
    keep.push_back(anno::test::read_file(file));
    return render::Scene("", anno::formats::insel::islands(keep.back()).value(),
                         std::move(textures), std::move(anlagen), render::kHeightLift);
}

/// Shares of the original's river sprite pixels (closed over the 50 % checkerboard) in
/// `view` that have no enhanced water within `reach` pixels, and of pixels with no water
/// at all, read from the same chunked field the viewer builds.
struct RiverCover {
    std::uint64_t sprite = 0, no_water = 0, far_from_water = 0;
};

RiverCover river_cover(const render::Scene& scene, std::size_t group, render::Rect view) {
    RiverCover out;
    const int reach = static_cast<int>(render::kTile[group].h) / 10 + 1;
    const int cx0 = render::div_euclid(view.x, render::kChunk);
    const int cy0 = render::div_euclid(view.y, render::kChunk);
    const int cx1 = render::div_euclid(view.x + view.w - 1, render::kChunk);
    const int cy1 = render::div_euclid(view.y + view.h - 1, render::kChunk);
    for (int cy = cy0; cy <= cy1; ++cy) {
        for (int cx = cx0; cx <= cx1; ++cx) {
            const render::Rect rect = render::chunk_rect(cx, cy);
            const auto [ground, probe] = scene.ground_with_probe(rect, group);
            const render::Canvas sprites = scene.river_sprites(rect, probe, group);
            const auto field = scene.water_field_from(rect, probe, group);
            auto water = [&](int x, int y) {
                // Out of the chunk counts as water (its neighbour decides).
                if (x < 0 || y < 0 || x >= rect.w || y >= rect.h) {
                    return true;
                }
                const auto i = static_cast<std::size_t>(y * rect.w + x);
                const float sd = (static_cast<float>(field.depth[i]) - render::sea::kDepthBias) /
                                 render::sea::kDepthScale;
                return field.cover[i] != 0 && sd >= 0.0F;
            };
            auto drawn = [&](int x, int y) {
                return x >= 0 && y >= 0 && x < rect.w && y < rect.h &&
                       sprites.pixels[static_cast<std::size_t>(y * rect.w + x) * 4 + 3] >= 128;
            };
            const auto inside = rect.intersect(view);
            if (!inside) {
                continue;
            }
            for (int y = inside->y - rect.y; y < inside->y - rect.y + inside->h; ++y) {
                for (int x = inside->x - rect.x; x < inside->x - rect.x + inside->w; ++x) {
                    bool near = false;
                    for (int dy = -1; dy <= 1 && !near; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            near = near || drawn(x + dx, y + dy);
                        }
                    }
                    if (!near) {
                        continue;
                    }
                    ++out.sprite;
                    if (water(x, y)) {
                        continue;
                    }
                    ++out.no_water;
                    bool close = false;
                    for (int dy = -reach; dy <= reach && !close; ++dy) {
                        for (int dx = -reach; dx <= reach; ++dx) {
                            close = close || water(x + dx, y + dy);
                        }
                    }
                    out.far_from_water += close ? 0U : 1U;
                }
            }
        }
    }
    return out;
}

} // namespace

TEST_CASE("enhanced river water covers the painted riverbed (river gaps)") {
    // Owner report 2026-10-19: stretches of the painted bed showed brown, no water,
    // in the enhanced mode (docs/render/iso.md, "River gaps"). The scene is the
    // owner's screenshot: D_256x256_0045_5B at group 1 (view 2494 x 1355 at scene
    // pixel (-96, 1934)), where 12 % of the original's river pixels were left
    // without water within 3 px; Single_24 is a scenario with rivers at all groups.
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    {
        const auto scene =
            load_islands(*dir, *dir / "Scenes" / "Islands" / "Dschungel" / "D_256x256_0045_5B.scp");
        for (const auto& [group, view] :
             {std::pair{std::size_t{1}, render::Rect{-96, 1934, 2494, 1355}},
              std::pair{std::size_t{2}, render::Rect{-48, 967, 1247, 678}},
              std::pair{std::size_t{0}, render::Rect{-192, 3868, 1600, 900}}}) {
            const RiverCover c = river_cover(scene, group, view);
            INFO("group " << group);
            CHECK(c.sprite > 5000); // the view shows rivers
            // Before the fix 5 to 12 % had no water within the reach.
            CHECK(c.far_from_water * 1000 <= c.sprite); // at most 0.1 %
            CHECK(c.no_water * 20 <= c.sprite);         // edge pixels at most 5 %
        }
    }
    {
        const auto scene = load_islands(*dir, *dir / "Scenes" / "0200Single" / "Single_24.sze");
        const auto b = *scene.bounds(1);
        const render::Rect view{b.x + b.w / 2 - 800, b.y + b.h / 2 - 450, 1600, 900};
        const RiverCover c = river_cover(scene, 1, view);
        CHECK(c.sprite > 5000);
        CHECK(c.far_from_water * 1000 <= c.sprite);
    }
}

namespace {

/// One island of the river flow corpus: the numbers of RiverFlow::build.
struct FlowRow {
    std::string name;
    std::size_t island = 0, pieces = 0, bridges = 0;
    render::river::FlowStats stats;
    double mask_ms = 0;
    std::size_t bad_cells = 0;
    float max_speed = 0;
};

} // namespace

TEST_CASE("river flow corpus: every island with rivers (rivers.md Q12)") {
    // The S4 budget gate on the game files: all 72 island templates with rivers
    // and the islands of the 0200Single / 0201SingleAddOn scenarios: no NaN,
    // residual <= 1e-3 (no fallback to the Eikonal field), the discharge
    // balanced, a flow that mostly runs down the distance to the outlets. The
    // time limits here only catch a pathology (several seconds, a loaded CTest
    // run); the budget (p95 <= 0.3 s, none above 1 s) is measured by
    // `river_qa --flow` on a quiet machine (docs/render/rivers.md).
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::vector<std::filesystem::path> files;
    for (const char* climate : {"Dschungel", "Nord", "Praerie", "Steppe", "Tundra"}) {
        for (const auto& p :
             anno::test::files_with_ext(*dir / "Scenes" / "Islands" / climate, "scp")) {
            files.push_back(p);
        }
    }
    const std::size_t templates = files.size();
    for (const char* folder : {"0200Single", "0201SingleAddOn"}) {
        for (const auto& p : anno::test::files_with_ext(*dir / "Scenes" / folder, "sze")) {
            files.push_back(p);
        }
    }
    const auto felder_buf = anno::test::read_file(*dir / "Textures" / "Felder.tex");
    const auto anl_buf = anno::test::read_file(*dir / "Anlagen.dat");
    const auto per_file = anno::test::parallel_indexed(files.size(), [&](std::size_t fi) {
        std::vector<FlowRow> rows;
        const auto buf = anno::test::read_file(files[fi]);
        auto anlagen = anno::formats::anlagen::AnlagenFile::parse(anl_buf).value();
        const auto felder = anno::formats::tex::TexFile::parse(felder_buf).value();
        auto textures = anno::assets::island::GroundTextures::load(felder).value();
        if (const auto c = render::water_colour(anlagen)) {
            textures.set_water(*c);
        }
        const render::Scene scene("", anno::formats::insel::islands(buf).value(),
                                  std::move(textures), std::move(anlagen), render::kHeightLift);
        render::river::RiverConfig cfg;
        cfg.log_fallback = false;
        for (std::size_t i = 0; i < scene.islands.size(); ++i) {
            if (scene.river_piece_count(i) == 0) {
                continue;
            }
            FlowRow row;
            row.name = files[fi].stem().string();
            row.island = i;
            row.pieces = scene.river_pieces(i).size();
            const auto t0 = std::chrono::steady_clock::now();
            const render::river::RiverMask mask = scene.river_mask(i, cfg);
            row.mask_ms =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                    .count();
            row.bridges = mask.bridges;
            const render::river::RiverFlow flow = render::river::RiverFlow::build(mask, cfg);
            row.stats = flow.stats();
            for (const auto& c : flow.cells()) {
                if (!std::isfinite(c.ux) || !std::isfinite(c.uy) || !std::isfinite(c.depth)) {
                    ++row.bad_cells;
                }
                row.max_speed = std::max(row.max_speed, std::hypot(c.ux, c.uy));
            }
            rows.push_back(std::move(row));
        }
        return rows;
    });
    std::size_t islands = 0, template_islands = 0, sea_less = 0, bridges = 0;
    double worst_flow_ms = 0;
    std::vector<std::string> sea_less_list;
    for (std::size_t fi = 0; fi < per_file.size(); ++fi) {
        for (const FlowRow& r : per_file[fi]) {
            ++islands;
            template_islands += fi < templates ? 1U : 0U;
            const auto& s = r.stats;
            INFO(r.name << " island " << r.island);
            CHECK(s.wet_cells > 0);
            CHECK(s.non_finite == 0);
            CHECK(r.bad_cells == 0);
            CHECK(s.fallback_components == 0); // no river needed the Eikonal field
            CHECK(s.residual <= 1e-3);
            CHECK(s.flux_imbalance <= 1e-3);
            CHECK(r.max_speed <= render::river::RiverConfig{}.speed_cap + 1e-4F);
            CHECK(s.speed_p50 >= render::river::RiverConfig{}.speed_floor - 1e-4F);
            CHECK(s.downstream_fraction >= 0.8F);
            // Rivers with a sea face are the ones whose direction is not a guess
            // (the others run to their own pseudo outlet): 86.5 % (a loop) and more on the corpus,
            // the median island 99.6 %.
            if (s.downstream_fraction_sea > 0.0F) {
                CHECK(s.downstream_fraction_sea >= 0.85F);
            }
            // The speed scale (OQ-065): nothing sits on the cap (at most 0.4 % do) and
            // the soft floor sets the speed of a minority of the cells (17 to 90 %, most
            // of a network is tributary): a regression to the sea faces as the reference
            // would put half of all cells on the cap.
            CHECK(s.cap_fraction < 0.05F);
            CHECK(s.floor_fraction < 0.95F);
            CHECK(s.build_ms < 5000.0);
            CHECK(r.mask_ms < 5000.0);
            sea_less += s.sea_less_components;
            bridges += r.bridges;
            worst_flow_ms = std::max(worst_flow_ms, s.build_ms);
            if (s.sea_less_components > 0) {
                sea_less_list.push_back(r.name + "#" + std::to_string(r.island) + ": " +
                                        std::to_string(s.sea_less_components) + " of " +
                                        std::to_string(s.components));
            }
        }
    }
    // 72 island templates have rivers (docs/render/rivers.md 1.2), plus the
    // scenarios' islands.
    CHECK(template_islands == 72);
    CHECK(islands > 72);
    // A ratchet on the pseudo outlets (OQ-067): 136 of 257 rivers have no sea contact.
    // More would mean that outlets were lost (the sea classification of the raster, the
    // links at falls and mouths); fewer is an improvement to be written down in
    // docs/render/rivers.md 14.3 and here.
    CHECK(sea_less <= 136);
    MESSAGE("islands " << islands << ", rivers without sea contact (decision D14) " << sea_less
                       << ", fall/mouth bridges " << bridges << ", slowest flow build "
                       << worst_flow_ms << " ms");
    for (const auto& l : sea_less_list) {
        MESSAGE("sea-less (pseudo outlet): " << l);
    }
}

namespace {

/// How well the river raster of one island (Scene::river_mask) agrees with the
/// river the original draws, at zoom group 0 (the group the raster is built at).
struct MaskFidelity {
    /// Pixels of the original's river sprites (closed over the 50 % checkerboard, stray
    /// pixels removed, depth-tested as the original; as river_qa's O) that lie kHalfCell
    /// pixels inside the sprite (a cell is 10 x 5 px: half of it is the resolution of the
    /// raster), and those of them whose terrain point is a wet cell of the raster.
    /// Those that lie over sea cells are counted apart (inner_sea).
    std::uint64_t inner = 0, inner_wet = 0, inner_sea = 0;
    /// Wet cells that exist only through a sprite (kSprite): how many, and how many of
    /// them project to a pixel within one pixel of a sprite pixel.
    std::uint64_t sprite_cells = 0, sprite_cells_on_sprite = 0;
    /// Wet cells (bridges excepted) and those with neither a sprite pixel nor drawn
    /// enhanced river water within kHalfCell pixels of their pixel.
    std::uint64_t wet_cells = 0, wet_cells_unsupported = 0;
    std::size_t chunks = 0;
};

constexpr int kHalfCell = 6;

MaskFidelity mask_fidelity(const render::Scene& scene, std::size_t island) {
    MaskFidelity out;
    const render::river::RiverConfig cfg;
    const render::river::RiverMask mask = scene.river_mask(island, cfg);
    const auto* iw = scene.island_water_info(island);
    if (iw == nullptr || mask.nx() == 0) {
        return out;
    }
    const render::Placement* place = nullptr;
    for (const auto& p : scene.layouts[0]) {
        if (p.index == island) {
            place = &p;
        }
    }
    if (place == nullptr) {
        return out;
    }
    const auto off = place->offset;
    const float lift_px = static_cast<float>(scene.lift.num) / static_cast<float>(scene.lift.den);
    const float half_w = static_cast<float>(render::kTile[0].w) / 2.0F;
    const float half_h = static_cast<float>(render::kTile[0].h) / 2.0F;
    const float cf = static_cast<float>(mask.cells_per_tile());
    // Scene pixel of the terrain point of island cell (cx, cy).
    auto pixel_of = [&](std::int32_t cx, std::int32_t cy) {
        const float x = (static_cast<float>(cx) + 0.5F) / cf,
                    y = (static_cast<float>(cy) + 0.5F) / cf;
        const float t = render::terrain_height(iw->heights, iw->w, iw->h, x, y);
        return std::pair{off.first + static_cast<int>(std::floor((x - y) * half_w)),
                         off.second + static_cast<int>(std::floor((x + y) * half_h - lift_px * t))};
    };
    // The chunks that hold wet cells (and their neighbours), so that a sprite the raster
    // misses next to some water is still seen.
    std::set<std::pair<int, int>> keys;
    for (std::int32_t cy = mask.y0(); cy < mask.y0() + static_cast<std::int32_t>(mask.ny()); ++cy) {
        for (std::int32_t cx = mask.x0(); cx < mask.x0() + static_cast<std::int32_t>(mask.nx());
             ++cx) {
            if (!mask.wet(cx, cy)) {
                continue;
            }
            const auto [px, py] = pixel_of(cx, cy);
            keys.emplace(render::div_euclid(px, render::kChunk),
                         render::div_euclid(py, render::kChunk));
        }
    }
    std::set<std::pair<int, int>> chunks;
    for (const auto& [kx, ky] : keys) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                chunks.emplace(kx + dx, ky + dy);
            }
        }
    }
    out.chunks = chunks.size();
    // Per chunk: bit 0 = closed sprite pixel, bit 1 = drawn enhanced river water.
    std::map<std::pair<int, int>, std::vector<std::uint8_t>> sprite_of;
    for (const auto& [kx, ky] : chunks) {
        const render::Rect rect = render::chunk_rect(kx, ky);
        const auto [ground, probe] = scene.ground_with_probe(rect, 0);
        const render::Canvas sprites = scene.river_sprites(rect, probe, 0);
        const auto field = scene.water_field_from(rect, probe, 0);
        const auto w = static_cast<std::size_t>(rect.w), h = static_cast<std::size_t>(rect.h);
        auto drawn = [&](int x, int y) {
            return x >= 0 && y >= 0 && x < rect.w && y < rect.h &&
                   sprites.pixels[(static_cast<std::size_t>(y) * w + static_cast<std::size_t>(x)) *
                                      4 +
                                  3] >= 128;
        };
        // As river_qa: a pixel is sprite when a pixel of its 3 x 3 neighbourhood is drawn
        // and the 5 x 5 around that one holds at least four drawn pixels.
        std::vector<std::uint8_t> sprite(w * h, 0);
        for (int y = 0; y < rect.h; ++y) {
            for (int x = 0; x < rect.w; ++x) {
                if (!drawn(x, y)) {
                    continue;
                }
                int near = 0;
                for (int yy = y - 2; yy <= y + 2; ++yy) {
                    for (int xx = x - 2; xx <= x + 2; ++xx) {
                        near += drawn(xx, yy) ? 1 : 0;
                    }
                }
                if (near < 4) {
                    continue;
                }
                for (int yy = std::max(y - 1, 0); yy <= std::min(y + 1, rect.h - 1); ++yy) {
                    for (int xx = std::max(x - 1, 0); xx <= std::min(x + 1, rect.w - 1); ++xx) {
                        sprite[static_cast<std::size_t>(yy) * w + static_cast<std::size_t>(xx)] = 1;
                    }
                }
            }
        }
        auto cell_wet = [&](std::size_t i) {
            if (probe.island[i] != island) {
                return false;
            }
            return mask.wet(static_cast<std::int32_t>(std::floor(probe.pos[i][0] * cf)),
                            static_cast<std::int32_t>(std::floor(probe.pos[i][1] * cf)));
        };
        for (int y = kHalfCell; y < rect.h - kHalfCell; ++y) {
            for (int x = kHalfCell; x < rect.w - kHalfCell; ++x) {
                const auto i = static_cast<std::size_t>(y) * w + static_cast<std::size_t>(x);
                bool inner = sprite[i] != 0;
                for (int yy = y - kHalfCell; yy <= y + kHalfCell && inner; ++yy) {
                    for (int xx = x - kHalfCell; xx <= x + kHalfCell; ++xx) {
                        inner = inner && sprite[static_cast<std::size_t>(yy) * w +
                                                static_cast<std::size_t>(xx)] != 0;
                    }
                }
                if (inner) {
                    // Sprites reach over the shore into the sea; the sea stays sea
                    // (never river water over the original's water), so those pixels are
                    // not the raster's to cover.
                    const auto ccx = static_cast<std::int32_t>(std::floor(probe.pos[i][0] * cf));
                    const auto ccy = static_cast<std::int32_t>(std::floor(probe.pos[i][1] * cf));
                    const bool in_sea = probe.island[i] == island &&
                                        (mask.flags_at(ccx, ccy) & render::river::kSea) != 0;
                    if (in_sea) {
                        ++out.inner_sea;
                    } else {
                        ++out.inner;
                        out.inner_wet += cell_wet(i) ? 1U : 0U;
                    }
                }
            }
        }
        for (std::size_t i = 0; i < sprite.size(); ++i) {
            if ((field.flags[i] & 2U) != 0 && field.cover[i] != 0) {
                sprite[i] = static_cast<std::uint8_t>(sprite[i] | 2U);
            }
        }
        sprite_of.emplace(std::pair{kx, ky}, std::move(sprite));
    }
    // Cells that are wet only through a sprite must lie under a sprite pixel; and every
    // wet cell must have a sprite or drawn river water about it.
    auto near_bits = [&](int px, int py, int radius, std::uint8_t bits) {
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                const int qx = px + dx, qy = py + dy;
                const int kx = render::div_euclid(qx, render::kChunk);
                const int ky = render::div_euclid(qy, render::kChunk);
                const auto it = sprite_of.find({kx, ky});
                if (it == sprite_of.end()) {
                    continue;
                }
                const render::Rect rect = render::chunk_rect(kx, ky);
                if ((it->second[static_cast<std::size_t>(qy - rect.y) *
                                    static_cast<std::size_t>(rect.w) +
                                static_cast<std::size_t>(qx - rect.x)] &
                     bits) != 0) {
                    return true;
                }
            }
        }
        return false;
    };
    for (std::int32_t cy = mask.y0(); cy < mask.y0() + static_cast<std::int32_t>(mask.ny()); ++cy) {
        for (std::int32_t cx = mask.x0(); cx < mask.x0() + static_cast<std::int32_t>(mask.nx());
             ++cx) {
            const std::uint8_t f = mask.flags_at(cx, cy);
            if ((f & render::river::kWet) == 0 || (f & render::river::kBridge) != 0) {
                continue;
            }
            const auto [px, py] = pixel_of(cx, cy);
            ++out.wet_cells;
            out.wet_cells_unsupported += near_bits(px, py, kHalfCell, 3) ? 0U : 1U;
            if ((f & render::river::kSprite) != 0) {
                ++out.sprite_cells;
                out.sprite_cells_on_sprite += near_bits(px, py, 1, 1) ? 1U : 0U;
            }
        }
    }
    return out;
}

} // namespace

TEST_CASE("river raster agrees with the river the original draws (mask fidelity, rivers.md R1)" *
          doctest::test_suite("heavy")) {
    // The sprite-derived part of the raster (forward projection of the cells, the
    // original's depth test, the closed 50 % checkerboard, the coverage window) is what
    // the flow of S5 is drawn under: compare it with the sprites of the original at zoom
    // group 0 and with the enhanced water the viewer draws.
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const std::vector<std::filesystem::path> files = {
        *dir / "Scenes" / "Islands" / "Dschungel" / "D_256x256_0045_5B.scp",
        *dir / "Scenes" / "Islands" / "Nord" / "N_256x256_0002_2B.scp",
        *dir / "Scenes" / "Islands" / "Praerie" / "Pr_128x128_0003_1B.scp",
        *dir / "Scenes" / "0200Single" / "Single_01.sze"};
    const auto rows = anno::test::parallel_indexed(files.size(), [&](std::size_t fi) {
        const auto scene = load_islands(*dir, files[fi]);
        std::vector<MaskFidelity> out;
        for (std::size_t i = 0; i < scene.islands.size(); ++i) {
            if (scene.river_piece_count(i) > 0) {
                out.push_back(mask_fidelity(scene, i));
            }
        }
        return out;
    });
    for (std::size_t fi = 0; fi < files.size(); ++fi) {
        for (const MaskFidelity& r : rows[fi]) {
            INFO(files[fi].filename().string());
            auto pct = [](std::uint64_t a, std::uint64_t b) {
                return 100.0 * static_cast<double>(a) /
                       static_cast<double>(std::max<std::uint64_t>(b, 1));
            };
            MESSAGE(files[fi].filename().string()
                    << ": chunks " << r.chunks << ", inner sprite px " << r.inner << " (+ "
                    << r.inner_sea << " over sea) wet " << pct(r.inner_wet, r.inner)
                    << " %, kSprite cells " << r.sprite_cells << " on a sprite "
                    << pct(r.sprite_cells_on_sprite, r.sprite_cells) << " %, wet cells "
                    << r.wet_cells << " unsupported " << pct(r.wet_cells_unsupported, r.wet_cells)
                    << " %");
            // Measured on the corpus: 98.9 to 100 %, 97.2 to 99.7 %, at most 0.32 %.
            // The hand mutations of the review (no depth test, a window of one pixel,
            // the fall and mouth shapes below the sea drawn depth-tested) all leave these
            // bounds.
            CHECK(r.inner > 100000);
            CHECK(r.inner_wet * 1000 >= r.inner * 985);
            CHECK(r.sprite_cells > 500);
            CHECK(r.sprite_cells_on_sprite * 100 >= r.sprite_cells * 96);
            CHECK(r.wet_cells_unsupported * 100 <= r.wet_cells);
        }
    }
}

TEST_CASE("river raster: built once per island on a worker, the same for every caller") {
    // Scene::river_mask is a pure function of the scene (no cache, safe from any
    // thread), so concurrent callers get identical rasters. S5 builds it once per
    // island on a worker (rivers.md 3.1), never on the window thread.
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto scene =
        load_islands(*dir, *dir / "Scenes" / "Islands" / "Dschungel" / "D_256x256_0045_5B.scp");
    REQUIRE(scene.river_piece_count(0) > 0);
    const render::river::RiverConfig cfg;
    std::vector<render::river::RiverMask> masks(4);
    {
        std::vector<std::thread> pool;
        for (std::size_t k = 0; k < masks.size(); ++k) {
            pool.emplace_back([&, k] { masks[k] = scene.river_mask(0, cfg); });
        }
        for (auto& t : pool) {
            t.join();
        }
    }
    CHECK(masks[0].wet_count() > 10000);
    for (const auto& m : masks) {
        CHECK(m.nx() == masks[0].nx());
        CHECK(m.ny() == masks[0].ny());
        CHECK(m.x0() == masks[0].x0());
        CHECK(m.y0() == masks[0].y0());
        CHECK(m.bridges == masks[0].bridges);
        CHECK(m.flags == masks[0].flags);
        CHECK(m.bed.size() == masks[0].bed.size());
        bool same = m.bed.size() == masks[0].bed.size();
        for (std::size_t i = 0; same && i < m.bed.size(); ++i) {
            // level is NaN where there is none: compare bit patterns.
            same = std::memcmp(&m.bed[i], &masks[0].bed[i], sizeof(float)) == 0 &&
                   std::memcmp(&m.level[i], &masks[0].level[i], sizeof(float)) == 0;
        }
        CHECK(same);
    }
}

TEST_CASE("water field: empty cells hold on every island (game files)") {
    // The empty-cell proof (docs/render/iso.md "Water field: empty cells") on
    // the real islands: every island template and the islands of the 0200Single
    // and 0201SingleAddOn scenarios, 9 positions in every empty tile (corners, edges, centre;
    // the synthetic tests sample more densely).
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::vector<std::filesystem::path> files;
    for (const char* climate : {"Dschungel", "Nord", "Praerie", "Steppe", "Tundra"}) {
        for (const auto& p :
             anno::test::files_with_ext(*dir / "Scenes" / "Islands" / climate, "scp")) {
            files.push_back(p);
        }
    }
    for (const char* folder : {"0200Single", "0201SingleAddOn"}) {
        for (const auto& p : anno::test::files_with_ext(*dir / "Scenes" / folder, "sze")) {
            files.push_back(p);
        }
    }
    const auto felder_buf = anno::test::read_file(*dir / "Textures" / "Felder.tex");
    const auto anl_buf = anno::test::read_file(*dir / "Anlagen.dat");
    struct Count {
        std::size_t tiles = 0, empty = 0, wrong = 0;
    };
    const auto per_file = anno::test::parallel_indexed(files.size(), [&](std::size_t fi) {
        Count c;
        const auto buf = anno::test::read_file(files[fi]);
        auto anlagen = anno::formats::anlagen::AnlagenFile::parse(anl_buf).value();
        const auto felder = anno::formats::tex::TexFile::parse(felder_buf).value();
        auto textures = anno::assets::island::GroundTextures::load(felder).value();
        const render::Scene scene("", anno::formats::insel::islands(buf).value(),
                                  std::move(textures), std::move(anlagen), render::kHeightLift);
        for (std::size_t i = 0; i < scene.islands.size(); ++i) {
            const render::sea::IslandWater* iw = scene.island_water_info(i);
            if (iw == nullptr) {
                continue;
            }
            c.tiles += iw->w * iw->h;
            for (std::size_t ty = 0; ty < iw->h; ++ty) {
                for (std::size_t tx = 0; tx < iw->w; ++tx) {
                    if (iw->empty_cell[ty * iw->w + tx] == 0) {
                        continue;
                    }
                    ++c.empty;
                    for (const float fy : {0.0F, 0.5F, 0.9999F}) {
                        for (const float fx : {0.0F, 0.5F, 0.9999F}) {
                            const auto t = render::sea::field_at(*iw, static_cast<float>(tx) + fx,
                                                                 static_cast<float>(ty) + fy);
                            c.wrong += t == render::sea::FieldTexel{} ? 0U : 1U;
                        }
                    }
                }
            }
        }
        return c;
    });
    Count all;
    for (const auto& c : per_file) {
        all.tiles += c.tiles;
        all.empty += c.empty;
        all.wrong += c.wrong;
    }
    MESSAGE(files.size() << " files, " << all.tiles << " tiles, " << all.empty << " empty");
    CHECK(all.empty > 0);
    CHECK(all.wrong == 0);
}
TEST_CASE("terrain_plane equals terrain_at per pixel (game files)") {
    // Chunk::terrain is built by Scene::terrain_plane (corners cached per tile);
    // it must equal the quantised terrain_at of every pixel, as built before.
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto buf =
        anno::test::read_file(*dir / "Scenes" / "Islands" / "Dschungel" / "D_256x256_0045_5B.scp");
    // The parsed files keep views into their buffers: the buffers outlive them.
    const auto felder_buf = anno::test::read_file(*dir / "Textures" / "Felder.tex");
    const auto anl_buf = anno::test::read_file(*dir / "Anlagen.dat");
    const auto felder = anno::formats::tex::TexFile::parse(felder_buf).value();
    const render::Scene scene("", anno::formats::insel::islands(buf).value(),
                              anno::assets::island::GroundTextures::load(felder).value(),
                              anno::formats::anlagen::AnlagenFile::parse(anl_buf).value(),
                              render::kHeightLift);
    std::size_t pixels = 0, ground = 0, differ = 0;
    for (std::size_t group = 0; group < render::kGroups; ++group) {
        const auto b = scene.layouts[group].front().bounds;
        for (const auto& [fx, fy] :
             {std::pair{0.5, 0.5}, std::pair{0.3, 0.6}, std::pair{0.7, 0.2}}) {
            const render::Rect rect{b.x + static_cast<int>(fx * b.w),
                                    b.y + static_cast<int>(fy * b.h), 300, 200};
            const auto probe = scene.ground_with_probe(rect, group).second;
            const auto plane = scene.terrain_plane(probe);
            REQUIRE(plane.size() == probe.island.size());
            for (std::size_t i = 0; i < plane.size(); ++i) {
                const int x = rect.x + static_cast<int>(i % 300);
                const int y = rect.y + static_cast<int>(i / 300);
                const auto z = scene.terrain_at(probe, rect, x, y);
                const std::uint16_t want = z ? render::sat_cast<std::uint16_t>(render::clampf(
                                                   roundf(*z * render::kTerrainScale), 0.0F,
                                                   static_cast<float>(render::kNoTerrain - 1)))
                                             : render::kNoTerrain;
                ++pixels;
                ground += want != render::kNoTerrain ? 1U : 0U;
                differ += plane[i] == want ? 0U : 1U;
            }
        }
    }
    MESSAGE(pixels << " pixels, " << ground << " on the ground");
    CHECK(ground > pixels / 2);
    CHECK(differ == 0);
}

// ---- Derived zoom levels (iso.md, "Continuous zoom, stage 3 design") ----

TEST_CASE(
    "derived levels: a chunk is the reduction of its four children, recursively (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto scene =
        load_islands(*dir, *dir / "Scenes" / "Islands" / "Dschungel" / "D_256x256_0045_5B.scp");
    scene.set_levels(render::ZoomLevels::with_derived(2));
    REQUIRE(scene.levels().size() == 5);
    const render::Px water = scene.textures.water();
    if (const auto* p2 = scene.sea(2); p2 != nullptr) {
        const auto* p3 = scene.sea(3);
        REQUIRE(p3 != nullptr);
        MESSAGE("sea pattern period at group 2: " << p2->width << " x " << p2->height
                                                  << ", level 3: " << p3->width << " x "
                                                  << p3->height);
        CHECK(p3->width == (p2->width + 1) / 2);
        CHECK(p3->height == (p2->height + 1) / 2);
        CHECK(p3->step_count() == p2->step_count());
    }
    const auto b = *scene.bounds(2);
    const int cx0 = render::div_euclid(b.x + b.w / 2, render::kChunk) & ~3,
              cy0 = render::div_euclid(b.y + b.h / 2, render::kChunk) & ~3;
    std::vector<render::Chunk> l2;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            l2.push_back(render::render_chunk(scene, 2, cx0 + x, cy0 + y, false, true));
        }
    }
    auto quad_of = [](const std::vector<render::Chunk>& v, std::size_t stride, std::size_t x0,
                      std::size_t y0) {
        return render::ChunkQuad{&v[y0 * stride + x0], &v[y0 * stride + x0 + 1],
                                 &v[(y0 + 1) * stride + x0], &v[(y0 + 1) * stride + x0 + 1]};
    };
    auto same = [](const render::Chunk& a, const render::Chunk& c) {
        return a.ground == c.ground && a.objects == c.objects && a.field == c.field &&
               a.terrain == c.terrain && a.base.empty() && c.base.empty();
    };
    std::vector<render::Chunk> l3;
    for (std::size_t qy = 0; qy < 2; ++qy) {
        for (std::size_t qx = 0; qx < 2; ++qx) {
            const render::Chunk want =
                render::reduce_chunks_2x(quad_of(l2, 4, 2 * qx, 2 * qy), water);
            // Level 3 chunk (cx0 / 2 + qx, cy0 / 2 + qy), built through render_chunk.
            const render::Chunk got =
                render::render_chunk(scene, 3, cx0 / 2 + static_cast<int>(qx),
                                     cy0 / 2 + static_cast<int>(qy), false, true);
            CHECK(same(got, want));
            CHECK(got.serial != 0);
            l3.push_back(want);
        }
    }
    const render::Chunk want4 = render::reduce_chunks_2x(quad_of(l3, 2, 0, 0), water);
    const render::Chunk got4 = render::render_chunk(scene, 4, cx0 / 4, cy0 / 4, false, true);
    CHECK(same(got4, want4));
    CHECK_FALSE(got4.ground.empty()); // the island is in it
}

TEST_CASE(
    "derived levels: the CPU frame draws, and is close to the reduced frame above (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto scene =
        load_islands(*dir, *dir / "Scenes" / "Islands" / "Dschungel" / "D_256x256_0045_5B.scp");
    scene.set_levels(render::ZoomLevels::with_derived(2));
    const auto b3 = *scene.bounds(3);
    constexpr int kW = 512, kH = 384;
    const render::Rect v3{b3.x + b3.w / 2 - kW / 2, b3.y + b3.h / 2 - kH / 2, kW, kH};
    const render::Rect v2{v3.x * 2, v3.y * 2, kW * 2, kH * 2};
    render::FrameRenderer fr(4);
    std::vector<std::uint32_t> f3(kW * kH), f2(static_cast<std::size_t>(kW) * 2 * kH * 2);
    fr.render(scene, v3, 3, 0, render::WaterStyle::Original, f3.data());
    fr.render(scene, v2, 2, 0, render::WaterStyle::Original, f2.data());
    // The mean colour per channel of the level 3 frame against the 2 x 2 mean of the
    // group 2 frame: the same picture reduced (the sea pattern and the chunks are
    // reductions of group 2's; only rounding and the shading grid differ).
    std::array<double, 3> m3{}, m2{};
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const std::uint32_t p = f3[static_cast<std::size_t>(y * kW + x)];
            for (int c = 0; c < 3; ++c) {
                m3[static_cast<std::size_t>(c)] += (p >> (16 - 8 * c)) & 0xFFU;
                for (int j = 0; j < 2; ++j) {
                    for (int i = 0; i < 2; ++i) {
                        const std::uint32_t q =
                            f2[static_cast<std::size_t>((2 * y + j) * kW * 2 + 2 * x + i)];
                        m2[static_cast<std::size_t>(c)] += ((q >> (16 - 8 * c)) & 0xFFU) / 4.0;
                    }
                }
            }
        }
    }
    for (std::size_t c = 0; c < 3; ++c) {
        INFO("channel " << c << ": level 3 " << m3[c] / (kW * kH) << ", reduced group 2 "
                        << m2[c] / (kW * kH));
        CHECK(std::abs(m3[c] - m2[c]) / (kW * kH) < 6.0);
    }
}

TEST_CASE(
    "derived levels: a zoom-out sweep builds every chunk once while it stays cached (game files)" *
    doctest::test_suite("heavy")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // Group 2, then levels 3, 4, 5 in turn, each showing the whole scene: the chunks of
    // a level are reduced from the cached chunks of the level above (shared, not copied)
    // and only the missing children are built, then cached like any chunk. Nothing is
    // evicted (spare_chunks large), so no key may be built twice: the sweep used to
    // rebuild the group-2 chunks of the whole scene for each level (4^(k-2) per chunk).
    for (const bool workers : {false, true}) {
        auto scene =
            load_islands(*dir, *dir / "Scenes" / "Islands" / "Dschungel" / "D_256x256_0045_5B.scp");
        scene.set_levels(render::ZoomLevels::with_derived(3));
        render::FrameRenderer fr(4);
        fr.spare_chunks = 100000;
        if (workers) {
            fr.set_workers(std::make_unique<render::ChunkWorkers>(scene, 4));
        }
        std::vector<std::size_t> shown;
        for (const std::size_t level :
             {std::size_t{2}, std::size_t{3}, std::size_t{4}, std::size_t{5}}) {
            const auto b = *scene.bounds(level);
            const render::Rect view{b.x, b.y, b.w, b.h};
            std::size_t missing = 1;
            for (int i = 0; i < 20000 && (missing != 0 || fr.busy()); ++i) {
                missing = std::get<2>(fr.update_chunks(scene, view, level));
                if (workers) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            }
            REQUIRE(missing == 0);
            std::size_t n = 0;
            for (int cy = render::div_euclid(b.y, render::kChunk);
                 cy <= render::div_euclid(b.y + b.h - 1, render::kChunk); ++cy) {
                for (int cx = render::div_euclid(b.x, render::kChunk);
                     cx <= render::div_euclid(b.x + b.w - 1, render::kChunk); ++cx) {
                    n += fr.chunk({level, cx, cy}) != nullptr ? 1U : 0U;
                }
            }
            shown.push_back(n);
        }
        // Every chunk in the cache was built exactly once, at every level.
        std::size_t group2 = 0, over = 0, total = 0;
        for (std::size_t level = 2; level <= 5; ++level) {
            const auto b = *scene.bounds(level);
            for (int cy = render::div_euclid(b.y, render::kChunk) - 2;
                 cy <= render::div_euclid(b.y + b.h - 1, render::kChunk) + 2; ++cy) {
                for (int cx = render::div_euclid(b.x, render::kChunk) - 2;
                     cx <= render::div_euclid(b.x + b.w - 1, render::kChunk) + 2; ++cx) {
                    const std::size_t n = fr.chunk_builds({level, cx, cy});
                    over += n > 1 ? 1U : 0U;
                    total += n;
                    group2 += level == 2 ? n : 0U;
                }
            }
        }
        MESSAGE((workers ? "workers" : "blocking")
                << ": chunks shown per level 2..5 = " << shown[0] << ", " << shown[1] << ", "
                << shown[2] << ", " << shown[3] << "; builds in all " << total << ", of group 2 "
                << group2 << ", keys built more than once " << over);
        CHECK(over == 0);
        CHECK(group2 >= shown[0]);
        // Group 2 holds the shown chunks and the children of the level 3 chunks shown: the
        // old sweep built 4^(k-2) group-2 chunks for each chunk shown at level k.
        CHECK(group2 <= shown[0] + 4 * shown[1]);
        fr.set_workers(nullptr);
    }
}

TEST_CASE("derived levels: a small level stays in the cache, a large one is trimmed (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto scene =
        load_islands(*dir, *dir / "Scenes" / "Islands" / "Dschungel" / "D_256x256_0045_5B.scp");
    scene.set_levels(render::ZoomLevels::with_derived(3));
    const auto b5 = *scene.bounds(5);
    render::FrameRenderer fr(4);
    fr.spare_chunks = 0;
    const render::Rect view5{b5.x, b5.y, b5.w, b5.h};
    fr.update_chunks(scene, view5, 5);
    // The chunks of level 5 (the levels below it that were built on the way are cached
    // too, as ordinary chunks).
    auto cached_at_5 = [&](const render::FrameRenderer& f) {
        std::size_t n = 0;
        for (int cy = render::div_euclid(b5.y, render::kChunk);
             cy <= render::div_euclid(b5.y + b5.h - 1, render::kChunk); ++cy) {
            for (int cx = render::div_euclid(b5.x, render::kChunk);
                 cx <= render::div_euclid(b5.x + b5.w - 1, render::kChunk); ++cx) {
                n += f.chunk({5, cx, cy}) != nullptr ? 1U : 0U;
            }
        }
        return n;
    };
    const std::size_t level5 = cached_at_5(fr);
    CHECK(level5 > 0);
    // A view elsewhere at group 2 with nothing spare: level 5 (a handful of chunks
    // for the whole scene) is below resident_level_chunks and stays.
    const auto b2 = *scene.bounds(2);
    fr.update_chunks(scene, render::Rect{b2.x, b2.y, 600, 600}, 2);
    const std::size_t kept = cached_at_5(fr);
    CHECK(kept == level5);
    // With the rule off the same sequence evicts them.
    render::FrameRenderer off(4);
    off.spare_chunks = 0;
    off.resident_level_chunks = 0;
    off.update_chunks(scene, view5, 5);
    off.update_chunks(scene, render::Rect{b2.x, b2.y, 600, 600}, 2);
    const std::size_t kept_off = cached_at_5(off);
    CHECK(kept_off < level5);
}
