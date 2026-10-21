// anno_app_core: PNG output (always) and scene loading (needs
// ANNO1503_GAME_DIR; skips otherwise).
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/cli.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/png.hpp>
#include <anno/app/zoom.hpp>
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

TEST_CASE("command line") {
    using anno::app::parse_args;
    using V = std::vector<std::string>;
    // Invalid input: no --view, odd count, bad values, unknown options.
    for (const V& bad :
         {V{}, V{"--view"}, V{"--view", "x.sze", "--group", "3"}, V{"--view", "x", "--bogus", "1"},
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
    // The level shown follows the zoom through the derived levels (nearest in ratio), and
    // the scale stays inside 0.69 .. 1.45 at every width: the composed grid stays O(window).
    ZoomState s;
    s.group = 1;
    s.now = s.target = 42.0;
    double lo = 10.0, hi = 0.0;
    for (int i = 0; i < 900; ++i) {
        s.target = zoom_target_after(s.target, 0.99, levels);
        s = zoom_advance(s, 8.0, 0, [](std::size_t) { return true; }, levels);
        const double sc = zoom_scale(s, levels);
        lo = std::min(lo, sc);
        hi = std::max(hi, sc);
    }
    CHECK(s.group == 5);
    CHECK(lo > 0.68);
    CHECK(hi < 1.46);
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

TEST_CASE("zoom_advance: the group switch has hysteresis, waits for ready, and is taken at rest") {
    using namespace anno::app;
    const auto& kt = anno::render::kTile;
    // The middle between groups 0 and 1 (tile widths 84 and 42), in the logarithm.
    const double mid = std::sqrt(static_cast<double>(kt[0].w) * static_cast<double>(kt[1].w));
    auto at = [&](double w, std::size_t group, auto&& ready) {
        ZoomState s = zoom_state(w, group);
        return zoom_advance(s, 8.0, 120, ready).group;
    };
    // Just on either side of the middle the shown group stays: the switch needs
    // the zoom to be kGroupSwitchBias closer (in the logarithm) to the other.
    CHECK(at(mid * 1.001, 1, kAllReady) == 1);
    CHECK(at(mid / 1.001, 0, kAllReady) == 0);
    // Far enough on the other side it switches.
    CHECK(at(mid * 1.05, 1, kAllReady) == 0);
    CHECK(at(mid / 1.05, 0, kAllReady) == 1);
    // A group that is not ready is not taken.
    CHECK(at(mid * 1.05, 1, [](std::size_t g) { return g != 0; }) == 1);
    // At rest the group is chosen too: the zoom 1.3 times group 1's tile width is
    // nearest group 1 itself, so a viewer left on group 0 comes back to it.
    CHECK(at(42.0 * 1.3, 0, kAllReady) == 1);
    // The ends: beyond group 0's tile width the nearest group is still 0.
    CHECK(at(static_cast<double>(kt[0].w) * kZoomEndFactor, 0, kAllReady) == 0);
    // The wanted width is kept inside the range.
    CHECK(zoom_target_after(42.0, 1000.0) ==
          doctest::Approx(static_cast<double>(kt[0].w) * kZoomEndFactor));
    CHECK(zoom_target_after(42.0, 0.001) ==
          doctest::Approx(static_cast<double>(kt[anno::render::kGroups - 1].w) / kZoomEndFactor));
}