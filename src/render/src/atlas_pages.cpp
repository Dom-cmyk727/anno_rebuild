#include <algorithm>
#include <map>
#include <thread>
#include <utility>

#include <anno/render/atlas_pages.hpp>
#include <anno/render/residency.hpp>

namespace anno::render {

namespace {

std::uint32_t round_up_even(std::uint32_t v) {
    return (v % 2 == 0) ? v : v + 1;
}

/// A page texel: 4 bytes at (x, y) of a `stride`-wide buffer.
void put_texel(std::vector<std::uint8_t>& buf, std::uint32_t stride, std::uint32_t x,
               std::uint32_t y, std::array<std::uint8_t, 4> rgba) {
    const std::size_t i = (std::size_t{y} * stride + x) * 4;
    buf[i + 0] = rgba[0];
    buf[i + 1] = rgba[1];
    buf[i + 2] = rgba[2];
    buf[i + 3] = rgba[3];
}

std::array<std::uint8_t, 4> get_texel(const std::vector<std::uint8_t>& buf, std::uint32_t stride,
                                      std::uint32_t x, std::uint32_t y) {
    const std::size_t i = (std::size_t{y} * stride + x) * 4;
    return {buf[i + 0], buf[i + 1], buf[i + 2], buf[i + 3]};
}

/// A frame's own pixel at (fx, fy), clamped to [0, width) x [0, height), premultiplied.
std::array<std::uint8_t, 4> premultiplied_clamped(const assets::Rgba& img, std::int64_t fx,
                                                  std::int64_t fy) {
    const auto cx =
        static_cast<std::uint32_t>(std::clamp<std::int64_t>(fx, 0, std::int64_t{img.width} - 1));
    const auto cy =
        static_cast<std::uint32_t>(std::clamp<std::int64_t>(fy, 0, std::int64_t{img.height} - 1));
    const std::size_t i = (std::size_t{cy} * img.width + cx) * 4;
    return premultiply_pixel(
        {img.pixels[i + 0], img.pixels[i + 1], img.pixels[i + 2], img.pixels[i + 3]});
}

/// Writes `img`'s own content into its slot (frame at slot-local (padding, padding)), then
/// dilates every slot texel outside the frame rect to the nearest frame texel, clamped
/// (RENDER-DIRECT-1b step 2). `slot_x`/`slot_y` are the slot's level-0 origin; `slot_w`/`slot_h`
/// its (already even) size, `>= img.width/height + 2 * padding`.
void write_level0(const assets::Rgba& img, std::vector<std::uint8_t>& level0,
                  std::uint32_t page_size, std::uint32_t slot_x, std::uint32_t slot_y,
                  std::uint32_t slot_w, std::uint32_t slot_h, std::uint32_t padding) {
    for (std::uint32_t y = 0; y < slot_h; ++y) {
        for (std::uint32_t x = 0; x < slot_w; ++x) {
            const std::int64_t fx = std::int64_t{x} - padding;
            const std::int64_t fy = std::int64_t{y} - padding;
            put_texel(level0, page_size, slot_x + x, slot_y + y,
                      premultiplied_clamped(img, fx, fy));
        }
    }
}

/// One mip level over `slot_w` x `slot_h` level-0 texels at (slot_x, slot_y): each output texel
/// is the 2 x 2 box of level 0 with the sum rounded half up, `(s + 2) / 4` per channel
/// (RENDER-DIRECT-1b step 2). `slot_w`/`slot_h` are even (the packer only ever places even-sized
/// slots), so this box never reads a texel of a different frame's slot.
void write_level1(const std::vector<std::uint8_t>& level0, std::uint32_t page_size,
                  std::vector<std::uint8_t>& level1, std::uint32_t mip_stride, std::uint32_t slot_x,
                  std::uint32_t slot_y, std::uint32_t slot_w, std::uint32_t slot_h) {
    for (std::uint32_t my = 0; my < slot_h / 2; ++my) {
        for (std::uint32_t mx = 0; mx < slot_w / 2; ++mx) {
            const std::uint32_t x0 = slot_x + 2 * mx, y0 = slot_y + 2 * my;
            const auto p00 = get_texel(level0, page_size, x0, y0);
            const auto p10 = get_texel(level0, page_size, x0 + 1, y0);
            const auto p01 = get_texel(level0, page_size, x0, y0 + 1);
            const auto p11 = get_texel(level0, page_size, x0 + 1, y0 + 1);
            std::array<std::uint8_t, 4> out{};
            for (std::size_t c = 0; c < 4; ++c) {
                const unsigned s = unsigned{p00[c]} + p10[c] + p01[c] + p11[c];
                out[c] = static_cast<std::uint8_t>((s + 2) / 4);
            }
            put_texel(level1, mip_stride, slot_x / 2 + mx, slot_y / 2 + my, out);
        }
    }
}

} // namespace

AtlasPages build_atlas_pages(std::span<const AtlasFrameInput> frames, std::uint32_t page_size,
                             std::uint32_t padding, std::size_t threads) {
    AtlasPages out;
    out.page_size = page_size;
    out.padding = padding;

    std::map<FrameId, std::shared_ptr<const assets::Rgba>> images;
    std::vector<FrameRef> refs;
    refs.reserve(frames.size());
    for (const auto& f : frames) {
        if (!f.image || f.image->width == 0 || f.image->height == 0) {
            continue; // an empty frame has nothing to place (upstream already skips length 0).
        }
        if (images.contains(f.id)) {
            // A second (or later) occurrence of this id: skipped, not placed again - first wins,
            // named in duplicate_ids (review F3), not silently dropped and not given a second
            // work item (which would race with the first's writer thread for the same slot).
            out.duplicate_ids.push_back(f.id);
            continue;
        }
        images[f.id] = f.image;
        refs.push_back(FrameRef{f.id, round_up_even(f.image->width + 2 * padding),
                                round_up_even(f.image->height + 2 * padding)});
    }

    const std::uint64_t layer_bytes = std::uint64_t{page_size} * page_size * 4;
    // A budget sized for one layer per frame in the worst case (never reached): ensure() never
    // has to evict within this single call, so the page count it reports is the real packing.
    const std::uint64_t budget = layer_bytes * (refs.empty() ? 1 : refs.size());
    AtlasResidency residency(page_size, budget);
    residency.ensure(refs, AtlasClass::Static);

    std::vector<std::pair<FrameId, AtlasPlace>> work;
    work.reserve(refs.size());
    for (const auto& r : refs) {
        if (const auto place = residency.place(r.id)) {
            work.emplace_back(r.id, *place);
        } else {
            out.oversized.push_back(r.id);
        }
    }

    const std::size_t layers = residency.stats().layers;
    const std::uint32_t mip_size = page_size / 2;
    const std::uint64_t mip_layer_bytes = std::uint64_t{mip_size} * mip_size * 4;
    out.level0.assign(layers, std::vector<std::uint8_t>(layer_bytes, 0));
    out.level1.assign(layers, std::vector<std::uint8_t>(mip_layer_bytes, 0));
    out.placements.resize(work.size());

    const std::size_t n = work.size();
    const std::size_t worker_count =
        std::max<std::size_t>(1, std::min(threads, std::max<std::size_t>(n, 1)));
    const std::size_t chunk = n == 0 ? 0 : (n + worker_count - 1) / worker_count;
    std::vector<std::thread> pool;
    for (std::size_t t = 0; t < worker_count; ++t) {
        const std::size_t begin = t * chunk;
        const std::size_t end = std::min(n, begin + chunk);
        if (begin >= end) {
            continue;
        }
        pool.emplace_back([&, begin, end]() {
            for (std::size_t i = begin; i < end; ++i) {
                const auto& [id, place] = work[i];
                const assets::Rgba& img = *images.at(id);
                write_level0(img, out.level0[place.layer], page_size, place.x, place.y, place.width,
                             place.height, padding);
                write_level1(out.level0[place.layer], page_size, out.level1[place.layer], mip_size,
                             place.x, place.y, place.width, place.height);
                // Review F4: the mip rectangle is computed from the slot's own level-0 origin
                // and the frame's level-0 end, each independently floor/ceil-divided by 2 - not
                // from width/height halved on their own (one texel short for an odd size) nor
                // from padding/2 added separately (wrong for an odd padding).
                const std::uint32_t mip_x0 = (place.x + padding) / 2;
                const std::uint32_t mip_x1 = (place.x + padding + img.width + 1) / 2; // ceil
                const std::uint32_t mip_y0 = (place.y + padding) / 2;
                const std::uint32_t mip_y1 = (place.y + padding + img.height + 1) / 2; // ceil
                out.placements[i] = AtlasFramePlacement{id,
                                                        place.layer,
                                                        place.x + padding,
                                                        place.y + padding,
                                                        img.width,
                                                        img.height,
                                                        mip_x0,
                                                        mip_y0,
                                                        mip_x1 - mip_x0,
                                                        mip_y1 - mip_y0};
            }
        });
    }
    for (auto& th : pool) {
        th.join();
    }
    return out;
}

} // namespace anno::render
