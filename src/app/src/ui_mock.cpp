#include <algorithm>
#include <array>
#include <fstream>
#include <map>

#include <anno/app/ui_mock.hpp>
#include <anno/assets/text.hpp>
#include <anno/formats/tex.hpp>
#include <anno/formats/zei.hpp>

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

/// A parsed `.zei` font plus the raw bytes it was parsed from - the same non-owning-`Bytes`
/// lifetime rule as LoadedTex above (`formats::zei::Glyph::data` spans into this buffer).
struct LoadedFont {
    std::vector<std::uint8_t> bytes;
    formats::zei::CharSet charset;
};

/// Caches a parsed `.tex` file (bytes and all, see LoadedTex) and a parsed `.zei` font (see
/// LoadedFont), mirroring `tools/ui_layout.py`'s `_tex_bank`/`_TEX_CACHE` (several elements of the
/// same screen share a file, e.g. every `fenster.tex` id). `compose_screen` and the standalone
/// `anno_ui_mock` tool each construct one locally for the lifetime of a single call/process;
/// `UiTexCache` (ui_mock.hpp) wraps the same class as `Impl` so `draw_live_screen_xrgb`'s
/// live-window caller can keep one across frames (ADR 0004 "Atlases": no re-decode per frame,
/// textures and fonts alike).
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

    /// The parsed font at `rel_path` (game-dir-relative), or nullptr when the file is missing or
    /// does not parse as a `.zei` CHARSET.
    const formats::zei::CharSet* font(const std::string& rel_path) {
        const auto it = fonts_.find(rel_path);
        if (it != fonts_.end())
            return it->second ? &it->second->charset : nullptr;
        std::filesystem::path p = rel_path;
        p.make_preferred();
        auto bytes = read_binary(game_dir_ / p);
        if (!bytes) {
            fonts_.emplace(rel_path, std::nullopt);
            return nullptr;
        }
        auto parsed = formats::zei::CharSet::parse(*bytes);
        if (!parsed.ok()) {
            fonts_.emplace(rel_path, std::nullopt);
            return nullptr;
        }
        const auto emplaced =
            fonts_.emplace(rel_path, LoadedFont{std::move(*bytes), std::move(parsed).value()})
                .first;
        return &emplaced->second->charset;
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
    std::map<std::string, std::optional<LoadedFont>> fonts_;
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

/// Like `blit`, but onto a canvas that starts fully transparent (every byte 0) and must track its
/// own alpha correctly rather than rely on the destination already being opaque everywhere (the
/// GPU overlay texture, R1's GPU path - the canvas is composited *over* the live GPU frame by the
/// shader, so a pixel this function never touches must read back alpha 0, not a stray opaque
/// black). A source pixel skips at alpha 0 (unchanged from `blit`); at alpha 255 it is a straight
/// overwrite (chrome sprites are always fully opaque or fully transparent, never partial); a
/// partial alpha (a glyph's own anti-aliased edge, `assets::render_text`) composites with the
/// standard Porter-Duff "over" against whatever (if anything) is already there, so two adjacent
/// partially-transparent writes blend correctly instead of one silently overwriting the other.
void blit_rgba_over(assets::Rgba& dst, const assets::Rgba& sprite, int x, int y) {
    for (int sy = 0; sy < sprite.height; ++sy) {
        const int dy = y + sy;
        if (dy < 0 || dy >= dst.height)
            continue;
        const std::size_t srow = static_cast<std::size_t>(sy) * sprite.width * 4;
        const std::size_t drow = static_cast<std::size_t>(dy) * dst.width * 4;
        for (int sx = 0; sx < sprite.width; ++sx) {
            const std::size_t so = srow + static_cast<std::size_t>(sx) * 4;
            const unsigned sa = sprite.pixels[so + 3];
            if (sa == 0)
                continue;
            const int dx = x + sx;
            if (dx < 0 || dx >= dst.width)
                continue;
            const std::size_t d = drow + static_cast<std::size_t>(dx) * 4;
            const unsigned da = dst.pixels[d + 3];
            if (sa == 255 || da == 0) {
                dst.pixels[d] = sprite.pixels[so];
                dst.pixels[d + 1] = sprite.pixels[so + 1];
                dst.pixels[d + 2] = sprite.pixels[so + 2];
                dst.pixels[d + 3] = static_cast<std::uint8_t>(sa);
                continue;
            }
            const unsigned out_a = sa + da * (255 - sa) / 255;
            for (std::size_t c = 0; c < 3; ++c) {
                const unsigned v =
                    (sprite.pixels[so + c] * sa + dst.pixels[d + c] * da * (255 - sa) / 255) /
                    std::max(out_a, 1U);
                dst.pixels[d + c] = static_cast<std::uint8_t>(v);
            }
            dst.pixels[d + 3] = static_cast<std::uint8_t>(out_a);
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

/// Like `blit_xrgb`, but alpha-blends instead of overwriting: `assets::render_text`'s glyph
/// pixels carry a real per-pixel alpha (the level-to-alpha mapping, anno/assets/text.hpp), unlike
/// chrome sprites' binary opaque-or-transparent pixels, so a straight overwrite would draw a hard
/// box around every glyph instead of letting it blend into whatever is already drawn under it
/// (the status bar's own graphic).
void blit_xrgb_alpha(std::uint32_t* dst, int dst_w, int dst_h, const assets::Rgba& sprite, int x,
                     int y) {
    for (int sy = 0; sy < sprite.height; ++sy) {
        const int dy = y + sy;
        if (dy < 0 || dy >= dst_h)
            continue;
        const std::size_t srow = static_cast<std::size_t>(sy) * sprite.width * 4;
        const std::size_t drow = static_cast<std::size_t>(dy) * static_cast<std::size_t>(dst_w);
        for (int sx = 0; sx < sprite.width; ++sx) {
            const std::size_t so = srow + static_cast<std::size_t>(sx) * 4;
            const unsigned alpha = sprite.pixels[so + 3];
            if (alpha == 0)
                continue;
            const int dx = x + sx;
            if (dx < 0 || dx >= dst_w)
                continue;
            const std::uint32_t existing = dst[drow + static_cast<std::size_t>(dx)];
            const unsigned dr = (existing >> 16) & 0xFF, dg = (existing >> 8) & 0xFF,
                           db = existing & 0xFF;
            const unsigned sr = sprite.pixels[so], sg = sprite.pixels[so + 1],
                           sb = sprite.pixels[so + 2];
            const unsigned r = (sr * alpha + dr * (255 - alpha)) / 255;
            const unsigned g = (sg * alpha + dg * (255 - alpha)) / 255;
            const unsigned b = (sb * alpha + db * (255 - alpha)) / 255;
            dst[drow + static_cast<std::size_t>(dx)] = (r << 16) | (g << 8) | b;
        }
    }
}

/// Renders `text` in `font` (assets::render_text) and blends it onto the live buffer at (x, y)
/// with `style`'s colour/alpha, top-left anchored - a no-op when `font` is null (the file was
/// missing or failed to parse) or `text` is empty.
void draw_text_xrgb(std::uint32_t* dst, int dst_w, int dst_h, const formats::zei::CharSet* font,
                    const std::string& text, int x, int y, const assets::TextStyle& style = {}) {
    if (font == nullptr || text.empty())
        return;
    const assets::Rgba image = assets::render_text(*font, text, style);
    blit_xrgb_alpha(dst, dst_w, dst_h, image, x, y);
}

/// `draw_text_xrgb`'s own contract, onto the R1 GPU-overlay canvas (`blit_rgba_over`) instead of
/// the live XRGB buffer.
void draw_text_rgba(assets::Rgba& dst, const formats::zei::CharSet* font, const std::string& text,
                    int x, int y, const assets::TextStyle& style = {}) {
    if (font == nullptr || text.empty())
        return;
    const assets::Rgba image = assets::render_text(*font, text, style);
    blit_rgba_over(dst, image, x, y);
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
                                  const ConstructionLayout& construction, const MaskLayout& masks,
                                  const UiMockSpec& screen, UiTexCache& cache_wrapper,
                                  const std::vector<std::pair<sim::TypeId, bool>>* eligibility,
                                  const HudCounters& counters, bool placement_open,
                                  bool demolish_mode) {
    if (dst == nullptr || dst_w != hud.resolution_w || dst_h != hud.resolution_h)
        return "draw_live_screen_xrgb: destination size does not match the layout's resolution";

    UiTexCache::Impl& cache = cache_wrapper.impl();
    const bool is_construction = screen.kind == UiMockScreen::Construction;
    // H2: the HUD digit font. MainSmall's digit glyphs (code 0x30-0x39) are byte-identical to
    // MainMedium's and HelpNormal's (checked by hand against the real .zei files, W4-UI-2
    // checkpoint that added this - same underlying konvertchars-built font at this size), so
    // which of the three names "the" original draws here does not change a single rendered
    // pixel; MainSmall is picked only because its name best fits a compact status-bar readout.
    // Null (missing/unparseable file) silently draws nothing - `draw_text_xrgb`'s own contract.
    const formats::zei::CharSet* digit_font = cache.font("toolgfx/mainsmall.zei");

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
    // H2: the digit font's measured glyph height (MainSmall '0' is 25 px tall, zei.md) and each
    // counter's own masks.txt rectangle (ADR 0004 R1: "no coordinate lives in code" - the same
    // rule category_at() follows for hud.txt's `viewbautab_0`). Left-aligned at the rectangle's
    // own left edge, vertically centred in its band by the glyph height; plain decimal digits, no
    // thousands separator (no RE evidence for one). A counter whose named mask is missing draws
    // nothing for it, never a guessed position. The colour/alpha mapping of the font's three
    // levels is a flagged default, not an RE fact - OQ-031.
    constexpr int kDigitGlyphH = 25;
    const auto digit_origin =
        [&masks](const std::string& mask_name) -> std::optional<std::pair<int, int>> {
        const UiMask* m = masks.mask(mask_name);
        if (m == nullptr)
            return std::nullopt;
        return std::make_pair(m->pos_x,
                              m->pos_y + (static_cast<int>(m->size_h) - kDigitGlyphH) / 2);
    };
    // The gold counter, shown on both the idle HUD and the construction screen (OQ-538's own
    // evidence: "On hud_idle_running the bar shows gold"). data/ui/masks.txt `gold_digits` - the
    // same sprite-sad measurement the chrome rectangles above were measured by; nothing drew into
    // it until this checkpoint, so it stayed a mask.
    if (counters.gold) {
        if (const auto pos = digit_origin("gold_digits")) {
            draw_text_xrgb(dst, dst_w, dst_h, digit_font, std::to_string(*counters.gold),
                           pos->first, pos->second);
        }
    }
    if (is_construction) {
        blit_el("baumat_tools");
        blit_el("baumat_wood");
        blit_el("baumat_bricks");
        blit_el("baumat_marble");
        // H2: the four build-material counters - construction-screen only (OQ-538, resolved:
        // "tools/wood/bricks digits are absent" from the idle HUD). masks.txt
        // baumat_{tools,wood,bricks,marble}_d, in that order (OQ-538's own segment order). Values
        // are the caller's (whole tons, already converted from the city's 8.8-fixed-point stock
        // by goods identity - HudCounters's own doc comment); this function only formats and
        // draws.
        if (counters.tools) {
            if (const auto pos = digit_origin("baumat_tools_d")) {
                draw_text_xrgb(dst, dst_w, dst_h, digit_font, std::to_string(*counters.tools),
                               pos->first, pos->second);
            }
        }
        if (counters.wood) {
            if (const auto pos = digit_origin("baumat_wood_d")) {
                draw_text_xrgb(dst, dst_w, dst_h, digit_font, std::to_string(*counters.wood),
                               pos->first, pos->second);
            }
        }
        if (counters.bricks) {
            if (const auto pos = digit_origin("baumat_bricks_d")) {
                draw_text_xrgb(dst, dst_w, dst_h, digit_font, std::to_string(*counters.bricks),
                               pos->first, pos->second);
            }
        }
        if (counters.marble) {
            if (const auto pos = digit_origin("baumat_marble_d")) {
                draw_text_xrgb(dst, dst_w, dst_h, digit_font, std::to_string(*counters.marble),
                               pos->first, pos->second);
            }
        }
    }
    blit_el("fragezeichen");
    blit_el("rosh_46");
    blit_el("rosh_48");
    blit_el("rosh_51");
    if (is_construction) {
        blit_el("einwohner");
        // H2: the residents counter - construction-screen only, same as the icon itself
        // (docs/re/ui.md 2.1: `STATUSBAR_EINWOHNER` is "absent from pass-2 idle"). masks.txt
        // `einwohner_digits` - a real measured rectangle; what format the original draws inside
        // it (a plain count vs. e.g. a ratio) is open - OQ-564 already covers "alignment within a
        // measured rectangle is not itself evidence".
        if (counters.residents) {
            if (const auto pos = digit_origin("einwohner_digits")) {
                draw_text_xrgb(dst, dst_w, dst_h, digit_font, std::to_string(*counters.residents),
                               pos->first, pos->second);
            }
        }
    }
    blit_el("zoom_plus");
    blit_el("zoom_minus");
    blit_el("lupe");
    blit_el("compass_w");
    blit_el("compass_e");

    if (is_construction) {
        blit_el("plankegross");
        if (placement_open) {
            // C1/C6: the building info/placement panel, drawn instead of the grid - the two
            // occupy the same screen region (this function's own doc comment) and
            // ConstructionMenu::click already resolves the pickaxe/cancel-X overlap by this same
            // flag, so the drawing follows the same split. Every element here is a real
            // sprite-sad-measured position (data/ui/hud.txt, shot r2_info_pioneer_house); the
            // panel's own cost row and name are not drawn - OQ-561, no position evidence exists.
            blit_el("info_panel");
            blit_el("infoexit");
            blit_el("rotate_left");
            blit_el("rotate_right");
            blit_el("betriebskosten");
        } else {
            // ADR 0004 row 5: 660 while demolishing, 659 otherwise (docs/re/ui.md line 192).
            blit_el("abrissicon", demolish_mode ? 660 : 659);

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
                // C2: the sim's live eligibility is the lock authority, never the cell's own
                // static `unlocked` field (compose_screen's oracle-matching behaviour keeps that
                // field, since its job is to match the *original's* fixed Citizen-start
                // screenshot).
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
    }

    return "";
}

formats::Result<assets::Rgba, std::string>
draw_live_screen_rgba(int w, int h, const HudLayout& hud, const ConstructionLayout& construction,
                      const MaskLayout& masks, const UiMockSpec& screen, UiTexCache& cache_wrapper,
                      const std::vector<std::pair<sim::TypeId, bool>>* eligibility,
                      const HudCounters& counters, bool placement_open, bool demolish_mode) {
    if (w != hud.resolution_w || h != hud.resolution_h)
        return std::string(
            "draw_live_screen_rgba: destination size does not match the layout's resolution");

    // The GPU path's CPU-side half: the same live state draw_live_screen_xrgb composes into the
    // CPU buffer, composed here instead into a canvas that starts fully transparent (so it can be
    // uploaded as one texture and blended *over* the GPU-rendered frame by a full-screen pass -
    // gpu.wgsl's `ui_overlay_fs`, viewer.cpp's draw_gpu). A separate function from
    // draw_live_screen_xrgb, not shared code, but over the same data/ui/ layouts and the same
    // live eligibility/counters/digit-rectangle lookups - the real safety net against the two
    // drifting apart is each path's own pixel comparison against the UI.1 oracle
    // (ui_mock_matches_oracle / live_screen_matches_oracle --renderer gpu), not independent
    // implementation of the digit literals.
    UiTexCache::Impl& cache = cache_wrapper.impl();
    const bool is_construction = screen.kind == UiMockScreen::Construction;
    const formats::zei::CharSet* digit_font = cache.font("toolgfx/mainsmall.zei");

    assets::Rgba canvas;
    canvas.width = static_cast<std::uint16_t>(w);
    canvas.height = static_cast<std::uint16_t>(h);
    canvas.pixels.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4, 0);

    auto blit_el = [&](const std::string& name, std::optional<int> tex_id_override = std::nullopt) {
        const UiElement* el = hud.element(name);
        if (!el || !el->has_pos || el->file.empty())
            return;
        const int tid = tex_id_override.value_or(el->tex_id.value_or(0));
        const auto sprite = cache.decode(el->file, tid);
        if (!sprite)
            return;
        blit_rgba_over(canvas, *sprite, el->pos_x, el->pos_y);
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
    // H2: masks.txt's own measured rectangles (ADR 0004 R1 - see draw_live_screen_xrgb's own
    // comment on digit_origin, mirrored here).
    constexpr int kDigitGlyphH = 25;
    const auto digit_origin =
        [&masks](const std::string& mask_name) -> std::optional<std::pair<int, int>> {
        const UiMask* m = masks.mask(mask_name);
        if (m == nullptr)
            return std::nullopt;
        return std::make_pair(m->pos_x,
                              m->pos_y + (static_cast<int>(m->size_h) - kDigitGlyphH) / 2);
    };
    if (counters.gold) {
        if (const auto pos = digit_origin("gold_digits")) {
            draw_text_rgba(canvas, digit_font, std::to_string(*counters.gold), pos->first,
                           pos->second);
        }
    }
    if (is_construction) {
        blit_el("baumat_tools");
        blit_el("baumat_wood");
        blit_el("baumat_bricks");
        blit_el("baumat_marble");
        if (counters.tools) {
            if (const auto pos = digit_origin("baumat_tools_d")) {
                draw_text_rgba(canvas, digit_font, std::to_string(*counters.tools), pos->first,
                               pos->second);
            }
        }
        if (counters.wood) {
            if (const auto pos = digit_origin("baumat_wood_d")) {
                draw_text_rgba(canvas, digit_font, std::to_string(*counters.wood), pos->first,
                               pos->second);
            }
        }
        if (counters.bricks) {
            if (const auto pos = digit_origin("baumat_bricks_d")) {
                draw_text_rgba(canvas, digit_font, std::to_string(*counters.bricks), pos->first,
                               pos->second);
            }
        }
        if (counters.marble) {
            if (const auto pos = digit_origin("baumat_marble_d")) {
                draw_text_rgba(canvas, digit_font, std::to_string(*counters.marble), pos->first,
                               pos->second);
            }
        }
    }
    blit_el("fragezeichen");
    blit_el("rosh_46");
    blit_el("rosh_48");
    blit_el("rosh_51");
    if (is_construction) {
        blit_el("einwohner");
        if (counters.residents) {
            if (const auto pos = digit_origin("einwohner_digits")) {
                draw_text_rgba(canvas, digit_font, std::to_string(*counters.residents), pos->first,
                               pos->second);
            }
        }
    }
    blit_el("zoom_plus");
    blit_el("zoom_minus");
    blit_el("lupe");
    blit_el("compass_w");
    blit_el("compass_e");

    if (is_construction) {
        blit_el("plankegross");
        if (placement_open) {
            blit_el("info_panel");
            blit_el("infoexit");
            blit_el("rotate_left");
            blit_el("rotate_right");
            blit_el("betriebskosten");
        } else {
            // ADR 0004 row 5: 660 while demolishing, 659 otherwise (docs/re/ui.md line 192).
            blit_el("abrissicon", demolish_mode ? 660 : 659);

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
                const bool allowed = live_allowed(eligibility, sim::TypeId{cell.id});
                if (allowed && thumb != 0) {
                    const auto icon = cache.decode("toolgfx/fenster.tex",
                                                   cell_icon_tex_id(thumb, cell.col, cell.row));
                    if (icon)
                        blit_rgba_over(canvas, *icon, x, y);
                } else if (!allowed && lock_sprite) {
                    blit_rgba_over(canvas, *lock_sprite, x, y);
                }
            }
        }
    }

    return canvas;
}

} // namespace anno::app
