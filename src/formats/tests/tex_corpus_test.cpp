// .tex / .inc / Anlagen.dat COLORS corpus tests (needs ANNO1503_GAME_DIR).
// Figures: docs/formats/tex.md, docs/formats/palette.md.
#include <algorithm>
#include <map>

#include <doctest/doctest.h>

#include <anno/formats/chunk.hpp>
#include <anno/formats/colors.hpp>
#include <anno/formats/inc.hpp>
#include <anno/formats/tex.hpp>
#include <anno/test/game_dir.hpp>

namespace tex = anno::formats::tex;

TEST_CASE("tex corpus") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping tex corpus: ANNO1503_GAME_DIR not set");
        return;
    }
    std::size_t files = 0, empty = 0, palettes = 0, textures = 0, named = 0, flagged = 0, rle = 0;
    std::map<tex::PixelFormat, std::size_t> formats;
    for (const auto& path : anno::test::files_with_ext(*dir, "tex")) {
        const auto buf = anno::test::read_file(path);
        auto f = tex::TexFile::parse(buf);
        INFO(path.string());
        REQUIRE(f);
        const auto& file = f.value();
        ++files;
        empty += file.palettes.empty() && file.textures.empty() ? 1 : 0;
        palettes += file.palettes.size();
        for (const auto& p : file.palettes) {
            if (p.flag != 0) {
                CHECK(p.flag == 2); // only in ToolGfx/fenster(8).tex (OQ-021)
                ++flagged;
            }
            for (std::size_t i = 3; i < p.entries.size(); i += 4) {
                REQUIRE(p.entries[i] == 0);
            }
        }
        for (const auto& t : file.textures) {
            ++textures;
            CHECK(t.flag == 0);
            const auto fmt = t.head.format();
            REQUIRE(fmt);
            ++formats[*fmt];
            named += t.name ? 1 : 0;
            CHECK(file.palette(t.head.palette_id) != nullptr);
            auto px = t.decode();
            REQUIRE(px);
            CHECK(px.value().size() == std::size_t{t.head.width} * t.head.height);
            if (*fmt != tex::PixelFormat::TexRgb) {
                const auto enc = tex::encode_rle(px.value(), t.head.width, t.head.height);
                CHECK(std::equal(enc.begin(), enc.end(), t.body.begin(), t.body.end()));
                ++rle;
            } else {
                CHECK(t.name.has_value());
            }
        }
    }
    CHECK(files == 1214);
    CHECK(empty == 338);
    CHECK(palettes == 1427);
    CHECK(textures == 5176);
    CHECK(formats[tex::PixelFormat::BshRgb] == 5028);
    CHECK(formats[tex::PixelFormat::BshRgbH] == 1);
    CHECK(formats[tex::PixelFormat::TexRgb] == 147);
    CHECK(named == 147);
    CHECK(flagged == 10);
    CHECK(rle == 5029);
}

TEST_CASE("inc corpus") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping inc corpus: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto paths = anno::test::files_with_ext(*dir, "inc");
    std::size_t empty = 0, records = 0, with_tex = 0, numbers_are_ids = 0;
    for (const auto& path : paths) {
        const auto buf = anno::test::read_file(path);
        auto entries = anno::formats::inc::parse_access(buf);
        INFO(path.string());
        REQUIRE(entries);
        empty += entries->empty() ? 1 : 0;
        records += entries->size();
        auto tex_path = path;
        tex_path.replace_extension("tex");
        if (!std::filesystem::exists(tex_path)) {
            continue;
        }
        ++with_tex;
        const auto tex_buf = anno::test::read_file(tex_path);
        auto t = tex::TexFile::parse(tex_buf);
        REQUIRE(t);
        const bool all = std::all_of(entries->begin(), entries->end(), [&](const auto& e) {
            return std::any_of(t.value().textures.begin(), t.value().textures.end(),
                               [&](const auto& x) { return x.id == e.number; });
        });
        numbers_are_ids += all ? 1 : 0;
    }
    CHECK(paths.size() == 1213);
    CHECK(empty == 340);
    CHECK(records == 1559);
    CHECK(with_tex == 1213);
    CHECK(numbers_are_ids == 1213);
}

TEST_CASE("colors corpus") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping colors corpus: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto buf = anno::test::read_file(*dir / "Anlagen.dat");
    auto ts = anno::formats::colors::parse_anlagen(buf);
    REQUIRE(ts);
    const auto& tables = ts.value();
    CHECK(tables.size() == 440);
    std::size_t magenta_at_1 = 0;
    for (std::size_t i = 0; i < tables.size(); ++i) {
        const auto& t = tables[i];
        CHECK(t.palette.id == i + 1);
        // 0 everywhere except 4 on id 109, Farbtabellen\Flagge01_01.PCX (OQ-022 a).
        CHECK(t.palette.flag == (t.palette.id == 109 ? 4u : 0u));
        const auto& e = t.palette.entries;
        magenta_at_1 += e[4] == 0xff && e[5] == 0 && e[6] == 0xff && e[7] == 0 ? 1 : 0;
    }
    CHECK(magenta_at_1 == 287);
    // Byte-identical re-encoding of every ENTRY payload.
    auto f = anno::formats::chunk::parse_file(buf);
    REQUIRE(f);
    const auto& entries = f.value().chunks[0].children[0].children;
    REQUIRE(entries.size() == tables.size());
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto enc = tables[i].to_bytes();
        CHECK(std::equal(enc.begin(), enc.end(), entries[i].payload.begin(),
                         entries[i].payload.end()));
    }
}
