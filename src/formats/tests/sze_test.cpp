#include <cstring>

#include <doctest/doctest.h>

#include <anno/formats/sze.hpp>
#include <anno/formats/tagged.hpp>

namespace tagged = anno::formats::tagged;
namespace sze = anno::formats::sze;
using anno::formats::ByteBuffer;

TEST_CASE("tagged records: short and long forms round trip") {
    ByteBuffer buf;
    REQUIRE(tagged::write_record("HEAD", false, ByteBuffer{1, 2, 3}, buf));
    REQUIRE(tagged::write_record("SEL_0", true, ByteBuffer(4, 9), buf));
    REQUIRE(tagged::write_record("STADT2", false, ByteBuffer{}, buf));
    CHECK(ByteBuffer(buf.begin(), buf.begin() + 11) ==
          ByteBuffer{'H', 'E', 'A', 'D', 0, 0, 3, 0, 1, 2, 3});
    CHECK(ByteBuffer(buf.begin() + 11, buf.begin() + 21) ==
          ByteBuffer{'S', 'E', 'L', '_', '0', 0, 0x00, 0xff, 4, 0});
    auto recs = tagged::parse_records(buf, 100);
    REQUIRE(recs);
    REQUIRE(recs.value().size() == 3);
    CHECK(recs.value()[1].name == "SEL_0");
    CHECK(recs.value()[1].offset == 111);
    CHECK(recs.value()[1].long_form);
    CHECK(recs.value()[2].offset == 125);
    ByteBuffer again;
    REQUIRE(tagged::write_records(recs.value(), again));
    CHECK(again == buf);
    // 24-bit length: the marker's low byte holds bits 16–23.
    ByteBuffer big;
    REQUIRE(tagged::write_record("STADT2", true, ByteBuffer(0x10005, 7), big));
    CHECK(ByteBuffer(big.begin() + 6, big.begin() + 10) == ByteBuffer{0x01, 0xFF, 0x05, 0x00});
    auto br = tagged::parse_records(big);
    REQUIRE(br);
    CHECK(br.value()[0].payload.size() == 0x10005);
}

TEST_CASE("tagged records: malformed input") {
    CHECK_FALSE(tagged::parse_records(ByteBuffer{'H', 'E', 'A', 'D', 0, 0, 1}));
    CHECK_FALSE(tagged::parse_records(ByteBuffer{'H', 'E', 'A', 'D', 0, 'x', 0, 0}));
    CHECK_FALSE(tagged::parse_records(ByteBuffer{'h', 'e', 'a', 'd', 0, 0, 0, 0}));
    CHECK_FALSE(tagged::parse_records(ByteBuffer{'H', 'E', 'A', 'D', 0, 0, 5, 0, 'a', 'b'}));
    ByteBuffer out;
    CHECK_FALSE(tagged::write_record("TOOLONG", false, ByteBuffer{}, out));
    CHECK_FALSE(tagged::write_record("x", false, ByteBuffer{}, out));
    CHECK_FALSE(tagged::write_record("BIG", false, ByteBuffer(0xFF00, 0), out));
    CHECK(tagged::write_record("BIG", true, ByteBuffer(0xFF00, 0), out));
}

TEST_CASE("unit prefix round trip") {
    ByteBuffer ship = {7, 0, 0xDE, 0x05, 2, 3, 0, 0};
    float x = 10.5f, y = 20.5f;
    std::uint32_t xb = 0, yb = 0;
    std::memcpy(&xb, &x, 4);
    std::memcpy(&yb, &y, 4);
    anno::formats::put_u32_le(ship, xb);
    anno::formats::put_u32_le(ship, yb);
    REQUIRE(tagged::write_record("HEAD", false, ByteBuffer{1, 2}, ship));
    auto u = sze::Unit::parse(ship);
    REQUIRE(u);
    CHECK(u.value().figure == 1502);
    CHECK(u.value().owner == 2);
    CHECK(u.value().attr == 3);
    CHECK(u.value().x() == 10.5f);
    CHECK(u.value().y() == 20.5f);
    CHECK(*u.value().to_bytes() == ship);
    CHECK(sze::nested_prefix("SPIELER", "SCHIFF") == 16u);
    CHECK(sze::nested_prefix("CREW", "ENTRY") == 6u);
    CHECK_FALSE(sze::nested_prefix("SPIELER", "HEAD"));
}
