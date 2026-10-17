// Scenario corpus test over all 50 .sze (needs ANNO1503_GAME_DIR).
// Figures: docs/formats/sze.md.
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

#include <doctest/doctest.h>

#include <anno/formats/insel.hpp>
#include <anno/formats/sze.hpp>
#include <anno/test/game_dir.hpp>

namespace sze = anno::formats::sze;
namespace tagged = anno::formats::tagged;

namespace {

struct Stats {
    std::size_t records = 0;
    std::map<std::pair<std::string, std::string>, std::size_t> long_forms;
    std::map<std::string, std::size_t> paths;
};

void walk(const std::vector<tagged::Record>& recs, std::string_view parent, const std::string& path,
          sze::Edition ed, Stats& st) {
    for (const auto& r : recs) {
        ++st.records;
        const std::string p = path + "/" + std::string(r.name);
        ++st.paths[p];
        if (r.long_form) {
            ++st.long_forms[{ed == sze::Edition::Addon ? "ANNOADDON1" : "ANNO",
                             std::string(r.name)}];
        }
        const auto k = sze::nested_prefix(parent, r.name);
        if (!k) {
            continue;
        }
        const auto body = r.payload.subspan(*k);
        auto kids = tagged::parse_records(body);
        INFO(p);
        REQUIRE(kids);
        anno::formats::ByteBuffer again;
        REQUIRE(tagged::write_records(kids.value(), again));
        CHECK(std::equal(again.begin(), again.end(), body.begin(), body.end()));
        walk(kids.value(), r.name, p, ed, st);
    }
}

/// A `\`-separated path below `base`, matched case-insensitively per segment.
std::optional<std::filesystem::path> find_ci(const std::filesystem::path& base,
                                             std::string_view rel) {
    auto lower = [](std::string s) {
        for (auto& c : s) {
            c = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
        }
        return s;
    };
    std::filesystem::path cur = base;
    std::size_t start = 0;
    while (start <= rel.size()) {
        const auto end = std::min(rel.find('\\', start), rel.size());
        const std::string seg = lower(std::string(rel.substr(start, end - start)));
        bool found = false;
        for (const auto& e : std::filesystem::directory_iterator(cur)) {
            if (lower(e.path().filename().string()) == seg) {
                cur = e.path();
                found = true;
                break;
            }
        }
        if (!found) {
            return std::nullopt;
        }
        start = end + 1;
    }
    return cur;
}

} // namespace

TEST_CASE("scene corpus") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping scene corpus: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto files = anno::test::files_with_ext(*dir / "Scenes", "sze");
    REQUIRE(files.size() == 50);
    Stats st;
    std::size_t base = 0, addon = 0, blocks = 0, comp = 0, ships = 0, soldiers = 0, link_files = 0,
                links = 0;
    std::size_t centred[2] = {0, 0};
    std::uint8_t attrs[2] = {0, 0};
    std::set<std::uint16_t> figures;
    std::map<std::string, std::size_t> without_31;
    std::map<std::pair<std::uint32_t, std::string>, std::size_t> heads;
    for (const auto& path : files) {
        const auto buf = anno::test::read_file(path);
        INFO(path.string());
        auto s = sze::Scene::parse(buf);
        REQUIRE(s);
        const auto& scene = s.value();
        (scene.edition == sze::Edition::Base ? base : addon) += 1;
        auto ls = scene.island_links();
        REQUIRE(ls);
        if (!ls.value().empty()) {
            CHECK(scene.edition == sze::Edition::Addon);
            ++link_files;
        }
        std::vector<std::uint32_t> numbers;
        for (const auto& i : anno::formats::insel::islands(buf).value()) {
            numbers.push_back(i.number);
        }
        const auto lchunks = scene.chunks("INSELLNK");
        for (std::size_t i = 0; i < ls.value().size(); ++i) {
            const auto& l = ls.value()[i];
            const auto enc = l.to_bytes();
            CHECK(std::equal(enc.begin(), enc.end(), lchunks[i]->payload.begin(),
                             lchunks[i]->payload.end()));
            numbers.push_back(l.number);
            const std::string rel(l.path.begin(), l.path.end());
            const auto t = find_ci(*dir / "Scenes", rel);
            REQUIRE(t);
            const auto tbuf = anno::test::read_file(*t);
            const auto ti = anno::formats::insel::islands(tbuf).value();
            CHECK(ti[0].version == anno::formats::insel::Version::Insel4);
            CHECK(ti[0].width == l.width);
            CHECK(ti[0].height == l.height);
            ++links;
        }
        std::sort(numbers.begin(), numbers.end());
        CHECK(std::adjacent_find(numbers.begin(), numbers.end()) == numbers.end());
        for (const auto* c : scene.chunks("COMPPROFIL2")) {
            auto recs = tagged::parse_records(c->payload);
            REQUIRE(recs);
            for (const auto& r : recs.value()) {
                CHECK((r.name == "DEF" || r.name == "ENTRY"));
            }
            ++comp;
        }
        auto ps = scene.players();
        REQUIRE(ps);
        const auto& players = ps.value();
        blocks += players.size();
        // Header series (HEAD in every block, player 31 last), then a state
        // series with the same indices where player 31 may be missing.
        const auto split =
            std::find_if(players.begin(), players.end(), [](const auto& p) { return !p.head(); });
        std::vector<std::uint32_t> head_idx, state_idx;
        for (auto it = players.begin(); it != split; ++it) {
            head_idx.push_back(it->index);
        }
        for (auto it = split; it != players.end(); ++it) {
            CHECK_FALSE(tagged::find(it->records, "HEAD"));
            state_idx.push_back(it->index);
        }
        REQUIRE_FALSE(head_idx.empty());
        CHECK(head_idx.back() == 31);
        if (state_idx.size() + 1 == head_idx.size()) {
            CHECK(std::equal(state_idx.begin(), state_idx.end(), head_idx.begin()));
            ++without_31[scene.edition == sze::Edition::Base ? "ANNO" : "ANNOADDON1"];
        } else {
            CHECK(state_idx == head_idx);
        }
        std::set<std::uint16_t> ids;
        const auto spieler = scene.chunks("SPIELER");
        for (std::size_t k = 0; k < players.size(); ++k) {
            const auto& p = players[k];
            CHECK(p.flag == 0);
            const auto enc = *p.to_bytes();
            CHECK(std::equal(enc.begin(), enc.end(), spieler[k]->payload.begin(),
                             spieler[k]->payload.end()));
            walk(p.records, "SPIELER", "SPIELER", scene.edition, st);
            if (auto h = p.head()) {
                const char* colour = h->colour == 0      ? "none"
                                     : h->colour <= 8    ? "preset"
                                     : h->colour == 0xFF ? "at start"
                                                         : "other";
                ++heads[{h->kind, colour}];
                if (p.index == 31) {
                    CHECK((h->kind == 2 || h->kind == 8));
                    CHECK(h->colour == 0);
                }
            }
            for (int kind = 0; kind < 2; ++kind) {
                auto us = p.units(kind == 0 ? "SCHIFF" : "SOLDAT");
                REQUIRE(us);
                for (const auto& u : us.value()) {
                    CHECK(u.owner == p.index);
                    CHECK(u.reserved == 0);
                    CHECK(ids.insert(u.id).second);
                    attrs[kind] = std::max(attrs[kind], u.attr);
                    float ix = 0, iy = 0;
                    if (std::modf(u.x(), &ix) == 0.5f && std::modf(u.y(), &iy) == 0.5f) {
                        ++centred[kind];
                    }
                    if (kind == 0) {
                        figures.insert(u.figure);
                        ++ships;
                    } else {
                        ++soldiers;
                    }
                }
            }
        }
    }
    CHECK(base == 23);
    CHECK(addon == 27);
    CHECK(link_files == 9);
    CHECK(links == 229);
    CHECK(blocks == 724);
    CHECK(comp == 50);
    CHECK(st.records == 29686);
    CHECK(without_31 == std::map<std::string, std::size_t>{{"ANNO", 22}});
    CHECK(st.long_forms == std::map<std::pair<std::string, std::string>, std::size_t>{
                               {{"ANNOADDON1", "PROD"}, 193}, {{"ANNOADDON1", "STADT2"}, 193}});
    CHECK(st.paths["SPIELER/STADT2"] == 272);
    CHECK(st.paths["SPIELER/STADT2/PROD"] == 272);
    CHECK(st.paths["SPIELER/SCHIFF/PRIVAT/NAME"] == 327);
    CHECK(st.paths["SPIELER/SOLDAT/HEAD"] == 2317);
    CHECK(ships == 327);
    CHECK(soldiers == 2317);
    CHECK(figures == std::set<std::uint16_t>{1501, 1502, 1503, 1504, 1505, 1506, 1507, 1508, 1509});
    CHECK(centred[0] == 310);
    CHECK(centred[1] == 2253);
    CHECK(attrs[0] == 14);
    CHECK(attrs[1] == 7);
    using H = std::map<std::pair<std::uint32_t, std::string>, std::size_t>;
    CHECK(heads == H{{{2, "none"}, 47},
                     {{8, "at start"}, 12},
                     {{8, "none"}, 4},
                     {{8, "preset"}, 28},
                     {{11, "at start"}, 8},
                     {{11, "preset"}, 1},
                     {{12, "at start"}, 33},
                     {{12, "preset"}, 47},
                     {{14, "none"}, 80},
                     {{15, "none"}, 41},
                     {{16, "none"}, 26},
                     {{17, "at start"}, 46}});
}
