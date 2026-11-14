// UI-WINDOW-1: where the in-game UI layer sits in the window, and how a window pixel maps back
// into it (ADR 0004 R3, decided by the owner 2026-11-14: default UI scale 1.0, the original's 1:1
// UI; the History Edition's scale rule as the `--ui-scale` option). Pure arithmetic, no SDL.
//
// Evidence (docs/re/he-ui-scale.md, re-checked for this packet with capstone on the HE x86-64
// binaries; RVAs):
// - the layer size: AnnoDisplay.dll tDisplay::OnResize 0x2ccf0. The client size is clamped up to
//   the reference (0x2cdb4..0x2cdca: max(W, 1024), max(H, 768); r14d = 0x400, ebp = 0x300 at
//   0x2cd8d/0x2cd92), then the step-down loop 0x2ce11..0x2ce8d (float32 throughout):
//   uiW = cvttss2si(float(W) * (1.0f / s)), uiH likewise; while uiW < 1024 or uiH < 768,
//   s += -0.1f (0xBDCCCCCD at 0xff901c); once s <= 1.0f the layer is computed once more with
//   1.0f / s of that stepped s and s becomes 1.0f (0x2ce63..0x2ce89). A setting <= 0 skips the
//   loop (0x2ce14 `comiss`/`jbe`), leaving the layer 0 x 0.
// - the 1200-line rule: UpdateGameSettings 0x2d943 (`cmp dword ptr [rax + r14*8 + 4], 0x4b0;
//   jle`, rax = [rdi+0x568] at 0x2d931, r14 = the settings field +0xd8 at 0x2d910) and 0x2da17
//   (`mov [AdditionalOptions+0xc], 1.0f`): the height of the display mode chosen in the options'
//   list (8-byte {w, h} entries, tDisplay::GetResolutions) of 1200 lines or fewer forces the
//   setting to 1.0. The rebuild has no mode list in a window; it reads the window's client height
//   instead, the windowed counterpart of the chosen mode's height (OQ-617).
// - the click mapping: UpdateResolutionAndClipping(IIII_N) 0x5a961..0x5aab1: kh = float(H) /
//   float(uiH); if float(W) > kh * float(uiW) (strict, `comiss`/`jbe` at 0x5a9a7) the layer is
//   height-limited: k = kh, offX = cvttss2si((float(W) - kh * uiW + 1.0f) * 0.5f), offY = 0;
//   otherwise width-limited: k = float(W) / float(uiW), offY = cvttss2si((float(H) - k * uiH +
//   1.0f) * 0.5f), offX = 0 (1.0f at 0xff8df8, 0.5f at 0xff8da0). k and the offsets reach only
//   tDisplay+0x1a8..0x1b4 (0x2d168..0x2d1b0), whose readers in AnnoDisplay are the getters
//   0x18940..0x18970 and copies (0x18fab, 0x19431) - no draw path. AnnoFrame.dll's window
//   procedure 0x22a26..0x22a88 uses them for clicks: u = cvttss2si((float(p) - float(off)) / k).
// - the draw: Texture::InitTexture 0x5c940 builds each layer's quad at 0x5c9dd..0x5cb2a from the
//   client size ([renderer+0x24], [+0x28]) and the layer size ([+0x208 + 0xa0*i], [+0x20c ...]):
//   the same aspect test, then off = cvttss2si((float(W) - kh * uiW) * 0.5f + 0.5f) (`mulss`
//   then `addss`, 0.5f at 0xff8da0), and the quad's corners are (off, 0) and (W - off, H)
//   (height-limited) or (0, off) and (W, H - off) (width-limited), UV 0..1. The viewport is the
//   whole client (RSSetViewports 0x5ac65..0x5acb4). So the layer is stretched to that rectangle
//   per axis, kx = (W - 2 off) / uiW and ky = H / uiH (or the transpose) - not by the uniform k.
//   At 2542 x 1351, 2.0: kx = 2542 / 1495, ky = 1351 / 794, the quad covers every client row.
//   The draw and the click mapping can differ by up to a pixel (OQ-619).
//
// The default (the original) is the same arithmetic with the setting 1.0: a window of at least
// 1024 x 768 gets a 1:1 layer of the window's own size (k 1, offsets 0); a smaller window gets a
// 1024 x 768 layer fitted into it (OQ-616: the original's display modes never were smaller).
#pragma once

#include <cstdint>
#include <optional>
#include <utility>

#include <anno/assets/texture.hpp>

namespace anno::app {

/// The numbers of the scale rule, as data with the original/HE values as defaults (CLAUDE.md "no
/// hardcoded limits"); each default's evidence is in this header's top comment.
struct UiScaleRule {
    /// The reference the layer never goes below (the original's 1024 x 768 HUD, data/ui/hud.txt
    /// `resolution`; HE's clamp at 0x2cdb4..0x2cdca).
    int ref_w = 1024, ref_h = 768;
    /// HE's step (0xBDCCCCCD = -0.1f, added once per iteration).
    float step = 0.1f;
    /// HE's 1200-line rule: at a client height of this or less, the setting counts as 1.0.
    int force_one_max_h = 1200;
    /// The HE options slider's range ("Interface-Skalierung", 1.0 to 2.0; he-ui-scale.md 1).
    float min_setting = 1.0f, max_setting = 2.0f;
};

/// The layer and its placement in a client of `client_w` x `client_h` pixels.
struct UiTransform {
    int client_w = 0, client_h = 0;
    /// The UI layer the HUD is composed in (at least ref_w x ref_h).
    int ui_w = 0, ui_h = 0;
    /// The effective scale after the step-down (HE's tDisplay+0x554): 1.0 when forced.
    float s = 1.0f;
    /// The click mapping's uniform factor and the layer's top-left (UpdateResolutionAndClipping
    /// 0x5a961; what AnnoFrame 0x22a26 divides by).
    float k = 1.0f;
    int off_x = 0, off_y = 0;
    /// The rectangle the layer is drawn into, [draw_x0, draw_x1) x [draw_y0, draw_y1) in client
    /// pixels (Texture::InitTexture 0x5c9dd..0x5cb2a): the layer is stretched to it per axis.
    int draw_x0 = 0, draw_y0 = 0, draw_x1 = 0, draw_y1 = 0;

    /// True when the layer is the client 1:1 (k exactly 1, no offset, same size): the UI is then
    /// drawn straight into the frame, never resampled.
    [[nodiscard]] bool identity() const {
        return k == 1.0f && off_x == 0 && off_y == 0 && ui_w == client_w && ui_h == client_h &&
               draw_x0 == 0 && draw_y0 == 0 && draw_x1 == client_w && draw_y1 == client_h;
    }

    /// The layer pixel under client pixel (`px`, `py`): `trunc((p - off) / k)` in float32
    /// (AnnoFrame 0x22a26). `px`/`py` are first floored to the pixel that holds them (the
    /// original's mouse messages carry whole pixels). nullopt when the result lies outside the
    /// layer (a letterbox bar of a small window).
    [[nodiscard]] std::optional<std::pair<int, int>> to_ui(double px, double py) const;
};

/// The original rule (the default): the HE arithmetic with the setting 1.0.
[[nodiscard]] UiTransform original_ui_transform(int client_w, int client_h,
                                                const UiScaleRule& rule = {});

/// HE's rule with the player's setting `setting` (clamped to the rule's slider range): 1.0 at
/// `force_one_max_h` lines or fewer, else stepped down until the layer keeps ref_w x ref_h.
[[nodiscard]] UiTransform he_ui_transform(int client_w, int client_h, float setting,
                                          const UiScaleRule& rule = {});

/// The UI scale as the window applies it: the original (setting nullopt) or HE's rule.
[[nodiscard]] inline UiTransform ui_transform(int client_w, int client_h,
                                              std::optional<float> setting,
                                              const UiScaleRule& rule = {}) {
    return setting ? he_ui_transform(client_w, client_h, *setting, rule)
                   : original_ui_transform(client_w, client_h, rule);
}

/// The UI layer `layer` (straight alpha, `t.ui_w` x `t.ui_h`) stretched per axis onto the draw
/// rectangle `t.draw_*` of a straight-alpha image of the client's size; transparent outside it.
/// Bilinear (HE's UI sampler: AnnoDisplay 0x5e081, D3D11 filter 0x15 MIN_MAG_MIP_LINEAR) with
/// Direct3D's pixel-centre convention, layer coordinate u = (x + 0.5 - x0) * uiW / (x1 - x0) -
/// 0.5 (v likewise), edges clamped. The colours are interpolated premultiplied, so a
/// transparent neighbour (alpha 0, colour 0) never darkens an opaque edge. Which address mode HE
/// uses for the UI quad (clamp or wrap, 0x5a10e / 0x5a121) is open (he-ui-scale.md "Open");
/// clamp is taken here.
[[nodiscard]] assets::Rgba scale_ui_layer(const assets::Rgba& layer, const UiTransform& t);

/// Keeps the last stretched layer: `scale` redoes `scale_ui_layer` only when the layer's pixels
/// or the transform changed since the previous call (the HUD changes far less often than the
/// window draws). One per window; not thread-safe.
class UiLayerScaler {
  public:
    const assets::Rgba& scale(const assets::Rgba& layer, const UiTransform& t);
    /// How many calls actually resampled (the others reused the previous result).
    [[nodiscard]] std::uint64_t resampled() const { return resampled_; }

  private:
    assets::Rgba layer_, out_;
    UiTransform t_;
    bool valid_ = false;
    std::uint64_t resampled_ = 0;
};

/// Draws the straight-alpha `overlay` (exactly `w` x `h`) over the XRGB frame `dst` (0x00RRGGBB
/// per pixel, viewer.cpp's CPU buffer): out = src * a + dst * (1 - a) per channel, integer,
/// rounding down - `blit_xrgb_alpha`'s own rule (ui_mock.cpp).
void blend_over_xrgb(std::uint32_t* dst, int w, int h, const assets::Rgba& overlay);

} // namespace anno::app
