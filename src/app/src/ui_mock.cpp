#include <algorithm>
#include <array>
#include <fstream>
#include <map>

#include <anno/app/ui_mock.hpp>
#include <anno/formats/tex.hpp>

namespace anno::app {

namespace {

std::optional<std::vector<std::uint8_t>> read_binary(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
    return bytes;
}

/// A parsed `.tex` file plus the raw bytes it was parsed from: `formats::tex::Texture::body` and
/// `Palette::entries` are `Bytes` (`std::span<const std::uint8_t>`, non-owning) spans *into that
/// buffer* (bytes.hpp, formats' zero-copy parsing convention), so the buffer must outlive the
/// parsed `TexFile` - keeping only the `TexFile` here (and letting the read buffer fall out of
/// scope) would leave every span inside it dangling.
struct LoadedTex {
    std::vector<std::uint8_t> bytes;
    formats::tex::TexFile file;
};

/// Caches a parsed `.tex` file (bytes and all, see LoadedTex), mirroring `tools/ui_layout.py`'s
/// `_tex_bank`/`_TEX_CACHE` (several elements of the same screen share a file, e.g. every
/// `fenster.tex` id). `compose_screen` and the standalone `anno_ui_mock` tool each construct one
/// locally for the lifetime of a single call/process; `UiTexCache` (ui_mock.hpp) wraps the same
/// class as `Impl` so `draw_live_screen_xrgb`'s live-window caller can keep one across frames.
class TexCache {
  public:
    explicit TexCache(std::filesystem::path game_dir) : game_dir_(std::move(game_dir)) {}

    /// Decodes texture `tid` of `rel_path` (game-dir-relative, forward slashes per data/ui/
    /// convention) to RGBA, or nullopt when the file or the id is missing.
    std::optional<assets::Rgba> decode(const std::string& rel_path, int tid) {
        const formats::tex::TexFile* file = file_of(rel_path);
        if (!file)
            return std::nullopt;
        for (const auto& t : file->textures) {
            if (t.id == static_cast<std::uint32_t>(tid)) {
                auto rgba = assets::to_rgba(*file, t);
                if (!rgba.ok())
                    return std::nullopt;
                return rgba.value();
            }
        }
        return std::nullopt;
    }

  private:
    const formats::tex::TexFile* file_of(const std::string& rel_path) {
        const auto it = files_.find(rel_path);
        if (it != files_.end())
            return it->second ? &it->second->file : nullptr;
        std::filesystem::path p = rel_path;
        p.make_preferred();
        auto bytes = read_binary(game_dir_ / p);
        if (!bytes) {
            files_.emplace(rel_path, std::nullopt);
            return nullptr;
        }
        auto parsed = formats::tex::TexFile::parse(*bytes);
        if (!parsed.ok()) {
            files_.emplace(rel_path, std::nullopt);
            return nullptr;
        }
        const auto emplaced =
            files_.emplace(rel_path, LoadedTex{std::move(*bytes), std::move(parsed).value()}).first;
        return &emplaced->second->file;
    }

    std::filesystem::path game_dir_;
    std::map<std::string, std::optional<LoadedTex>> files_;
};

/// `tools/ui_layout.py blit_rgba`: skips transparent source pixels, clips to the canvas, writes
/// RGB only (the canvas's own alpha stays 255 throughout - compose_screen's output has no alpha).
void blit(assets::Rgba& dst, const assets::Rgba& sprite, int x, int y) {
    for (int sy = 0; sy < sprite.height; ++sy) {
        const int dy = y + sy;
        if (dy < 0 || dy >= dst.height)
            continue;
        const std::size_t srow = static_cast<std::size_t>(sy) * sprite.width * 4;
        const std::size_t drow = static_cast<std::size_t>(dy) * dst.width * 4;
        for (int sx = 0; sx < sprite.width; ++sx) {
            const std::size_t so = srow + static_cast<std::size_t>(sx) * 4;
            if (sprite.pixels[so + 3] == 0)
                continue;
            const int dx = x + sx;
            if (dx < 0 || dx >= dst.width)
                continue;
            const std::size_t d = drow + static_cast<std::size_t>(dx) * 4;
            dst.pixels[d] = sprite.pixels[so];
            dst.pixels[d + 1] = sprite.pixels[so + 1];
            dst.pixels[d + 2] = sprite.pixels[so + 2];
        }
    }
}

/// `cell_icon_tex_id` (tools/ui_layout.py): record index i = row*4+col, three consecutive
/// textures per record (idle, hover, pressed; docs/re/ui.md section 2.4).
int cell_icon_tex_id(int thumb_tex, int col, int row) {
    return thumb_tex + 3 * (row * 4 + col);
}

/// `blit`'s own semantic (skip transparent, clip, overwrite - never blended), packed into
/// viewer.cpp's `buffer_`/`blend_preview` XRGB convention (0x00RRGGBB per `std::uint32_t`)
/// instead of a 4-byte-per-pixel `assets::Rgba`.
void blit_xrgb(std::uint32_t* dst, int dst_w, int dst_h, const assets::Rgba& sprite, int x, int y) {
    for (int sy = 0; sy < sprite.height; ++sy) {
        const int dy = y + sy;
        if (dy < 0 || dy >= dst_h)
            continue;
        const std::size_t srow = static_cast<std::size_t>(sy) * sprite.width * 4;
        const std::size_t drow = static_cast<std::size_t>(dy) * static_cast<std::size_t>(dst_w);
        for (int sx = 0; sx < sprite.width; ++sx) {
            const std::size_t so = srow + static_cast<std::size_t>(sx) * 4;
            if (sprite.pixels[so + 3] == 0)
                continue;
            const int dx = x + sx;
            if (dx < 0 || dx >= dst_w)
                continue;
            const std::uint32_t r = sprite.pixels[so], g = sprite.pixels[so + 1],
                                b = sprite.pixels[so + 2];
            dst[drow + static_cast<std::size_t>(dx)] = (r << 16) | (g << 8) | b;
        }
    }
}

/// Whether `type` is allowed per `eligibility` (ascending by id; nullptr or no entry: false -
/// never guessed open, matching ConstructionMenu::click's own rule).
bool live_allowed(const std::vector<std::pair<sim::TypeId, bool>>* eligibility, sim::TypeId type) {
    if (eligibility == nullptr)
        return false;
    const auto it =
        std::lower_bound(eligibility->begin(), eligibility->end(), type,
                         [](const auto& entry, sim::TypeId t) { return entry.first.v < t.v; });
    return it != eligibility->end() && it->first == type && it->second;
}

} // namespace

struct UiTexCache::Impl : TexCache {
    using TexCache::TexCache;
};

UiTexCache::UiTexCache(std::filesystem::path game_dir)
    : impl_(std::make_unique<Impl>(std::move(game_dir))) {}
UiTexCache::~UiTexCache() = default;
UiTexCache::UiTexCache(UiTexCache&&) noexcept = default;
UiTexCache& UiTexCache::operator=(UiTexCache&&) noexcept = default;

std::optional<UiMockSpec> parse_screen_name(const std::string& screen) {
    if (screen == "hud_idle")
        return UiMockSpec{UiMockScreen::Hud, ""};
    static const std::array<const char*, 8> kCategories = {
        "public", "roads", "farms", "workshops", "coastal", "mines", "military", "ornamental"};
    for (const auto* cat : kCategories) {
        if (screen == std::string("construction_") + cat)
            return UiMockSpec{UiMockScreen::Construction, cat};
    }
    return std::nullopt;
}

formats::Result<assets::Rgba, std::string> compose_screen(const HudLayout& hud,
                                                          const ConstructionLayout& construction,
                                                          const UiMockSpec& screen,
                                                          const std::filesystem::path& game_dir) {
    assets::Rgba canvas;
    canvas.width = static_cast<std::uint16_t>(hud.resolution_w);
    canvas.height = static_cast<std::uint16_t>(hud.resolution_h);
    canvas.pixels.assign(static_cast<std::size_t>(canvas.width) * canvas.height * 4, 0);
    for (std::size_t i = 3; i < canvas.pixels.size(); i += 4)
        canvas.pixels[i] = 255; // opaque throughout; the oracle's output carries no alpha

    TexCache cache(game_dir);
    const bool is_construction = screen.kind == UiMockScreen::Construction;

    auto blit_el = [&](const std::string& name, std::optional<int> tex_id_override = std::nullopt) {
        const UiElement* el = hud.element(name);
        if (!el || !el->has_pos || el->file.empty())
            return;
        const int tid = tex_id_override.value_or(el->tex_id.value_or(0));
        const auto sprite = cache.decode(el->file, tid);
        if (!sprite)
            return;
        blit(canvas, *sprite, el->pos_x, el->pos_y);
    };

    blit_el("weltkarte");
    blit_el("mainuntergrund");
    if (!is_construction) {
        blit_el("tab_construction", 8);
        blit_el("tab_military", 13);
        blit_el("tab_help", 10);
        blit_el("tab_options", 11);
    } else {
        blit_el("tab_construction", 12);
        blit_el("tab_military", 9);
        blit_el("tab_help", 10);
        blit_el("tab_options", 11);
    }
    blit_el("statusbar");
    blit_el("gold");
    if (is_construction) {
        blit_el("baumat_tools");
        blit_el("baumat_wood");
        blit_el("baumat_bricks");
        blit_el("baumat_marble");
    }
    blit_el("fragezeichen");
    blit_el("rosh_46");
    blit_el("rosh_48");
    blit_el("rosh_51");
    if (is_construction)
        blit_el("einwohner");
    blit_el("zoom_plus");
    blit_el("zoom_minus");
    blit_el("lupe");
    blit_el("compass_w");
    blit_el("compass_e");

    if (is_construction) {
        blit_el("plankegross");
        blit_el("abrissicon", 659);

        const int ox = construction.grid.origin_x;
        const int oy = construction.grid.origin_y;
        const int stride = construction.grid.cell_px;
        const auto* cat = construction.category(screen.category);
        const int thumb = cat ? cat->thumb_tex : 0;

        const UiElement* lock_el = hud.element("no_klick");
        const int lock_id = lock_el && lock_el->tex_id ? *lock_el->tex_id : 56;
        const std::string lock_file =
            (lock_el && !lock_el->file.empty()) ? lock_el->file : "toolgfx/fenster.tex";
        const auto lock_sprite = cache.decode(lock_file, lock_id);

        for (const auto& cell : construction.cells) {
            if (cell.category != screen.category)
                continue;
            const int x = ox + cell.col * stride;
            const int y = oy + cell.row * stride;
            if (cell.unlocked && thumb != 0) {
                const auto icon = cache.decode("toolgfx/fenster.tex",
                                               cell_icon_tex_id(thumb, cell.col, cell.row));
                if (icon)
                    blit(canvas, *icon, x, y);
            } else if (!cell.unlocked && lock_sprite) {
                blit(canvas, *lock_sprite, x, y);
            }
        }
    }

    return canvas;
}

std::string draw_live_screen_xrgb(std::uint32_t* dst, int dst_w, int dst_h, const HudLayout& hud,
                                  const ConstructionLayout& construction, const UiMockSpec& screen,
                                  UiTexCache& cache_wrapper,
                                  const std::vector<std::pair<sim::TypeId, bool>>* eligibility) {
    if (dst == nullptr || dst_w != hud.resolution_w || dst_h != hud.resolution_h)
        return "draw_live_screen_xrgb: destination size does not match the layout's resolution";

    UiTexCache::Impl& cache = cache_wrapper.impl();
    const bool is_construction = screen.kind == UiMockScreen::Construction;

    // Mirrors compose_screen's own blit_el exactly (ADR 0004 R1: a divergence here is a bug, not
    // a second design); the only difference from compose_screen below is the lock source.
    auto blit_el = [&](const std::string& name, std::optional<int> tex_id_override = std::nullopt) {
        const UiElement* el = hud.element(name);
        if (!el || !el->has_pos || el->file.empty())
            return;
        const int tid = tex_id_override.value_or(el->tex_id.value_or(0));
        const auto sprite = cache.decode(el->file, tid);
        if (!sprite)
            return;
        blit_xrgb(dst, dst_w, dst_h, *sprite, el->pos_x, el->pos_y);
    };

    blit_el("weltkarte");
    blit_el("mainuntergrund");
    if (!is_construction) {
        blit_el("tab_construction", 8);
        blit_el("tab_military", 13);
        blit_el("tab_help", 10);
        blit_el("tab_options", 11);
    } else {
        blit_el("tab_construction", 12);
        blit_el("tab_military", 9);
        blit_el("tab_help", 10);
        blit_el("tab_options", 11);
    }
    blit_el("statusbar");
    blit_el("gold");
    if (is_construction) {
        blit_el("baumat_tools");
        blit_el("baumat_wood");
        blit_el("baumat_bricks");
        blit_el("baumat_marble");
    }
    blit_el("fragezeichen");
    blit_el("rosh_46");
    blit_el("rosh_48");
    blit_el("rosh_51");
    if (is_construction)
        blit_el("einwohner");
    blit_el("zoom_plus");
    blit_el("zoom_minus");
    blit_el("lupe");
    blit_el("compass_w");
    blit_el("compass_e");

    if (is_construction) {
        blit_el("plankegross");
        blit_el("abrissicon", 659);

        const int ox = construction.grid.origin_x;
        const int oy = construction.grid.origin_y;
        const int stride = construction.grid.cell_px;
        const auto* cat = construction.category(screen.category);
        const int thumb = cat ? cat->thumb_tex : 0;

        const UiElement* lock_el = hud.element("no_klick");
        const int lock_id = lock_el && lock_el->tex_id ? *lock_el->tex_id : 56;
        const std::string lock_file =
            (lock_el && !lock_el->file.empty()) ? lock_el->file : "toolgfx/fenster.tex";
        const auto lock_sprite = cache.decode(lock_file, lock_id);

        for (const auto& cell : construction.cells) {
            if (cell.category != screen.category)
                continue;
            const int x = ox + cell.col * stride;
            const int y = oy + cell.row * stride;
            // C2: the sim's live eligibility is the lock authority, never the cell's own static
            // `unlocked` field (compose_screen's oracle-matching behaviour keeps that field,
            // since its job is to match the *original's* fixed Citizen-start screenshot).
            const bool allowed = live_allowed(eligibility, sim::TypeId{cell.id});
            if (allowed && thumb != 0) {
                const auto icon = cache.decode("toolgfx/fenster.tex",
                                               cell_icon_tex_id(thumb, cell.col, cell.row));
                if (icon)
                    blit_xrgb(dst, dst_w, dst_h, *icon, x, y);
            } else if (!allowed && lock_sprite) {
                blit_xrgb(dst, dst_w, dst_h, *lock_sprite, x, y);
            }
        }
    }

    return "";
}

} // namespace anno::app
