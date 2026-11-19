// The fertility indicator over a farm's placement ghost (see the header; docs/re/ui.md 4.1).
#include <algorithm>
#include <utility>

#include <anno/app/fertility_indicator.hpp>
#include <anno/app/loader.hpp>

namespace anno::app {

std::string fertility_closed_gate(const FertilityGates& gates) {
    if (!gates.preview_valid) {
        return "preview-valid";
    }
    if (!gates.last_ghost) {
        return "last-ghost";
    }
    if (!gates.rect_nonempty) {
        return "rect-nonempty";
    }
    if (!gates.type_found) {
        return "type-found";
    }
    if (!gates.projected) {
        return "projected";
    }
    return {};
}

std::optional<std::uint32_t> fertility_crop_object(const sim::BuildingType& type) {
    if (type.input_goods.empty()) {
        return std::nullopt;
    }
    const sim::GoodsId input = type.input_goods[0];
    if (input < kFertilityGoodsFirst || input > kFertilityGoodsLast) {
        return std::nullopt;
    }
    // The HEAD reader zero-fills a short record (56-byte buffer, D 0x10009334), so a record that
    // ends before the u16 reads 0 there.
    if (type.head_raw.size() < kFertilityCropHeadOffset + 2) {
        return std::nullopt;
    }
    const std::uint32_t crop =
        static_cast<std::uint32_t>(type.head_raw[kFertilityCropHeadOffset]) |
        (static_cast<std::uint32_t>(type.head_raw[kFertilityCropHeadOffset + 1]) << 8U);
    if (crop == 0) {
        return std::nullopt;
    }
    return crop;
}

std::vector<std::uint32_t> fertility_row_half_widths(std::uint32_t r) {
    // 0x10047a5b-0x10047aa1, register by register: c = ecx, s = [esp+0x1c], e = edi, a = eax,
    // i = edx, b = ebx. The original compares e and a unsigned (`ja`) after 32-bit wrap-around.
    std::vector<std::uint32_t> tab(static_cast<std::size_t>(r) + 1, 0);
    std::uint32_t c = r;
    std::uint32_t s = 2 * r;
    std::uint32_t e = r * r;
    std::uint32_t a = (r - 1) * r;
    std::uint32_t i = 0;
    std::uint32_t b = 0;
    while (true) {
        e += 0xffffffffU - b; // edi += -1 - ebx
        tab[i] = c;
        if (!(e > a)) {
            s -= 2;
            tab[c] = i;
            c -= 1;
            a -= s;
        }
        i += 1;
        b += 2;
        // `cmp edx, ecx; jle`: a signed compare.
        if (!(static_cast<std::int64_t>(i) <=
              static_cast<std::int64_t>(static_cast<std::int32_t>(c)))) {
            break;
        }
    }
    // 0x10047aa7-0x10047ac7: small radii widen their outer rows by one.
    if (r <= 3) {
        const std::uint32_t q = (r + 1) / 4;
        for (std::uint32_t k = r - q; k <= r; ++k) {
            tab[k] += 1;
        }
    }
    return tab;
}

std::vector<sim::TileXY> fertility_area(sim::TileXY origin, std::uint32_t w, std::uint32_t h,
                                        std::uint32_t r, const TileRect& island) {
    std::vector<sim::TileXY> out;
    if (w == 0 || h == 0) {
        return out;
    }
    const std::vector<std::uint32_t> tab = fertility_row_half_widths(r);
    const std::int64_t x0 = origin.x, y0 = origin.y;
    const std::int64_t cx0 = x0 + (static_cast<std::int64_t>(w) - 1) / 2;
    const std::int64_t cx1 = cx0 + (static_cast<std::int64_t>(w) - 1) % 2;
    const std::int64_t cy0 = y0 + (static_cast<std::int64_t>(h) - 1) / 2;
    const std::int64_t cy1 = cy0 + (static_cast<std::int64_t>(h) - 1) % 2;
    const std::int64_t ix0 = island.x0, iy0 = island.y0;
    const std::int64_t ix1 = ix0 + island.w, iy1 = iy0 + island.h;
    const auto row = [&](std::int64_t y, std::uint32_t half) {
        if (y < iy0 || y >= iy1) {
            return;
        }
        const std::int64_t from = std::max(cx0 - static_cast<std::int64_t>(half), ix0);
        const std::int64_t to = std::min(cx1 + static_cast<std::int64_t>(half) + 1, ix1);
        for (std::int64_t x = from; x < to; ++x) {
            const bool in_footprint = x >= x0 && x < x0 + static_cast<std::int64_t>(w) && y >= y0 &&
                                      y < y0 + static_cast<std::int64_t>(h);
            if (!in_footprint) {
                out.push_back(
                    sim::TileXY{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)});
            }
        }
    };
    for (std::uint32_t k = 0; k <= r; ++k) {
        const std::int64_t up = cy0 - static_cast<std::int64_t>(k);
        const std::int64_t down = cy1 + static_cast<std::int64_t>(k);
        row(up, tab[k]);
        if (down != up) {
            row(down, tab[k]);
        }
    }
    return out;
}

std::uint32_t fertility_value(std::uint64_t good, std::uint64_t n) {
    if (n == 0) {
        return 0;
    }
    return static_cast<std::uint32_t>(good * 10 / n);
}

FertilityDecision fertility_decision(const sim::BuildingType* type, sim::TileXY origin,
                                     std::uint8_t rotation, const TileRect& island,
                                     const FertilityGates& gates,
                                     const FertilityTileSource* source) {
    FertilityDecision d;
    // The draw's gates (0x10048464-0x100484e8) come before the bar function.
    FertilityGates before = gates;
    before.projected = true; // tested after the count
    if (const std::string closed = fertility_closed_gate(before); !closed.empty()) {
        d.outcome = FertilityDecision::Outcome::GateClosed;
        d.reason = closed;
        return d;
    }
    if (type == nullptr) {
        d.outcome = FertilityDecision::Outcome::GateClosed;
        d.reason = "type-found";
        return d;
    }
    const std::optional<std::uint32_t> crop = fertility_crop_object(*type);
    if (!crop) {
        d.outcome = FertilityDecision::Outcome::NotEligible;
        d.reason = "not-eligible";
        return d;
    }
    if (source == nullptr) {
        d.outcome = FertilityDecision::Outcome::NeedsSim;
        d.reason = std::string(kFertilityNeedsSim);
        return d;
    }
    const bool odd = (rotation & 1U) != 0;
    const std::uint32_t w = odd ? type->height : type->width;
    const std::uint32_t h = odd ? type->width : type->height;
    const std::vector<sim::TileXY> area = fertility_area(origin, w, h, type->action_radius, island);
    for (const sim::TileXY t : area) {
        if (!source->crop_dry(t, *crop) && source->crop_placeable(t, *crop)) {
            ++d.good;
        }
    }
    d.n = area.size();
    d.frame = fertility_value(d.good, d.n);
    if (!gates.projected) {
        d.outcome = FertilityDecision::Outcome::GateClosed;
        d.reason = "projected";
        return d;
    }
    d.outcome = FertilityDecision::Outcome::Draw;
    return d;
}

std::string fertility_label(const FertilityDecision& decision) {
    switch (decision.outcome) {
    case FertilityDecision::Outcome::GateClosed:
        return "gate:" + decision.reason;
    case FertilityDecision::Outcome::NotEligible:
        return "not-eligible";
    case FertilityDecision::Outcome::NeedsSim:
        return std::string(kFertilityNeedsSim);
    case FertilityDecision::Outcome::Draw:
        return "frame " + std::to_string(decision.frame);
    }
    return "unknown";
}

formats::Result<std::vector<assets::unit::UnitFrames>, std::string>
load_fertility_frames(const std::filesystem::path& game_dir) {
    auto sprites = load_figure_facings(game_dir, kFertilityFigure, kFertilityFrames, std::nullopt);
    if (!sprites.ok()) {
        return "fertility indicator sprite (figure " + std::to_string(kFertilityFigure) +
               "): " + sprites.error();
    }
    return std::move(sprites).value().facings;
}

void blend_fertility_frame(const assets::Rgba& image, int anchor_x, int anchor_y, int px, int py,
                           int view_w, int view_h, std::uint32_t* xrgb) {
    const int left = px - anchor_x;
    const int top = py - anchor_y;
    for (int y = 0; y < image.height; ++y) {
        const int vy = top + y;
        if (vy < 0 || vy >= view_h) {
            continue;
        }
        for (int x = 0; x < image.width; ++x) {
            const int vx = left + x;
            if (vx < 0 || vx >= view_w) {
                continue;
            }
            const std::size_t at =
                (static_cast<std::size_t>(y) * image.width + static_cast<std::size_t>(x)) * 4;
            const std::uint32_t alpha = image.pixels[at + 3];
            if (alpha == 0) {
                continue;
            }
            std::uint32_t& dst =
                xrgb[static_cast<std::size_t>(vy) * static_cast<std::size_t>(view_w) +
                     static_cast<std::size_t>(vx)];
            std::uint32_t outc = 0;
            for (std::uint32_t ch = 0; ch < 3; ++ch) {
                const std::uint32_t shift = 16U - 8U * ch;
                const std::uint32_t src = image.pixels[at + ch];
                const std::uint32_t old = (dst >> shift) & 0xffU;
                const std::uint32_t mixed = (src * alpha + old * (255U - alpha) + 127U) / 255U;
                outc |= mixed << shift;
            }
            dst = outc;
        }
    }
}

} // namespace anno::app
