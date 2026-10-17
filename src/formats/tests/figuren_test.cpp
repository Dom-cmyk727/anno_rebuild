#include <doctest/doctest.h>

#include <anno/formats/figuren.hpp>

namespace fig = anno::formats::figuren;
using anno::formats::ByteBuffer;

namespace {
std::vector<std::optional<std::uint8_t>> indices(const fig::Pixels& px) {
    std::vector<std::optional<std::uint8_t>> out;
    for (const auto& p : px) {
        out.push_back(p ? std::optional<std::uint8_t>(p->index) : std::nullopt);
    }
    return out;
}
using Idx = std::vector<std::optional<std::uint8_t>>;
constexpr std::nullopt_t N = std::nullopt;
} // namespace

TEST_CASE("frame round trip") {
    // Zoom-2 frame of entry 1546 (Nebel01), bytes from FigurenHeader.dat (P).
    const ByteBuffer raw = {0x5a, 0,    0x2c, 0,   0,    0,    8, 0, 'B',  'S', 'H',  '_',
                            'R',  'G',  'B',  'A', 'V',  0,    0, 0, 0x2e, 0,   0x24, 0,
                            0x4a, 0xcb, 0xca, 0,   0x3e, 0x0c, 0, 0, 0,    0,   0,    0};
    auto f = fig::Frame::parse(raw);
    REQUIRE(f);
    CHECK(f->width == 90);
    CHECK(f->height == 44);
    CHECK(f->anchor_x == 46);
    CHECK(f->anchor_y == 36);
    CHECK(f->offset == 0x00cacb4a);
    CHECK(f->length == 0xc3e);
    CHECK(f->format() == fig::FrameFormat::RgbAV);
    const auto back = f->to_bytes();
    CHECK(ByteBuffer(back.begin(), back.end()) == raw);
}

TEST_CASE("decode formats") {
    auto v = fig::decode_frame(fig::FrameFormat::RgbV,
                               ByteBuffer{1, 2, 7, 8, 0xFE, 0, 3, 1, 2, 3, 0xFF}, 3, 2);
    REQUIRE(v);
    CHECK(indices(v.value()) == Idx{N, 7, 8, 1, 2, 3});
    auto z = fig::decode_frame(fig::FrameFormat::RgbZ, ByteBuffer{0, 1, 0xfb, 0x44, 0xFF}, 1, 1);
    REQUIRE(z);
    CHECK(*z.value()[0] == fig::Pixel{0x44, 0xfb, 0});
    auto a = fig::decode_frame(fig::FrameFormat::RgbAV, ByteBuffer{1, 0xC2, 5, 6, 0xFF}, 3, 1);
    REQUIRE(a);
    CHECK(a.value()[2]->index == 6);
    CHECK(a.value()[2]->level == 3);
    auto s = fig::decode_frame(fig::FrameFormat::Alpha, ByteBuffer{1, 2, 0xFE, 0, 1, 0xFF}, 3, 2);
    REQUIRE(s);
    std::vector<bool> cov;
    for (const auto& p : s.value()) {
        cov.push_back(p.has_value());
    }
    CHECK(cov == std::vector<bool>{false, true, true, true, false, false});
    auto empty = fig::decode_frame(fig::FrameFormat::RgbV, ByteBuffer{}, 2, 2);
    REQUIRE(empty);
    CHECK(indices(empty.value()) == Idx{N, N, N, N});
    CHECK_FALSE(fig::decode_frame(fig::FrameFormat::RgbV, ByteBuffer{0, 3, 1, 1, 1, 0xFF}, 2, 1));
    CHECK_FALSE(fig::decode_frame(fig::FrameFormat::RgbZ, ByteBuffer{0, 1, 9, 0xFF}, 1, 1));
}

TEST_CASE("clipped and extended decoding, WZ columns") {
    const ByteBuffer data = {0, 3, 0xa1, 1, 0xa2, 2, 0xa3, 3, 0xFE, 0xFF};
    CHECK_FALSE(fig::decode_frame(fig::FrameFormat::RgbZ, data, 2, 2));
    auto c = fig::decode_frame_clipped(fig::FrameFormat::RgbZ, data, 2, 2);
    REQUIRE(c);
    CHECK(c.value().second == 1);
    CHECK(indices(c.value().first) == Idx{1, 2, N, N});
    auto e = fig::decode_frame_extended(fig::FrameFormat::RgbZ, data, 2, 2);
    REQUIRE(e);
    CHECK(e.value().second == 3);
    CHECK(indices(e.value().first) == Idx{1, 2, 3, N, N, N});
    CHECK(fig::decode_frame_extended(fig::FrameFormat::RgbZ, ByteBuffer{0, 1, 9, 9, 0xFF}, 2, 1)
              .value()
              .second == 2);
    auto wz = fig::decode_frame(fig::FrameFormat::RgbWZ,
                                ByteBuffer{0, 2, 1, 7, 2, 8, 0xFE, 0, 1, 3, 9, 0xFF}, 4, 2);
    REQUIRE(wz);
    CHECK(indices(wz.value()) == Idx{7, N, 8, N, N, 9, N, N});
    auto beyond =
        fig::decode_frame_clipped(fig::FrameFormat::RgbAH, ByteBuffer{3, 0x42, 7, 8, 0xFF}, 2, 1);
    REQUIRE(beyond);
    CHECK(beyond.value().second == 2);
}

TEST_CASE("locate spans parts") {
    const std::vector<std::uint64_t> sizes = {10, 5, 20};
    auto pieces = [](const std::optional<std::vector<fig::Piece>>& p) {
        std::vector<std::tuple<std::size_t, std::uint64_t, std::uint64_t>> out;
        for (const auto& x : *p) {
            out.emplace_back(x.part, x.start, x.end);
        }
        return out;
    };
    using T = std::vector<std::tuple<std::size_t, std::uint64_t, std::uint64_t>>;
    CHECK(pieces(fig::locate(sizes, 2, 3)) == T{{0, 2, 5}});
    CHECK(pieces(fig::locate(sizes, 8, 10)) == T{{0, 8, 10}, {1, 0, 5}, {2, 0, 3}});
    CHECK(pieces(fig::locate(sizes, 30, 5)) == T{{2, 15, 20}});
    CHECK_FALSE(fig::locate(sizes, 30, 6).has_value());
    CHECK(fig::locate(sizes, 4, 0)->empty());
    CHECK(fig::volume_file_name(0, 3) == "Figuren003.dat");
}
