// A CPU model of the GPU's binned objects pass (docs/render/gpu-chunks.md 5.1),
// shared by the synthetic and the corpus tests of the chunk draw list.
#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

#include <anno/render/chunk_draws.hpp>
#include <anno/render/frame_table.hpp>
#include <anno/render/iso.hpp>

namespace anno::render::test {

namespace flags = object_flags;

/// What the GPU's binned pass computes (gpu-chunks.md 5.1), per pixel, from the
/// bins alone: the oracle that the bins and the record fields are sufficient.
inline void replay_binned(const ChunkDraws& d, const FrameTable& t, std::vector<Px>& px,
                          std::vector<std::uint8_t>& base) {
    const int w = d.rect.w, h = d.rect.h;
    px.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), Px{0, 0, 0, 0});
    base.assign(px.size(), 255);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t bin = static_cast<std::size_t>(y) / d.bin_px * d.bins_x +
                                    static_cast<std::size_t>(x) / d.bin_px;
            Px acc{0, 0, 0, 0}, loc{0, 0, 0, 0};
            std::uint8_t abase = 255, lbase = 255;
            std::optional<std::uint32_t> seg;
            auto flush = [&] {
                if (seg) {
                    render::over(acc, loc);
                    if (loc[3] != 0) {
                        abase = lbase;
                    }
                    loc = Px{0, 0, 0, 0};
                    lbase = 255;
                    seg.reset();
                }
            };
            for (std::uint32_t k = d.bin_offsets[bin]; k < d.bin_offsets[bin + 1]; ++k) {
                const ObjectRecord r = d.object(d.bin_indices[k]);
                Px* dst = &acc;
                std::uint8_t* dbase = &abase;
                if (r.clip == render::kClipNone) {
                    flush();
                } else {
                    if (!seg || *seg != r.clip) {
                        flush();
                        seg = r.clip;
                    }
                    dst = &loc;
                    dbase = &lbase;
                    const Rect& c = d.clips[r.clip];
                    const int sx = d.rect.x + x, sy = d.rect.y + y;
                    if (sx < c.x || sy < c.y || sx >= c.x + c.w || sy >= c.y + c.h) {
                        continue;
                    }
                }
                const auto f = t.get(r.frame);
                const int lx = d.rect.x + x - r.x, ly = d.rect.y + y - r.y;
                if (lx < 0 || ly < 0 || lx >= f->image->width || ly >= f->image->height) {
                    continue;
                }
                const std::uint8_t* tx =
                    &f->image->pixels[(static_cast<std::size_t>(ly) * f->image->width +
                                       static_cast<std::size_t>(lx)) *
                                      4];
                if (tx[3] == 0) {
                    continue;
                }
                const bool shadow = (r.flags & flags::kShadow) != 0;
                render::over(*dst, shadow ? render::kShadow : Px{tx[0], tx[1], tx[2], tx[3]});
                if ((r.flags & flags::kTrackBase) != 0) {
                    const int keel = f->keel[static_cast<std::size_t>(lx)];
                    *dbase = keel + d.base_band >= f->header.anchor_y
                                 ? static_cast<std::uint8_t>(std::min(keel - ly, 254))
                                 : std::uint8_t{255};
                }
            }
            flush();
            const auto i = static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                           static_cast<std::size_t>(x);
            px[i] = acc;
            base[i] = abase;
        }
    }
}

} // namespace anno::render::test
