// Synthetic tests of the chunk draw list, the frame table and the replay
// (docs/render/gpu-chunks.md 3, 4.1, 7.1, 7.3): hand-made frames, no game files.
#include <algorithm>
#include <cstdint>
#include <memory>
#include <set>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/render/chunk_draws.hpp>
#include <anno/render/frame_table.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/scene.hpp>

#include "chunk_draws_model.hpp"

namespace render = anno::render;
namespace fig = anno::formats::figuren;
using render::Canvas;
using render::ChunkDraws;
using render::ChunkDrawsBuilder;
using render::FrameId;
using render::FrameKey;
using render::FrameTable;
using render::ObjectRecord;
using render::Px;
using render::Rect;
using render::Rgba;
using render::UnitFrameId;
namespace flags = render::object_flags;

namespace {

fig::Frame header(std::uint16_t w, std::uint16_t h, std::int16_t ax, std::int16_t ay) {
    fig::Frame f;
    f.width = w;
    f.height = h;
    f.anchor_x = ax;
    f.anchor_y = ay;
    return f;
}

template <class F> std::shared_ptr<const Rgba> image(std::uint16_t w, std::uint16_t h, F&& px) {
    auto img = std::make_shared<Rgba>();
    img->width = w;
    img->height = h;
    img->pixels.resize(std::size_t{w} * h * 4);
    for (std::uint16_t y = 0; y < h; ++y) {
        for (std::uint16_t x = 0; x < w; ++x) {
            const Px p = px(x, y);
            for (std::size_t c = 0; c < p.size(); ++c) {
                img->pixels[(std::size_t{y} * w + x) * 4 + c] = p[c];
            }
        }
    }
    return img;
}

FrameId add(FrameTable& t, std::uint32_t id, const fig::Frame& h,
            const std::shared_ptr<const Rgba>& img) {
    const auto got =
        t.intern(FrameKey::building({id, 0, false, 0, 0}), [&] { return std::pair{h, img}; });
    REQUIRE(got.has_value());
    return *got;
}

/// The frames of the synthetic scene (zoom group 2: base band 6).
struct Frames {
    FrameTable table;
    FrameId block, glass, shadow, checker, wide, unit, dot, ghost;
    Frames() {
        // Opaque block, anchor at the bottom centre.
        block = add(table, 1, header(20, 12, 10, 11),
                    image(20, 12, [](int, int) { return Px{200, 60, 30, 255}; }));
        // Alpha levels 64 / 128 / 191 / 255 in column groups of four.
        glass = add(table, 2, header(16, 16, 8, 15), image(16, 16, [](int x, int) {
                        constexpr std::uint8_t kLevels[4] = {64, 128, 191, 255};
                        return Px{30, 200, 90, kLevels[x / 4]};
                    }));
        // A shadow mask: only its alpha matters.
        shadow = add(table, 3, header(24, 8, 12, 4),
                     image(24, 8, [](int, int) { return Px{0, 0, 0, 255}; }));
        // The decoded WZ pattern: alpha 0 / 255 alternating per column, the phase
        // toggling every two rows.
        checker = add(table, 4, header(14, 10, 7, 9), image(14, 10, [](int x, int y) {
                          return ((x + y / 2) % 2 == 0) ? Px{240, 240, 40, 255} : Px{0, 0, 0, 0};
                      }));
        // Negative anchors (the box lies right of and below the anchor) and an
        // image wider than its frame box (12 columns for a frame of 8).
        wide = add(table, 5, header(8, 6, -3, -2), image(12, 6, [](int x, int) {
                       return Px{20, 20, 220, static_cast<std::uint8_t>(x < 8 ? 255 : 128)};
                   }));
        unit = add(table, 6, header(6, 10, 3, 9),
                   image(6, 10, [](int, int) { return Px{255, 255, 255, 255}; }));
        dot = add(table, 7, header(1, 1, 0, 0),
                  image(1, 1, [](int, int) { return Px{10, 20, 30, 255}; }));
        ghost = add(table, 8, header(1, 1, 0, 0),
                    image(1, 1, [](int, int) { return Px{200, 100, 50, 128}; }));
    }
};

constexpr int kBand = 6; // base_band of zoom group 2: 10 * 6 / 10

ObjectRecord rec(FrameId f, int x, int y, std::uint32_t fl) {
    ObjectRecord r;
    r.frame = f;
    r.x = x;
    r.y = y;
    r.flags = fl;
    return r;
}

/// Two overlapping islands and an unclipped tail over a 96 x 64 chunk.
ChunkDraws build(const Frames& fr, std::uint32_t bin_px, bool tail = true) {
    constexpr std::uint32_t kTrack = flags::kTrackBase;
    ChunkDrawsBuilder b(Rect{0, 0, 96, 64}, 2, true, kBand, bin_px);
    // Island A: x 0..59.
    b.begin_island(Rect{0, 0, 60, 64});
    b.add(rec(fr.shadow, 6, 26, flags::kShadow));
    b.add(rec(fr.block, 4, 20, kTrack));
    b.add(rec(fr.glass, 10, 24, kTrack)); // translucent over the block
    b.add(rec(fr.checker, 30, 10, kTrack));
    for (int i = 0; i < 3; ++i) {
        b.add(rec(fr.dot, 50, 50, flags::kShadow)); // compounding shadows
    }
    b.add(rec(fr.ghost, 50, 50, kTrack)); // translucent over three shadows
    b.add(rec(fr.wide, 36, 40, kTrack));
    // Island B: x 40..95, drawn after A. Its shadow over A's checker sprite is
    // the shadow-only pixel that resets the base plane.
    b.begin_island(Rect{40, 0, 56, 64});
    b.add(rec(fr.shadow, 38, 8, flags::kShadow));
    b.add(rec(fr.unit, 60, 20, kTrack));
    b.add(rec(fr.block, 30, 20, kTrack)); // half outside B's clip
    if (tail) {
        // Not clipped to an island: straight onto the chunk canvas.
        b.begin_unclipped();
        b.add(rec(fr.shadow, 70, 50, flags::kShadow));
        b.add(rec(fr.glass, 74, 50, kTrack));
        b.add(rec(fr.block, 2, 2, kTrack));
    }
    return b.finish(fr.table);
}

Px at(const Canvas& c, int x, int y) {
    const std::size_t i = (static_cast<std::size_t>(y - c.origin.second) * c.width +
                           static_cast<std::size_t>(x - c.origin.first)) *
                          4;
    return {c.pixels[i], c.pixels[i + 1], c.pixels[i + 2], c.pixels[i + 3]};
}

std::uint8_t base_at(const Canvas& c, int x, int y) {
    return c.base[static_cast<std::size_t>(y - c.origin.second) * c.width +
                  static_cast<std::size_t>(x - c.origin.first)];
}

/// The reference: the old algorithm of Scene::render (a local canvas per island,
/// blit_sprite, composite), written out over the records, with blit_sprite
/// computing its own keel.
Canvas reference(const ChunkDraws& d, const FrameTable& t) {
    Canvas canvas = render::blank(d.rect);
    if (d.bases) {
        canvas.base.assign(canvas.width * canvas.height, 255);
        canvas.base_band = d.base_band;
    }
    const auto all = [](int, int, Px p) -> std::optional<Px> { return p; };
    const std::size_t n = d.object_count();
    for (std::size_t i = 0; i < n;) {
        const ObjectRecord r = d.object(i);
        if (r.clip == render::kClipNone) {
            const auto f = t.get(r.frame);
            render::blit_sprite(canvas, *f->image, f->header,
                                {r.x + f->header.anchor_x, r.y + f->header.anchor_y},
                                (r.flags & flags::kShadow) != 0, all);
            ++i;
            continue;
        }
        Canvas local = render::blank(d.clips[r.clip]);
        if (d.bases) {
            local.base.assign(local.width * local.height, 255);
            local.base_band = d.base_band;
        }
        for (; i < n && d.object(i).clip == r.clip; ++i) {
            const ObjectRecord q = d.object(i);
            const auto f = t.get(q.frame);
            render::blit_sprite(local, *f->image, f->header,
                                {q.x + f->header.anchor_x, q.y + f->header.anchor_y},
                                (q.flags & flags::kShadow) != 0, all);
        }
        render::composite(canvas, local, {0, 0});
    }
    return canvas;
}

} // namespace

TEST_CASE("object record: 32 bytes, little-endian, the fields at their offsets") {
    ObjectRecord r;
    r.x = -5;
    r.y = 7;
    r.frame = 0x01020304U;
    r.flags = 0xA503U;
    r.clip = 9;
    r.colour = 0x11223344U;
    r.depth = -1;
    std::vector<std::uint8_t> b;
    render::append_object_record(b, r);
    const std::vector<std::uint8_t> want = {
        0xFB, 0xFF, 0xFF, 0xFF, // x
        0x07, 0x00, 0x00, 0x00, // y
        0x04, 0x03, 0x02, 0x01, // frame
        0x03, 0xA5, 0x00, 0x00, // flags
        0x09, 0x00, 0x00, 0x00, // clip
        0x44, 0x33, 0x22, 0x11, // colour
        0xFF, 0xFF, 0xFF, 0xFF, // depth
        0x00, 0x00, 0x00, 0x00, // pad
    };
    CHECK(render::kObjectRecordBytes == 32);
    CHECK(b == want);
    CHECK(render::read_object_record(b, 0) == r);
    // Records are read at multiples of 32.
    render::append_object_record(b, ObjectRecord{1, 2, 3, 4, 5, 6, 7});
    CHECK(b.size() == 64);
    CHECK(render::read_object_record(b, 1) == ObjectRecord{1, 2, 3, 4, 5, 6, 7});
}

TEST_CASE("ground record and clip table: the layout of gpu-chunks.md 3.2 and 3.3") {
    render::GroundRecord g;
    g.ox = -1;
    g.oy = 2;
    g.texture = 146;
    g.cell = 0x123;
    g.h_a = 10;
    g.h_c = 20;
    g.h_b = 30;
    g.h_d = 40;
    g.clip = 3;
    std::vector<std::uint8_t> b;
    render::append_ground_record(b, g);
    const std::vector<std::uint8_t> want = {
        0xFF, 0xFF, 0xFF, 0xFF, // ox
        0x02, 0x00, 0x00, 0x00, // oy
        0x92, 0x00, 0x00, 0x00, // texture
        0x23, 0x01, 0x00, 0x00, // cell
        0x0A, 0x00, 0x14, 0x00, // h01: A, C
        0x1E, 0x00, 0x28, 0x00, // h23: B, D
        0x03, 0x00, 0x00, 0x00, // clip
        0x00, 0x00, 0x00, 0x00, // pad
    };
    CHECK(b == want);
    CHECK(render::read_ground_record(b, 0) == g);
    const std::vector<Rect> clips = {{-2, 3, 10, 4}};
    const std::vector<std::uint8_t> cb = render::clip_table_bytes(clips);
    const std::vector<std::uint8_t> cwant = {0xFE, 0xFF, 0xFF, 0xFF, 0x03, 0x00, 0x00, 0x00,
                                             0x08, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00};
    CHECK(cb == cwant);
}

TEST_CASE("frame table: dense ids, one entry per key, keel rows, undecodable frames left out") {
    FrameTable t;
    const auto img = image(4, 3, [](int x, int y) {
        // Column 0 opaque in rows 0-2, column 1 in row 0 only, column 2 never,
        // column 3 in row 1.
        const bool on = x == 0 || (x == 1 && y == 0) || (x == 3 && y == 1);
        return on ? Px{1, 2, 3, 255} : Px{0, 0, 0, 0};
    });
    int made = 0;
    auto make = [&] {
        ++made;
        return std::pair{header(4, 3, 1, 2), img};
    };
    const auto a = t.intern(FrameKey::building({7, 0, false, 1, 0}), make);
    const auto a2 = t.intern(FrameKey::building({7, 0, false, 1, 0}), make);
    const auto b = t.intern(FrameKey::building({7, 0, true, 1, 0}), make);
    const auto u = t.intern(FrameKey::unit({true, 3, 0, UnitFrameId::kNoColour}, 1, false), make);
    REQUIRE((a && a2 && b && u));
    CHECK(*a == 0);
    CHECK(*a2 == 0);
    CHECK(*b == 1);
    CHECK(*u == 2);
    CHECK(made == 3); // the second request of a key does not decode
    CHECK(t.size() == 3);
    const auto f = t.get(*a);
    REQUIRE(f != nullptr);
    CHECK(f->keel == std::vector<std::uint16_t>{2, 0, 0, 1});
    CHECK(f->header.anchor_x == 1);
    CHECK(f->width() == 4);
    CHECK(t.get(99) == nullptr);
    // A frame with no image is not entered: no id, no gap.
    const auto none = t.intern(FrameKey::building({8, 0, false, 1, 0}), [] {
        return std::pair{header(1, 1, 0, 0), std::shared_ptr<const Rgba>()};
    });
    CHECK(!none.has_value());
    CHECK(t.size() == 3);
    const auto c = t.intern(FrameKey::building({9, 0, false, 1, 0}), make);
    CHECK(*c == 3);
}

TEST_CASE("frame table: units with the same frame identity share one id") {
    FrameTable t;
    const auto img = image(2, 2, [](int, int) { return Px{1, 2, 3, 255}; });
    int made = 0;
    auto make = [&] {
        ++made;
        return std::pair{header(2, 2, 0, 0), img};
    };
    const UnitFrameId red{true, 12, 3, 0xAA0000U};
    const UnitFrameId blue{true, 12, 3, 0x0000AAU};
    const UnitFrameId next{true, 12, 4, 0xAA0000U};
    const auto a = t.intern(FrameKey::unit(red, 1, false), make);
    const auto b = t.intern(FrameKey::unit(red, 1, false), make);
    REQUIRE((a && b));
    CHECK(*a == *b); // two identical soldiers
    CHECK(made == 1);
    // Colour, frame index, group and shadow each make a different frame.
    CHECK(*t.intern(FrameKey::unit(blue, 1, false), make) != *a);
    CHECK(*t.intern(FrameKey::unit(next, 1, false), make) != *a);
    CHECK(*t.intern(FrameKey::unit(red, 2, false), make) != *a);
    CHECK(*t.intern(FrameKey::unit(red, 1, true), make) != *a);
    CHECK(t.size() == 5);
    // Without an identity the decoded image is the identity.
    const auto p = t.intern(FrameKey::unit_image(img.get(), 1, false), make);
    const auto q = t.intern(FrameKey::unit_image(img.get(), 1, false), make);
    REQUIRE((p && q));
    CHECK(*p == *q);
    CHECK(t.size() == 6);
}

TEST_CASE("frame table: threads interning the same keys agree on the ids") {
    FrameTable t;
    constexpr int kKeys = 64, kThreads = 6;
    std::vector<std::vector<FrameId>> seen(kThreads, std::vector<FrameId>(kKeys));
    {
        std::vector<std::jthread> pool;
        for (int th = 0; th < kThreads; ++th) {
            pool.emplace_back([&, th] {
                for (int i = 0; i < kKeys; ++i) {
                    // Every thread walks the keys in its own order.
                    const int k = (i * (2 * th + 1) + th) % kKeys;
                    const auto id = t.intern(
                        FrameKey::building({static_cast<std::uint32_t>(k), 0, false, 0, 0}), [&] {
                            return std::pair{header(2, 2, 0, 0), image(2, 2, [](int, int) {
                                                 return Px{1, 1, 1, 255};
                                             })};
                        });
                    seen[static_cast<std::size_t>(th)][static_cast<std::size_t>(k)] = *id;
                }
            });
        }
    }
    CHECK(t.size() == static_cast<std::size_t>(kKeys));
    std::set<FrameId> distinct;
    for (int k = 0; k < kKeys; ++k) {
        for (int th = 1; th < kThreads; ++th) {
            CHECK(seen[static_cast<std::size_t>(th)][static_cast<std::size_t>(k)] ==
                  seen[0][static_cast<std::size_t>(k)]);
        }
        distinct.insert(seen[0][static_cast<std::size_t>(k)]);
    }
    CHECK(distinct.size() == static_cast<std::size_t>(kKeys));
    CHECK(*distinct.begin() == 0);
    CHECK(*distinct.rbegin() == static_cast<FrameId>(kKeys - 1));
}

TEST_CASE("draw list: records, clip table (only islands with records), sorted frame set") {
    const Frames fr;
    const ChunkDraws d = build(fr, 16);
    // Island A 9 records, island B 3, the unclipped tail 3.
    CHECK(d.object_count() == 15);
    CHECK(d.objects.size() == d.object_count() * render::kObjectRecordBytes);
    REQUIRE(d.clips.size() == 2);
    CHECK(d.clips[0] == Rect{0, 0, 60, 64});
    CHECK(d.clips[1] == Rect{40, 0, 56, 64});
    CHECK(d.object(0).clip == 0);
    CHECK(d.object(d.object_count() - 1).clip == render::kClipNone);
    CHECK(std::is_sorted(d.frame_set.begin(), d.frame_set.end()));
    CHECK(std::adjacent_find(d.frame_set.begin(), d.frame_set.end()) == d.frame_set.end());
    CHECK(d.frame_set.size() == 8);
    // An island that adds nothing leaves no clip entry.
    ChunkDrawsBuilder b(Rect{0, 0, 64, 64}, 2, false, 0);
    b.begin_island(Rect{0, 0, 10, 10});
    b.begin_island(Rect{10, 10, 10, 10});
    b.add(rec(fr.dot, 12, 12, 0));
    b.begin_island(Rect{30, 30, 10, 10});
    const ChunkDraws e = b.finish(fr.table);
    CHECK(e.clips.size() == 1);
    CHECK(e.clips[0] == Rect{10, 10, 10, 10});
    // Nothing recorded: no bins, an empty list.
    ChunkDrawsBuilder none(Rect{0, 0, 64, 64}, 2, false, 0);
    const ChunkDraws n = none.finish(fr.table);
    CHECK(n.object_count() == 0);
    CHECK(n.bin_offsets.empty());
}

TEST_CASE("replay equals the old algorithm, with the base plane") {
    const Frames fr;
    const ChunkDraws d = build(fr, 16);
    std::size_t missing = 0;
    const Canvas got = render::replay(d, fr.table, &missing);
    const Canvas want = reference(d, fr.table);
    CHECK(missing == 0);
    REQUIRE(got.pixels.size() == want.pixels.size());
    CHECK(got.pixels == want.pixels);
    CHECK(got.base == want.base);
    CHECK(got.base_band == kBand);
    // Without the unclipped tail, and without bases.
    const ChunkDraws d2 = build(fr, 16, false);
    CHECK(render::replay(d2, fr.table).pixels == reference(d2, fr.table).pixels);
}

TEST_CASE("replay: hand-computed pixels pin the reference itself") {
    const Frames fr;
    const ChunkDraws d = build(fr, 16);
    const Canvas c = render::replay(d, fr.table);
    // Three shadows compound: alpha 96, 155, 192 (over, floor division); a
    // translucent pixel (alpha 128) over them, then the island over the empty
    // chunk canvas. At (50, 50): (114, 57, 28, 223), base 0 (a one-pixel frame
    // at its anchor row: keel 0 - row 0).
    CHECK(at(c, 50, 50) == Px{114, 57, 28, 223});
    CHECK(base_at(c, 50, 50) == 0);
    // Only the three shadows, one pixel over: (0, 0, 0, 192), base untouched.
    ChunkDrawsBuilder b(Rect{0, 0, 8, 8}, 2, true, kBand);
    b.begin_island(Rect{0, 0, 8, 8});
    for (int i = 0; i < 3; ++i) {
        b.add(rec(fr.dot, 3, 3, flags::kShadow));
    }
    const Canvas s = render::replay(b.finish(fr.table), fr.table);
    CHECK(at(s, 3, 3) == Px{0, 0, 0, 192});
    CHECK(base_at(s, 3, 3) == 255);
    // The block of island A (anchor (10, 11), 20 x 12 at (4, 20)): the bottom
    // row of a column is base 0 and the top row 11 (keel row - row).
    CHECK(base_at(c, 5, 31) == 0);
    CHECK(base_at(c, 5, 20) == 11);
    CHECK(at(c, 5, 20) == Px{200, 60, 30, 255});
    // Island B's block starts at x = 30 but B's clip starts at 40: 35, 25 is
    // not drawn, 45, 25 is.
    CHECK(at(c, 35, 25)[3] == 0);
    CHECK(base_at(c, 35, 25) == 255);
    CHECK(at(c, 45, 25) == Px{200, 60, 30, 255});
    // The checker sprite of A at (30, 10) stands on base values; B's shadow (38,
    // 8, 24 x 8, clipped to x >= 40) over its pixels at x 40..43 is shadow only
    // in B's canvas, so composite() copies base 255 over A's value. Pixel (42,
    // 12): the checker is opaque where (x + y / 2) is even: column 12, row 2
    // -> 12 + 1 = 13 odd, transparent; column 12, row 3 (y = 13) 12 + 1 odd;
    // pick (41, 12): column 11, row 2 -> 11 + 1 = 12 even: opaque.
    CHECK(at(c, 41, 12) != Px{0, 0, 0, 0});
    CHECK(base_at(c, 41, 12) == 255);
    // The same pixel with island A alone has a real base value.
    ChunkDrawsBuilder a(Rect{0, 0, 96, 64}, 2, true, kBand);
    a.begin_island(Rect{0, 0, 60, 64});
    a.add(rec(fr.checker, 30, 10, flags::kTrackBase));
    const Canvas ca = render::replay(a.finish(fr.table), fr.table);
    CHECK(at(ca, 41, 12) == Px{240, 240, 40, 255});
    CHECK(base_at(ca, 41, 12) == 5); // keel row 7 of column 11, minus row 2
    // The unclipped tail goes straight onto the chunk: the block at (2, 2) is
    // drawn at x = 2 although no island clip holds it.
    CHECK(at(c, 2, 2) == Px{200, 60, 30, 255});
}

TEST_CASE("replay: a frame the table does not hold is skipped and counted") {
    const Frames fr;
    ChunkDrawsBuilder b(Rect{0, 0, 16, 16}, 2, false, 0);
    b.begin_island(Rect{0, 0, 16, 16});
    b.add(rec(fr.dot, 1, 1, 0));
    b.add(rec(77, 2, 2, 0)); // never interned
    const ChunkDraws d = b.finish(fr.table);
    std::size_t missing = 0;
    const Canvas c = render::replay(d, fr.table, &missing);
    CHECK(missing == 1);
    CHECK(at(c, 1, 1) == Px{10, 20, 30, 255});
}

TEST_CASE("bins: every record is in exactly the bins its clipped image box touches, in order") {
    const Frames fr;
    for (const std::uint32_t bin_px : {8U, 16U, 32U, 40U}) {
        const ChunkDraws d = build(fr, bin_px);
        CAPTURE(bin_px);
        REQUIRE(d.bins_x == (96 + bin_px - 1) / bin_px);
        REQUIRE(d.bins_y == (64 + bin_px - 1) / bin_px);
        const std::size_t bins = std::size_t{d.bins_x} * d.bins_y;
        REQUIRE(d.bin_offsets.size() == bins + 1);
        CHECK(d.bin_offsets.front() == 0);
        CHECK(d.bin_offsets.back() == d.bin_indices.size());
        for (std::size_t y = 0; y < d.bins_y; ++y) {
            for (std::size_t x = 0; x < d.bins_x; ++x) {
                const std::size_t b = y * d.bins_x + x;
                const Rect cell{static_cast<int>(x * bin_px), static_cast<int>(y * bin_px),
                                static_cast<int>(bin_px), static_cast<int>(bin_px)};
                std::vector<std::uint32_t> want;
                for (std::size_t i = 0; i < d.object_count(); ++i) {
                    const ObjectRecord r = d.object(i);
                    const auto f = fr.table.get(r.frame);
                    std::optional<Rect> box =
                        Rect{r.x, r.y, f->image->width, f->image->height}.intersect(d.rect);
                    if (box && r.clip != render::kClipNone) {
                        box = box->intersect(d.clips[r.clip]);
                    }
                    if (box && box->intersect(cell)) {
                        want.push_back(static_cast<std::uint32_t>(i));
                    }
                }
                const std::vector<std::uint32_t> got(d.bin_indices.begin() + d.bin_offsets[b],
                                                     d.bin_indices.begin() + d.bin_offsets[b + 1]);
                CHECK(got == want);
            }
        }
    }
}

TEST_CASE("the binned per-pixel pass (the GPU's algorithm) equals the replay, base included") {
    const Frames fr;
    for (const std::uint32_t bin_px : {8U, 32U}) {
        const ChunkDraws d = build(fr, bin_px);
        const Canvas c = render::replay(d, fr.table);
        std::vector<Px> px;
        std::vector<std::uint8_t> base;
        render::test::replay_binned(d, fr.table, px, base);
        REQUIRE(px.size() == c.width * c.height);
        std::size_t bad = 0, bad_base = 0;
        for (std::size_t i = 0; i < px.size(); ++i) {
            const Px p{c.pixels[i * 4], c.pixels[i * 4 + 1], c.pixels[i * 4 + 2],
                       c.pixels[i * 4 + 3]};
            bad += px[i] == p ? 0U : 1U;
            bad_base += base[i] == c.base[i] ? 0U : 1U;
        }
        CHECK(bad == 0);
        CHECK(bad_base == 0);
    }
}
