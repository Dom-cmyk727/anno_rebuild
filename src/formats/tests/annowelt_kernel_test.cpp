#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <string>

#include <doctest/doctest.h>

#include <anno/formats/annowelt.hpp>
#include <anno/formats/sha256.hpp>
#include <anno/test/game_dir.hpp>
#include <anno/test/wildlife_kernel_fixture.hpp>

namespace wt = anno::formats::annowelt;
namespace kf = anno::test::kernel_fixture;
namespace {
constexpr std::array<std::uint8_t, 25> first{3,   10,  17,  24,  31,  38,  45,  52,  59,
                                             66,  73,  80,  87,  94,  101, 108, 115, 122,
                                             129, 136, 143, 150, 157, 164, 171};
constexpr std::array<std::uint8_t, 25> last{0,  0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 12,
                                            24, 0, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0};
template <class Change> void refuses(Change change, const std::string& reason) {
    auto image = kf::image();
    REQUIRE(wt::read_ground_kernel(image, 6));
    change(image);
    const auto before = image;
    const auto result = wt::read_ground_kernel(image, 6);
    REQUIRE_FALSE(result);
    CHECK(result.error().find(reason) != std::string::npos);
    CHECK(image == before);
}
} // namespace

TEST_CASE("ground kernel: exact authored rows translate RVA and own their bytes") {
    auto image = kf::image();
    const auto original = image;
    const auto a = wt::read_ground_kernel(image, 0);
    const auto b = wt::read_ground_kernel(image, 6);
    REQUIRE(a);
    REQUIRE(b);
    CHECK(a.value().weights == first);
    CHECK(b.value().weights == last);
    CHECK(a.value().selector == 0);
    CHECK(b.value().selector == 6);
    CHECK(a.value().rva == 0xca008);
    CHECK(b.value().rva == 0xca09e);
    CHECK(a.value().raw_offset == 0x308);
    CHECK(b.value().raw_offset == 0x39e);
    for (const auto* value : {&a.value(), &b.value()}) {
        CHECK(value->time_date_stamp == 0x44c0c353);
        CHECK(value->size_of_image == 0xf1000);
        CHECK(value->image_base == 0x10000000);
        CHECK(value->source_sha256 == anno::formats::sha256_hex(original));
    }
    CHECK(image == original);
    std::fill(image.begin(), image.end(), 0);
    image.clear();
    image.shrink_to_fit();
    CHECK(a.value().weights == first);
    CHECK(b.value().weights == last);
    CHECK(b.value().source_sha256 == anno::formats::sha256_hex(original));
}

TEST_CASE("ground kernel: exact section and EOF end is legal but unknown backed selector refuses") {
    const auto boundary = wt::read_ground_kernel(kf::image(0xb7), 6);
    REQUIRE(boundary);
    CHECK(boundary.value().weights == last);
    auto one_short = kf::image(0xb7);
    one_short.pop_back();
    const auto snapshot = one_short;
    CHECK_FALSE(wt::read_ground_kernel(one_short, 6));
    CHECK(one_short == snapshot);
    // An empty section covers no byte, even if its RVA is within this row.
    auto empty_section = kf::image();
    kf::u16(empty_section, kf::pe + 6, 2);
    kf::u32(empty_section, kf::section + 40 + 12, 0xca0a8);
    REQUIRE(wt::read_ground_kernel(empty_section, 6));
    // Plenty of raw and virtual backing beyond row 6: not an accidental OOB refusal.
    const auto extended = kf::image(0x100);
    REQUIRE(wt::read_ground_kernel(extended, 6));
    const auto unknown = wt::read_ground_kernel(extended, 7);
    REQUIRE_FALSE(unknown);
    CHECK(unknown.error() == "ground kernel: unsupported selector");
    CHECK_FALSE(wt::read_ground_kernel(extended, std::numeric_limits<std::uint32_t>::max()));
}

TEST_CASE("ground kernel: malformed headers and unsupported profile refuse without mutation") {
    refuses([](auto& b) { b.at(0) = 0; }, "PE");
    refuses([](auto& b) { b.at(kf::pe) = 0; }, "PE");
    refuses([](auto& b) { kf::u16(b, kf::opt, 0x20b); }, "PE");
    refuses([](auto& b) { kf::u16(b, kf::pe + 4, 0x8664); }, "identity");
    refuses([](auto& b) { kf::u32(b, kf::pe + 8, 0); }, "build");
    refuses([](auto& b) { kf::u32(b, kf::opt + 0x38, 0); }, "build");
    refuses([](auto& b) { kf::u32(b, kf::opt + 0x1c, 0); }, "identity");
    refuses([](auto& b) { b.resize(kf::opt + 0x3b); }, "PE");
    refuses([](auto& b) { b.resize(kf::section + 39); }, "PE");
    refuses([](auto& b) { kf::u32(b, 0x3c, 0xfffffff0); }, "PE");
    refuses([](auto& b) { kf::u16(b, kf::pe + 6, 0xffff); }, "PE");
}

TEST_CASE("ground kernel: complete initialized readable nonwritable backing is required") {
    refuses([](auto& b) { kf::u32(b, kf::section + 8, 0xb6); }, "file-backed");
    refuses([](auto& b) { kf::u32(b, kf::section + 16, 0xb6); }, "file-backed");
    refuses([](auto& b) { b.pop_back(); }, "truncated backing");
    refuses([](auto& b) { kf::u32(b, kf::section + 36, 0x40); }, "readable");
    refuses([](auto& b) { kf::u32(b, kf::section + 36, 0xc0000040); }, "non-writable");
    refuses([](auto& b) { kf::u32(b, kf::section + 12, 0x1000); }, "outside section");
    refuses([](auto& b) { kf::u32(b, kf::section + 12, 0xfffffff0); }, "outside section");
    refuses([](auto& b) { kf::u32(b, kf::section + 16, 0xffffffff); }, "extent");
    refuses([](auto& b) { kf::u32(b, kf::section + 8, 0xffffffff); }, "extent");
    refuses([](auto& b) { kf::u32(b, kf::section + 20, 0xfffffff0); }, "truncated backing");
}

TEST_CASE("ground kernel: full partial and virtual-only overlap refuse in either section order") {
    for (const bool partial : {false, true})
        for (const bool virtual_only : {false, true})
            for (const bool reversed : {false, true}) {
                CAPTURE(partial);
                CAPTURE(virtual_only);
                CAPTURE(reversed);
                refuses(
                    [&](auto& b) {
                        kf::u16(b, kf::pe + 6, 2);
                        const auto second = kf::section + 40;
                        std::copy_n(b.begin() + kf::section, 40, b.begin() + second);
                        kf::u32(b, second + 12, partial ? 0xca0a8 : kf::va);
                        kf::u32(b, second + 8, partial ? 5 : 0xc0);
                        kf::u32(b, second + 16, virtual_only ? 0 : (partial ? 5 : 0xc0));
                        if (reversed)
                            std::swap_ranges(b.begin() + kf::section, b.begin() + second,
                                             b.begin() + second);
                    },
                    "ambiguous");
            }
}

TEST_CASE("ground kernel: original seven selected outputs agree with independent extraction") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping original ground kernel comparison: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto image = anno::test::read_file(*dir / "AnnoWelt.dll");
    REQUIRE(anno::formats::sha256_hex(image) ==
            "f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2");
    anno::formats::ByteBuffer selected;
    for (std::uint32_t i = 0; i < 7; ++i) {
        const auto row = wt::read_ground_kernel(image, i);
        REQUIRE(row);
        CHECK(row.value().selector == i);
        CHECK(row.value().rva == 0xca008 + 25 * i);
        CHECK(row.value().raw_offset == 0xca008 + 25 * i);
        selected.insert(selected.end(), row.value().weights.begin(), row.value().weights.end());
    }
    // pefile extraction predates the decoder; only its digest is distributable.
    CHECK(anno::formats::sha256_hex(selected) ==
          "61d6b93386c1dd714d9bf99f89824934ea4bc33f3f3c05205ab88606b4ff7615");
    if (const auto* path = std::getenv("ANNO_WILDLIFE_KERNEL_ORACLE"); path && *path) {
        const auto oracle = anno::test::read_file(path);
        REQUIRE(oracle.size() == 175);
        CHECK(selected == oracle);
        MESSAGE("independent pefile extraction compared: selectors 7, bytes 175");
    } else {
        MESSAGE("private pefile bytes not supplied; independent extraction SHA comparison only");
    }
}
