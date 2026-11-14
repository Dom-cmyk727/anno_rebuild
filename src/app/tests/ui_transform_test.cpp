// UI-WINDOW-1: the UI layer's transform (ui_transform.hpp), its placement of the reference
// layout (ui_layout.hpp place_hud / ui_hit) and the click path through both, against hand-derived
// float32 values (an independent Python float32 model of the HE disassembly, kept with the
// packet's evidence in cache/, not in git) and the project's real data/ui files. The pixel cases
// at the end need the original files (ANNO1503_GAME_DIR) and skip without them.
#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>

#include <doctest/doctest.h>

#include <anno/app/construction_menu.hpp>
#include <anno/app/ui_layout.hpp>
#include <anno/app/ui_mock.hpp>
#include <anno/app/ui_transform.hpp>
#include <anno/test/game_dir.hpp>

namespace fs = std::filesystem;
namespace app = anno::app;

namespace {

fs::path data_ui_dir() {
    return fs::path(ANNO_REPO_DIR) / "data" / "ui";
}

std::uint32_t bits(float f) {
    return std::bit_cast<std::uint32_t>(f);
}

struct Layouts {
    app::HudLayout hud;
    app::ConstructionLayout construction;
    app::MaskLayout masks;
};

Layouts load() {
    const auto hud = app::load_hud_layout(data_ui_dir() / "hud.txt");
    const auto con = app::load_construction_layout(data_ui_dir() / "construction.txt");
    const auto masks = app::load_mask_layout(data_ui_dir() / "masks.txt");
    REQUIRE_MESSAGE(hud.ok(), (hud.ok() ? std::string() : hud.error()));
    REQUIRE_MESSAGE(con.ok(), (con.ok() ? std::string() : con.error()));
    REQUIRE_MESSAGE(masks.ok(), (masks.ok() ? std::string() : masks.error()));
    return {hud.value(), con.value(), masks.value()};
}

} // namespace

TEST_CASE("ui transform: he-ui-scale.md's prediction table in exact float32") {
    // 2542 x 1351, the three captured settings (he-ui-scale.md section 2, visual check).
    const auto s10 = app::he_ui_transform(2542, 1351, 1.0f);
    CHECK(s10.ui_w == 2542);
    CHECK(s10.ui_h == 1351);
    CHECK(s10.k == 1.0f);
    CHECK(s10.identity());

    const auto s15 = app::he_ui_transform(2542, 1351, 1.5f);
    CHECK(s15.s == 1.5f);
    CHECK(s15.ui_w == 1694);
    CHECK(s15.ui_h == 900);
    CHECK(bits(s15.k) == 0x3fc01358U); // 1.50059032
    CHECK(s15.off_x == 0);
    CHECK(s15.off_y == 0);
    CHECK_FALSE(s15.identity());

    const auto s20 = app::he_ui_transform(2542, 1351, 2.0f);
    CHECK(bits(s20.s) == 0x3fd99999U); // 2.0f - 0.1f - 0.1f - 0.1f = 1.69999993
    CHECK(s20.ui_w == 1495);
    CHECK(s20.ui_h == 794);
    CHECK(bits(s20.k) == 0x3fd9a48fU); // 1.70033443, width-limited
    CHECK(s20.off_x == 0);
    CHECK(s20.off_y == 0);
    // The panel's left edge, in client pixels: (1495 - 245) * k = 2125.4 (measured 2125).
    CHECK(static_cast<int>(static_cast<float>(s20.ui_w - 245) * s20.k) == 2125);

    // At 1200 lines or fewer the setting counts as 1.0 (UpdateGameSettings 0x2d943).
    for (const float s : {1.5f, 2.0f}) {
        CAPTURE(s);
        const auto hd = app::he_ui_transform(1920, 1080, s);
        CHECK(hd.identity());
        CHECK(hd.ui_w == 1920);
        CHECK(hd.ui_h == 1080);
        const auto ref = app::he_ui_transform(1024, 768, s);
        CHECK(ref.identity());
    }
    // One line above the rule: the loop runs (1201 / 1.5 = 800.7 keeps 768 lines).
    const auto above = app::he_ui_transform(1920, 1201, 1.5f);
    CHECK(above.ui_w == 1280);
    CHECK(above.ui_h == 800);
    // The slider's range clamps the setting (3.0 behaves as 2.0).
    const auto clamped = app::he_ui_transform(2542, 1351, 3.0f);
    CHECK(clamped.ui_w == 1495);
    CHECK(clamped.ui_h == 794);
}

TEST_CASE("ui transform: the step-down is float32 and not decimal") {
    // 2091 x 1307 at 1.7: float(1307) * (1.0f / 1.7f) truncates to 768 with float32's 1.7f
    // (1.70000005) and 2091 * that to 1229; exact decimals would give 1230 x 768 (and a
    // double-precision reciprocal 1230 too). Distinguishes the original's arithmetic.
    const auto t = app::he_ui_transform(2091, 1307, 1.7f);
    CHECK(t.ui_w == 1229);
    CHECK(t.ui_h == 768);
    CHECK(bits(t.k) == 0x3fd9c6edU);
    // 2560 x 1440 at 2.0 steps twice to 1.79999995 and lands on exactly 800 lines.
    const auto q = app::he_ui_transform(2560, 1440, 2.0f);
    CHECK(bits(q.s) == 0x3fe66666U);
    CHECK(q.ui_w == 1422);
    CHECK(q.ui_h == 800);
}

TEST_CASE("ui transform: the original rule is 1:1 at 1024 x 768 and up and fitted below it") {
    for (const auto& [w, h] : {std::pair{1024, 768}, std::pair{2542, 1351}, std::pair{1920, 1080},
                              std::pair{3840, 2160}, std::pair{1280, 1024}}) {
        CAPTURE(w);
        CAPTURE(h);
        const auto t = app::original_ui_transform(w, h);
        CHECK(t.identity());
        CHECK(t.ui_w == w);
        CHECK(t.ui_h == h);
    }
    // Below 1024 x 768 the layer stays 1024 x 768 and is fitted into the window (OQ-616).
    const auto narrow = app::original_ui_transform(1024, 700); // height-limited
    CHECK(narrow.ui_w == 1024);
    CHECK(narrow.ui_h == 768);
    CHECK(bits(narrow.k) == 0x3f695555U); // 700 / 768
    CHECK(narrow.off_x == 45);
    CHECK(narrow.off_y == 0);
    const auto tall = app::original_ui_transform(700, 768); // width-limited
    CHECK(bits(tall.k) == 0x3f2f0000U);                     // 700 / 1024 = 0.68359375
    CHECK(tall.off_x == 0);
    CHECK(tall.off_y == 122);
    const auto small = app::original_ui_transform(800, 600);
    CHECK(small.k == 0.78125f);
    CHECK(small.off_x == 0);
    CHECK(small.off_y == 0);
}

TEST_CASE("ui transform: clicks map back with trunc((p - off) / k)") {
    const auto s20 = app::he_ui_transform(2542, 1351, 2.0f);
    // 2125 / 1.70033443 = 1249.75 -> 1249; 2126 -> 1250.34 -> 1250 (the panel's first column).
    CHECK(s20.to_ui(2125.0, 0.0) == std::optional<std::pair<int, int>>({1249, 0}));
    CHECK(s20.to_ui(2126.0, 0.0) == std::optional<std::pair<int, int>>({1250, 0}));
    CHECK(s20.to_ui(2541.0, 1350.0) == std::optional<std::pair<int, int>>({1494, 793}));
    // A fractional pointer position counts as the pixel that holds it.
    CHECK(s20.to_ui(2126.9, 0.2) == std::optional<std::pair<int, int>>({1250, 0}));
    // 1:1: the identity.
    const auto one = app::original_ui_transform(2542, 1351);
    CHECK(one.to_ui(2300.0, 212.0) == std::optional<std::pair<int, int>>({2300, 212}));
    // The letterbox of a small window is not the layer.
    const auto narrow = app::original_ui_transform(1024, 700);
    CHECK_FALSE(narrow.to_ui(10.0, 10.0).has_value());
    CHECK(narrow.to_ui(45.0, 0.0) == std::optional<std::pair<int, int>>({0, 0}));
    CHECK_FALSE(narrow.to_ui(1023.0, 10.0).has_value()); // (1023 - 45) / 0.911 = 1073 > 1023
}

TEST_CASE("ui placement: anchors and the status bar span and the hit classes by the real data") {
    const Layouts l = load();
    const auto& hud = l.hud;
    // At the reference nothing moves: every element where hud.txt measured it.
    const app::UiPlacement ref = app::place_hud(hud, 1024, 768);
    CHECK(ref.dx == 0);
    CHECK(ref.dy == 0);
    CHECK(ref.statusbar_end == 746);
    for (const auto& e : hud.elements) {
        CAPTURE(e.name);
        CHECK(ref.place(e.anchor, e.pos_x, e.pos_y) == std::pair{e.pos_x, e.pos_y});
    }
    // The original's other two widths: their own graphic's width (873, 1001).
    CHECK(app::place_hud(hud, 1152, 864).statusbar_end == 873);
    CHECK(app::place_hud(hud, 1280, 1024).statusbar_end == 1001);

    const app::UiPlacement big = app::place_hud(hud, 2542, 1351);
    CHECK(big.statusbar_end == 2263); // 2542 - 245 - 34, the HE capture's question mark
    const auto* panel = hud.element("panel");
    const auto* mark = hud.element("fragezeichen");
    const auto* gold = hud.element("gold");
    const auto* tab = hud.element("tab_construction");
    const auto* pause = hud.element("pause_parchment");
    REQUIRE(panel != nullptr);
    REQUIRE(mark != nullptr);
    REQUIRE(gold != nullptr);
    REQUIRE(tab != nullptr);
    REQUIRE(pause != nullptr);
    CHECK(big.place(panel->anchor, panel->pos_x, panel->pos_y) == std::pair{2297, 0});
    CHECK(big.place(mark->anchor, mark->pos_x, mark->pos_y) == std::pair{2263, 1316});
    CHECK(big.place(gold->anchor, gold->pos_x, gold->pos_y) == std::pair{0, 1316});
    CHECK(big.place(tab->anchor, tab->pos_x, tab->pos_y) == std::pair{2297, 181});
    CHECK(big.place(pause->anchor, pause->pos_x, pause->pos_y) == std::pair{222, 191}); // OQ-618
    const auto* digits = l.masks.mask("gold_digits");
    REQUIRE(digits != nullptr);
    CHECK(big.place(digits->anchor, digits->pos_x, digits->pos_y) == std::pair{34, 1316});
    CHECK(big.place(l.construction.grid.anchor, l.construction.grid.origin_x,
                    l.construction.grid.origin_y) == std::pair{2326, 305});

    // Hits: the tab maps back to its reference centre; the map is not chrome; the status bar,
    // its added span, the question mark and the filler are chrome.
    auto h = app::ui_hit(hud, big, 2297 + 30, 212);
    CHECK(h.chrome);
    CHECK(h.ref == std::optional<std::pair<int, int>>({809, 212}));
    CHECK_FALSE(app::ui_hit(hud, big, 1000, 500).chrome);
    CHECK_FALSE(app::ui_hit(hud, big, 2296, 500).chrome);
    h = app::ui_hit(hud, big, 100, 1340); // the bar's measured part
    CHECK(h.chrome);
    CHECK(h.ref == std::optional<std::pair<int, int>>({100, 757}));
    h = app::ui_hit(hud, big, 1500, 1340); // the span a wider layer adds
    CHECK(h.chrome);
    CHECK_FALSE(h.ref.has_value());
    h = app::ui_hit(hud, big, 2270, 1320); // the question mark
    CHECK(h.chrome);
    CHECK(h.ref == std::optional<std::pair<int, int>>({753, 737}));
    h = app::ui_hit(hud, big, 2400, 1200); // the filler under the panel
    CHECK(h.chrome);
    CHECK_FALSE(h.ref.has_value());
    CHECK_FALSE(app::ui_hit(hud, big, 100, 1315).chrome); // one row above the bar
    // The pause parchment is chrome only while paused.
    CHECK_FALSE(app::ui_hit(hud, big, 300, 300, false).chrome);
    CHECK(app::ui_hit(hud, big, 300, 300, true).chrome);
    // At the reference the status bar is chrome too (before UI-WINDOW-1 a click there went to
    // the map): finding of the owner's playtest.
    CHECK(app::ui_hit(hud, ref, 100, 750).chrome);
    CHECK_FALSE(app::ui_hit(hud, ref, 100, 732).chrome);
}

TEST_CASE("ui click path: a construction cell and the tab at 1024 x 768 and at 2542 x 1351 with "
          "UI scale 1.0 and 2.0") {
    const Layouts l = load();
    const app::ConstructionMenu menu(l.hud, l.construction);
    const auto cell = l.construction.grid.click_centre(1, 0); // (885, 330)
    REQUIRE(cell.has_value());
    struct Case {
        int w, h;
        std::optional<float> scale;
    };
    for (const Case c :
         {Case{1024, 768, std::nullopt}, Case{2542, 1351, std::nullopt}, Case{2542, 1351, 2.0f}}) {
        CAPTURE(c.w);
        CAPTURE(c.scale.value_or(0.0f));
        const auto t = app::ui_transform(c.w, c.h, c.scale);
        const auto p = app::place_hud(l.hud, t.ui_w, t.ui_h);
        // The client pixel at the centre of a layer pixel (what a click on it reports).
        const auto client = [&](int ux, int uy) {
            return std::pair{t.off_x + (ux + 0.5) * t.k, t.off_y + (uy + 0.5) * t.k};
        };
        const auto resolve = [&](std::pair<double, double> pt) {
            const auto u = t.to_ui(pt.first, pt.second);
            REQUIRE(u.has_value());
            return app::ui_hit(l.hud, p, u->first, u->second);
        };
        // The cell: its reference centre placed in the layer, then shown in the client.
        const auto [cx, cy] = p.place(l.construction.grid.anchor, cell->first, cell->second);
        const auto hit = resolve(client(cx, cy));
        CHECK(hit.chrome);
        REQUIRE(hit.ref.has_value());
        CHECK(menu.cell_at(hit.ref->first, hit.ref->second) == std::optional{std::pair{1, 0}});
        const std::vector<std::pair<anno::sim::TypeId, bool>> eligibility = {
            {anno::sim::TypeId{l.construction.cell("public", 1, 0)->id}, true}};
        const auto r = menu.click(hit.ref->first, hit.ref->second, "public", false, &eligibility);
        CHECK(r.kind == app::ConstructionClickKind::Arm);
        // The tab's reference centre (hud.txt click tab_construction 809 212).
        const auto* tab = l.hud.element("tab_construction");
        const auto [tx, ty] = p.place(tab->anchor, 809, 212);
        const auto tab_hit = resolve(client(tx, ty));
        CHECK(tab_hit.ref == std::optional<std::pair<int, int>>({809, 212}));
        // A map pixel left of the panel is no chrome.
        CHECK_FALSE(resolve(client(p.ui_w - 300, 300)).chrome);
    }
}

TEST_CASE("ui layer: the right chrome at 2542 x 1351 is the 1024 x 768 chrome shifted by W - "
          "1024 and the column below it is the filler") {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("skipping: ANNO1503_GAME_DIR is not set");
        return;
    }
    const Layouts l = load();
    app::UiTexCache cache(*game);
    const app::UiMockSpec hud_screen{app::UiMockScreen::Hud, ""};
    const auto small = app::draw_live_screen_rgba(1024, 768, l.hud, l.construction, l.masks,
                                                  hud_screen, cache, nullptr);
    const auto big = app::draw_live_screen_rgba(2542, 1351, l.hud, l.construction, l.masks,
                                                hud_screen, cache, nullptr);
    REQUIRE(small.ok());
    REQUIRE(big.ok());
    const auto px = [](const anno::assets::Rgba& im, int x, int y) {
        const std::size_t o =
            (static_cast<std::size_t>(y) * im.width + static_cast<std::size_t>(x)) * 4;
        return std::array{im.pixels[o], im.pixels[o + 1], im.pixels[o + 2], im.pixels[o + 3]};
    };
    std::size_t compared = 0, differ = 0, opaque = 0;
    for (int y = 0; y < 733; ++y) {
        for (int x = 779; x < 1024; ++x) {
            const auto a = px(small.value(), x, y);
            const auto b = px(big.value(), x + 2542 - 1024, y);
            ++compared;
            opaque += a[3] == 255 ? 1U : 0U;
            differ += a == b ? 0U : 1U;
        }
    }
    CHECK(compared == 245U * 733U);
    CHECK(opaque > compared / 2); // the compared region is real chrome, not empty
    CHECK(differ == 0);
    // The status bar's icons: the gold symbol at (0, H - 35) equals the reference's (0, 733).
    for (int y = 0; y < 35; ++y) {
        for (int x = 0; x < 33; ++x) {
            CHECK(px(small.value(), x, 733 + y) == px(big.value(), x, 1316 + y));
        }
    }
    // Negative control: the same chrome unshifted (as the old 1024 x 768-only path placed it) is
    // not what the big layer holds there.
    std::size_t unshifted_same = 0;
    for (int y = 0; y < 733; ++y) {
        for (int x = 779; x < 1024; ++x) {
            unshifted_same += px(small.value(), x, y) == px(big.value(), x, y) ? 1U : 0U;
        }
    }
    CHECK(unshifted_same < compared / 10);
    // The column below row 768 is opaque all the way down (stone, then the filler picture).
    for (const int y : {768, 900, 1094, 1095, 1350}) {
        CAPTURE(y);
        CHECK(px(big.value(), 2400, y)[3] == 255);
    }
    // 1280 x 1024: the gap is exactly FUELLER03's 256 rows, drawn from row 768 (the original's
    // own 1280 x 1024 choice, AnnoFrame 0x1006331f).
    const auto mode = app::draw_live_screen_rgba(1280, 1024, l.hud, l.construction, l.masks,
                                                 hud_screen, cache, nullptr);
    REQUIRE(mode.ok());
    std::size_t filler_opaque = 0;
    for (int y = 768; y < 1024; ++y) {
        for (int x = 1035; x < 1280; ++x) {
            filler_opaque += px(mode.value(), x, y)[3] == 255 ? 1U : 0U;
        }
    }
    CHECK(filler_opaque == 245U * 256U);
}
