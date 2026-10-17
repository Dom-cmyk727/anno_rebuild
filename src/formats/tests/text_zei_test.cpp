// Plain-text data and fonts: unit tests and corpus tests (needs
// ANNO1503_GAME_DIR). Figures: docs/formats/data-text.md, docs/formats/zei.md.
#include <algorithm>
#include <set>
#include <string_view>

#include <doctest/doctest.h>

#include <anno/formats/anlagen.hpp>
#include <anno/formats/tex.hpp>
#include <anno/formats/text.hpp>
#include <anno/formats/zei.hpp>
#include <anno/test/game_dir.hpp>

namespace text = anno::formats::text;
using anno::formats::ByteBuffer;
using anno::formats::Bytes;

namespace {
ByteBuffer b(std::string_view s) {
    return ByteBuffer(reinterpret_cast<const std::uint8_t*>(s.data()),
                      reinterpret_cast<const std::uint8_t*>(s.data()) + s.size());
}
std::string str(Bytes x) {
    return std::string(x.begin(), x.end());
}
std::string pattern(const std::vector<text::Entry>& r) {
    std::string s;
    for (const auto& e : r) {
        s += (s.empty() ? "" : " ") + str(e.key);
    }
    return s;
}
} // namespace

TEST_CASE("text lines round trip and keyed kinds") {
    for (std::string_view s : {std::string_view("a\r\nb\nc"), std::string_view("a\r\n"),
                               std::string_view(""), std::string_view("x\ry\r\n")}) {
        const auto buf = b(s);
        CHECK(text::write_lines(text::lines(buf)) == buf);
    }
    CHECK(text::keyed_kind(b("  ; only a comment")).tag == text::KeyedKindTag::Blank);
    // keyed_kind returns views into its input: keep the buffers alive.
    const auto sec_line = b("[WAREN]  ; c");
    auto sec = text::keyed_kind(sec_line);
    CHECK(sec.tag == text::KeyedKindTag::Section);
    CHECK(str(sec.name) == "WAREN");
    const auto entry_line = b("Name :  \"a;b\" ; c");
    auto e = text::keyed_kind(entry_line);
    CHECK(e.tag == text::KeyedKindTag::Entry);
    CHECK(str(e.value) == "\"a;b\"");
    CHECK(text::keyed_kind(b("Pos     0, 0")).tag == text::KeyedKindTag::Other);
    const auto other_line = b("EndObj;");
    CHECK(str(text::keyed_kind(other_line).name) == "EndObj");
    CHECK(text::fields(b("100,  90 , \"a,b\", -3")).size() == 4);
    CHECK(*text::ints(b("100, 100, 100   10,    20,  50")) ==
          std::vector<std::int64_t>{100, 100, 100, 10, 20, 50});
    CHECK(text::fixed(b("-0.25"), 2) == -25);
    CHECK(text::fixed(b("0.4"), 2) == 40);
    CHECK(text::fixed(b("15"), 2) == 1500);
    CHECK_FALSE(text::fixed(b("0.125"), 2));
    for (std::string_view bad : {"", "-", ".5", "5.", "1.2.3", "x", "1e3", "+1"}) {
        CHECK_FALSE(text::fixed(b(bad), 2));
    }
    const auto id_line = b("501:Pioneer house");
    auto it = text::id_text(id_line);
    REQUIRE(it);
    CHECK(it->first == 501);
    CHECK_FALSE(text::id_text(b("Profile: %s")));
}

TEST_CASE("keyed and texte corpus") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping keyed and texte corpus: ANNO1503_GAME_DIR not set");
        return;
    }
    struct Want {
        const char* name;
        std::size_t lines, blank, sections, entries, other;
    };
    for (const Want& w :
         {Want{"BGruppen.dat", 615, 178, 4, 433, 0}, Want{"Felder.dat", 349, 96, 4, 248, 1},
          Want{"Highmap.dat", 30, 7, 5, 18, 0}, Want{"ScapeIcon.dat", 62, 27, 4, 31, 0},
          Want{"Wachstum.dat", 1567, 420, 2, 1145, 0}, Want{"font.dat", 27, 14, 0, 10, 3},
          Want{"htmlpic.dat", 25, 8, 4, 13, 0}, Want{"music.dat", 115, 69, 3, 43, 0},
          Want{"statusbar.dat", 119, 26, 4, 89, 0}, Want{"wegspeed.dat", 63, 12, 1, 50, 0}}) {
        INFO(w.name);
        const auto buf = anno::test::read_file(*dir / "Data" / w.name);
        const auto ls = text::lines(buf);
        CHECK(text::write_lines(ls) == buf);
        std::size_t n[4] = {0, 0, 0, 0};
        for (const auto& [l, k] : text::keyed(buf)) {
            ++n[static_cast<int>(k.tag)];
        }
        CHECK(ls.size() == w.lines);
        CHECK(n[0] == w.blank);
        CHECK(n[1] == w.sections);
        CHECK(n[2] == w.entries);
        CHECK(n[3] == w.other);
    }
    const auto tbuf = anno::test::read_file(*dir / "Texte.dat");
    CHECK(text::write_lines(text::lines(tbuf)) == tbuf);
    const auto t = text::Texte::parse(tbuf);
    CHECK(t.preamble.empty());
    CHECK(t.sections.size() == 118);
    const auto anl_buf = anno::test::read_file(*dir / "Anlagen.dat");
    auto anl = anno::formats::anlagen::AnlagenFile::parse(anl_buf);
    REQUIRE(anl);
    const auto* names = t.section("ANLAGEN");
    REQUIRE(names);
    std::set<std::uint32_t> ids;
    for (const auto& l : names->lines) {
        auto x = text::id_text(l);
        REQUIRE(x);
        CHECK(anl.value().entry(x->first) != nullptr);
        ids.insert(x->first);
    }
    CHECK(ids.size() == 326);
    CHECK(t.section("SHIPS")->lines.size() == 107);
    CHECK(t.section("STAEDTE")->lines.size() == 160);
}

TEST_CASE("keyed records corpus") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping keyed records corpus: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto bg = anno::test::read_file(*dir / "Data" / "BGruppen.dat");
    auto [whead, goods] = text::split_records(text::section_entries(bg, "WAREN"), "Id");
    CHECK(whead.empty());
    CHECK(goods.size() == 56);
    const auto [ghead, groups] =
        text::split_records(text::section_entries(bg, "BGRUPPE"), "BGruppeNr");
    CHECK(groups.size() == 5);
    std::size_t needs = 0;
    for (const auto& g : groups) {
        const auto [h, wares] = text::split_records(g, "Ware");
        for (const auto& w : wares) {
            CHECK(pattern(w) == "Ware Menge Priority Force");
            CHECK(text::fixed(w[1].value, 2).has_value());
            ++needs;
        }
    }
    CHECK(needs == 47);
    const auto wa = anno::test::read_file(*dir / "Data" / "Wachstum.dat");
    const auto [head, records] = text::split_records(text::section_entries(wa, "WACHSTUM"), "Id");
    CHECK(pattern(head) == "Size");
    CHECK(records.size() == 146);
    std::size_t missing_comma = 0;
    std::set<std::uint32_t> ids;
    for (const auto& r : records) {
        ids.insert(static_cast<std::uint32_t>(*text::to_int(r[0].value)));
        for (std::size_t k = 1; k < 7; ++k) {
            if (text::fields(r[k].value).size() != 6) {
                CHECK(r[k].line == 765);
                ++missing_comma;
            }
            CHECK(text::ints(r[k].value)->size() == 6);
        }
    }
    CHECK(missing_comma == 1);
    const auto fbuf = anno::test::read_file(*dir / "Textures" / "Felder.tex");
    auto felder = anno::formats::tex::TexFile::parse(fbuf);
    REQUIRE(felder);
    std::set<std::uint32_t> tex_ids;
    for (const auto& x : felder.value().textures) {
        tex_ids.insert(x.id);
    }
    std::vector<std::uint32_t> diff;
    std::set_difference(tex_ids.begin(), tex_ids.end(), ids.begin(), ids.end(),
                        std::back_inserter(diff));
    CHECK(diff == std::vector<std::uint32_t>{116});
    CHECK(std::includes(tex_ids.begin(), tex_ids.end(), ids.begin(), ids.end()));
}

TEST_CASE("zei corpus") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping zei corpus: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto files = anno::test::files_with_ext(*dir, "zei");
    CHECK(files.size() == 11);
    std::size_t reencoded = 0;
    for (const auto& path : files) {
        INFO(path.string());
        const auto buf = anno::test::read_file(path);
        auto cs = anno::formats::zei::CharSet::parse(buf);
        REQUIRE(cs);
        CHECK(cs.value().to_bytes() == buf);
        std::set<std::size_t> rows;
        for (const auto& g : cs.value().glyphs) {
            CHECK(g.data.empty() == (g.code < 32));
            if (g.data.empty()) {
                continue;
            }
            auto img = g.decode();
            REQUIRE(img);
            rows.insert(img.value().height);
            CHECK(img.value().width == g.width);
            anno::formats::tex::Pixels px(img.value().levels.begin(), img.value().levels.end());
            const auto enc =
                anno::formats::tex::encode_rle(px, img.value().width, img.value().height);
            reencoded += std::equal(enc.begin(), enc.end(), g.data.begin(), g.data.end()) ? 1 : 0;
        }
        CHECK(rows.size() == 1);
    }
    CHECK(reencoded == 11 * 224 - 2 * 224);
}
