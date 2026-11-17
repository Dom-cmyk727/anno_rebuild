#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

#include <anno/app/info_preview.hpp>

namespace anno::app::info_preview {

namespace {

/// AnnoGame.dll 0x1000bb60's tables, as f32 like the original: the thirds 0x100217c4 (k / 3
/// rounded to 3 decimals), the centring constant 0x10021a84 (0.167), the side offsets 0x10021828.
double third(unsigned k) {
    return static_cast<double>(
        static_cast<float>(std::round(static_cast<double>(k) * 1000.0 / 3.0) / 1000.0));
}
constexpr double kSixth = static_cast<double>(0.167F);
constexpr std::array<std::pair<double, double>, 4> kSide = {
    std::pair{-0.5, 0.0}, std::pair{0.0, 0.5}, std::pair{0.5, 0.0}, std::pair{0.0, -0.5}};

/// View.dll 0x10088af0: scale 59.1633 (0x100a2790) times cos 45 degrees (screen x per tile of
/// y - x) and times sin 45 degrees sin(42 pi / 256) (screen y per tile of x + y).
constexpr double kScale = static_cast<double>(59.1633F);
constexpr double kCos45 = 0.7071067811865476;
constexpr double kSinTilt = 0.4928981922297840; // sin(42 pi / 256), Rx 0x10088c80 angle 0x2a
constexpr double kProjX = kScale * kCos45;
constexpr double kProjY = kScale * kCos45 * kSinTilt;

struct Rect {
    int x0 = std::numeric_limits<int>::max(), y0 = std::numeric_limits<int>::max();
    int x1 = std::numeric_limits<int>::min(), y1 = std::numeric_limits<int>::min();
    void add(int x, int y, int w, int h) {
        x0 = std::min(x0, x);
        y0 = std::min(y0, y);
        x1 = std::max(x1, x + w);
        y1 = std::max(y1, y + h);
    }
};

struct Placed {
    const assets::Rgba* image = nullptr;
    int x = 0, y = 0;
};

/// The parts at `rotation` in draw order (building, then arrows), each at its projected point
/// minus its anchor (AnnoDisplay.dll 0x1000ac80). Empty when that turn has no building.
std::vector<Placed> parts(const Input& in, unsigned rotation) {
    std::vector<Placed> out;
    const unsigned r = rotation & 3U;
    if (r >= in.building.size() || in.building[r].image == nullptr)
        return out;
    const unsigned cw = r % 2 == 0 ? in.w : in.h, ch = r % 2 == 0 ? in.h : in.w;
    const auto [bx, by] = project(cw / 2.0, ch / 2.0);
    out.push_back(
        {in.building[r].image, bx - in.building[r].anchor_x, by - in.building[r].anchor_y});
    if (in.arrow_frames.empty())
        return out;
    for (const EntranceArrow& a : in.arrows) {
        const ArrowPlace p = place_arrow(a, in.w, in.h, r);
        const Sprite& s = in.arrow_frames[arrow_frame(p.direction, in.arrow_frames.size())];
        if (s.image == nullptr)
            continue;
        const auto [sx, sy] = project(p.x, p.y);
        out.push_back({s.image, sx - s.anchor_x, sy - s.anchor_y});
    }
    return out;
}

} // namespace

std::vector<EntranceArrow> entrance_arrows(const formats::objectpos::ObjectPos& objectpos) {
    std::vector<EntranceArrow> out;
    for (const auto& a : objectpos.conpo2()) {
        if (a.side <= 3)
            out.push_back(EntranceArrow{a.x3, a.y3, a.side});
    }
    return out;
}

std::pair<int, int> project(double x, double y) {
    return {static_cast<int>(kProjX * (y - x)), static_cast<int>(kProjY * (x + y))};
}

ArrowPlace place_arrow(const EntranceArrow& arrow, unsigned w, unsigned h, unsigned rotation) {
    const auto [dx, dy] = kSide[arrow.side & 3U];
    const double u = third(arrow.x3) + kSixth + dx;
    const double v = third(arrow.y3) + kSixth + dy;
    const double fw = w, fh = h;
    ArrowPlace p;
    switch (rotation & 3U) {
    case 0:
        p.x = u;
        p.y = v;
        break;
    case 1:
        p.x = v;
        p.y = fw - u;
        break;
    case 2:
        p.x = fw - u;
        p.y = fh - v;
        break;
    default:
        p.x = fh - v;
        p.y = u;
        break;
    }
    p.direction = (rotation + arrow.side) & 3U;
    return p;
}

std::size_t arrow_frame(unsigned direction, std::size_t frames) {
    if (frames == 0)
        return 0;
    const std::size_t mask = frames - 1;
    const unsigned shift = mask >= 15 ? 0U : (mask < 7 ? 2U : 1U);
    return ((static_cast<std::size_t>(direction) << 2U) >> shift) & mask;
}

std::optional<assets::Rgba> compose(const Input& in, unsigned rotation) {
    std::array<Rect, 4> rects{};
    for (unsigned r = 0; r < 4; ++r) {
        const auto ps = parts(in, r);
        if (ps.empty())
            return std::nullopt;
        for (const Placed& p : ps)
            rects[r].add(p.x, p.y, p.image->width, p.image->height);
    }
    int cw = 0, ch = 0;
    for (const Rect& b : rects) {
        cw = std::max(cw, b.x1 - b.x0);
        ch = std::max(ch, b.y1 - b.y0);
    }
    const Rect& cur = rects[rotation & 3U];
    // Halving toward zero (cdq; sub; sar), as C++ integer division does.
    const int ox = cur.x0 + (cur.x1 - cur.x0 - cw) / 2;
    const int oy = cur.y0 + (cur.y1 - cur.y0 - ch) / 2;
    assets::Rgba out;
    out.width = static_cast<std::uint16_t>(cw);
    out.height = static_cast<std::uint16_t>(ch);
    out.pixels.assign(static_cast<std::size_t>(cw) * static_cast<std::size_t>(ch) * 4, 0);
    for (const Placed& p : parts(in, rotation)) {
        const assets::Rgba& img = *p.image;
        for (int sy = 0; sy < img.height; ++sy) {
            const int ty = p.y - oy + sy;
            if (ty < 0 || ty >= ch)
                continue;
            for (int sx = 0; sx < img.width; ++sx) {
                const int tx = p.x - ox + sx;
                const std::size_t s =
                    (static_cast<std::size_t>(sy) * img.width + static_cast<std::size_t>(sx)) * 4;
                if (tx < 0 || tx >= cw || img.pixels[s + 3] == 0)
                    continue;
                const std::size_t d = (static_cast<std::size_t>(ty) * static_cast<std::size_t>(cw) +
                                       static_cast<std::size_t>(tx)) *
                                      4;
                std::copy_n(img.pixels.begin() + static_cast<std::ptrdiff_t>(s), 3,
                            out.pixels.begin() + static_cast<std::ptrdiff_t>(d));
                out.pixels[d + 3] = 255;
            }
        }
    }
    return out;
}

std::pair<unsigned, unsigned> scaled_size(unsigned w, unsigned h, unsigned bw, unsigned bh) {
    const double a = static_cast<double>(w) / bw;
    const double b = static_cast<double>(static_cast<float>(static_cast<double>(h) / bh));
    double s = a > b ? static_cast<double>(static_cast<float>(a)) : b;
    if (!(s > 1.0))
        s = 1.0;
    return {static_cast<unsigned>(w / s), static_cast<unsigned>(h / s)};
}

} // namespace anno::app::info_preview
