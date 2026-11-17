// Island unit tests on our own bytes (fixtures/synthetic/insel_synthetic and
// inline buffers). Spec: docs/formats/insel.md.
#include <filesystem>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/insel.hpp>
#include <anno/test/game_dir.hpp>

namespace insel = anno::formats::insel;
using anno::formats::ByteBuffer;

namespace {

ByteBuffer bytes(std::string_view s) {
    return ByteBuffer(s.begin(), s.end());
}

/// The ANBAU decode of the only island in a synthetic fixture file.
anno::formats::Result<std::optional<insel::CropList>, std::string> fixture_crops(const char* name) {
    const auto buf =
        anno::test::read_file(std::filesystem::path(ANNO_FIXTURES_DIR) / "insel_synthetic" / name);
    auto is = insel::islands(buf);
    REQUIRE(is);
    REQUIRE(is.value().size() == 1);
    return is.value()[0].crops();
}

} // namespace

TEST_CASE("ANBAU: parse and write round trip") {
    using namespace std::string_literals;
    for (const auto& payload : {
             bytes(""),
             bytes("GETREIDE\0"s),
             bytes("GETREIDE\0TABAKBAUM\0KR\xC4UTER\0KARTOFFEL\0"s),
             bytes("A,B\\C\0x\0"s),
         }) {
        auto list = insel::CropList::parse(payload);
        REQUIRE(list);
        auto again = list.value().write();
        REQUIRE(again);
        CHECK(again.value() == payload);
    }
    auto list = insel::CropList::parse(bytes("GETREIDE\0KR\xC4UTER\0"s));
    REQUIRE(list);
    CHECK(list.value().names == std::vector<std::string>{"GETREIDE", "KR\xC4UTER"});
    CHECK(insel::CropList::parse(ByteBuffer{}).value().names.empty());
}

TEST_CASE("ANBAU: malformed payloads and names") {
    using namespace std::string_literals;
    auto check_error = [](const ByteBuffer& payload, const std::string& error) {
        auto list = insel::CropList::parse(payload);
        REQUIRE_FALSE(list);
        CHECK(list.error() == error);
    };
    check_error(bytes("GETREIDE"), "name not terminated");
    check_error(bytes("GETREIDE\0TABAK"s), "name not terminated");
    check_error(bytes("\0"s), "empty name");
    check_error(bytes("GETREIDE\0\0"s), "empty name");
    check_error(bytes("GETREIDE\0\0TABAK\0"s), "empty name");

    insel::CropList empty_name{{"GETREIDE", ""}};
    REQUIRE_FALSE(empty_name.write());
    CHECK(empty_name.write().error() == "empty name");
    insel::CropList nul_name{{"GET\0REIDE"s}};
    REQUIRE_FALSE(nul_name.write());
    CHECK(nul_name.write().error() == "NUL in name");
}

TEST_CASE("ANBAU: synthetic fixture islands") {
    auto crops = fixture_crops("crops.scp");
    REQUIRE(crops);
    REQUIRE(crops.value().has_value());
    CHECK(crops.value()->names == std::vector<std::string>{"GETREIDE", "KR\xC4UTER", "A,B\\C"});

    auto empty = fixture_crops("crops_empty.scp");
    REQUIRE(empty);
    REQUIRE(empty.value().has_value());
    CHECK(empty.value()->names.empty());

    auto none = fixture_crops("old.scp");
    REQUIRE(none);
    CHECK_FALSE(none.value().has_value());

    auto bad = fixture_crops("crops_unterminated.scp");
    REQUIRE_FALSE(bad);
    CHECK(bad.error() == "name not terminated");
    bad = fixture_crops("crops_empty_name.scp");
    REQUIRE_FALSE(bad);
    CHECK(bad.error() == "empty name");
    bad = fixture_crops("crops_twice.scp");
    REQUIRE_FALSE(bad);
    CHECK(bad.error() == "twice");
}
