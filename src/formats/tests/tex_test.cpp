#include <string_view>

#include <doctest/doctest.h>

#include <anno/formats/colors.hpp>
#include <anno/formats/inc.hpp>
#include <anno/formats/tex.hpp>

namespace tex = anno::formats::tex;
using anno::formats::ByteBuffer;
using anno::formats::Bytes;

namespace {
ByteBuffer bytes(std::string_view s) {
    return ByteBuffer(reinterpret_cast<const std::uint8_t*>(s.data()),
                      reinterpret_cast<const std::uint8_t*>(s.data()) + s.size());
}
} // namespace

TEST_CASE("RLE encodes and decodes two rows") {
    const tex::Pixels px = {std::nullopt, 7, 8, std::nullopt, 1, 2, 3, 4};
    const ByteBuffer data = tex::encode_rle(px, 4, 2);
    CHECK(data == ByteBuffer{1, 2, 7, 8, 1, 0, 0xFE, 0, 4, 1, 2, 3, 4, 0xFF});
    auto back = tex::decode_rle(data, 4, 2);
    REQUIRE(back);
    CHECK(back.value() == px);
}

TEST_CASE("RLE splits long gaps and spans") {
    tex::Pixels px(300);
    for (int i = 0; i < 600; ++i) {
        px.emplace_back(static_cast<std::uint8_t>(i % 200));
    }
    const ByteBuffer data = tex::encode_rle(px, 900, 1);
    std::vector<std::pair<int, int>> runs;
    for (std::size_t i = 0; data[i] != 0xFF; i += 2 + data[i + 1]) {
        runs.emplace_back(data[i], data[i + 1]);
    }
    CHECK(runs == std::vector<std::pair<int, int>>{{253, 0}, {47, 206}, {0, 253}, {0, 141}});
    CHECK(tex::decode_rle(data, 900, 1).value() == px);
    CHECK(tex::encode_rle(tex::Pixels(300), 300, 1) == ByteBuffer{253, 0, 47, 0, 0xFF});
}

TEST_CASE("RLE rejects overflow and wrong height") {
    CHECK_FALSE(tex::decode_rle(ByteBuffer{0, 3, 1, 1, 1, 0xFF}, 2, 1));
    CHECK_FALSE(tex::decode_rle(ByteBuffer{0, 1, 1, 0xFF}, 1, 2));
    CHECK_FALSE(tex::decode_rle(ByteBuffer{0, 1, 1}, 1, 1));
}

TEST_CASE("HEAD round trip") {
    // TEXTURE/HEAD of the first texture in Textures/debug.tex (P).
    const ByteBuffer raw = {0x24, 0,   0x23, 0,   0,   0, 8, 0, 0,    0, 'B',  'S',
                            'H',  '_', 'R',  'G', 'B', 0, 0, 0, 0xff, 0, 0xff, 0,
                            0x12, 0,   0x23, 0,   0,   0, 0, 0, 0,    0, 0,    0};
    auto h = tex::Head::parse(raw);
    REQUIRE(h);
    CHECK(h->width == 36);
    CHECK(h->height == 35);
    CHECK(h->format() == tex::PixelFormat::BshRgb);
    const auto back = h->to_bytes();
    CHECK(ByteBuffer(back.begin(), back.end()) == raw);
}

TEST_CASE("ACCESS records") {
    const ByteBuffer p = bytes(std::string_view("image001.pcx,1\0image002.pcx,2\0", 30));
    auto r = anno::formats::inc::parse_records(p);
    REQUIRE(r);
    REQUIRE(r->size() == 2);
    CHECK((*r)[1].number == 2);
    CHECK(ByteBuffer((*r)[1].source.begin(), (*r)[1].source.end()) == bytes("image002.pcx"));
    CHECK(anno::formats::inc::parse_records(Bytes())->empty());
    CHECK_FALSE(anno::formats::inc::parse_records(bytes("image001.pcx,1")));
    CHECK_FALSE(anno::formats::inc::parse_records(bytes(std::string_view("image001.pcx\0", 13))));
}

TEST_CASE("colour table entry parses, rejects and round-trips") {
    ByteBuffer e = {0, 0, 0, 0, 7, 0, 0, 0};
    const ByteBuffer body = bytes(std::string_view("BODY\0\0\x00\x04", 8));
    e.insert(e.end(), body.begin(), body.end());
    for (int i = 0; i < 256; ++i) {
        e.insert(e.end(),
                 {static_cast<std::uint8_t>(i), 0x20, static_cast<std::uint8_t>(0xff - i), 0});
    }
    static constexpr char kName[] = "Farbtabellen\\Gr\xe4ser01.PCX"; // + its NUL
    const ByteBuffer name = bytes(std::string_view(kName, sizeof kName));
    const ByteBuffer tag = bytes(std::string_view("NAME\0\0", 6));
    e.insert(e.end(), tag.begin(), tag.end());
    anno::formats::put_u16_le(e, static_cast<std::uint16_t>(name.size()));
    e.insert(e.end(), name.begin(), name.end());
    auto t = anno::formats::colors::parse_entry(e);
    REQUIRE(t);
    CHECK(t.value().palette.id == 7);
    CHECK(t.value().palette.rgb(1) == tex::Rgb{0xfe, 0x20, 1});
    CHECK(t.value().to_bytes() == e);
    ByteBuffer bad = e;
    bad[0x0E] = 1;
    CHECK_FALSE(anno::formats::colors::parse_entry(bad));
    bad = e;
    bad.push_back(0);
    CHECK_FALSE(anno::formats::colors::parse_entry(bad));
    bad = e;
    bad[anno::formats::colors::kNameOffset] = 0;
    CHECK_FALSE(anno::formats::colors::parse_entry(bad));
}
