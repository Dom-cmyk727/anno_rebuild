// Synthetic tests of anno_assets (no game files needed).
#include <array>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <string_view>

#include <doctest/doctest.h>

#include <anno/assets/building.hpp>
#include <anno/assets/figure.hpp>
#include <anno/assets/island.hpp>
#include <anno/assets/texture.hpp>
#include <anno/assets/unit.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/bytes.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/formats/insel.hpp>
#include <anno/formats/sze.hpp>
#include <anno/formats/tagged.hpp>
#include <anno/formats/tex.hpp>
#include <anno/test/game_dir.hpp>

namespace assets = anno::assets;
namespace fig = anno::formats::figuren;
using anno::formats::ByteBuffer;

namespace {

void append(ByteBuffer& out, std::string_view s) {
    out.insert(out.end(), s.begin(), s.end());
}

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

/// A chunk: 12-byte NUL-padded name, u32 length, payload.
ByteBuffer rec(std::string_view name, const ByteBuffer& payload) {
    ByteBuffer out(12, 0);
    std::memcpy(out.data(), name.data(), name.size());
    put_u32(out, static_cast<std::uint32_t>(payload.size()));
    append(out, payload);
    return out;
}

ByteBuffer cat(std::initializer_list<ByteBuffer> parts) {
    ByteBuffer out;
    for (const auto& p : parts) {
        append(out, p);
    }
    return out;
}

/// A 36-byte frame header.
ByteBuffer frame(std::string_view format, std::uint16_t w, std::uint16_t h, std::uint32_t offset,
                 std::uint32_t length) {
    ByteBuffer raw;
    for (std::uint16_t v : {w, h, std::uint16_t{0}, std::uint16_t{8}}) {
        put_u16(raw, v);
    }
    ByteBuffer f(12, 0);
    std::memcpy(f.data(), format.data(), format.size());
    append(raw, f);
    put_u16(raw, 0);
    put_u16(raw, 0);
    for (std::uint32_t v : {offset, length, std::uint32_t{0}}) {
        put_u32(raw, v);
    }
    return raw;
}

} // namespace

TEST_CASE("RLE texture to RGBA") {
    // Palette 3: index i = [B=i, G=0x10, R=0xff-i, 0].
    ByteBuffer pal;
    for (unsigned i = 0; i < 256; ++i) {
        pal.insert(pal.end(),
                   {static_cast<std::uint8_t>(i), 0x10, static_cast<std::uint8_t>(0xff - i), 0});
    }
    const ByteBuffer color =
        rec("COLOR", cat({ByteBuffer{0, 0, 0, 0, 3, 0, 0, 0}, rec("BODY", pal)}));
    ByteBuffer head = {2, 0, 1, 0, 3, 0, 8, 0, 0, 0};
    append(head, std::string_view("BSH_RGB\0\0\0", 10));
    append(head, ByteBuffer{0xff, 0, 0xff, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    const ByteBuffer body = {1, 1, 0x20, 0xFF}; // skip 1, one pixel of index 0x20
    const ByteBuffer texture = rec(
        "TEXTURE", cat({ByteBuffer{0, 0, 0, 0, 9, 0, 0, 0}, rec("HEAD", head), rec("BODY", body)}));
    const ByteBuffer buf = rec("ANNO", cat({color, texture}));
    auto file = anno::formats::tex::TexFile::parse(buf);
    REQUIRE(file);
    auto img = assets::to_rgba(file.value(), file.value().textures[0]);
    REQUIRE(img);
    CHECK(img.value().width == 2);
    CHECK(img.value().height == 1);
    CHECK(img.value().pixels == std::vector<std::uint8_t>{0, 0, 0, 0, 0xdf, 0x10, 0x20, 255});
}

TEST_CASE("island ground: cells, water and missing textures") {
    // Felder-like file: palette 0 (index i = [B=i, G=0, R=0]) and a 256 × 256
    // TEX_RGB texture 11 whose pixel (x, y) has index x ^ y.
    ByteBuffer pal;
    for (unsigned i = 0; i < 256; ++i) {
        pal.insert(pal.end(), {static_cast<std::uint8_t>(i), 0, 0, 0});
    }
    const ByteBuffer color = rec("COLOR", cat({ByteBuffer(8, 0), rec("BODY", pal)}));
    ByteBuffer head = {0, 1, 0, 1, 0, 0, 8, 0, 0, 0};
    append(head, std::string_view("TEX_RGB\0\0\0", 10));
    append(head, ByteBuffer(16, 0));
    ByteBuffer body;
    for (unsigned y = 0; y < 256; ++y) {
        for (unsigned x = 0; x < 256; ++x) {
            body.push_back(static_cast<std::uint8_t>(x ^ y));
        }
    }
    const ByteBuffer texture =
        rec("TEXTURE",
            cat({ByteBuffer{0, 0, 0, 0, 11, 0, 0, 0}, rec("HEAD", head), rec("BODY", body)}));
    const ByteBuffer felder_buf = rec("ANNO", cat({color, texture}));
    auto felder = anno::formats::tex::TexFile::parse(felder_buf);
    REQUIRE(felder);
    auto textures = assets::island::GroundTextures::load(felder.value());
    REQUIRE(textures);
    // 3 × 1 island: texture 11 cell (1, 2) rotated once, water, texture 99.
    const anno::formats::insel::GroundCell cell{11, 1, 2, 1};
    ByteBuffer gfx;
    for (std::uint16_t g : {cell.to_gfx_id(), std::uint16_t{0}, std::uint16_t{99 << 8}}) {
        put_u16(gfx, g);
    }
    const ByteBuffer heights(3, 0);
    ByteBuffer payload;
    for (const std::uint16_t v : std::array<std::uint16_t, 4>{0, 0, 3, 1}) {
        put_u16(payload, v);
    }
    put_u32(payload, 0);
    append(payload, rec("HEIGHT", anno::formats::insel::encode_blocks(heights, 3, 1, 1)[0]));
    append(payload, rec("GFXID", anno::formats::insel::encode_blocks(gfx, 3, 1, 2)[0]));
    auto island = anno::formats::insel::Island::from_insel5(payload);
    REQUIRE(island);
    auto img = assets::island::ground_top_down(island.value(), textures.value(), 32);
    REQUIRE(img);
    CHECK(img.value().width == 96);
    CHECK(img.value().height == 32);
    // Output (0, 0) of tile 0 is tile position (0, 0) of 32 → rotated
    // (31, 0) → texture x (32 + 31)·256/192 = 84, y 64·256/192 = 85.
    const auto& px = img.value().pixels;
    const std::uint8_t want = 84 ^ 85;
    CHECK(std::vector<std::uint8_t>(px.begin(), px.begin() + 4) ==
          std::vector<std::uint8_t>{0, 0, want, 255});
    const auto at = [&px](std::size_t x) {
        return assets::island::Colour{px[x * 4], px[x * 4 + 1], px[x * 4 + 2], px[x * 4 + 3]};
    };
    CHECK(at(32) == assets::island::kWater);
    CHECK(at(64) == assets::island::kMissing);
    CHECK(textures.value().average(11) == assets::island::Colour{0, 0, 127, 255});
    CHECK_FALSE(textures.value().average(12));
}

TEST_CASE("recolour rule") {
    using assets::figure::recolour;
    using Rgb = assets::figure::Rgb;
    const Rgb blue = assets::figure::kOriginalPlayerColours[0];
    // Pure green 255: v = trunc(0.3333 · 255) = 84, w = 252.
    CHECK(recolour({0, 255, 0}, blue) == Rgb{0, 184, 252});
    // A darker marker shade scales the player colour down.
    CHECK(recolour({0, 156, 0}, blue) == Rgb{0, 112, 153});
    // Grass-like green whose green does not dominate enough stays.
    CHECK(recolour({100, 150, 80}, blue) == Rgb{100, 150, 80});
    // Boundary: G² must exceed trunc(1.5 · (R² + B²)).
    CHECK(recolour({10, 12, 0}, blue) == Rgb{10, 12, 0}); // 144 > 150? no
    CHECK(recolour({10, 13, 0}, blue) != Rgb{10, 13, 0}); // 169 > 150
    CHECK(recolour({255, 0, 255}, blue) == Rgb{255, 0, 255});
    CHECK(recolour({0, 0, 0}, blue) == Rgb{0, 0, 0});
}

TEST_CASE("sprite and shadow frames to RGBA") {
    // Colour table 0: index i = [B=i, G=0x10, R=0xff-i, 0].
    ByteBuffer table(8, 0);
    append(table, std::string_view("BODY\0\0\x00\x04", 8));
    for (unsigned i = 0; i < 256; ++i) {
        table.insert(table.end(),
                     {static_cast<std::uint8_t>(i), 0x10, static_cast<std::uint8_t>(0xff - i), 0});
    }
    append(table, std::string_view("NAME\0\0\x02\0a\0", 10));
    // Volume: Z frame (skip 1, one [z, index] pair), ALPHA frame (cover 1).
    const ByteBuffer volume = {1, 1, 0x77, 0x20, 0xFF, 0, 1, 0xFF};
    // TEXTURES: group 0, 2 frames.
    ByteBuffer tex;
    put_u32(tex, 0);
    put_u32(tex, 2);
    append(tex, frame("BSH_RGBZ", 2, 1, 0, 5));
    append(tex, frame("BSH_ALPHA", 2, 1, 5, 3));
    ByteBuffer set(16, 0);
    append(set, rec("TEXTURES", tex));
    ByteBuffer entry;
    put_u32(entry, 0);
    put_u32(entry, 5);
    append(entry, rec("COLORS", rec("ENTRY", table)));
    append(entry, rec("HAEUSER", rec("ENTRY", set)));
    ByteBuffer buf;
    append(buf, std::string_view("ANNO", 4));
    buf.resize(16, 0);
    append(buf, rec("FIGUREN", rec("ENTRY", entry)));
    auto h = fig::FigurenHeader::parse(buf);
    REQUIRE(h);
    const fig::Entry& e = h.value().entries[0];
    const auto& frames = e.gfx[0].frame_sets[0].frames;
    auto sprite = assets::figure::to_rgba(e, frames[0], volume);
    REQUIRE(sprite);
    CHECK(sprite.value().pixels == std::vector<std::uint8_t>{0, 0, 0, 0, 0xdf, 0x10, 0x20, 255});
    auto shadow = assets::figure::to_rgba(e, frames[1], volume);
    REQUIRE(shadow);
    CHECK(shadow.value().pixels == std::vector<std::uint8_t>{0, 0, 0, 255, 0, 0, 0, 0});
    auto outside = assets::figure::to_rgba(e, frames[0], anno::formats::Bytes(volume).first(3));
    REQUIRE_FALSE(outside);
    CHECK(outside.error().kind == assets::Error::Kind::OutsideVolume);
    // In a player's colour: index 0x20 = (0xdf, 0x10, 0x20) does not change.
    auto coloured = assets::figure::decode(e, frames[0], anno::formats::Bytes(volume).first(5),
                                           assets::figure::kOriginalPlayerColours[1]);
    REQUIRE(coloured);
    CHECK(coloured.value() == sprite.value());
}

namespace {

/// A SPIELER chunk with only a HEAD record (kind, colour; docs/formats/sze.md).
ByteBuffer player(std::uint32_t index, std::uint32_t kind, std::uint8_t colour) {
    ByteBuffer head(anno::formats::sze::kPlayerHeadLen, 0);
    for (int i = 0; i < 4; ++i) {
        head[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(kind >> (8 * i));
    }
    head[0x4C] = colour;
    ByteBuffer payload;
    put_u32(payload, 0);
    put_u32(payload, index);
    REQUIRE(anno::formats::tagged::write_record("HEAD", false, head, payload));
    return rec("SPIELER", payload);
}

anno::assets::unit::StartColours colours(const std::vector<ByteBuffer>& players, std::uint32_t n) {
    ByteBuffer scene;
    for (const auto& p : players) {
        append(scene, p);
    }
    const ByteBuffer buf = rec("ANNO", rec("SCENE", scene));
    auto s = anno::formats::sze::Scene::parse(buf);
    REQUIRE(s);
    auto c = anno::assets::unit::start_colours(s.value(), n);
    REQUIRE(c);
    return c.value();
}

} // namespace

TEST_CASE("start colours: removed humans, shared and exhausted pools") {
    using Slots = std::map<std::uint32_t, std::uint8_t>;
    // 8/11 human, 12 computer, 14 native peoples, 2 player 31.
    const std::vector<ByteBuffer> players = {
        player(0, 8, 0xFF),  // first human: lowest free colour (stand-in for the pick)
        player(1, 11, 0xFF), // second human slot without colour: removed
        player(2, 12, 3),    // preset computer colour: kept, stays in the pool
        player(3, 12, 0xFF), // computer: lowest colour left
        player(4, 14, 0xFF), // other type: no colour
        player(5, 12, 0xFF), // computer: gets 3 too (shared with player 2)
        player(31, 2, 0xFF), // skipped by every game-start loop
    };
    auto c = colours(players, 8);
    CHECK(c.slots == Slots{{0, 1}, {2, 3}, {3, 2}, {5, 3}});
    CHECK(c.removed == std::set<std::uint32_t>{1});
    // Two colour slots: the preset 3 is out of range, the pool runs out.
    c = colours(players, 2);
    CHECK(c.slots == Slots{{0, 1}, {3, 2}});
    CHECK(c.removed == std::set<std::uint32_t>{1});
    // A human's preset colour leaves the pool.
    c = colours({player(0, 8, 1), player(1, 12, 0xFF)}, 8);
    CHECK(c.slots == Slots{{0, 1}, {1, 2}});
    // 0xFF is never a slot, however many colours there are.
    c = colours({player(0, 12, 0xFF), player(1, 12, 0xFF)}, 1000);
    CHECK(c.slots == Slots{{0, 1}, {1, 2}});
}

TEST_CASE("building frames: palette id 0, widened rows, error kinds") {
    const auto buf = anno::test::read_file(std::filesystem::path(ANNO_FIXTURES_DIR) /
                                           "anlagen_synthetic" / "Anlagen.dat");
    auto file = anno::formats::anlagen::AnlagenFile::parse(buf);
    REQUIRE(file);
    const fig::Entry* e = file.value().entry(20);
    REQUIRE(e != nullptr);
    fig::Frame f = e->gfx.at(0).frame_sets.at(0).frames.at(0);
    // Colour table index 1 names id 0, which never matches a table.
    auto r = assets::building::to_rgba(file.value(), *e, 0, f);
    REQUIRE_FALSE(r);
    CHECK(r.error().kind == assets::Error::Kind::MissingPalette);
    // Index 0 names table 1. BSH_RGBWZ puts stored pixel x of row 0 on
    // column 2x, so the two pixels land on columns 0 and 2 of a 2-wide frame:
    // the image is widened to 3 columns (OQ-025).
    f.palette = 0;
    r = assets::building::to_rgba(file.value(), *e, 0, f);
    REQUIRE(r);
    CHECK(r.value().width == 3);
    CHECK(r.value().height == 1);
    const auto& pal = file.value().colors.at(0).palette;
    const auto c5 = pal.rgb(5), c6 = pal.rgb(6);
    CHECK(r.value().pixels == std::vector<std::uint8_t>{c5[0], c5[1], c5[2], 255, 0, 0, 0, 0, c6[0],
                                                        c6[1], c6[2], 255});
    // Unknown format; missing volume; data outside the volume.
    fig::Frame bad = f;
    bad.format_raw.fill(0);
    CHECK(assets::building::to_rgba(file.value(), *e, 0, bad).error().kind ==
          assets::Error::Kind::UnknownFormat);
    CHECK(assets::building::to_rgba(file.value(), *e, 7, f).error().kind ==
          assets::Error::Kind::OutsideVolume);
    bad = f;
    bad.length = 8;
    CHECK(assets::building::to_rgba(file.value(), *e, 0, bad).error().kind ==
          assets::Error::Kind::OutsideVolume);
}

TEST_CASE("texture without its palette, oversized ground") {
    // A texture whose HEAD names palette 3, in a file without palettes.
    ByteBuffer head = {1, 0, 1, 0, 3, 0, 8, 0, 0, 0};
    append(head, std::string_view("TEX_RGB\0\0\0", 10));
    append(head, ByteBuffer(16, 0));
    const ByteBuffer texture = rec(
        "TEXTURE", cat({ByteBuffer{0, 0, 0, 0, 9, 0, 0, 0}, rec("HEAD", head), rec("BODY", {7})}));
    const ByteBuffer buf = rec("ANNO", texture);
    auto file = anno::formats::tex::TexFile::parse(buf);
    REQUIRE(file);
    auto img = assets::to_rgba(file.value(), file.value().textures[0]);
    REQUIRE_FALSE(img);
    CHECK(img.error().kind == assets::Error::Kind::MissingTexturePalette);
    CHECK(img.error().message() == "palette 3 not in file");
    // Without a 256 × 256 texture nothing is loaded.
    auto none = assets::island::GroundTextures::load(file.value());
    REQUIRE(none);
    CHECK_FALSE(none.value().average(9));
    // A 2048-tile-wide island at 32 px per tile exceeds 65,535 pixels.
    const std::uint16_t w = 2048;
    ByteBuffer payload;
    for (const std::uint16_t v : std::array<std::uint16_t, 4>{0, 0, w, 1}) {
        put_u16(payload, v);
    }
    put_u32(payload, 0);
    const ByteBuffer heights(w, 0), gfx(std::size_t{w} * 2, 0);
    for (const auto& b : anno::formats::insel::encode_blocks(heights, w, 1, 1)) {
        append(payload, rec("HEIGHT", b));
    }
    for (const auto& b : anno::formats::insel::encode_blocks(gfx, w, 1, 2)) {
        append(payload, rec("GFXID", b));
    }
    auto island = anno::formats::insel::Island::from_insel5(payload);
    REQUIRE(island);
    CHECK_FALSE(assets::island::ground_top_down(island.value(), none.value(), 32));
    CHECK(assets::island::ground_top_down(island.value(), none.value(), 1));
}
