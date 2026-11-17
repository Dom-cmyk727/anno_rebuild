// RENDER-DIRECT-1b step 4: the texel-exact proof on real decoded game data, over the static+
// water class and the figures of three scenes' start units (game files; "heavy": a scene's whole
// frame set, decoded, held at once), exactly as the packet names them:
// `Scenes/0010SingleEndlos/02_1024_easy_128.sze` (1a's own worst scene), one campaign scene, and
// `data/maps/lan5.txt`. Every frame is checked, not a sample (review F2: an earlier revision of
// this file sampled every 7th frame and excluded `lan5.txt` on a proportionality call that
// turned out to rest on an unreleased Debug build's own timing - both reverted). `lan5.txt` loads
// through `app::load_play`/`load_placed_scene` (the same app-layer loader `bench_frame`'s
// `load_any_scene` uses for a fixed map); `anno_render_tests` links `anno_app_core` for this one
// case (`anno_render_gpu_tests` already does, for its own tests) - its own units have no start
// units (`load_placed_scene` does not populate `scene.units`), so only its static+water class is
// checked.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/loader.hpp>
#include <anno/app/play.hpp>
#include <anno/assets/building.hpp>
#include <anno/assets/figure.hpp>
#include <anno/assets/island.hpp>
#include <anno/assets/unit.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/formats/insel.hpp>
#include <anno/formats/sze.hpp>
#include <anno/formats/tex.hpp>
#include <anno/render/atlas_pages.hpp>
#include <anno/render/frame_table.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/scene.hpp>
#include <anno/test/game_dir.hpp>

namespace app = anno::app;

namespace render = anno::render;
namespace fig = anno::formats::figuren;
using Rgba = anno::assets::Rgba;

namespace {

std::vector<std::uint8_t> read_all(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    REQUIRE_MESSAGE(bool(in), "cannot open " << p.string());
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), {});
}

/// A render::Scene built directly from the original files, no app-layer loader (the same
/// pattern render_corpus_test.cpp's own load_islands uses). Buffers are kept for the test run.
render::Scene load_scene_direct(const std::filesystem::path& dir, const std::filesystem::path& file,
                                std::deque<std::vector<std::uint8_t>>& keep) {
    keep.push_back(read_all(dir / "Textures" / "Felder.tex"));
    const auto felder = anno::formats::tex::TexFile::parse(keep.back()).value();
    keep.push_back(read_all(dir / "Anlagen.dat"));
    auto anlagen = anno::formats::anlagen::AnlagenFile::parse(keep.back()).value();
    auto textures = anno::assets::island::GroundTextures::load(felder).value();
    if (const auto c = render::water_colour(anlagen)) {
        textures.set_water(*c);
    }
    keep.push_back(read_all(file));
    return render::Scene("", anno::formats::insel::islands(keep.back()).value(),
                         std::move(textures), std::move(anlagen), render::kHeightLift);
}

/// A fixed map (`data/maps/*.txt`), loaded the app layer's own way (`bench_frame`'s own
/// `load_any_scene` uses the identical two calls for its `.txt` branch - no second loading path):
/// `app::load_play` resolves the layout's templates into placed islands, `app::load_placed_scene`
/// builds the `render::Scene` from them. `scene->units` stays empty (`load_placed_scene` does not
/// populate it), so a fixed map has no figures to check here (review F2).
std::shared_ptr<render::Scene> load_fixed_map(const std::filesystem::path& dir,
                                              const std::filesystem::path& file) {
    auto play = app::load_play(dir, file, app::FixedPlayOptions{});
    REQUIRE_MESSAGE(bool(play), "load_play: " << (play ? "" : play.error()));
    auto loaded = app::load_placed_scene(dir, play.value().name, play.value().placed);
    REQUIRE_MESSAGE(bool(loaded), "load_placed_scene: " << (loaded ? "" : loaded.error()));
    return loaded.value().scene;
}

struct VolumeParts {
    std::vector<std::filesystem::path> paths;
    std::vector<std::uint64_t> sizes;
};

VolumeParts volume_parts(const std::filesystem::path& dir, std::uint32_t group) {
    VolumeParts out;
    for (std::uint32_t part = 0;; ++part) {
        const auto p = dir / fig::volume_file_name(group, part);
        std::error_code ec;
        const auto size = std::filesystem::file_size(p, ec);
        if (ec) {
            break;
        }
        out.paths.push_back(p);
        out.sizes.push_back(size);
    }
    return out;
}

/// The scenario's start units, decoded (mirrors app/src/loader.cpp's own load_units: plain file
/// I/O and the public assets::unit API, not a second decode path). `sze_bytes` must outlive the
/// return value's own use: formats::sze::Scene holds spans into it, not a copy. Likewise
/// FigurenHeader.dat's own bytes must outlive `header`'s use below (formats::figuren::Entry
/// holds spans into them too) - binding a named `header_bytes` instead of passing read_all(...)
/// straight into parse() avoids the same dangling-temporary bug this function's sze_bytes
/// parameter was already changed to avoid.
std::vector<render::UnitSprite> load_units(const std::filesystem::path& dir,
                                           const std::vector<std::uint8_t>& sze_bytes) {
    auto scenario = anno::formats::sze::Scene::parse(sze_bytes);
    REQUIRE_MESSAGE(bool(scenario), "sze parse: " << (scenario ? "" : scenario.error()));
    const auto header_bytes = read_all(dir / "FigurenHeader.dat");
    auto header = fig::FigurenHeader::parse(header_bytes);
    REQUIRE_MESSAGE(bool(header), "FigurenHeader parse");
    auto parts = std::make_shared<std::map<std::uint32_t, VolumeParts>>();
    const anno::assets::unit::VolumeReader read =
        [dir, parts](std::uint32_t group, std::uint64_t offset,
                     std::uint64_t length) -> std::optional<std::vector<std::uint8_t>> {
        auto it = parts->find(group);
        if (it == parts->end()) {
            it = parts->emplace(group, volume_parts(dir, group)).first;
        }
        const auto pieces = fig::locate(it->second.sizes, offset, length);
        if (!pieces) {
            return std::nullopt;
        }
        std::vector<std::uint8_t> data;
        for (const auto& piece : *pieces) {
            std::ifstream in(it->second.paths[piece.part], std::ios::binary);
            in.seekg(static_cast<std::streamoff>(piece.start));
            std::vector<char> buf(static_cast<std::size_t>(piece.end - piece.start));
            if (!buf.empty() && !in.read(buf.data(), static_cast<std::streamsize>(buf.size()))) {
                return std::nullopt;
            }
            data.insert(data.end(), buf.begin(), buf.end());
        }
        return data;
    };
    const std::array kinds = {anno::assets::unit::UnitKind::Ship,
                              anno::assets::unit::UnitKind::Soldier};
    auto units = anno::assets::unit::units(scenario.value(), header.value(), kinds,
                                           anno::assets::figure::kOriginalPlayerColours, read);
    REQUIRE_MESSAGE(bool(units), "units(): " << (units ? "" : units.error()));
    std::vector<render::UnitSprite> out;
    for (auto& u : units.value().units) {
        render::UnitSprite s{u.x, u.y, std::nullopt, {}, {}};
        auto convert = [&](const std::vector<std::optional<anno::assets::unit::DecodedFrame>>& v,
                           bool shadow) {
            std::vector<std::optional<render::UnitFrame>> r;
            for (const auto& f : v) {
                r.push_back(f ? std::optional<render::UnitFrame>(
                                    render::to_unit_frame(*f, u.figure, shadow))
                              : std::nullopt);
            }
            return r;
        };
        s.frames = convert(u.frames, false);
        s.shadows = convert(u.shadows, true);
        out.push_back(std::move(s));
    }
    return out;
}

/// The static+water frames of `scene` at `group` (dedup via FrameTable + SpriteCache, the
/// engine's own decode path: assets::building::to_rgba, smooth_2x2 for the enhanced-water
/// variant - no second decoder). Mirrors RENDER-DIRECT-1a's own enumeration (prepare_objects,
/// animation()/frame_index(), the enhanced-water exclusion constants), now decoding instead of
/// only sizing.
std::vector<render::AtlasFrameInput> collect_static_water(const render::Scene& scene,
                                                          std::uint32_t group,
                                                          render::SpriteCache& cache,
                                                          render::FrameTable& table) {
    auto add_object = [&](const render::Prepared& p, bool enhanced_eligible) {
        const std::uint32_t id = p.first.anlage;
        const fig::Entry* entry = scene.anlagen.entry(id);
        if (entry == nullptr || entry->gfx.empty()) {
            return;
        }
        const fig::GfxSet& set = entry->gfx.front();
        const fig::FrameSet* frames = nullptr;
        for (const auto& fs : set.frame_sets) {
            if (fs.group == group) {
                frames = &fs;
                break;
            }
        }
        if (frames == nullptr) {
            return;
        }
        const std::size_t n = std::max<std::size_t>(render::animation(set).first, 1);
        const std::uint64_t ms = render::animation(set).second;
        const std::size_t total = frames->frames.size();
        const std::size_t rotations = std::max<std::size_t>(total / n, 1);
        const std::size_t block = (std::size_t{p.first.rotation} % rotations) * n;
        const std::size_t phase =
            id == render::kSurfAnlage
                ? (3 * std::size_t{p.first.y} + 5 * std::size_t{p.first.x}) % 32
                : std::size_t{p.first.phase()};
        const bool animated = ms != 0 && n > 1;
        const std::size_t step_count = animated ? n : 1;
        const std::size_t first_step = phase % n;
        for (std::size_t i = 0; i < step_count; ++i) {
            const std::size_t step = animated ? i : first_step;
            const std::size_t k = block + step;
            if (k >= total || frames->frames[k].length == 0) {
                continue;
            }
            const render::SpriteKey key{id, 0, false, group, k};
            if (const auto fid =
                    table.intern_building(scene.anlagen, cache, key, frames->frames[k])) {
                // the caller appends via the shared `out` vector below.
                (void)fid;
            }
            if (enhanced_eligible) {
                const render::SpriteKey smoothed{id, render::kSmoothed, false, group, k};
                table.intern_building(scene.anlagen, cache, smoothed, frames->frames[k]);
            }
        }
    };
    std::vector<render::AtlasFrameInput> out;
    std::map<render::FrameId, bool> already; // dedupe across islands/groups within this table use
    auto harvest = [&] {
        for (render::FrameId id = 0; id < static_cast<render::FrameId>(table.size()); ++id) {
            if (already.contains(id)) {
                continue;
            }
            already[id] = true;
            if (const auto data = table.get(id)) {
                out.push_back(render::AtlasFrameInput{id, data->image});
            }
        }
    };
    for (const auto& island : scene.islands) {
        const auto statics = render::prepare_objects(island, scene.anlagen, render::kObjectRecords,
                                                     render::Objects::NotWater);
        for (const auto& p : statics) {
            add_object(p, false);
        }
        const auto below = render::prepare_objects(island, scene.anlagen, render::kObjectRecords,
                                                   render::Objects::WaterBelowSea);
        for (const auto& p : below) {
            const std::uint32_t id = p.first.anlage;
            const bool is_fall =
                std::find(render::kRiverFallAnlagen.begin(), render::kRiverFallAnlagen.end(), id) !=
                render::kRiverFallAnlagen.end();
            add_object(p, id != render::kSurfAnlage && !is_fall);
        }
        const auto above = render::prepare_objects(island, scene.anlagen, render::kObjectRecords,
                                                   render::Objects::WaterAboveSea);
        for (const auto& p : above) {
            const std::uint32_t id = p.first.anlage;
            const bool is_river =
                std::find(render::kRiverAnlagen.begin(), render::kRiverAnlagen.end(), id) !=
                render::kRiverAnlagen.end();
            const bool is_fall =
                std::find(render::kRiverFallAnlagen.begin(), render::kRiverFallAnlagen.end(), id) !=
                render::kRiverFallAnlagen.end();
            add_object(p, !is_river && !is_fall);
        }
    }
    harvest();
    return out;
}

/// The figure frames `units` actually use at `group` (dedup via FrameTable, keyed the same way
/// the real frame table would for a unit frame).
std::vector<render::AtlasFrameInput> collect_figures(const std::vector<render::UnitSprite>& units,
                                                     std::uint32_t group,
                                                     render::FrameTable& table) {
    std::vector<render::AtlasFrameInput> out;
    std::map<render::FrameId, bool> already;
    auto add = [&](const std::vector<std::optional<render::UnitFrame>>& frames, bool shadow) {
        if (group >= frames.size() || !frames[group]) {
            return;
        }
        const render::UnitFrame& f = *frames[group];
        const auto id = table.intern(render::FrameKey::unit(f.id, group, shadow),
                                     [&] { return std::pair{f.frame, f.image}; });
        if (id && !already.contains(*id)) {
            already[*id] = true;
            out.push_back(render::AtlasFrameInput{*id, f.image});
        }
    };
    for (const auto& u : units) {
        add(u.frames, false);
        add(u.shadows, true);
    }
    return out;
}

/// `img`'s own pixel at (x, y), premultiplied - the expectation every level-0 texel of its own
/// rectangle is checked against directly (review F2: every frame, not a sample).
std::array<std::uint8_t, 4> expected_level0(const Rgba& img, std::uint32_t x, std::uint32_t y) {
    const std::size_t si = (std::size_t{y} * img.width + x) * 4;
    return render::premultiply_pixel(
        {img.pixels[si + 0], img.pixels[si + 1], img.pixels[si + 2], img.pixels[si + 3]});
}

void check_pages_against_source(const std::vector<render::AtlasFrameInput>& frames,
                                const render::AtlasPages& pages, const char* what) {
    CHECK_MESSAGE(pages.oversized.empty(), what << ": no frame is too large for one page");
    CHECK_MESSAGE(pages.duplicate_ids.empty(), what << ": no frame id repeated");
    // Every frame, every level-0 texel of its own rectangle, and every mip texel of its own mip
    // rectangle (review F2: the packet's own step 4 asks this of every frame, not a sample - the
    // synthetic tests prove the rules exhaustively on small cases, this proves no real frame is
    // the exception).
    for (const auto& f : frames) {
        const render::AtlasFramePlacement* place = nullptr;
        for (const auto& p : pages.placements) {
            if (p.id == f.id) {
                place = &p;
                break;
            }
        }
        REQUIRE_MESSAGE(place != nullptr, what << ": frame " << f.id << " was not placed");
        REQUIRE(place->width == f.image->width);
        REQUIRE(place->height == f.image->height);
        const auto& l0 = pages.level0[place->layer];
        for (std::uint32_t y = 0; y < f.image->height; ++y) {
            for (std::uint32_t x = 0; x < f.image->width; ++x) {
                const std::size_t pi =
                    (std::size_t{place->y + y} * pages.page_size + (place->x + x)) * 4;
                const std::array<std::uint8_t, 4> got{l0[pi], l0[pi + 1], l0[pi + 2], l0[pi + 3]};
                CHECK(got == expected_level0(*f.image, x, y));
            }
        }
        // The mip rectangle, box-filtered from level 0 at the same slot (not from the frame's own
        // source pixels directly - the mip rule averages dilated padding too, at an odd
        // width/height's one-texel-wider mip edge, exactly the case review F4 found wrong). A
        // mip texel at absolute page coordinate (mx_abs, my_abs) is written from the level-0 2x2
        // box at (2*mx_abs, 2*my_abs) (write_level1: slot_x is even, so slot_x/2 is exact, and
        // this holds for every mip texel of the whole slot, frame sub-rectangle included).
        const auto& l1 = pages.level1[place->layer];
        const std::uint32_t mip_stride = pages.page_size / 2;
        for (std::uint32_t my = 0; my < place->mip_height; ++my) {
            for (std::uint32_t mx = 0; mx < place->mip_width; ++mx) {
                const std::uint32_t mx_abs = place->mip_x + mx, my_abs = place->mip_y + my;
                const std::uint32_t l0x = 2 * mx_abs, l0y = 2 * my_abs; // absolute page coords
                std::array<std::uint8_t, 4> expected{};
                for (std::size_t c = 0; c < 4; ++c) {
                    unsigned s = 0;
                    for (const auto [dx, dy] :
                         {std::pair{0U, 0U}, {1U, 0U}, {0U, 1U}, {1U, 1U}}) {
                        // Frame-local coordinate of this level-0 texel (place->x/y is the
                        // frame's own absolute origin); clamped exactly as write_level0 clamps
                        // the dilated padding.
                        const std::int64_t fx = std::int64_t{l0x + dx} - place->x;
                        const std::int64_t fy = std::int64_t{l0y + dy} - place->y;
                        const auto cx = static_cast<std::uint32_t>(std::clamp<std::int64_t>(
                            fx, 0, std::int64_t{f.image->width} - 1));
                        const auto cy = static_cast<std::uint32_t>(std::clamp<std::int64_t>(
                            fy, 0, std::int64_t{f.image->height} - 1));
                        s += expected_level0(*f.image, cx, cy)[c];
                    }
                    expected[c] = static_cast<std::uint8_t>((s + 2) / 4);
                }
                const std::size_t mi = (std::size_t{my_abs} * mip_stride + mx_abs) * 4;
                const std::array<std::uint8_t, 4> got{l1[mi], l1[mi + 1], l1[mi + 2], l1[mi + 3]};
                CHECK(got == expected);
            }
        }
    }
    MESSAGE(what << ": " << frames.size() << " frames checked (every frame, every texel)");
}

} // namespace

TEST_CASE("atlas pages: static+water and figures are texel exact on real scenes (game files)" *
          doctest::test_suite("heavy")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto t0 = std::chrono::steady_clock::now();
    const std::vector<std::filesystem::path> scenes = {
        *dir / "Scenes" / "0010SingleEndlos" / "02_1024_easy_128.sze",
        *dir / "Scenes" / "0100Kampagne01" / "S01_01_01.sze"};
    for (const auto& file : scenes) {
        std::error_code ec;
        if (!std::filesystem::exists(file, ec)) {
            MESSAGE("skipping (not found): " << file.string());
            continue;
        }
        std::deque<std::vector<std::uint8_t>> keep;
        const render::Scene scene = load_scene_direct(*dir, file, keep);
        const auto sze_bytes = read_all(file);
        const auto units = load_units(*dir, sze_bytes);
        for (std::uint32_t group = 0; group < render::kGroups; ++group) {
            render::SpriteCache cache;
            render::FrameTable static_table, figure_table;
            const auto static_frames = collect_static_water(scene, group, cache, static_table);
            const auto figure_frames = collect_figures(units, group, figure_table);
            const auto static_pages = render::build_atlas_pages(static_frames, 2048, 2, 4);
            const auto figure_pages = render::build_atlas_pages(figure_frames, 2048, 2, 4);
            check_pages_against_source(static_frames, static_pages, "static+water");
            check_pages_against_source(figure_frames, figure_pages, "figures");
        }
    }

    // data/maps/lan5.txt (review F2): the packet's third named scene, loaded through the app
    // layer (load_fixed_map) since a fixed map's islands come from several .scp templates
    // resolved that way, not read directly like a scenario's own islands above. No figures:
    // load_placed_scene does not populate scene->units (atlas.md section 5's own finding).
    const std::filesystem::path lan5 =
        std::filesystem::path(ANNO_FIXTURES_DIR).parent_path().parent_path() / "data" / "maps" /
        "lan5.txt";
    std::error_code lan5_ec;
    if (!std::filesystem::exists(lan5, lan5_ec)) {
        MESSAGE("skipping lan5.txt (not found): " << lan5.string());
    } else {
        const auto scene = load_fixed_map(*dir, lan5);
        CHECK_MESSAGE(scene->units.empty(),
                     "lan5.txt: load_placed_scene does not populate units (expected)");
        for (std::uint32_t group = 0; group < render::kGroups; ++group) {
            render::SpriteCache cache;
            render::FrameTable static_table;
            const auto static_frames = collect_static_water(*scene, group, cache, static_table);
            const auto static_pages = render::build_atlas_pages(static_frames, 2048, 2, 4);
            check_pages_against_source(static_frames, static_pages, "lan5.txt static+water");
        }
    }
    const double wall_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    MESSAGE("atlas pages: whole case wall time " << wall_ms << " ms");
}
