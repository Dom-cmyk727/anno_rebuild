// RENDER-DIRECT-1b step 4: the texel-exact proof on real decoded game data, over the static+
// water class and the figures of two scenario scenes' start units (game files; "heavy": a
// scene's whole frame set, decoded, held at once): `Scenes/0010SingleEndlos/
// 02_1024_easy_128.sze` (1a's own worst scene) and one campaign scene. The packet's third named
// scene, `data/maps/lan5.txt`, is in `anno_app_tests` instead (`atlas_pages_lan5_test.cpp`): a
// fixed map loads through `app::load_play`/`load_placed_scene`, and `anno_render` sits below
// `anno_app_core` (which depends on it) - `anno_render_tests` must not depend on `anno_app_core`
// to stay below it too (orchestrator, after review F2; `anno_render_gpu_tests`'s own link to
// `anno_app_core` is the one, pre-existing exception, not a precedent to extend). Every frame is
// checked, not a sample (review F2: an earlier revision of this file sampled every 7th frame and
// excluded `lan5.txt` on a proportionality call that turned out to rest on an unreleased Debug
// build's own timing - both reverted; the deviation itself should have gone to the orchestrator
// before being built, not reported afterward). The frame-gathering and the checker
// (`collect_static_water`, `check_pages_against_source`) are shared with the lan5 case via
// `anno/test/atlas_pages_check.hpp` (`src/test_support`, header-only) rather than copied.
#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
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
#include <anno/test/atlas_pages_check.hpp>
#include <anno/test/game_dir.hpp>

namespace render = anno::render;
namespace fig = anno::formats::figuren;

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

/// The figure frames `units` actually use at `group` (dedup via FrameTable, keyed the same way
/// the real frame table would for a unit frame). Not shared with the lan5 case (unlike
/// collect_static_water/check_pages_against_source, anno/test/atlas_pages_check.hpp): a fixed
/// map has no start units to decode (load_placed_scene does not populate scene.units).
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
        // ANNO1503_GAME_DIR set but a named scene missing from it is a failure, not a skip
        // (CLAUDE.md: a folder without the expected files fails with one clear message; only
        // an unset directory skips, through anno::test::game_dir above).
        if (!std::filesystem::exists(file, ec)) {
            FAIL("named scene is not in this game directory: " << file.string());
        }
        std::deque<std::vector<std::uint8_t>> keep;
        const render::Scene scene = load_scene_direct(*dir, file, keep);
        const auto sze_bytes = read_all(file);
        const auto units = load_units(*dir, sze_bytes);
        for (std::uint32_t group = 0; group < render::kGroups; ++group) {
            render::SpriteCache cache;
            render::FrameTable static_table, figure_table;
            const auto static_frames =
                anno::test::collect_static_water(scene, group, cache, static_table);
            const auto figure_frames = collect_figures(units, group, figure_table);
            const auto static_pages = render::build_atlas_pages(static_frames, 2048, 2, 4);
            const auto figure_pages = render::build_atlas_pages(figure_frames, 2048, 2, 4);
            anno::test::check_pages_against_source(static_frames, static_pages, "static+water");
            anno::test::check_pages_against_source(figure_frames, figure_pages, "figures");
        }
    }
}
