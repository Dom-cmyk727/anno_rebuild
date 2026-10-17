// anno_app_core: PNG output (always) and scene loading (needs
// ANNO1503_GAME_DIR; skips otherwise).
#include <cstring>
#include <fstream>
#include <iterator>

#include <doctest/doctest.h>

#include <anno/app/loader.hpp>
#include <anno/app/png.hpp>
#include <anno/test/game_dir.hpp>

namespace fs = std::filesystem;

namespace {

std::uint32_t be32(const std::vector<std::uint8_t>& b, std::size_t o) {
    return std::uint32_t{b[o]} << 24 | std::uint32_t{b[o + 1]} << 16 |
           std::uint32_t{b[o + 2]} << 8 | std::uint32_t{b[o + 3]};
}

/// Reads back a PNG written by write_png: checks every chunk CRC, inflates
/// the stored deflate blocks, checks the Adler-32 and returns the RGBA rows.
std::vector<std::uint8_t> read_stored_png(const fs::path& p, std::uint32_t& w, std::uint32_t& h) {
    std::ifstream in(p, std::ios::binary);
    const std::vector<std::uint8_t> b(std::istreambuf_iterator<char>(in), {});
    REQUIRE(b.size() > 8);
    CHECK(std::memcmp(b.data(), "\x89PNG\r\n\x1a\n", 8) == 0);
    std::vector<std::uint8_t> z;
    for (std::size_t o = 8; o < b.size();) {
        const std::uint32_t len = be32(b, o);
        const std::string type(b.begin() + static_cast<std::ptrdiff_t>(o + 4),
                               b.begin() + static_cast<std::ptrdiff_t>(o + 8));
        const std::span<const std::uint8_t> body(b.data() + o + 4, len + 4);
        CHECK(anno::app::crc32(body) == be32(b, o + 8 + len));
        if (type == "IHDR") {
            w = be32(b, o + 8);
            h = be32(b, o + 12);
        } else if (type == "IDAT") {
            z.insert(z.end(), b.begin() + static_cast<std::ptrdiff_t>(o + 8),
                     b.begin() + static_cast<std::ptrdiff_t>(o + 8 + len));
        }
        o += 12 + len;
    }
    REQUIRE(z.size() >= 6);
    CHECK(z[0] == 0x78);
    std::vector<std::uint8_t> raw;
    std::size_t i = 2;
    for (bool last = false; !last;) {
        last = (z[i] & 1) != 0;
        const std::size_t n = z[i + 1] | std::size_t{z[i + 2]} << 8;
        CHECK((n ^ (z[i + 3] | std::size_t{z[i + 4]} << 8)) == 0xFFFF);
        raw.insert(raw.end(), z.begin() + static_cast<std::ptrdiff_t>(i + 5),
                   z.begin() + static_cast<std::ptrdiff_t>(i + 5 + n));
        i += 5 + n;
    }
    std::uint32_t a = 1, s = 0;
    for (const std::uint8_t v : raw) {
        a = (a + v) % 65521;
        s = (s + a) % 65521;
    }
    CHECK(be32(z, i) == (s << 16 | a));
    std::vector<std::uint8_t> rgba;
    const std::size_t row = std::size_t{w} * 4;
    for (std::uint32_t y = 0; y < h; ++y) {
        CHECK(raw[y * (row + 1)] == 0);
        rgba.insert(rgba.end(), raw.begin() + static_cast<std::ptrdiff_t>(y * (row + 1) + 1),
                    raw.begin() + static_cast<std::ptrdiff_t>((y + 1) * (row + 1)));
    }
    return rgba;
}

} // namespace

TEST_CASE("CRC-32 check value") {
    const std::string s = "123456789";
    CHECK(anno::app::crc32({reinterpret_cast<const std::uint8_t*>(s.data()), s.size()}) ==
          0xCBF43926U);
}

TEST_CASE("PNG round trip, including more than one stored block") {
    const fs::path out = fs::temp_directory_path() / "anno_app_test.png";
    // 200 × 100 RGBA: 80,100 filtered bytes, two stored blocks.
    std::vector<std::uint8_t> px(200 * 100 * 4);
    for (std::size_t k = 0; k < px.size(); ++k) {
        px[k] = static_cast<std::uint8_t>(k * 7 + k / 13);
    }
    REQUIRE(anno::app::write_png(out, 200, 100, px).empty());
    std::uint32_t w = 0, h = 0;
    CHECK(read_stored_png(out, w, h) == px);
    CHECK(w == 200);
    CHECK(h == 100);
    CHECK_FALSE(anno::app::write_png(out, 3, 3, px).empty());
    fs::remove(out);
}

TEST_CASE("scene loading: islands, links and units") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // Single_01: 15 INSEL5 islands, 5 ships; no links.
    auto s1 = anno::app::load_scene(*dir, *dir / "Scenes" / "0200Single" / "Single_01.sze");
    REQUIRE(s1);
    CHECK(s1.value().scene->islands.size() == 15);
    CHECK(s1.value().scene->units.size() == 5);
    // Case-insensitive path resolution as on Windows.
    CHECK(anno::app::find_ci(*dir / "Scenes", "0200SINGLE\\single_01.SZE"));
    CHECK_FALSE(anno::app::find_ci(*dir / "Scenes", "nope\\x.scp"));
    // An endless game links its islands (INSELLNK): stand-ins are loaded.
    const auto endless = anno::test::files_with_ext(*dir / "Scenes" / "0010SingleEndlos", "sze");
    REQUIRE_FALSE(endless.empty());
    auto s2 = anno::app::load_scene(*dir, endless.front());
    REQUIRE(s2);
    CHECK(s2.value().scene->islands.size() > 0);
    bool noted = false;
    for (const auto& n : s2.value().notes) {
        noted = noted || n.find("stand-ins") != std::string::npos;
    }
    CHECK(noted);
    // The game directory is found from a file below it.
    const auto found =
        anno::app::find_game_dir(*dir, *dir / "Scenes" / "0200Single" / "Single_01.sze");
    REQUIRE(found);
    CHECK(found.value() == *dir);
}
