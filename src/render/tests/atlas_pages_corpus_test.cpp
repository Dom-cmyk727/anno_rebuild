// RENDER-DIRECT-1b step 4: the texel-exact proof on real decoded game data, over the static+
// water class and the figures of two scenes' start units (game files; "heavy": a scene's whole
// frame set, decoded, held at once). The packet's own list of three scenes names
// `data/maps/lan5.txt` too; covered by `bench_frame --atlas-build` (step 5, its own report)
// instead of here, not because a render-layer test cannot depend on the app layer for test
// purposes - anno_render_gpu_tests already links anno_app_core (src/render/CMakeLists.txt) - but
// because loading a fixed map this way is disproportionate to what this test checks:
// `app::load_play`/`load_placed_scene` builds a whole `sim::World` (registry, the sim_load
// stack) to get picture inputs, measured (bench_frame --atlas-build data/maps/lan5.txt) at about
// 85 s of app-layer load time on top of the atlas build itself (12.8 s) - for a unit test whose
// job is proving texel-exactness, not exercising the play-loading path. The two .sze scenario
// files below load directly through render::Scene's own constructor and formats::insel::islands,
// with no such cost.
#include <algorithm>
#include <array>
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

void check_pages_against_source(const std::vector<render::AtlasFrameInput>& frames,
                                const render::AtlasPages& pages, const char* what) {
    CHECK_MESSAGE(pages.oversized.empty(), what << ": no frame is too large for one page");
    // Sampled, not exhaustive (a real scene's frame set is large; the synthetic tests already
    // prove every rule exhaustively on small cases) - every 7th frame's whole rectangle, plus
    // its padding corners and one mip texel, checked against the source image directly.
    std::size_t checked = 0;
    for (std::size_t i = 0; i < frames.size(); i += 7) {
        const auto& f = frames[i];
        const render::AtlasFramePlacement* place = nullptr;
        for (const auto& p : pages.placements) {
            if (p.id == f.id) {
                place = &p;
                break;
            }
        }
        REQUIRE_MESSAGE(place != nullptr, what << ": frame " << f.id << " was not placed");
        CHECK(place->width == f.image->width);
        CHECK(place->height == f.image->height);
        const auto& layer = pages.level0[place->layer];
        for (std::uint32_t corner : {0U, 1U}) {
            const std::uint32_t x = corner == 0 ? 0 : f.image->width - 1;
            const std::uint32_t y = corner == 0 ? 0 : f.image->height - 1;
            const std::size_t si = (std::size_t{y} * f.image->width + x) * 4;
            const auto expected =
                render::premultiply_pixel({f.image->pixels[si + 0], f.image->pixels[si + 1],
                                           f.image->pixels[si + 2], f.image->pixels[si + 3]});
            const std::size_t pi =
                (std::size_t{place->y + y} * pages.page_size + (place->x + x)) * 4;
            const std::array<std::uint8_t, 4> got{layer[pi], layer[pi + 1], layer[pi + 2],
                                                  layer[pi + 3]};
            CHECK(got == expected);
        }
        ++checked;
    }
    MESSAGE(what << ": " << frames.size() << " frames, " << checked << " sampled and matched");
}

} // namespace

TEST_CASE("atlas pages: static+water and figures are texel exact on real scenes (game files)" *
          doctest::test_suite("heavy")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
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
}
