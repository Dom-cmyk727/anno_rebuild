// anno_ui_mock: the shipped CPU reference composition of one HUD/construction screen, written as
// a PNG so `tools/test/ui_mock_compare.py` can diff it against the UI.1 Python oracle's own mock
// pixel by pixel (ADR 0004 "Rendering"/"Testing"; W4-UI-2 H4/C4).
//
// UI-WINDOW-1: `--live <W>x<H> [--ui-scale <s>]` instead writes what the live window overlays
// on a W x H frame: draw_live_screen_rgba at the UI transform's layer size, stretched to the
// window when the transform is not 1:1 (ui_transform.hpp), over black. No live counters and no
// eligibility (every cell locked): the chrome only, for comparing placement and scale against
// captures (tools/test/ui_window_compare.py).
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

#include <anno/app/png.hpp>
#include <anno/app/ui_layout.hpp>
#include <anno/app/ui_mock.hpp>
#include <anno/app/ui_transform.hpp>

namespace {

struct Args {
    std::filesystem::path layout, game_dir, out;
    std::string screen;
    std::optional<std::pair<int, int>> live;
    std::optional<float> ui_scale;
};

std::optional<Args> parse(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> std::optional<std::string> {
            if (i + 1 >= argc)
                return std::nullopt;
            return argv[++i];
        };
        if (arg == "--layout") {
            auto v = next();
            if (!v)
                return std::nullopt;
            a.layout = *v;
        } else if (arg == "--game-dir") {
            auto v = next();
            if (!v)
                return std::nullopt;
            a.game_dir = *v;
        } else if (arg == "--out") {
            auto v = next();
            if (!v)
                return std::nullopt;
            a.out = *v;
        } else if (arg == "--screen") {
            auto v = next();
            if (!v)
                return std::nullopt;
            a.screen = *v;
        } else if (arg == "--live") {
            auto v = next();
            int w = 0, h = 0;
            if (!v || std::sscanf(v->c_str(), "%dx%d", &w, &h) != 2 || w <= 0 || h <= 0 ||
                w > 0xFFFF || h > 0xFFFF)
                return std::nullopt;
            a.live = std::pair{w, h};
        } else if (arg == "--ui-scale") {
            auto v = next();
            float s = 0.0f;
            if (!v || std::sscanf(v->c_str(), "%f", &s) != 1 || !(s >= 1.0f) || !(s <= 2.0f))
                return std::nullopt;
            a.ui_scale = s;
        } else {
            return std::nullopt;
        }
    }
    if (a.ui_scale && !a.live)
        return std::nullopt;
    if (a.layout.empty() || a.game_dir.empty() || a.out.empty() || a.screen.empty())
        return std::nullopt;
    return a;
}

} // namespace

int main(int argc, char** argv) {
    const auto args = parse(argc, argv);
    if (!args) {
        std::fprintf(stderr,
                     "usage: anno_ui_mock --layout <data/ui dir> --screen <name> --game-dir "
                     "<dir> --out <file.png> [--live <W>x<H> [--ui-scale <1.0-2.0>]]\n");
        return 2;
    }
    const auto spec = anno::app::parse_screen_name(args->screen);
    if (!spec) {
        std::fprintf(stderr, "unknown screen '%s'\n", args->screen.c_str());
        return 1;
    }
    const auto hud = anno::app::load_hud_layout(args->layout / "hud.txt");
    if (!hud.ok()) {
        std::fprintf(stderr, "hud.txt: %s\n", hud.error().c_str());
        return 1;
    }
    const auto construction =
        anno::app::load_construction_layout(args->layout / "construction.txt");
    if (!construction.ok()) {
        std::fprintf(stderr, "construction.txt: %s\n", construction.error().c_str());
        return 1;
    }
    if (args->live) {
        const auto masks = anno::app::load_mask_layout(args->layout / "masks.txt");
        if (!masks.ok()) {
            std::fprintf(stderr, "masks.txt: %s\n", masks.error().c_str());
            return 1;
        }
        const auto t =
            anno::app::ui_transform(args->live->first, args->live->second, args->ui_scale);
        anno::app::UiTexCache cache(args->game_dir);
        const auto layer =
            anno::app::draw_live_screen_rgba(t.ui_w, t.ui_h, hud.value(), construction.value(),
                                             masks.value(), *spec, cache, nullptr);
        if (!layer.ok()) {
            std::fprintf(stderr, "draw: %s\n", layer.error().c_str());
            return 1;
        }
        anno::assets::Rgba frame =
            t.identity() ? layer.value() : anno::app::scale_ui_layer(layer.value(), t);
        for (std::size_t i = 0; i + 3 < frame.pixels.size(); i += 4) {
            // Over black, as compose_screen's canvas: straight alpha to opaque.
            const unsigned a = frame.pixels[i + 3];
            for (std::size_t c = 0; c < 3; ++c)
                frame.pixels[i + c] = static_cast<std::uint8_t>(frame.pixels[i + c] * a / 255);
            frame.pixels[i + 3] = 255;
        }
        std::printf("layer %dx%d s %.7g k %.9g off %d %d\n", t.ui_w, t.ui_h,
                    static_cast<double>(t.s), static_cast<double>(t.k), t.off_x, t.off_y);
        const auto err = anno::app::write_png(args->out, frame.width, frame.height, frame.pixels);
        if (!err.empty()) {
            std::fprintf(stderr, "write_png: %s\n", err.c_str());
            return 1;
        }
        return 0;
    }
    const auto composed =
        anno::app::compose_screen(hud.value(), construction.value(), *spec, args->game_dir);
    if (!composed.ok()) {
        std::fprintf(stderr, "compose: %s\n", composed.error().c_str());
        return 1;
    }
    const auto& img = composed.value();
    const auto err = anno::app::write_png(args->out, img.width, img.height, img.pixels);
    if (!err.empty()) {
        std::fprintf(stderr, "write_png: %s\n", err.c_str());
        return 1;
    }
    return 0;
}
