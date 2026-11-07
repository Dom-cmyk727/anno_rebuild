// anno_ui_mock: the shipped CPU reference composition of one HUD/construction screen, written as
// a PNG so `tools/test/ui_mock_compare.py` can diff it against the UI.1 Python oracle's own mock
// pixel by pixel (ADR 0004 "Rendering"/"Testing"; W4-UI-2 H4/C4).
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>

#include <anno/app/png.hpp>
#include <anno/app/ui_layout.hpp>
#include <anno/app/ui_mock.hpp>

namespace {

struct Args {
    std::filesystem::path layout, game_dir, out;
    std::string screen;
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
        } else {
            return std::nullopt;
        }
    }
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
                     "<dir> --out <file.png>\n");
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
