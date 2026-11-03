// anno_app_core: PNG output (always) and scene loading (needs
// ANNO1503_GAME_DIR; skips otherwise).
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/cli.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/png.hpp>
#include <anno/app/still.hpp>
#include <anno/app/zoom.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/bytes.hpp>
#include <anno/formats/insel.hpp>
#include <anno/test/game_dir.hpp>

namespace fs = std::filesystem;

namespace {

std::uint32_t be32(const std::vector<std::uint8_t>& b, std::size_t o) {
    return std::uint32_t{b[o]} << 24 | std::uint32_t{b[o + 1]} << 16 |
           std::uint32_t{b[o + 2]} << 8 | std::uint32_t{b[o + 3]};
}

/// Reads back a PNG written by write_png: checks every chunk CRC, inflates
/// the stored deflate blocks, checks the Adler-32 and returns the RGBA rows.
std::vector<std::uint8_t> read_stored_png(const fs::path& p, std::uint32_t& w, std::uint32_t& h) {
    std::ifstream in(p, std::ios::binary);
    const std::vector<std::uint8_t> b(std::istreambuf_iterator<char>(in), {});
    REQUIRE(b.size() > 8);
    CHECK(std::memcmp(b.data(), "\x89PNG\r\n\x1a\n", 8) == 0);
    std::vector<std::uint8_t> z;
    for (std::size_t o = 8; o < b.size();) {
        const std::uint32_t len = be32(b, o);
        const std::string type(b.begin() + static_cast<std::ptrdiff_t>(o + 4),
                               b.begin() + static_cast<std::ptrdiff_t>(o + 8));
        const std::span<const std::uint8_t> body(b.data() + o + 4, len + 4);
        CHECK(anno::app::crc32(body) == be32(b, o + 8 + len));
        if (type == "IHDR") {
            w = be32(b, o + 8);
            h = be32(b, o + 12);
        } else if (type == "IDAT") {
            z.insert(z.end(), b.begin() + static_cast<std::ptrdiff_t>(o + 8),
                     b.begin() + static_cast<std::ptrdiff_t>(o + 8 + len));
        }
        o += 12 + len;
    }
    REQUIRE(z.size() >= 6);
    CHECK(z[0] == 0x78);
    std::vector<std::uint8_t> raw;
    std::size_t i = 2;
    for (bool last = false; !last;) {
        last = (z[i] & 1) != 0;
        const std::size_t n = z[i + 1] | std::size_t{z[i + 2]} << 8;
        CHECK((n ^ (z[i + 3] | std::size_t{z[i + 4]} << 8)) == 0xFFFF);
        raw.insert(raw.end(), z.begin() + static_cast<std::ptrdiff_t>(i + 5),
                   z.begin() + static_cast<std::ptrdiff_t>(i + 5 + n));
        i += 5 + n;
    }
    std::uint32_t a = 1, s = 0;
    for (const std::uint8_t v : raw) {
        a = (a + v) % 65521;
        s = (s + a) % 65521;
    }
    CHECK(be32(z, i) == (s << 16 | a));
    std::vector<std::uint8_t> rgba;
    const std::size_t row = std::size_t{w} * 4;
    for (std::uint32_t y = 0; y < h; ++y) {
        CHECK(raw[y * (row + 1)] == 0);
        rgba.insert(rgba.end(), raw.begin() + static_cast<std::ptrdiff_t>(y * (row + 1) + 1),
                    raw.begin() + static_cast<std::ptrdiff_t>((y + 1) * (row + 1)));
    }
    return rgba;
}

} // namespace

TEST_CASE("CRC-32 check value") {
    const std::string s = "123456789";
    CHECK(anno::app::crc32({reinterpret_cast<const std::uint8_t*>(s.data()), s.size()}) ==
          0xCBF43926U);
}

TEST_CASE("PNG round trip, including more than one stored block") {
    const fs::path out = fs::temp_directory_path() / "anno_app_test.png";
    // 200 × 100 RGBA: 80,100 filtered bytes, two stored blocks.
    std::vector<std::uint8_t> px(200 * 100 * 4);
    for (std::size_t k = 0; k < px.size(); ++k) {
        px[k] = static_cast<std::uint8_t>(k * 7 + k / 13);
    }
    REQUIRE(anno::app::write_png(out, 200, 100, px).empty());
    std::uint32_t w = 0, h = 0;
    CHECK(read_stored_png(out, w, h) == px);
    CHECK(w == 200);
    CHECK(h == 100);
    CHECK_FALSE(anno::app::write_png(out, 3, 3, px).empty());
    fs::remove(out);
}

TEST_CASE("scene loading: islands, links and units") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // Single_01: 15 INSEL5 islands, 5 ships; no links.
    auto s1 = anno::app::load_scene(*dir, *dir / "Scenes" / "0200Single" / "Single_01.sze");
    REQUIRE(s1);
    CHECK(s1.value().scene->islands.size() == 15);
    CHECK(s1.value().scene->units.size() == 5);
    // Case-insensitive path resolution as on Windows.
    CHECK(anno::app::find_ci(*dir / "Scenes", "0200SINGLE\\single_01.SZE"));
    CHECK_FALSE(anno::app::find_ci(*dir / "Scenes", "nope\\x.scp"));
    // An endless game links its islands (INSELLNK): stand-ins are loaded.
    const auto endless = anno::test::files_with_ext(*dir / "Scenes" / "0010SingleEndlos", "sze");
    REQUIRE_FALSE(endless.empty());
    auto s2 = anno::app::load_scene(*dir, endless.front());
    REQUIRE(s2);
    CHECK(s2.value().scene->islands.size() > 0);
    bool noted = false;
    for (const auto& n : s2.value().notes) {
        noted = noted || n.find("stand-ins") != std::string::npos;
    }
    CHECK(noted);
}

TEST_CASE("game directory discovery") {
    // Unique per run, so parallel test runs (dev, asan) do not collide.
    const fs::path root =
        fs::temp_directory_path() /
        ("anno_app_test_gamedir_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::remove_all(root);
    fs::create_directories(root / "Scenes" / "x");
    std::ofstream(root / "Anlagen.dat").put('\0');
    const fs::path file = root / "Scenes" / "x" / "a.sze";
    // Explicit beats the environment, the environment beats the search.
    CHECK(anno::app::find_game_dir(fs::path("E"), file, fs::path("V")).value() == fs::path("E"));
    CHECK(anno::app::find_game_dir(std::nullopt, file, fs::path("V")).value() == fs::path("V"));
    // The nearest ancestor holding Anlagen.dat.
    const auto found = anno::app::find_game_dir(std::nullopt, file, std::nullopt);
    REQUIRE(found);
    CHECK(fs::equivalent(found.value(), root));
    fs::remove(root / "Anlagen.dat");
    CHECK_FALSE(anno::app::find_game_dir(std::nullopt, file, std::nullopt));
    fs::remove_all(root);
}

TEST_CASE("derived zoom levels for --png: the level table, the bound and a derived picture") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto loaded = anno::app::load_scene(*dir, *dir / "Scenes" / "0200Single" / "Single_01.sze");
    REQUIRE(loaded);
    anno::render::Scene& scene = *loaded.value().scene;
    CHECK(anno::app::set_derived_levels(scene, 2, 8).empty());
    CHECK(scene.levels().size() == anno::render::kGroups);
    CHECK(anno::app::set_derived_levels(scene, 5, 8).empty());
    CHECK(scene.levels().size() == 6);
    CHECK(scene.levels()[5].derived);
    CHECK(scene.levels().ancestor(5) == 2);
    CHECK(anno::app::set_derived_levels(scene, 10, 8).empty());
    CHECK_FALSE(anno::app::set_derived_levels(scene, 11, 8).empty());
    CHECK(scene.levels().size() == 11); // a refused group changes nothing
    REQUIRE(anno::app::set_derived_levels(scene, 3, 8).empty());
    const auto b = scene.bounds(3);
    REQUIRE(b);
    const anno::render::Rect view{b->x + b->w / 2 - 100, b->y + b->h / 2 - 60, 200, 120};
    anno::render::FrameStats frames;
    const auto a = anno::app::derived_view(scene, view, 3, anno::render::Layers::all(), frames);
    REQUIRE(a);
    CHECK(a->width == 200);
    CHECK(a->height == 120);
    CHECK(frames.chunks_built > 0);
    // Opaque, and a picture rather than one colour.
    std::set<std::uint32_t> colours;
    std::size_t opaque = 0;
    for (std::size_t i = 0; i + 3 < a->pixels.size(); i += 4) {
        colours.insert(std::uint32_t{a->pixels[i]} << 16 | std::uint32_t{a->pixels[i + 1]} << 8 |
                       a->pixels[i + 2]);
        opaque += a->pixels[i + 3] == 255 ? 1U : 0U;
    }
    CHECK(opaque == a->width * a->height);
    CHECK(colours.size() > 8);
    // The same view again is the same picture.
    anno::render::FrameStats again;
    const auto c = anno::app::derived_view(scene, view, 3, anno::render::Layers::all(), again);
    REQUIRE(c);
    CHECK(c->pixels == a->pixels);
}

TEST_CASE("command line") {
    using anno::app::parse_args;
    using V = std::vector<std::string>;
    // Invalid input: no --view, odd count, bad values, unknown options.
    for (const V& bad :
         {V{}, V{"--view"}, V{"--view", "x.sze", "--group", "27"}, V{"--view", "x", "--bogus", "1"},
          V{"--view", "x", "--size", "0x5"}, V{"--view", "x", "--size", "-5x5"},
          V{"--view", "x", "--size", "10x"}, V{"--view", "x", "--size", "70000x10"},
          V{"--view", "x", "--at", "1,2,3"}, V{"--view", "x", "--at", "2000000000,0"},
          V{"--view", "x", "--time", "-1"}, V{"--view", "x", "--time", "+-1"},
          V{"--view", "x", "--water", "nice"}, V{"--view", "x", "--renderer", "vulkan"}}) {
        CHECK_FALSE(parse_args(bad));
    }
    const auto a = parse_args({"--view", "Single_01.sze", "--group", "+1", "--png", "o.png", "--at",
                               "10,-4", "--size", "800x600", "--time", "1234", "--water",
                               "enhanced", "--renderer", "cpu", "--game-dir", "g"});
    REQUIRE(a);
    CHECK(a->file == fs::path("Single_01.sze"));
    CHECK(a->group == std::optional<std::size_t>(1));
    CHECK(a->at == std::optional<std::pair<int, int>>({10, -4}));
    CHECK(a->size == std::pair{800, 600});
    CHECK(a->time == 1234);
    CHECK(a->water == anno::render::WaterStyle::Enhanced);
    CHECK(a->renderer == anno::app::Renderer::Cpu);
    // Defaults as the Rust viewer.
    const auto d = parse_args({"--view", "x.scp"});
    REQUIRE(d);
    CHECK_FALSE(d->group);
    CHECK(d->size == std::pair{1600, 900});
    CHECK(d->water == anno::render::WaterStyle::Original);
    CHECK_FALSE(d->water_given); // the opening may switch to enhanced water
    const auto wo = parse_args({"--view", "x.scp", "--water", "original"});
    REQUIRE(wo);
    CHECK(wo->water == anno::render::WaterStyle::Original);
    CHECK(wo->water_given); // an explicit choice the opening keeps (review 2026-11-03)
    CHECK(d->shore == anno::render::kShoreHeight); // corners at 16 are water by default
    const auto sh = parse_args({"--view", "x.scp", "--shore", "16"});
    REQUIRE(sh);
    CHECK(sh->shore == 16.0F);
    CHECK_FALSE(parse_args({"--view", "x.scp", "--shore", "-1"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--shore", "256"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--shore", "nan"}));
    // The viewer shows the flow at gain 4 with the median-cell speed (OQ-083, OQ-065).
    using Ref = anno::render::river::RiverConfig::Reference;
    CHECK(d->river_gain == 4.0F);
    CHECK(d->river_speed == Ref::MedianCell);
    CHECK(d->river_gap_patch == 8.0F);
    const auto f = parse_args({"--view", "x.scp", "--river-speed", "fastest", "--river-gain", "1"});
    REQUIRE(f);
    CHECK(f->river_speed == Ref::FastestSection);
    CHECK(f->river_gain == 1.0F);
    CHECK_FALSE(parse_args({"--view", "x.scp", "--river-speed", "fast"}));
    CHECK(d->lookahead_chunks == 3);
    const auto la = parse_args({"--view", "x.scp", "--lookahead-chunks", "2"});
    REQUIRE(la);
    CHECK(la->lookahead_chunks == 2);
    CHECK_FALSE(parse_args({"--view", "x.scp", "--lookahead-chunks", "-1"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--lookahead-chunks", "17"}));
    CHECK(parse_args({"--view", "x.scp", "--lookahead-chunks", "16"}));
    const auto ring = parse_args({"--view", "x.scp", "--lookahead-chunks", "0"});
    REQUIRE(ring);
    CHECK(ring->lookahead_chunks == 0);
    // The zoom options: continuous is the default (OQ-095) and not "explicit";
    // stepped is the faithful option; the factor and easing are each bounded.
    CHECK(d->zoom.mode == anno::app::ZoomMode::Continuous);
    CHECK_FALSE(d->zoom.mode_explicit);
    const auto zs = parse_args({"--view", "x.scp", "--zoom-mode", "stepped"});
    REQUIRE(zs);
    CHECK(zs->zoom.mode == anno::app::ZoomMode::Stepped);
    CHECK(zs->zoom.mode_explicit);
    CHECK(d->zoom.step == 1.1);
    CHECK(d->zoom.ease_ms == 120U);
    const auto zc = parse_args({"--view", "x.scp", "--zoom-mode", "continuous", "--zoom-step",
                                "1.25", "--zoom-ease", "0"});
    REQUIRE(zc);
    CHECK(zc->zoom.mode == anno::app::ZoomMode::Continuous);
    CHECK(zc->zoom.mode_explicit);
    CHECK(zc->zoom.step == 1.25);
    CHECK(zc->zoom.ease_ms == 0U);
    CHECK_FALSE(parse_args({"--view", "x.scp", "--zoom-mode", "smooth"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--zoom-step", "1.0"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--zoom-step", "2.5"}));
    CHECK(parse_args({"--view", "x.scp", "--zoom-step", "2"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--zoom-ease", "2001"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--zoom-ease", "-5"}));
    // --min-zoom and --max-derived-levels: the far end of the continuous zoom.
    CHECK(d->zoom.min_zoom == 0.0);
    CHECK(d->zoom.max_derived_levels == 8U);
    const auto mz =
        parse_args({"--view", "x.scp", "--min-zoom", "2.5", "--max-derived-levels", "3"});
    REQUIRE(mz);
    CHECK(mz->zoom.min_zoom == 2.5);
    CHECK(mz->zoom.max_derived_levels == 3U);
    CHECK_FALSE(parse_args({"--view", "x.scp", "--min-zoom", "0.009"}));
    // --zoom-max-upscale: 1 (default) .. 2.
    CHECK(d->zoom.max_upscale == 1.0);
    const auto mu = parse_args({"--view", "x.scp", "--zoom-max-upscale", "1.45"});
    REQUIRE(mu);
    CHECK(mu->zoom.max_upscale == 1.45);
    CHECK(parse_args({"--view", "x.scp", "--zoom-max-upscale", "1"}));
    CHECK(parse_args({"--view", "x.scp", "--zoom-max-upscale", "2"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--zoom-max-upscale", "0.99"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--zoom-max-upscale", "2.01"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--zoom-max-upscale", "nan"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--zoom-max-upscale", ""}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--min-zoom", "20.1"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--min-zoom", "nan"}));
    CHECK(parse_args({"--view", "x.scp", "--min-zoom", "0.01"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--max-derived-levels", "25"}));
    CHECK(parse_args({"--view", "x.scp", "--max-derived-levels", "0"}));
    // UTF-8 paths round-trip.
    const std::string umlaut = "Insel_\xc3\xa4.scp";
    CHECK(anno::app::to_utf8(anno::app::utf8_path(umlaut)) == umlaut);
}

TEST_CASE("zoom over derived levels: the range follows the last level and the minimum tile") {
    using namespace anno::app;
    const auto levels = anno::render::ZoomLevels::with_derived(3); // tile widths 84 42 20 10 5 2.5
    CHECK(zoom_target_after(20.0, 0.0001, levels) == doctest::Approx(2.5 / kZoomEndFactor));
    CHECK(zoom_target_after(20.0, 0.0001, levels, 6.0) == 6.0);
    CHECK(zoom_target_after(20.0, 0.0001, levels, 1.0) == doctest::Approx(2.5 / kZoomEndFactor));
    CHECK(zoom_target_after(42.0, 1000.0, levels, 6.0) == doctest::Approx(84.0 * kZoomEndFactor));
    // The level shown follows the zoom through the derived levels: the coarsest level
    // whose tile width is at least the zoom, so the scale stays in [0.95 / 2.1, 1] at
    // every width below group 0 (the widest ratio between two levels is 2.1, the coarser
    // level is taken at 5 % below its tile width): the composed grid stays O(window),
    // never magnified.
    ZoomState s;
    s.group = 1;
    s.now = s.target = 42.0;
    double lo = 10.0, hi = 0.0;
    for (int i = 0; i < 900; ++i) {
        s.target = zoom_target_after(s.target, 0.99, levels);
        s = zoom_advance(s, 8.0, 0, [](std::size_t) { return true; }, levels);
        const double sc = zoom_scale(s, levels);
        // The last level is reduced further, down to its tile width / sqrt 2.
        if (s.group + 1 < levels.size()) {
            lo = std::min(lo, sc);
        }
        hi = std::max(hi, sc);
    }
    CHECK(s.group == 5);
    CHECK(lo >= 0.95 / 2.1 - 1e-9);
    CHECK(hi <= 1.0 + 1e-9);
    // And back out: the finer level is taken as soon as the scale would exceed 1.
    double hi_up = 0.0;
    for (int i = 0; i < 900; ++i) {
        s.target = zoom_target_after(s.target, 1.01, levels);
        s = zoom_advance(s, 8.0, 0, [](std::size_t) { return true; }, levels);
        if (s.now <= levels.tile_w(0)) {
            hi_up = std::max(hi_up, zoom_scale(s, levels));
        }
    }
    CHECK(hi_up <= 1.0 + 1e-9);
    CHECK(s.group == 0);
    // Above group 0 magnifying cannot be avoided: up to sqrt 2, the first group only.
    CHECK(zoom_scale(s, levels) == doctest::Approx(kZoomEndFactor));
    // --zoom-max-upscale 1.45: the first stage 3 look, the finer level only past 1.45.
    ZoomState old;
    old.group = 1;
    old.now = old.target = 42.0;
    double hi_old = 0.0;
    for (int i = 0; i < 900; ++i) {
        old.target = zoom_target_after(old.target, 0.99, levels);
        old = zoom_advance(old, 8.0, 0, [](std::size_t) { return true; }, levels, 1.45);
        hi_old = std::max(hi_old, zoom_scale(old, levels));
    }
    CHECK(hi_old > 1.3);
    CHECK(hi_old <= 1.45 + 1e-9);
}

TEST_CASE("zoom_args: the bounds of the zoom options are inclusive and exact") {
    using anno::app::parse_args;
    CHECK(parse_args({"--view", "x.scp", "--zoom-step", "1.01"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--zoom-step", "1.009"}));
    CHECK(parse_args({"--view", "x.scp", "--zoom-ease", "2000"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--zoom-ease", "2001"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--zoom-step", "nan"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--zoom-step", "inf"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--zoom-step", ""}));
    const auto back =
        parse_args({"--view", "x.scp", "--zoom-mode", "continuous", "--zoom-mode", "stepped"});
    REQUIRE(back);
    CHECK(back->zoom.mode == anno::app::ZoomMode::Stepped);
}

namespace {

anno::app::ZoomState zoom_state(double now, std::size_t group) {
    anno::app::ZoomState s;
    s.now = s.target = now;
    s.group = group;
    s.cam_x = 1000.0;
    s.cam_y = 700.0;
    return s;
}

const auto kAllReady = [](std::size_t) { return true; };

/// The scene point under the screen offset (ax, ay) from the window centre, in
/// the scene pixels of group 0 (the same for every group): cam + a / scale,
/// converted by the tile ratio.
std::pair<double, double> scene_point(const anno::app::ZoomState& s) {
    const double k = static_cast<double>(anno::render::kTile[0].w) /
                     static_cast<double>(anno::render::kTile[s.group].w);
    const double sc = anno::app::zoom_scale(s);
    return {(s.cam_x + s.anchor_x / sc) * k, (s.cam_y + s.anchor_y / sc) * k};
}

} // namespace

TEST_CASE("zoom_advance: the point under the anchor stays fixed, also across a group switch") {
    using namespace anno::app;
    ZoomState s = zoom_state(42.0, 1);
    s.anchor_x = 300.0;
    s.anchor_y = -120.0;
    const auto before = scene_point(s);
    // Zoom out in small steps to the end of the range, through the switch to group 2.
    s.target = zoom_target_after(s.target, 0.0001);
    std::size_t switches = 0;
    for (int i = 0; i < 400; ++i) {
        const std::size_t g = s.group;
        s = zoom_advance(s, 8.0, 120, kAllReady);
        switches += s.group != g ? 1U : 0U;
        const auto p = scene_point(s);
        // Integer tile ratios differ from the exact scale ratio by the tile size's
        // rounding only through the conversion, which the ratio keeps exact.
        CHECK(p.first == doctest::Approx(before.first).epsilon(1e-9));
        CHECK(p.second == doctest::Approx(before.second).epsilon(1e-9));
    }
    CHECK(switches == 1);
    CHECK(s.group == 2);
    CHECK(s.now == s.target);
}

TEST_CASE("zoom_advance: easing covers 95 % in ease_ms, steps compose, 0 jumps, rest stays") {
    using namespace anno::app;
    ZoomState s = zoom_state(42.0, 1);
    s.target = 42.0 * 1.3;
    const double start = std::log(s.target / s.now);
    // One step of the whole ease time: e^-3 of the distance is left.
    const ZoomState one = zoom_advance(s, 120.0, 120, kAllReady);
    CHECK(std::log(one.target / one.now) / start == doctest::Approx(std::exp(-3.0)).epsilon(1e-9));
    // The exponential composes: two half steps equal one whole.
    ZoomState two = zoom_advance(s, 60.0, 120, kAllReady);
    two = zoom_advance(two, 60.0, 120, kAllReady);
    CHECK(two.now == doctest::Approx(one.now).epsilon(1e-12));
    // No time, no move; ease 0 jumps to the target at once.
    CHECK(zoom_advance(s, 0.0, 120, kAllReady).now == s.now);
    CHECK(zoom_advance(s, 5.0, 0, kAllReady).now == s.target);
    // Far from the target it converges monotonically and lands exactly (the snap).
    double last = s.now;
    for (int i = 0; i < 200 && s.now != s.target; ++i) {
        s = zoom_advance(s, 8.0, 120, kAllReady);
        CHECK(s.now >= last);
        last = s.now;
    }
    CHECK(s.now == s.target);
    // At rest nothing moves (the camera stays).
    const ZoomState rest = zoom_advance(s, 8.0, 120, kAllReady);
    CHECK(rest.cam_x == s.cam_x);
    CHECK(rest.now == s.now);
}

TEST_CASE(
    "zoom_advance: the level switch has hysteresis on the coarse side, waits for ready, bridges") {
    using namespace anno::app;
    const auto& kt = anno::render::kTile;
    auto at = [&](double w, std::size_t group, auto&& ready) {
        ZoomState s = zoom_state(w, group);
        return zoom_advance(s, 8.0, 120, ready).group;
    };
    // Group 1 (tile width 42) shows a zoom up to its own tile width, magnified never.
    CHECK(at(42.0, 1, kAllReady) == 1);
    CHECK(at(42.0 * 1.001, 1, kAllReady) == 0); // the scale would exceed 1: finer at once
    CHECK(at(30.0, 1, kAllReady) == 1);         // 30 < 42: reduced, stays
    // The coarser level is taken only once the zoom is 5 % below its tile width.
    CHECK(at(20.0 / 0.95 * 1.001, 1, kAllReady) == 1); // 21.05 : group 2 (20) not yet
    CHECK(at(20.0 / 0.95 * 0.999, 1, kAllReady) == 1); // fits 1.0 but not 0.95 * 20 = 19
    CHECK(at(19.0 * 0.999, 1, kAllReady) == 2);
    // ... and back: from group 2 the finer level is taken as the scale passes 1.
    CHECK(at(19.5, 2, kAllReady) == 2);
    CHECK(at(20.0 * 1.001, 2, kAllReady) == 1);
    // A resting zoom does not flip between two levels (hysteresis band 19 .. 20).
    CHECK(at(19.5, 1, kAllReady) == 1);
    CHECK(at(19.5, 2, kAllReady) == 2);
    // A coarser level that is not ready is waited for, the current one stays.
    CHECK(at(15.0, 1, [](std::size_t g) { return g != 2; }) == 1);
    // A finer level that is not ready is bridged by the next coarser one that is
    // (magnified): zooming in from group 2 to 50 wants group 0.
    CHECK(at(50.0, 2, [](std::size_t g) { return g != 0; }) == 1);
    CHECK(at(50.0, 2, [](std::size_t g) { return g == 2; }) == 2);
    CHECK(at(50.0, 2, kAllReady) == 0);
    // At rest the level is chosen too: a viewer left on group 0 at 30 comes down to 1.
    CHECK(at(30.0, 0, kAllReady) == 1);
    // The ends: beyond group 0's tile width the first group is the only choice.
    CHECK(at(static_cast<double>(kt[0].w) * kZoomEndFactor, 0, kAllReady) == 0);
    // The wanted width is kept inside the range.
    CHECK(zoom_target_after(42.0, 1000.0) ==
          doctest::Approx(static_cast<double>(kt[0].w) * kZoomEndFactor));
    CHECK(zoom_target_after(42.0, 0.001) ==
          doctest::Approx(static_cast<double>(kt[anno::render::kGroups - 1].w) / kZoomEndFactor));
}

TEST_CASE("zoom_advance: the zoom is held while the next coarser level is not ready") {
    using namespace anno::app;
    const auto levels = anno::render::ZoomLevels::with_derived(3); // 84 42 20 10 5 2.5
    // Group 2 (tile 20), zooming out for a long time with levels 0..2 ready only: the
    // zoom stops where level 3 (tile 10) would be taken, 0.95 x 10, so the scale on
    // group 2 stays at 0.475 and the composed grid at 2.1 x the window per axis.
    ZoomState s = zoom_state(20.0, 2);
    const auto only_built = [](std::size_t g) { return g <= 2; };
    s.target = 1.0;
    for (int i = 0; i < 400; ++i) {
        s = zoom_advance(s, 8.0, 120, only_built, levels);
    }
    CHECK(s.group == 2);
    CHECK(s.now == doctest::Approx(0.95 * 10.0));
    CHECK(zoom_scale(s, levels) == doctest::Approx(0.475));
    CHECK(zoom_hold_floor(2, only_built, levels, 1.0) == doctest::Approx(9.5));
    // Level 3 becomes ready: it is taken, and the zoom runs on to its own hold at level 4.
    const auto up_to_3 = [](std::size_t g) { return g <= 3; };
    for (int i = 0; i < 400; ++i) {
        s = zoom_advance(s, 8.0, 120, up_to_3, levels);
    }
    CHECK(s.group == 3);
    CHECK(s.now == doctest::Approx(0.95 * 5.0));
    // Zooming in is never held, and neither is the last level or a ready next level.
    CHECK(zoom_hold_floor(5, only_built, levels, 1.0) == 0.0);
    CHECK(zoom_hold_floor(2, kAllReady, levels, 1.0) == 0.0);
    s.target = 18.0;
    for (int i = 0; i < 400; ++i) {
        s = zoom_advance(s, 8.0, 120, only_built, levels);
    }
    CHECK(s.now == doctest::Approx(18.0));
    // A coarser level that fits but is not ready: the coarsest ready one in between.
    ZoomState t = zoom_state(12.0, 1);
    CHECK(zoom_advance(t, 8.0, 0, [](std::size_t g) { return g <= 2; }, levels).group == 2);
    t = zoom_state(9.0, 1);
    CHECK(zoom_advance(t, 8.0, 0, [](std::size_t g) { return g <= 2; }, levels).group == 2);
    CHECK(zoom_advance(t, 8.0, 0, [](std::size_t g) { return g <= 3; }, levels).group == 3);
    // The 8192 limit of the grid: a window the hold alone would not keep within it gets
    // its scale raised by resampled_view (gpu_test: "resampled_view: ... limits").
}

TEST_CASE("--zoom-factor and --resample: the headless picture at a fractional scale") {
    using anno::app::parse_args;
    const auto d = parse_args({"--view", "x.scp", "--png", "o.png"});
    REQUIRE(d);
    CHECK(d->zoom_factor == 1.0);
    CHECK_FALSE(d->resample.has_value());
    const auto z = parse_args(
        {"--view", "x.scp", "--png", "o.png", "--zoom-factor", "1.3", "--resample", "area"});
    REQUIRE(z);
    CHECK(z->zoom_factor == 1.3);
    CHECK(z->resample == anno::render::ResampleFilter::Area);
    for (const char* f : {"nearest", "bilinear", "box", "area"}) {
        CHECK(parse_args({"--view", "x.scp", "--png", "o.png", "--resample", f}));
    }
    CHECK_FALSE(parse_args({"--view", "x.scp", "--png", "o.png", "--resample", "lanczos"}));
    CHECK(parse_args({"--view", "x.scp", "--png", "o.png", "--zoom-factor", "0.015625"}));
    CHECK(parse_args({"--view", "x.scp", "--png", "o.png", "--zoom-factor", "64"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--png", "o.png", "--zoom-factor", "0.015"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--png", "o.png", "--zoom-factor", "64.5"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--png", "o.png", "--zoom-factor", "nan"}));
    // The headless options need --png.
    CHECK_FALSE(parse_args({"--view", "x.scp", "--zoom-factor", "1.3"}));
    CHECK_FALSE(parse_args({"--view", "x.scp", "--resample", "box"}));
    CHECK(parse_args({"--view", "x.scp", "--zoom-factor", "1"}));
}

TEST_CASE("zoom_advance: the camera move of a zoom at the cursor is not a pan (pan_centre)") {
    using namespace anno::app;
    const auto levels = anno::render::ZoomLevels::with_derived(3);
    // A zoom out at an anchor 300 px right of the window centre: the camera moves, the
    // pan centre (camera minus the zoom's displacement) stays where it was, also across
    // the switches between levels (converted by the tile ratio like the camera).
    ZoomState s = zoom_state(42.0, 1);
    s.anchor_x = 300.0;
    s.anchor_y = -120.0;
    const auto pan0 = pan_centre(s);
    const double cam0 = s.cam_x;
    double moved = 0;
    std::size_t switches = 0;
    for (int i = 0; i < 600; ++i) {
        const std::size_t before = s.group;
        s.target = zoom_target_after(s.target, 0.99, levels);
        s = zoom_advance(s, 8.0, 120, kAllReady, levels);
        switches += s.group != before ? 1U : 0U;
        // In the pixels of the group shown: the pan centre scales with the tile width.
        const double k = levels.tile_w(s.group) / levels.tile_w(1);
        CHECK(pan_centre(s).first == doctest::Approx(pan0.first * k).epsilon(1e-9));
        CHECK(pan_centre(s).second == doctest::Approx(pan0.second * k).epsilon(1e-9));
        moved = std::max(moved, std::abs(s.cam_x / k - cam0));
    }
    CHECK(switches >= 3);
    // The camera did move (else the test proves nothing), by hundreds of group-1 pixels.
    CHECK(moved > 100.0);
    // A pan adds to the camera only: the pan centre follows it.
    const auto before = pan_centre(s);
    s.cam_x += 25.0;
    CHECK(pan_centre(s).first == before.first + 25.0);
}

namespace {

/// HEAD's flag word of `id` (bytes 0x1c-0x1d), 0 without one.
std::uint16_t flags_of(const anno::formats::anlagen::AnlagenFile& anlagen, std::uint32_t id) {
    const auto* e = anlagen.entry(id);
    const auto head = e != nullptr ? e->record("HEAD") : std::nullopt;
    return head && head->size() >= 0x1e ? anno::formats::u16_le(*head, 0x1c) : std::uint16_t{0};
}

/// The first type whose flag word satisfies `pred`, and whose footprint satisfies `size`.
std::optional<std::uint32_t>
first_type(const anno::formats::anlagen::AnlagenFile& anlagen,
           const std::function<bool(std::uint16_t)>& pred,
           const std::function<bool(std::uint8_t, std::uint8_t)>& size) {
    for (const auto& e : anlagen.entries) {
        const auto fp = anlagen.footprint(e.id);
        if (fp && pred(flags_of(anlagen, e.id)) && size(fp->first, fp->second)) {
            return e.id;
        }
    }
    return std::nullopt;
}

} // namespace

// Owner playtest 2026-11-03, round 6: trees stood inside a founded Kontor. The original writes the
// building over every footprint tile (AnnoWelt.dll 0x100823ed); a type with flag 0x10 keeps an
// anchored flag-0x10 object (0x1008218f). Types and sizes from the real Anlagen.dat.
TEST_CASE(
    "scenery under a building: footprint, rotation and the flag 0x10 exception (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto buf = anno::test::read_file(*dir / "Anlagen.dat");
    auto parsed = anno::formats::anlagen::AnlagenFile::parse(buf);
    REQUIRE(parsed);
    const auto& anlagen = parsed.value();
    const auto any = [](std::uint8_t, std::uint8_t) { return true; };
    // The warehouse: 4 x 4, flag word 0x2087 (no 0x10).
    REQUIRE(anlagen.footprint(1302) == std::pair<std::uint8_t, std::uint8_t>{4, 4});
    CHECK(flags_of(anlagen, 1302) == 0x2087);
    // A tree: flag 0x20 (placement passes over it), not 0x10.
    const auto tree = first_type(anlagen, [](std::uint16_t f) { return f == 0x0029; }, any);
    REQUIRE(tree.has_value());
    const auto tree_id = static_cast<std::uint16_t>(*tree);
    anno::app::SceneBuilding kontor;
    kontor.type = 1302;
    kontor.x = 10;
    kontor.y = 20;
    for (std::uint8_t rot = 0; rot < 4; ++rot) {
        kontor.rotation = rot;
        CHECK(anno::app::scenery_under_building(anlagen, kontor, tree_id, 10, 20));
        CHECK(anno::app::scenery_under_building(anlagen, kontor, tree_id, 13, 23));
        CHECK_FALSE(anno::app::scenery_under_building(anlagen, kontor, tree_id, 14, 20));
        CHECK_FALSE(anno::app::scenery_under_building(anlagen, kontor, tree_id, 10, 24));
        CHECK_FALSE(anno::app::scenery_under_building(anlagen, kontor, tree_id, 9, 20));
        CHECK_FALSE(anno::app::scenery_under_building(anlagen, kontor, tree_id, 10, 19));
    }
    // A non-square type without 0x10: an odd rotation swaps its width and height.
    const auto tall = first_type(
        anlagen, [](std::uint16_t f) { return (f & 0x10U) == 0; },
        [](std::uint8_t w, std::uint8_t h) { return w >= 1 && h > w; });
    REQUIRE(tall.has_value());
    const auto [w, h] = *anlagen.footprint(*tall);
    MESSAGE("non-square type " << *tall << ": " << int{w} << " x " << int{h});
    anno::app::SceneBuilding b;
    b.type = *tall;
    b.x = 50;
    b.y = 60;
    const auto under = [&](int x, int y) {
        return anno::app::scenery_under_building(anlagen, b, tree_id, static_cast<std::uint16_t>(x),
                                                 static_cast<std::uint16_t>(y));
    };
    for (const std::uint8_t rot : {std::uint8_t{0}, std::uint8_t{2}}) {
        b.rotation = rot;
        CHECK(under(50 + w - 1, 60 + h - 1));
        CHECK_FALSE(under(50 + w, 60));
        CHECK_FALSE(under(50, 60 + h));
    }
    for (const std::uint8_t rot : {std::uint8_t{1}, std::uint8_t{3}}) {
        b.rotation = rot; // swapped: h wide, w tall
        CHECK(under(50 + h - 1, 60 + w - 1));
        CHECK_FALSE(under(50, 60 + h - 1));
        CHECK_FALSE(under(50 + h, 60));
    }
    // A type with flag 0x10 keeps a flag-0x10 object under it, not a tree.
    const auto overlay = first_type(anlagen, [](std::uint16_t f) { return (f & 0x10U) != 0; }, any);
    const auto rock = first_type(anlagen, [](std::uint16_t f) { return f == 0x8018; }, any);
    REQUIRE(overlay.has_value());
    REQUIRE(rock.has_value());
    anno::app::SceneBuilding o;
    o.type = *overlay;
    o.x = 5;
    o.y = 5;
    CHECK_FALSE(
        anno::app::scenery_under_building(anlagen, o, static_cast<std::uint16_t>(*rock), 5, 5));
    CHECK(anno::app::scenery_under_building(anlagen, o, tree_id, 5, 5));
    // A type the file does not have changes nothing.
    anno::app::SceneBuilding unknown;
    unknown.type = 0xFFF;
    CHECK_FALSE(anno::app::scenery_under_building(anlagen, unknown, tree_id, 0, 0));
}

// The same through the picture's rewrite of a real island: a warehouse put over a tree of an
// original island template drops that tree and keeps every other scenery item; without Anlagen.dat
// (the old behaviour) the tree stays.
TEST_CASE("scenery under a building: the island rewrite drops the tree a warehouse covers (game "
          "files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto anl_buf = anno::test::read_file(*dir / "Anlagen.dat");
    auto parsed = anno::formats::anlagen::AnlagenFile::parse(anl_buf);
    REQUIRE(parsed);
    const auto& anlagen = parsed.value();
    // The island templates, in a fixed order; the first one with a tree item is used.
    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(*dir)) {
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (entry.is_regular_file() && ext == ".scp") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    REQUIRE_FALSE(files.empty());
    const auto gelaende = [](const anno::formats::insel::Island& island) {
        std::vector<anno::formats::insel::PlacedObject> out;
        for (const auto& rec : island.records) {
            if (rec.name() != "GELAENDE") {
                continue;
            }
            auto block = anno::formats::insel::Block::parse(rec.payload);
            REQUIRE(block);
            const auto& data = block.value().data;
            for (std::size_t o = 0; o + anno::formats::insel::kItemLen <= data.size();
                 o += anno::formats::insel::kItemLen) {
                out.push_back(anno::formats::insel::PlacedObject::parse(
                    data.subspan(o, anno::formats::insel::kItemLen), block.value()));
            }
        }
        return out;
    };
    for (const fs::path& file : files) {
        const auto buf = anno::test::read_file(file);
        auto islands = anno::formats::insel::islands(buf);
        if (!islands || islands.value().empty()) {
            continue;
        }
        const anno::formats::insel::Island source = islands.value().front();
        const auto items = gelaende(source);
        const auto tree = std::find_if(items.begin(), items.end(), [&](const auto& it) {
            return flags_of(anlagen, it.anlage) == 0x0029 && it.x >= 3 && it.y >= 3;
        });
        if (tree == items.end()) {
            continue;
        }
        MESSAGE(file.filename().string()
                << ": tree " << tree->anlage << " at " << tree->x << "," << tree->y);
        // The warehouse's footprint ends on the tree's tile (bottom-right corner).
        anno::app::SceneBuilding kontor;
        kontor.island = source.number;
        kontor.type = 1302;
        kontor.x = static_cast<std::uint16_t>(tree->x - 3);
        kontor.y = static_cast<std::uint16_t>(tree->y - 3);
        std::size_t covered = 0;
        for (const auto& it : items) {
            covered +=
                anno::app::scenery_under_building(anlagen, kontor, it.anlage, it.x, it.y) ? 1U : 0U;
        }
        anno::app::SceneObjects objects;
        objects.is_building = [](std::uint16_t id) { return id == 1302; };
        const auto rewrite = [&](const anno::formats::anlagen::AnlagenFile* with) {
            anno::formats::insel::Island island = source;
            std::vector<std::vector<std::uint8_t>> storage;
            std::vector<std::string> notes;
            std::vector<anno::app::OmittedObject> omitted;
            anno::app::apply_island_buildings(island, {kontor}, objects, storage, notes, omitted,
                                              with);
            return gelaende(island);
        };
        const auto after = rewrite(&anlagen);
        const auto before = rewrite(nullptr);
        CHECK(before.size() == items.size());
        CHECK(covered >= 1);
        CHECK(after.size() == items.size() - covered);
        const auto has_tree = [&](const std::vector<anno::formats::insel::PlacedObject>& list) {
            return std::any_of(list.begin(), list.end(), [&](const auto& it) {
                return it.anlage == tree->anlage && it.x == tree->x && it.y == tree->y;
            });
        };
        CHECK(has_tree(before));
        CHECK_FALSE(has_tree(after));
        // A warehouse (no flag 0x10) keeps no item anchored in its footprint.
        for (const auto& it : after) {
            CHECK_FALSE((it.x >= kontor.x && it.x < kontor.x + 4 && it.y >= kontor.y &&
                         it.y < kontor.y + 4));
        }
        return;
    }
    FAIL("no island template with a tree in " << dir->string());
}

// Every island of the shipped scenarios rewritten with its own buildings: the rewrite drops exactly
// the scenery items that scenery_under_building names for some building over their anchor (the
// rewrite marks the island's tiles once, review 2026-11-03: a test per item and building took 63 s
// on endless02.sze's island 1), including the flag-0x10 objects a flag-0x10 type keeps (rocks under
// mines).
TEST_CASE("scenery under a building: the shipped scenarios' islands drop exactly the covered "
          "scenery (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto anl_buf = anno::test::read_file(*dir / "Anlagen.dat");
    auto parsed = anno::formats::anlagen::AnlagenFile::parse(anl_buf);
    REQUIRE(parsed);
    const auto& anlagen = parsed.value();
    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(*dir / "Scenes")) {
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (entry.is_regular_file() && ext == ".sze") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    REQUIRE_FALSE(files.empty());
    using Item = std::tuple<std::uint16_t, std::uint16_t, std::uint16_t>; // id, x, y
    const auto items_of = [](const anno::formats::insel::Island& island, const char* name) {
        std::vector<anno::formats::insel::PlacedObject> out;
        for (const auto& rec : island.records) {
            if (rec.name() != name) {
                continue;
            }
            auto block = anno::formats::insel::Block::parse(rec.payload);
            if (!block) {
                continue;
            }
            const auto& data = block.value().data;
            for (std::size_t o = 0; o + anno::formats::insel::kItemLen <= data.size();
                 o += anno::formats::insel::kItemLen) {
                out.push_back(anno::formats::insel::PlacedObject::parse(
                    data.subspan(o, anno::formats::insel::kItemLen), block.value()));
            }
        }
        return out;
    };
    std::size_t islands_n = 0, dropped = 0, kept_by_exception = 0;
    double total_ms = 0, worst_ms = 0;
    for (const fs::path& file : files) {
        const auto buf = anno::test::read_file(file);
        auto islands = anno::formats::insel::islands(buf);
        if (!islands) {
            continue;
        }
        for (const auto& source : islands.value()) {
            if (source.number > 0xFFFF) {
                continue;
            }
            std::vector<anno::app::SceneBuilding> buildings;
            std::set<std::uint16_t> ids;
            for (const auto& obj : items_of(source, "PROD2")) {
                anno::app::SceneBuilding b;
                b.island = source.number;
                b.type = obj.anlage;
                b.x = obj.x;
                b.y = obj.y;
                b.rotation = obj.rotation;
                b.height = obj.height();
                buildings.push_back(b);
                ids.insert(obj.anlage);
            }
            if (buildings.empty()) {
                continue;
            }
            // The buildings over each tile (by their footprint boxes), then the expected drops.
            std::map<std::pair<std::uint32_t, std::uint32_t>, std::vector<std::size_t>> over;
            for (std::size_t i = 0; i < buildings.size(); ++i) {
                const auto& b = buildings[i];
                const auto fp = anlagen.footprint(b.type);
                if (!fp) {
                    continue;
                }
                const bool odd = (b.rotation & 1U) != 0;
                const std::uint32_t w = odd ? fp->second : fp->first;
                const std::uint32_t h = odd ? fp->first : fp->second;
                for (std::uint32_t y = b.y; y < std::uint32_t{b.y} + h; ++y) {
                    for (std::uint32_t x = b.x; x < std::uint32_t{b.x} + w; ++x) {
                        over[{x, y}].push_back(i);
                    }
                }
            }
            std::multiset<Item> expected;
            for (const auto& it : items_of(source, "GELAENDE")) {
                if (ids.contains(it.anlage)) {
                    expected.insert({it.anlage, it.x, it.y});
                    continue;
                }
                const auto found = over.find({it.x, it.y});
                const bool drop =
                    found != over.end() &&
                    std::any_of(found->second.begin(), found->second.end(), [&](std::size_t i) {
                        return anno::app::scenery_under_building(anlagen, buildings[i], it.anlage,
                                                                 it.x, it.y);
                    });
                if (drop) {
                    ++dropped;
                } else {
                    expected.insert({it.anlage, it.x, it.y});
                    kept_by_exception += found != over.end() ? 1U : 0U;
                }
            }
            anno::app::SceneObjects objects;
            objects.is_building = [&ids](std::uint16_t id) { return ids.contains(id); };
            anno::formats::insel::Island island = source;
            std::vector<std::vector<std::uint8_t>> storage;
            std::vector<std::string> notes;
            std::vector<anno::app::OmittedObject> omitted;
            const auto t0 = std::chrono::steady_clock::now();
            anno::app::apply_island_buildings(island, buildings, objects, storage, notes, omitted,
                                              &anlagen);
            const double ms =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                    .count();
            total_ms += ms;
            worst_ms = std::max(worst_ms, ms);
            std::multiset<Item> got;
            for (const auto& it : items_of(island, "GELAENDE")) {
                got.insert({it.anlage, it.x, it.y});
            }
            CHECK_MESSAGE(got == expected, file.filename().string() << " island " << source.number);
            ++islands_n;
        }
    }
    MESSAGE(islands_n << " islands with buildings: " << dropped << " scenery items dropped, "
                      << kept_by_exception
                      << " anchored under a building and kept (flag 0x10); rewrites " << total_ms
                      << " ms in all, at most " << worst_ms << " ms");
    CHECK(islands_n > 100);
    CHECK(dropped > 0);
    CHECK(kept_by_exception > 0);
}
