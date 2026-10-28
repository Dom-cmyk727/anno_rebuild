// Probe-only ground canvases and cropped fall masks (no game files).
// A ground probe filled with store_pixels false matches the probe filled
// with the RGBA buffers kept, and the omitted canvas has no pixels.
// apply_fall_masks on a mask cropped to the opaque pixels plus the blur
// margin matches the same mask on a full-field canvas, including an empty
// mask, a sprite on the field edge, and two sprites whose blur overlaps.

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

#include <doctest/doctest.h>

#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/bytes.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/scene.hpp>

namespace {

using anno::formats::ByteBuffer;

void append(ByteBuffer& out, const ByteBuffer& b) {
    out.insert(out.end(), b.begin(), b.end());
}

void put_u16(ByteBuffer& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
}

void put_u32(ByteBuffer& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
    }
}

ByteBuffer rec(std::string_view name, const ByteBuffer& payload) {
    ByteBuffer out(12, 0);
    std::memcpy(out.data(), name.data(), name.size());
    put_u32(out, static_cast<std::uint32_t>(payload.size()));
    append(out, payload);
    return out;
}

/// `Island` records are spans into `bytes` (`formats::Bytes`). The buffer has
/// to outlive every `Scene` built from `island`.
struct OwnedIsland {
    ByteBuffer bytes;
    anno::formats::insel::Island island;
};

OwnedIsland island_at(std::uint16_t x, std::uint16_t y) {
    ByteBuffer gfx;
    for (int i = 0; i < 3; ++i) {
        put_u16(gfx, 0);
    }
    const ByteBuffer heights(3, 4);
    ByteBuffer payload;
    for (const std::uint16_t v : std::array<std::uint16_t, 4>{x, y, 3, 1}) {
        put_u16(payload, v);
    }
    put_u32(payload, 0);
    append(payload, rec("HEIGHT", anno::formats::insel::encode_blocks(heights, 3, 1, 1)[0]));
    append(payload, rec("GFXID", anno::formats::insel::encode_blocks(gfx, 3, 1, 2)[0]));
    auto island = anno::formats::insel::Island::from_insel5(payload);
    REQUIRE(island);
    // Moving the vector keeps its heap buffer, so the spans stay valid.
    OwnedIsland owned;
    owned.bytes = std::move(payload);
    owned.island = std::move(island).value();
    return owned;
}

anno::render::Scene scene_of(std::vector<anno::formats::insel::Island> islands) {
    return anno::render::Scene("", std::move(islands), anno::assets::island::GroundTextures{},
                               anno::formats::anlagen::AnlagenFile{}, anno::render::kNoLift);
}

void check_probe(const anno::render::sea::SceneProbe& a, const anno::render::sea::SceneProbe& b) {
    CHECK(a.island == b.island);
    REQUIRE(a.pos.size() == b.pos.size());
    for (std::size_t i = 0; i < a.pos.size(); ++i) {
        CHECK(a.pos[i][0] == b.pos[i][0]);
        CHECK(a.pos[i][1] == b.pos[i][1]);
    }
}

void check_omitted(const anno::render::Scene& scene, anno::render::Rect rect) {
    const auto kept = scene.ground_with_probe(rect, 2, true);
    const auto omitted = scene.ground_with_probe(rect, 2, false);
    CHECK(omitted.first.pixels.empty());
    CHECK(omitted.first.width == kept.first.width);
    CHECK(omitted.first.height == kept.first.height);
    CHECK(omitted.first.origin == kept.first.origin);
    if (rect.w > 0 && rect.h > 0) {
        CHECK(kept.first.pixels.size() ==
              static_cast<std::size_t>(rect.w) * static_cast<std::size_t>(rect.h) * 4);
    }
    check_probe(kept.second, omitted.second);
}

anno::render::sea::WaterField field_of(anno::render::Rect rect) {
    anno::render::sea::WaterField f;
    f.rect = rect;
    const auto n = static_cast<std::size_t>(std::max(rect.w, 0) * std::max(rect.h, 0));
    f.cover.assign(n, 0);
    f.shore.assign(n, 0);
    f.depth.assign(n, 40);
    f.flags.assign(n, 0);
    return f;
}

anno::render::Canvas canvas_of(anno::render::Rect rect) {
    anno::render::Canvas c;
    c.origin = {rect.x, rect.y};
    c.width = static_cast<std::size_t>(std::max(rect.w, 0));
    c.height = static_cast<std::size_t>(std::max(rect.h, 0));
    c.pixels.assign(c.width * c.height * 4, 0);
    return c;
}

void paint(anno::render::Canvas& c, int x, int y) {
    const int lx = x - c.origin.first;
    const int ly = y - c.origin.second;
    REQUIRE(lx >= 0);
    REQUIRE(ly >= 0);
    REQUIRE(static_cast<std::size_t>(lx) < c.width);
    REQUIRE(static_cast<std::size_t>(ly) < c.height);
    auto* p = c.pixels.data() +
              (static_cast<std::size_t>(ly) * c.width + static_cast<std::size_t>(lx)) * 4;
    p[0] = 10;
    p[1] = 20;
    p[2] = 30;
    p[3] = 255;
}

/// Opaque pixels grown by the blur margin and clipped to `field`, which is
/// the window mask_from keeps (scene.cpp).
anno::render::Canvas crop(const anno::render::Canvas& full, anno::render::Rect field, int margin) {
    int x0 = static_cast<int>(full.width);
    int y0 = static_cast<int>(full.height);
    int x1 = -1;
    int y1 = -1;
    for (int y = 0; y < static_cast<int>(full.height); ++y) {
        for (int x = 0; x < static_cast<int>(full.width); ++x) {
            const auto index =
                (static_cast<std::size_t>(y) * full.width + static_cast<std::size_t>(x)) * 4 + 3;
            if (full.pixels[index] == 0) {
                continue;
            }
            x0 = std::min(x0, x);
            x1 = std::max(x1, x);
            y0 = std::min(y0, y);
            y1 = std::max(y1, y);
        }
    }
    REQUIRE(x1 >= 0);
    const anno::render::Rect grown{full.origin.first + x0 - margin,
                                   full.origin.second + y0 - margin, (x1 - x0 + 1) + 2 * margin,
                                   (y1 - y0 + 1) + 2 * margin};
    const auto clipped = grown.intersect(field);
    REQUIRE(clipped);
    anno::render::Canvas out = canvas_of(*clipped);
    for (int y = 0; y < clipped->h; ++y) {
        for (int x = 0; x < clipped->w; ++x) {
            const int sx = clipped->x + x;
            const int sy = clipped->y + y;
            const int fx = sx - full.origin.first;
            const int fy = sy - full.origin.second;
            if (fx < 0 || fy < 0 || static_cast<std::size_t>(fx) >= full.width ||
                static_cast<std::size_t>(fy) >= full.height) {
                continue;
            }
            const auto* src =
                full.pixels.data() +
                (static_cast<std::size_t>(fy) * full.width + static_cast<std::size_t>(fx)) * 4;
            auto* dst = out.pixels.data() +
                        (static_cast<std::size_t>(y) * out.width + static_cast<std::size_t>(x)) * 4;
            std::memcpy(dst, src, 4);
        }
    }
    return out;
}

bool opaque(const anno::render::Canvas& c) {
    for (std::size_t i = 3; i < c.pixels.size(); i += 4) {
        if (c.pixels[i] != 0) {
            return true;
        }
    }
    return false;
}

void expect_same_masks(const anno::render::Scene& scene, anno::render::sea::WaterField base,
                       const anno::render::Canvas& falls, const anno::render::Canvas& mouths,
                       const anno::render::Canvas& beds, std::size_t group) {
    const int radius = std::max(1, anno::render::kTile[group].h / 10);
    const int margin = 2 * radius + 1;
    auto window = [&](const anno::render::Canvas& c) {
        return opaque(c) ? crop(c, base.rect, margin) : c;
    };
    auto full = base;
    auto cropped = base;
    scene.apply_fall_masks(full, falls, mouths, beds, group);
    scene.apply_fall_masks(cropped, window(falls), window(mouths), window(beds), group);
    CHECK(full.cover == cropped.cover);
    CHECK(full.shore == cropped.shore);
    CHECK(full.depth == cropped.depth);
    CHECK(full.flags == cropped.flags);
    CHECK(full.flags != base.flags);
}

} // namespace

TEST_CASE("omitted ground pixels keep the probe") {
    const auto left = island_at(0, 0);
    const auto right = island_at(4, 0);
    const auto scene = scene_of({left.island, right.island});
    const auto bounds = scene.bounds(2);
    REQUIRE(bounds);
    check_omitted(scene, *bounds);
    const anno::render::Rect edge{bounds->x + bounds->w / 2, bounds->y, std::max(bounds->w / 2, 1),
                                  std::max(bounds->h, 1)};
    check_omitted(scene, edge);
    check_omitted(scene, anno::render::Rect{bounds->x - 50, bounds->y - 50, 4, 4});
    check_omitted(scene, anno::render::Rect{0, 0, 0, 0});

    const auto kept = scene.ground_with_probe(*bounds, 2, true);
    bool painted = false;
    bool probed = false;
    for (std::uint8_t a : kept.first.pixels) {
        if (a != 0) {
            painted = true;
            break;
        }
    }
    for (const auto id : kept.second.island) {
        if (id != anno::render::sea::kNoIsland) {
            probed = true;
            break;
        }
    }
    CHECK(painted);
    CHECK(probed);
}

TEST_CASE("cropped fall masks match the full field") {
    const auto scene =
        anno::render::Scene("", {}, anno::assets::island::GroundTextures{},
                            anno::formats::anlagen::AnlagenFile{}, anno::render::kNoLift);
    const anno::render::Rect rect{0, 0, 32, 24};
    auto falls = canvas_of(rect);
    auto mouths = canvas_of(rect);
    auto beds = canvas_of(rect);
    paint(falls, 8, 8);
    paint(falls, 10, 8);
    paint(mouths, 18, 12);
    paint(beds, 8, 9);
    expect_same_masks(scene, field_of(rect), falls, mouths, beds, 2);
    expect_same_masks(scene, field_of(rect), falls, mouths, beds, 0);

    const anno::render::Rect shifted{-6, -3, 20, 16};
    auto edge_falls = canvas_of(shifted);
    auto edge_beds = canvas_of(shifted);
    paint(edge_falls, shifted.x, shifted.y);
    paint(edge_beds, shifted.x + shifted.w - 1, shifted.y + shifted.h - 1);
    expect_same_masks(scene, field_of(shifted), edge_falls, canvas_of(shifted), edge_beds, 2);

    auto base = field_of(rect);
    const auto before = base.flags;
    anno::render::Canvas empty;
    scene.apply_fall_masks(base, empty, empty, empty, 2);
    CHECK(base.flags == before);
    scene.apply_fall_masks(base, canvas_of(rect), canvas_of(rect), canvas_of(rect), 2);
    CHECK(base.flags == before);
}
