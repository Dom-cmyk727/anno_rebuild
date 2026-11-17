// RENDER-DIRECT-1b step 4: the texel-exact proof on synthetic frames (no game files). Every
// claim is checked against the decoder's own output (or a hand-computed expectation from it),
// never against the page itself - a bug that always agrees with itself would otherwise pass.
#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/assets/texture.hpp>
#include <anno/render/atlas_pages.hpp>

namespace render = anno::render;
using Rgba = anno::assets::Rgba;

namespace {

/// A synthetic straight-alpha frame, `w` x `h`, pixel (x, y) = pattern(x, y) (so every texel is
/// distinct and position is recoverable from its value).
std::shared_ptr<const Rgba> make_frame(
    std::uint16_t w, std::uint16_t h,
    const std::function<std::array<std::uint8_t, 4>(std::uint32_t, std::uint32_t)>& pattern) {
    auto img = std::make_shared<Rgba>();
    img->width = w;
    img->height = h;
    img->pixels.resize(std::size_t{w} * h * 4);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            const auto p = pattern(x, y);
            const std::size_t i = (std::size_t{y} * w + x) * 4;
            img->pixels[i + 0] = p[0];
            img->pixels[i + 1] = p[1];
            img->pixels[i + 2] = p[2];
            img->pixels[i + 3] = p[3];
        }
    }
    return img;
}

std::array<std::uint8_t, 4> get_texel(const std::vector<std::uint8_t>& page, std::uint32_t stride,
                                      std::uint32_t x, std::uint32_t y) {
    const std::size_t i = (std::size_t{y} * stride + x) * 4;
    return {page[i + 0], page[i + 1], page[i + 2], page[i + 3]};
}

/// Hashes every page (level 0 and the one mip level) of `pages` into one value, byte order and
/// layer order both fixed, so it does not depend on anything but the bytes themselves.
std::uint64_t hash_pages(const render::AtlasPages& pages) {
    std::uint64_t h = 1469598103934665603ULL; // FNV-1a 64-bit offset basis.
    auto mix = [&h](std::uint8_t b) {
        h ^= b;
        h *= 1099511628211ULL;
    };
    for (const auto& layer : pages.level0) {
        for (const std::uint8_t b : layer) {
            mix(b);
        }
    }
    for (const auto& layer : pages.level1) {
        for (const std::uint8_t b : layer) {
            mix(b);
        }
    }
    return h;
}

const render::AtlasFramePlacement& placement_of(const render::AtlasPages& pages,
                                                render::FrameId id) {
    for (const auto& p : pages.placements) {
        if (p.id == id) {
            return p;
        }
    }
    FAIL("no placement for frame ", id);
    static const render::AtlasFramePlacement none;
    return none;
}

} // namespace

TEST_CASE("build_atlas_pages: every frame's own rectangle equals the decoder, premultiplied") {
    // A 1 x 1 frame, odd-sized frames, and edges at alpha 0 and alpha 255 (RENDER-DIRECT-1b step
    // 4's own required cases), each with a position-recoverable pattern.
    auto one_by_one = make_frame(1, 1, [](std::uint32_t, std::uint32_t) {
        return std::array<std::uint8_t, 4>{10, 20, 30, 128};
    });
    auto odd = make_frame(5, 3, [](std::uint32_t x, std::uint32_t y) {
        return std::array<std::uint8_t, 4>{
            static_cast<std::uint8_t>(x * 10 + 1), static_cast<std::uint8_t>(y * 10 + 2),
            static_cast<std::uint8_t>(x + y), static_cast<std::uint8_t>(40 + x * y)};
    });
    auto alpha_edges = make_frame(4, 4, [](std::uint32_t x, std::uint32_t y) {
        const std::uint8_t a = (x == 0 || y == 0) ? 0 : (x == 3 || y == 3) ? 255 : 128;
        return std::array<std::uint8_t, 4>{static_cast<std::uint8_t>(50 + x),
                                           static_cast<std::uint8_t>(60 + y), 70, a};
    });

    const std::vector<render::AtlasFrameInput> frames = {
        {1, one_by_one}, {2, odd}, {3, alpha_edges}};
    const auto pages = render::build_atlas_pages(frames, 64, 2, 1);
    REQUIRE(pages.oversized.empty());

    for (const auto& [id, img] :
         std::vector<std::pair<render::FrameId, std::shared_ptr<const Rgba>>>{
             {1, one_by_one}, {2, odd}, {3, alpha_edges}}) {
        const auto& place = placement_of(pages, id);
        CHECK(place.width == img->width);
        CHECK(place.height == img->height);
        const auto& layer = pages.level0[place.layer];
        for (std::uint32_t y = 0; y < img->height; ++y) {
            for (std::uint32_t x = 0; x < img->width; ++x) {
                const std::size_t si = (std::size_t{y} * img->width + x) * 4;
                const auto expected =
                    render::premultiply_pixel({img->pixels[si + 0], img->pixels[si + 1],
                                               img->pixels[si + 2], img->pixels[si + 3]});
                const auto got = get_texel(layer, pages.page_size, place.x + x, place.y + y);
                CHECK(got == expected);
            }
        }
    }
}

TEST_CASE("build_atlas_pages: every padding texel is the clamp-to-edge rule") {
    auto frame = make_frame(3, 2, [](std::uint32_t x, std::uint32_t y) {
        return std::array<std::uint8_t, 4>{static_cast<std::uint8_t>(10 + x),
                                           static_cast<std::uint8_t>(20 + y), 5, 200};
    });
    const std::vector<render::AtlasFrameInput> frames = {{1, frame}};
    constexpr std::uint32_t kPadding = 2;
    const auto pages = render::build_atlas_pages(frames, 64, kPadding, 1);
    const auto& place = placement_of(pages, 1);
    const auto& layer = pages.level0[place.layer];
    const std::uint32_t slot_x = place.x - kPadding, slot_y = place.y - kPadding;
    const std::uint32_t slot_w = place.width + 2 * kPadding, slot_h = place.height + 2 * kPadding;
    // The reported place.width/height already include the padding round-up; recompute the
    // slot bounds directly from the frame's own placement instead of guessing the round-up.
    for (std::uint32_t y = 0; y < slot_h; ++y) {
        for (std::uint32_t x = 0; x < slot_w; ++x) {
            const bool inside_frame = x >= kPadding && x < kPadding + frame->width &&
                                      y >= kPadding && y < kPadding + frame->height;
            if (inside_frame) {
                continue; // checked by the rectangle test above.
            }
            const auto fx = static_cast<std::uint32_t>(
                std::clamp<std::int64_t>(std::int64_t{x} - kPadding, 0, frame->width - 1));
            const auto fy = static_cast<std::uint32_t>(
                std::clamp<std::int64_t>(std::int64_t{y} - kPadding, 0, frame->height - 1));
            const std::size_t si = (std::size_t{fy} * frame->width + fx) * 4;
            const auto expected =
                render::premultiply_pixel({frame->pixels[si + 0], frame->pixels[si + 1],
                                           frame->pixels[si + 2], frame->pixels[si + 3]});
            const auto got = get_texel(layer, pages.page_size, slot_x + x, slot_y + y);
            CHECK(got == expected);
        }
    }
}

TEST_CASE("build_atlas_pages: every mip texel is the 2x2 box of level 0, rounded half up") {
    auto frame = make_frame(6, 4, [](std::uint32_t x, std::uint32_t y) {
        return std::array<std::uint8_t, 4>{static_cast<std::uint8_t>(x * 17 + 3),
                                           static_cast<std::uint8_t>(y * 23 + 1),
                                           static_cast<std::uint8_t>(x ^ y), 180};
    });
    const std::vector<render::AtlasFrameInput> frames = {{1, frame}};
    const auto pages = render::build_atlas_pages(frames, 64, 2, 1);
    const auto& place = placement_of(pages, 1);
    const auto& l0 = pages.level0[place.layer];
    const auto& l1 = pages.level1[place.layer];
    const std::uint32_t slot_x = place.x - pages.padding, slot_y = place.y - pages.padding;
    const std::uint32_t slot_w = place.width + 2 * pages.padding;
    const std::uint32_t slot_h = place.height + 2 * pages.padding;
    REQUIRE(slot_w % 2 == 0);
    REQUIRE(slot_h % 2 == 0);
    for (std::uint32_t my = 0; my < slot_h / 2; ++my) {
        for (std::uint32_t mx = 0; mx < slot_w / 2; ++mx) {
            const std::uint32_t x0 = slot_x + 2 * mx, y0 = slot_y + 2 * my;
            const auto p00 = get_texel(l0, pages.page_size, x0, y0);
            const auto p10 = get_texel(l0, pages.page_size, x0 + 1, y0);
            const auto p01 = get_texel(l0, pages.page_size, x0, y0 + 1);
            const auto p11 = get_texel(l0, pages.page_size, x0 + 1, y0 + 1);
            std::array<std::uint8_t, 4> expected{};
            for (std::size_t c = 0; c < 4; ++c) {
                const unsigned s = unsigned{p00[c]} + p10[c] + p01[c] + p11[c];
                expected[c] = static_cast<std::uint8_t>((s + 2) / 4);
            }
            const auto got = get_texel(l1, pages.page_size / 2, slot_x / 2 + mx, slot_y / 2 + my);
            CHECK(got == expected);
        }
    }
}

TEST_CASE("build_atlas_pages: texels outside every slot are 0, and a frame exactly filling a "
          "page after padding places cleanly") {
    // A page of 8 holds one frame of 8 - 2*padding = 4 exactly, with nothing left over.
    auto frame = make_frame(4, 4, [](std::uint32_t x, std::uint32_t y) {
        return std::array<std::uint8_t, 4>{static_cast<std::uint8_t>(1 + x),
                                           static_cast<std::uint8_t>(1 + y), 1, 255};
    });
    const std::vector<render::AtlasFrameInput> frames = {{1, frame}};
    const auto pages = render::build_atlas_pages(frames, 8, 2, 1);
    REQUIRE(pages.oversized.empty());
    REQUIRE(pages.level0.size() == 1);
    const auto& place = placement_of(pages, 1);
    CHECK(place.width == 4);
    CHECK(place.height == 4);

    // A second, separate scene: a tiny frame on a much larger page leaves everything else 0.
    auto small = make_frame(2, 2, [](std::uint32_t, std::uint32_t) {
        return std::array<std::uint8_t, 4>{9, 9, 9, 255};
    });
    const std::vector<render::AtlasFrameInput> small_frames = {{1, small}};
    const auto small_pages = render::build_atlas_pages(small_frames, 64, 2, 1);
    const auto& small_place = placement_of(small_pages, 1);
    const std::uint32_t slot_x = small_place.x - small_pages.padding;
    const std::uint32_t slot_y = small_place.y - small_pages.padding;
    const std::uint32_t slot_w = small_place.width + 2 * small_pages.padding;
    const std::uint32_t slot_h = small_place.height + 2 * small_pages.padding;
    std::uint64_t nonzero_outside = 0;
    const auto& layer = small_pages.level0[small_place.layer];
    for (std::uint32_t y = 0; y < small_pages.page_size; ++y) {
        for (std::uint32_t x = 0; x < small_pages.page_size; ++x) {
            const bool inside_slot =
                x >= slot_x && x < slot_x + slot_w && y >= slot_y && y < slot_y + slot_h;
            if (inside_slot) {
                continue;
            }
            const auto t = get_texel(layer, small_pages.page_size, x, y);
            if (t != std::array<std::uint8_t, 4>{0, 0, 0, 0}) {
                ++nonzero_outside;
            }
        }
    }
    CHECK(nonzero_outside == 0);
}

TEST_CASE("build_atlas_pages: a frame too large for one page even alone is named, not dropped "
          "silently") {
    auto huge = make_frame(100, 100, [](std::uint32_t, std::uint32_t) {
        return std::array<std::uint8_t, 4>{1, 2, 3, 255};
    });
    const std::vector<render::AtlasFrameInput> frames = {{1, huge}};
    const auto pages = render::build_atlas_pages(frames, 64, 2, 1);
    CHECK(pages.oversized == std::vector<render::FrameId>{1});
    CHECK(pages.placements.empty());
}

TEST_CASE("build_atlas_pages: the pages do not depend on the thread count (RENDER-DIRECT-1b "
          "step 3)") {
    std::vector<render::AtlasFrameInput> frames;
    std::uint32_t seed = 12345;
    auto next = [&] {
        seed = seed * 1664525U + 1013904223U;
        return seed;
    };
    for (render::FrameId id = 0; id < 200; ++id) {
        const auto w = static_cast<std::uint16_t>(2 + next() % 29);
        const auto h = static_cast<std::uint16_t>(2 + next() % 29);
        frames.push_back({id, make_frame(w, h, [&next](std::uint32_t, std::uint32_t) {
                              const auto v = next();
                              return std::array<std::uint8_t, 4>{
                                  static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(v >> 8),
                                  static_cast<std::uint8_t>(v >> 16),
                                  static_cast<std::uint8_t>(v >> 24)};
                          })});
    }
    const auto one = render::build_atlas_pages(frames, 128, 2, 1);
    const auto many = render::build_atlas_pages(frames, 128, 2, 8);
    REQUIRE(one.level0.size() == many.level0.size());
    CHECK(hash_pages(one) == hash_pages(many));
}
