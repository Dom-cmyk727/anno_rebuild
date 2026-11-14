// UI-WINDOW-1: the UI layer's size and placement (ui_transform.hpp; evidence there). Every step
// is float32 in the original's order: plain float arithmetic, which the project's strict
// floating-point flags (/fp:strict, -ffp-contract=off) keep in float32, matching the original's
// `divss`/`mulss`/`addss`/`cvttss2si` one for one.
#include <algorithm>
#include <cmath>
#include <vector>

#include <anno/app/ui_transform.hpp>

namespace anno::app {

namespace {

/// `cvttss2si`: truncation toward zero. The inputs here are window sizes and coordinates, far
/// inside int's range.
int trunc_to_int(float v) {
    return static_cast<int>(v);
}

/// OnResize's layer size for the clamped client (`w`, `h`) and the setting `s0`, with the
/// effective scale it ends at (AnnoDisplay 0x2ce11..0x2ce8d).
void layer_size(int w, int h, float s0, const UiScaleRule& rule, UiTransform& out) {
    float s = s0;
    int uw = 0, uh = 0; // r9d, ecx = r15d = 0 (0x2cd58, 0x2ce02/0x2ce05)
    if (s > 0.0f) {
        const float step = -rule.step;
        for (;;) {
            if (uw >= rule.ref_w && uh >= rule.ref_h)
                break; // 0x2ce21..0x2ce28, checked before the first computation as well
            const float r = 1.0f / s;
            uw = trunc_to_int(static_cast<float>(w) * r);
            uh = trunc_to_int(static_cast<float>(h) * r);
            if (uw >= rule.ref_w && uh >= rule.ref_h)
                break; // 0x2ce51..0x2ce58
            s += step; // 0x2ce5a
            if (!(s > 1.0f)) {
                // 0x2ce63..0x2ce89: one more layer with 1.0f / s of the stepped s, then s = 1.0f.
                const float r1 = 1.0f / s;
                s = 1.0f;
                uw = trunc_to_int(static_cast<float>(w) * r1);
                uh = trunc_to_int(static_cast<float>(h) * r1);
                break;
            }
        }
    }
    out.s = s;
    out.ui_w = uw;
    out.ui_h = uh;
}

/// UpdateResolutionAndClipping's uniform aspect fit of the layer into the client (0x5a961..
/// 0x5aab1).
void fit(UiTransform& t) {
    t.draw_x0 = t.draw_y0 = 0;
    t.draw_x1 = t.client_w;
    t.draw_y1 = t.client_h;
    if (t.ui_w <= 0 || t.ui_h <= 0 || t.client_w <= 0 || t.client_h <= 0) {
        t.k = 1.0f;
        t.off_x = t.off_y = 0;
        return;
    }
    const float cw = static_cast<float>(t.client_w), ch = static_cast<float>(t.client_h);
    const float lw = static_cast<float>(t.ui_w), lh = static_cast<float>(t.ui_h);
    const float kh = ch / lh;
    const float span = kh * lw;
    if (cw > span) {
        t.k = kh;
        t.off_x = trunc_to_int((cw - span + 1.0f) * 0.5f);
        t.off_y = 0;
        // Texture::InitTexture 0x5ca1d..0x5ca2d: (W - span) * 0.5f + 0.5f, right = W - off.
        const int off = trunc_to_int((cw - span) * 0.5f + 0.5f);
        t.draw_x0 = off;
        t.draw_x1 = t.client_w - off;
    } else {
        t.k = cw / lw;
        t.off_x = 0;
        t.off_y = trunc_to_int((ch - t.k * lh + 1.0f) * 0.5f);
        // 0x5ca32..0x5ca51: k = W / uiW, (H - k * uiH) * 0.5f + 0.5f, bottom = H - off.
        const int off = trunc_to_int((ch - t.k * lh) * 0.5f + 0.5f);
        t.draw_y0 = off;
        t.draw_y1 = t.client_h - off;
    }
}

UiTransform make(int client_w, int client_h, float setting, const UiScaleRule& rule) {
    UiTransform t;
    t.client_w = std::max(client_w, 0);
    t.client_h = std::max(client_h, 0);
    // 0x2cdb4..0x2cdca: the layer is never smaller than the reference.
    const int w = std::max(t.client_w, rule.ref_w);
    const int h = std::max(t.client_h, rule.ref_h);
    layer_size(w, h, setting, rule, t);
    fit(t);
    return t;
}

} // namespace

std::optional<std::pair<int, int>> UiTransform::to_ui(double px, double py) const {
    if (!(k > 0.0f) || !std::isfinite(px) || !std::isfinite(py))
        return std::nullopt;
    // Whole pixels first (the original's WM_* coordinates are integers), then AnnoFrame
    // 0x22a26's float32 form.
    const float fx = static_cast<float>(std::floor(px));
    const float fy = static_cast<float>(std::floor(py));
    const float ux = (fx - static_cast<float>(off_x)) / k;
    const float uy = (fy - static_cast<float>(off_y)) / k;
    // Outside the layer: a letterbox bar. `cvttss2si` would fold (-1, 0) onto 0; the bar left of
    // the layer is not UI, so that fold is not followed (the original never letterboxes its UI at
    // its own display modes).
    if (ux < 0.0f || uy < 0.0f)
        return std::nullopt;
    const int x = trunc_to_int(ux), y = trunc_to_int(uy);
    if (x >= ui_w || y >= ui_h)
        return std::nullopt;
    return std::pair{x, y};
}

assets::Rgba scale_ui_layer(const assets::Rgba& layer, const UiTransform& t) {
    assets::Rgba out;
    if (t.client_w <= 0 || t.client_h <= 0 || t.client_w > 0xFFFF || t.client_h > 0xFFFF)
        return out;
    out.width = static_cast<std::uint16_t>(t.client_w);
    out.height = static_cast<std::uint16_t>(t.client_h);
    out.pixels.assign(
        static_cast<std::size_t>(t.client_w) * static_cast<std::size_t>(t.client_h) * 4, 0);
    const int lw = layer.width, lh = layer.height;
    if (lw <= 0 || lh <= 0 || t.draw_x1 <= t.draw_x0 || t.draw_y1 <= t.draw_y0 ||
        layer.pixels.size() < static_cast<std::size_t>(lw) * static_cast<std::size_t>(lh) * 4)
        return out;
    // The draw rectangle (Texture::InitTexture's quad), clipped to the client; the layer is
    // stretched to it per axis.
    const int x_lo = std::clamp(t.draw_x0, 0, t.client_w);
    const int x_hi = std::clamp(t.draw_x1, 0, t.client_w);
    const int y_lo = std::clamp(t.draw_y0, 0, t.client_h);
    const int y_hi = std::clamp(t.draw_y1, 0, t.client_h);
    struct Tap {
        int i0 = 0, i1 = 0;
        float f = 0.0f;
    };
    const auto taps = [](int lo, int hi, int x0, int x1, int size) {
        const double per = static_cast<double>(size) / static_cast<double>(x1 - x0);
        std::vector<Tap> v(static_cast<std::size_t>(std::max(hi - lo, 0)));
        for (int c = lo; c < hi; ++c) {
            const double u = (c + 0.5 - x0) * per - 0.5;
            const double fl = std::floor(u);
            const int i0 = static_cast<int>(fl);
            v[static_cast<std::size_t>(c - lo)] = {std::clamp(i0, 0, size - 1),
                                                   std::clamp(i0 + 1, 0, size - 1),
                                                   static_cast<float>(u - fl)};
        }
        return v;
    };
    const std::vector<Tap> xs = taps(x_lo, x_hi, t.draw_x0, t.draw_x1, lw);
    const std::vector<Tap> ys = taps(y_lo, y_hi, t.draw_y0, t.draw_y1, lh);
    const std::uint8_t* src = layer.pixels.data();
    const auto row = static_cast<std::size_t>(lw) * 4;
    for (int y = y_lo; y < y_hi; ++y) {
        const Tap& ty = ys[static_cast<std::size_t>(y - y_lo)];
        const std::uint8_t* r0 = src + static_cast<std::size_t>(ty.i0) * row;
        const std::uint8_t* r1 = src + static_cast<std::size_t>(ty.i1) * row;
        std::uint8_t* d = out.pixels.data() +
                          (static_cast<std::size_t>(y) * static_cast<std::size_t>(t.client_w) +
                           static_cast<std::size_t>(x_lo)) *
                              4;
        for (int x = x_lo; x < x_hi; ++x, d += 4) {
            const Tap& tx = xs[static_cast<std::size_t>(x - x_lo)];
            const std::uint8_t* p00 = r0 + static_cast<std::size_t>(tx.i0) * 4;
            const std::uint8_t* p01 = r0 + static_cast<std::size_t>(tx.i1) * 4;
            const std::uint8_t* p10 = r1 + static_cast<std::size_t>(tx.i0) * 4;
            const std::uint8_t* p11 = r1 + static_cast<std::size_t>(tx.i1) * 4;
            if ((p00[3] | p01[3] | p10[3] | p11[3]) == 0)
                continue; // all four transparent: stays 0
            const float w00 = (1.0f - tx.f) * (1.0f - ty.f), w01 = tx.f * (1.0f - ty.f);
            const float w10 = (1.0f - tx.f) * ty.f, w11 = tx.f * ty.f;
            const float a00 = p00[3] * w00, a01 = p01[3] * w01, a10 = p10[3] * w10,
                        a11 = p11[3] * w11;
            const float a = a00 + a01 + a10 + a11;
            if (a <= 0.0f)
                continue;
            for (int c = 0; c < 3; ++c) {
                const float v = (p00[c] * a00 + p01[c] * a01 + p10[c] * a10 + p11[c] * a11) / a;
                d[c] = static_cast<std::uint8_t>(std::clamp(v + 0.5f, 0.0f, 255.0f));
            }
            d[3] = static_cast<std::uint8_t>(std::clamp(a + 0.5f, 0.0f, 255.0f));
        }
    }
    return out;
}

const assets::Rgba& UiLayerScaler::scale(const assets::Rgba& layer, const UiTransform& t) {
    const bool same_t = t_.client_w == t.client_w && t_.client_h == t.client_h &&
                        t_.ui_w == t.ui_w && t_.ui_h == t.ui_h && t_.draw_x0 == t.draw_x0 &&
                        t_.draw_x1 == t.draw_x1 && t_.draw_y0 == t.draw_y0 &&
                        t_.draw_y1 == t.draw_y1;
    if (valid_ && same_t && layer_.width == layer.width && layer_.height == layer.height &&
        layer_.pixels == layer.pixels)
        return out_;
    out_ = scale_ui_layer(layer, t);
    layer_ = layer;
    t_ = t;
    valid_ = true;
    ++resampled_;
    return out_;
}

void blend_over_xrgb(std::uint32_t* dst, int w, int h, const assets::Rgba& overlay) {
    if (dst == nullptr || w <= 0 || h <= 0 || overlay.width != w || overlay.height != h ||
        overlay.pixels.size() < static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4)
        return;
    const std::size_t n = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    const std::uint8_t* s = overlay.pixels.data();
    for (std::size_t i = 0; i < n; ++i, s += 4) {
        const unsigned a = s[3];
        if (a == 0)
            continue;
        if (a == 255) {
            dst[i] = (std::uint32_t{s[0]} << 16) | (std::uint32_t{s[1]} << 8) | s[2];
            continue;
        }
        const std::uint32_t e = dst[i];
        const unsigned dr = (e >> 16) & 0xFF, dg = (e >> 8) & 0xFF, db = e & 0xFF;
        const unsigned r = (s[0] * a + dr * (255 - a)) / 255;
        const unsigned g = (s[1] * a + dg * (255 - a)) / 255;
        const unsigned b = (s[2] * a + db * (255 - a)) / 255;
        dst[i] = (r << 16) | (g << 8) | b;
    }
}

UiTransform original_ui_transform(int client_w, int client_h, const UiScaleRule& rule) {
    return make(client_w, client_h, 1.0f, rule);
}

UiTransform he_ui_transform(int client_w, int client_h, float setting, const UiScaleRule& rule) {
    float s = std::isfinite(setting) ? std::clamp(setting, rule.min_setting, rule.max_setting)
                                     : rule.min_setting;
    if (client_h <= rule.force_one_max_h)
        s = 1.0f; // UpdateGameSettings 0x2d943 / 0x2da17
    return make(client_w, client_h, s, rule);
}

} // namespace anno::app
