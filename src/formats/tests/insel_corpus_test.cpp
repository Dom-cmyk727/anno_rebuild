// Island corpus tests over all .scp templates and .sze scenarios (needs
// ANNO1503_GAME_DIR). Figures: docs/formats/insel.md.
#include <algorithm>
#include <map>
#include <set>

#include <doctest/doctest.h>

#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/formats/tex.hpp>
#include <anno/test/game_dir.hpp>

namespace insel = anno::formats::insel;
using anno::formats::Bytes;

namespace {

/// Checks one island; returns its number of tiles.
std::size_t check(const insel::Island& i, const std::string& ctx) {
    INFO(ctx);
    const std::size_t w = i.width, h = i.height;
    auto hs = i.heights();
    REQUIRE(hs);
    auto gs = i.gfx_ids();
    REQUIRE(gs);
    auto raw = [&](std::string_view name) {
        std::vector<std::vector<std::uint8_t>> out;
        for (const auto& r : i.records) {
            if (r.name() == name) {
                out.emplace_back(r.payload.begin(), r.payload.end());
            }
        }
        return out;
    };
    CHECK(raw("HEIGHT") == insel::encode_blocks(hs.value(), w, h, 1));
    anno::formats::ByteBuffer gb;
    for (auto g : gs.value()) {
        anno::formats::put_u16_le(gb, g);
    }
    CHECK(raw("GFXID") == insel::encode_blocks(gb, w, h, 2));
    for (std::string_view name : {"GELAENDE", "PROD2", "FLUESSE"}) {
        auto bs = i.blocks(name);
        REQUIRE(bs);
        const unsigned step = name == "FLUESSE" ? 8 : 32;
        for (const auto& b : bs.value()) {
            CHECK(b.data.size() % 6 == 0);
            CHECK(b.x % step == 0);
            CHECK(b.y % step == 0);
        }
    }
    CHECK(i.fertility().has_value());
    auto info = i.record("INFO");
    REQUIRE(info);
    CHECK(info->size() == 12);
    return w * h;
}

} // namespace

TEST_CASE("insel corpus") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping insel corpus: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto scp = anno::test::files_with_ext(*dir / "Scenes", "scp");
    const auto sze = anno::test::files_with_ext(*dir / "Scenes", "sze");
    CHECK(scp.size() == 465);
    CHECK(sze.size() == 50);
    std::size_t v4 = 0, v5 = 0, tiles = 0;
    std::vector<std::string> misnamed;
    for (const auto& path : scp) {
        const auto buf = anno::test::read_file(path);
        auto is = insel::islands(buf);
        INFO(path.string());
        REQUIRE(is);
        REQUIRE(is.value().size() == 1);
        const auto& i = is.value()[0];
        (i.version == insel::Version::Insel4 ? v4 : v5) += 1;
        CHECK(i.x == 0);
        CHECK(i.y == 0);
        CHECK(i.number == 0);
        // File names carry the size as _<w>x<h>_.
        const std::string name = path.filename().string();
        const auto a = name.find('_'), b = name.find('_', a + 1);
        char size[16];
        std::snprintf(size, sizeof size, "%03ux%03u", unsigned{i.width}, unsigned{i.height});
        if (name.substr(a + 1, b - a - 1) != size) {
            misnamed.push_back(name);
        }
        tiles += check(i, path.string());
    }
    CHECK(v4 == 21);
    CHECK(v5 == 444);
    std::sort(misnamed.begin(), misnamed.end());
    CHECK(misnamed == std::vector<std::string>{"D_080x080_0022_2B.scp", "D_080x080_0023_1B.scp"});
    CHECK(tiles == 7952640);

    std::size_t count = 0;
    for (const auto& path : sze) {
        const auto buf = anno::test::read_file(path);
        auto is = insel::islands(buf);
        REQUIRE(is);
        std::set<std::uint32_t> numbers;
        for (const auto& i : is.value()) {
            CHECK(i.version == insel::Version::Insel5);
            ++count;
            check(i, path.string());
            CHECK(numbers.insert(i.number).second); // unique per scenario
        }
    }
    CHECK(count == 668);
}

TEST_CASE("ground textures") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping ground textures: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto felder_buf = anno::test::read_file(*dir / "Textures" / "Felder.tex");
    auto felder = anno::formats::tex::TexFile::parse(felder_buf);
    REQUIRE(felder);
    std::set<std::uint32_t> ids;
    for (const auto& t : felder.value().textures) {
        ids.insert(t.id);
    }
    auto files = anno::test::files_with_ext(*dir / "Scenes", "scp");
    const auto sze = anno::test::files_with_ext(*dir / "Scenes", "sze");
    files.insert(files.end(), sze.begin(), sze.end());
    std::size_t water = 0, ground = 0;
    std::set<std::uint8_t> used;
    std::map<std::string, std::size_t> missing;
    for (const auto& path : files) {
        const auto buf = anno::test::read_file(path);
        auto is = insel::islands(buf);
        REQUIRE(is);
        for (const auto& i : is.value()) {
            for (auto g : i.gfx_ids().value()) {
                const auto cell = insel::GroundCell::from_gfx_id(g);
                if (cell.texture == 0) {
                    ++water;
                } else if (ids.count(cell.texture)) {
                    ++ground;
                    used.insert(cell.texture);
                } else {
                    CHECK(cell.texture == 205);
                    ++missing[path.filename().string()];
                }
            }
        }
    }
    CHECK(water == 3804534);
    CHECK(ground == 13402214);
    CHECK(used.size() == 141);
    CHECK(ids.size() == 147);
    CHECK(missing == std::map<std::string, std::size_t>{{"00_1024_easyentry_01.sze", 9},
                                                        {"N_256x256_0015_5B.scp", 9},
                                                        {"S01_12_01.sze", 9},
                                                        {"endless01.sze", 9}});
}

TEST_CASE("placed objects corpus") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping placed objects corpus: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto anl_buf = anno::test::read_file(*dir / "Anlagen.dat");
    auto anl = anno::formats::anlagen::AnlagenFile::parse(anl_buf);
    REQUIRE(anl);
    auto files = anno::test::files_with_ext(*dir / "Scenes", "scp");
    const auto sze = anno::test::files_with_ext(*dir / "Scenes", "sze");
    files.insert(files.end(), sze.begin(), sze.end());
    std::map<std::string, std::size_t> items, dups;
    std::size_t overlap_swap = 0, overlap_plain = 0;
    auto overlaps = [&](const std::set<insel::PlacedObject>& objs, bool swap) {
        std::map<std::pair<unsigned, unsigned>, unsigned> covered;
        for (const auto& o : objs) {
            const auto fp = *anl.value().footprint(o.anlage);
            const auto [w, h] = swap ? o.footprint(fp.first, fp.second) : fp;
            for (unsigned y = o.y; y < o.y + unsigned{h}; ++y) {
                for (unsigned x = o.x; x < o.x + unsigned{w}; ++x) {
                    ++covered[{x, y}];
                }
            }
        }
        return std::count_if(covered.begin(), covered.end(),
                             [](const auto& c) { return c.second > 1; });
    };
    for (const auto& path : files) {
        const auto buf = anno::test::read_file(path);
        for (const auto& i : insel::islands(buf).value()) {
            for (std::string name : {"GELAENDE", "PROD2", "FLUESSE"}) {
                auto objs = i.objects(name);
                REQUIRE(objs);
                items[name] += objs.value().size();
                for (const auto& o : objs.value()) {
                    REQUIRE(anl.value().entry(o.anlage) != nullptr);
                    CHECK(o.reserved == 0);
                    CHECK((o.x < i.width && o.y < i.height));
                }
                const std::set<insel::PlacedObject> unique(objs.value().begin(),
                                                           objs.value().end());
                dups[name] += objs.value().size() - unique.size();
                if (name == "PROD2") {
                    overlap_swap += static_cast<std::size_t>(overlaps(unique, true));
                    overlap_plain += static_cast<std::size_t>(overlaps(unique, false));
                }
            }
        }
    }
    CHECK(items == std::map<std::string, std::size_t>{
                       {"FLUESSE", 33106}, {"GELAENDE", 4475412}, {"PROD2", 76863}});
    CHECK(dups["GELAENDE"] == 97041);
    CHECK(dups["PROD2"] == 485);
    CHECK(overlap_swap == 115);
    CHECK(overlap_plain == 1350);
}
