#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include <anno/formats/chunk.hpp>

namespace chunk = anno::formats::chunk;
using anno::formats::ByteBuffer;
using anno::formats::Bytes;

namespace {

ByteBuffer make(std::string_view name, const ByteBuffer& payload) {
    ByteBuffer v(chunk::kNameLen, 0);
    for (std::size_t i = 0; i < name.size(); ++i) {
        v[i] = static_cast<std::uint8_t>(name[i]);
    }
    anno::formats::put_u32_le(v, static_cast<std::uint32_t>(payload.size()));
    v.insert(v.end(), payload.begin(), payload.end());
    return v;
}

ByteBuffer cat(ByteBuffer a, const ByteBuffer& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

ByteBuffer write(const chunk::ChunkFile& f) {
    ByteBuffer out;
    REQUIRE(chunk::write_file(f, out));
    return out;
}

} // namespace

TEST_CASE("nested exact fit") {
    const ByteBuffer buf = make("ANNO", cat(make("HEAD", {1, 2}), make("BODY", {3})));
    auto f = chunk::parse_file(buf);
    REQUIRE(f);
    const auto& root = f.value().chunks.at(0);
    CHECK(root.name() == "ANNO");
    REQUIRE(root.is_container);
    CHECK(root.children.size() == 2);
    CHECK(root.children[0].name() == "HEAD");
    CHECK(root.children[0].offset == 16);
    CHECK(root.children[1].payload.size() == 1);
    CHECK(f.value().trailing.empty());
    CHECK(chunk::tree(f.value()) ==
          "ANNO off=0 size=35\n  HEAD off=16 size=2\n  BODY off=34 size=1\n");
    CHECK(write(f.value()) == buf);
}

TEST_CASE("garbage after NUL is kept") {
    ByteBuffer buf = make("ANNO", {9, 9});
    buf[5] = 0xE0;
    buf[11] = 0x01;
    auto f = chunk::parse_file(buf);
    REQUIRE(f);
    CHECK(f.value().chunks[0].name() == "ANNO");
    CHECK(write(f.value()) == buf);
}

TEST_CASE("opaque payload has no children") {
    auto f = chunk::parse_file(make("ANNO", ByteBuffer(20, 0x41)));
    REQUIRE(f);
    CHECK_FALSE(f.value().chunks[0].is_container);
}

TEST_CASE("trailing bytes after root") {
    ByteBuffer buf = cat(make("ANNO", make("HEAD", {1})), ByteBuffer{7, 7, 7});
    auto f = chunk::parse_file(buf);
    REQUIRE(f);
    CHECK(f.value().chunks.size() == 1);
    CHECK(f.value().trailing.size() == 3);
    CHECK(f.value().trailing_offset == 33);
    CHECK(chunk::tree(f.value()) ==
          "ANNO off=0 size=17\n  HEAD off=16 size=1\nTRAILING off=33 len=3\n");
    CHECK(write(f.value()) == buf);
}

TEST_CASE("rejects bad names and sizes") {
    auto e = chunk::parse_file(ByteBuffer{1, 2, 3});
    REQUIRE_FALSE(e);
    CHECK(e.error().kind == chunk::ErrorKind::Truncated);
    e = chunk::parse_file(make("anno", {}));
    REQUIRE_FALSE(e);
    CHECK(e.error().kind == chunk::ErrorKind::InvalidName);
    e = chunk::parse_file(make("A", {}));
    REQUIRE_FALSE(e);
    CHECK(e.error().kind == chunk::ErrorKind::InvalidName);
    ByteBuffer big = make("ANNO", {1});
    big[12] = 9;
    e = chunk::parse_file(big);
    REQUIRE_FALSE(e);
    CHECK(e.error().kind == chunk::ErrorKind::SizeOutOfBounds);
    CHECK(e.error().size == 9);
    CHECK(e.error().message() == "chunk at 0 declares size 9 past end of data");
}

TEST_CASE("twelve-character name without NUL is read") {
    auto f = chunk::parse_file(make("ABCDEFGHIJKL", {}));
    REQUIRE(f);
    CHECK(f.value().chunks[0].name() == "ABCDEFGHIJKL");
}

TEST_CASE("rebuilds container from children") {
    const ByteBuffer buf = make("ANNO", cat(make("HEAD", {1, 2}), make("BODY", {3})));
    auto f = chunk::parse_file(buf);
    REQUIRE(f);
    auto file = f.value();
    ByteBuffer longer{4, 5, 6, 7};
    file.chunks[0].children[1].payload = Bytes(longer);
    const ByteBuffer out = write(file);
    CHECK(out == make("ANNO", cat(make("HEAD", {1, 2}), make("BODY", {4, 5, 6, 7}))));
}

TEST_CASE("name field validates") {
    CHECK(chunk::name_field("INSEL5").has_value());
    CHECK_FALSE(chunk::name_field("insel").has_value());
    CHECK_FALSE(chunk::name_field("X").has_value());
    CHECK_FALSE(chunk::name_field("ABCDEFGHIJKL").has_value()); // no room for the NUL
    CHECK((*chunk::name_field("AB"))[2] == 0);
}

TEST_CASE("depth limit matches reference") {
    // Seven nested levels: the seventh payload is not probed (kMaxDepth 6).
    ByteBuffer inner = make("LEAF", ByteBuffer(16, 0));
    for (int i = 0; i < 7; ++i) {
        inner = make("NEST", inner);
    }
    auto f = chunk::parse_file(inner);
    REQUIRE(f);
    const chunk::Chunk* c = &f.value().chunks[0];
    int depth = 0;
    while (c->is_container) {
        c = &c->children[0];
        ++depth;
    }
    CHECK(depth == 6);
}
