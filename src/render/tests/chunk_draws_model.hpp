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
/// frame.hpp's kNoTerrain (not included: the model needs only chunk_draws.hpp).
inline constexpr std::uint16_t kNoTerrainModel = 0xFFFF;

/// What the GPU's binned pass computes (gpu-chunks.md 5.1), per pixel, from the
/// bins alone: the oracle that the bins and the record fields are sufficient.
/// With d.depth_test, the depth of gpu.wgsl's chunk_build_cs (depth.md 2.2) from
/// `terrain` (Chunk::terrain; null or empty = none), and its submerged plane into
/// `submerged` when given.
inline void replay_binned(const ChunkDraws& d, const FrameTable& t, std::vector<Px>& px,
                          std::vector<std::uint8_t>& base,
                          const std::vector<std::uint16_t>* terrain = nullptr,
                          std::vector<std::uint8_t>* submerged = nullptr) {
    const int w = d.rect.w, h = d.rect.h;
    px.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), Px{0, 0, 0, 0});
    base.assign(px.size(), 255);
    if (submerged != nullptr) {
        submerged->assign(px.size(), 0);
    }
    const bool has_terrain = terrain != nullptr && terrain->size() == px.size();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::int64_t y2 = 2 * (std::int64_t{d.rect.y} + y) + 1;
            const std::size_t ti = static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                                   static_cast<std::size_t>(x);
            std::int32_t dz = depth::kNone;
            if (has_terrain && (*terrain)[ti] != kNoTerrainModel) {
                dz = depth::at(d.depth, y2, (*terrain)[ti]);
            }
            bool tested = false;
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
                if (!shadow && d.depth_test) {
                    const depth::Rule rule = flags::depth_rule(r.flags);
                    if (rule.cls == depth::Class::None) {
                        tested = false;
                    } else {
                        const std::size_t zi = static_cast<std::size_t>(ly) * f->image->width +
                                               static_cast<std::size_t>(lx);
                        const std::uint8_t z = zi < f->image->z.size() ? f->image->z[zi] : 0;
                        const depth::Draw dd{
                            r.depth, rule, f->header.anchor_y,
                            64 * static_cast<std::int64_t>((r.flags & flags::kHeightMask) >>
                                                           flags::kHeightShift)};
                        const std::int32_t pd = depth::pixel(d.depth, dd, ly, y2, z);
                        if (pd > dz) {
                            continue;
                        }
                        if (rule.writes && !(rule.skip_half && tx[3] == depth::kHalfAlpha)) {
                            dz = pd;
                        }
                        tested = true;
                    }
                }
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
            if (submerged != nullptr && d.depth_test && tested &&
                depth::at(d.depth, y2, d.sea_h64) <= dz) {
                (*submerged)[i] = 1;
            }
        }
    }
}

} // namespace anno::render::test
