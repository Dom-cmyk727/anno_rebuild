// FigurenHeader.dat + Figuren*.dat and Anlagen.dat corpus tests (needs
// ANNO1503_GAME_DIR). Figures: docs/formats/figuren.md, docs/formats/anlagen.md.
#include <algorithm>
#include <map>
#include <tuple>

#include <doctest/doctest.h>

#include <anno/formats/anlagen.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/test/game_dir.hpp>

namespace fig = anno::formats::figuren;
using anno::formats::Bytes;

namespace {

struct Volume {
    std::vector<std::uint8_t> data;
    std::vector<std::uint64_t> sizes;
};

Volume volume(const std::filesystem::path& dir, std::uint32_t group) {
    Volume v;
    for (std::uint32_t part = 0;; ++part) {
        const auto p = dir / fig::volume_file_name(group, part);
        if (!std::filesystem::exists(p)) {
            break;
        }
        const auto b = anno::test::read_file(p);
        v.sizes.push_back(b.size());
        v.data.insert(v.data.end(), b.begin(), b.end());
    }
    return v;
}

struct Ref {
    std::uint32_t group;
    bool shadow;
    const fig::Frame* frame;
};

} // namespace

TEST_CASE("figuren corpus") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping figuren corpus: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto buf = anno::test::read_file(*dir / "FigurenHeader.dat");
    auto parsed = fig::FigurenHeader::parse(buf);
    REQUIRE(parsed);
    const auto& h = parsed.value();
    CHECK(h.to_bytes() == buf);

    REQUIRE(h.entries.size() == 368);
    CHECK(std::is_sorted(h.entries.begin(), h.entries.end(),
                         [](const auto& a, const auto& b) { return a.id < b.id; }));
    CHECK(h.entries.front().id == 1);
    CHECK(h.entries.back().id == 1666);
    std::map<std::size_t, std::size_t> palettes;
    for (const auto& e : h.entries) {
        CHECK(e.flag == 0);
        ++palettes[e.palettes.size()];
    }
    CHECK(palettes == std::map<std::size_t, std::size_t>{{0, 1}, {1, 282}, {2, 60}, {3, 25}});

    std::size_t sets = 0, shadows = 0;
    std::vector<Ref> frames;
    for (const auto& e : h.entries) {
        for (const auto& g : e.gfx) {
            ++sets;
            std::vector<std::uint32_t> groups;
            std::vector<std::size_t> counts;
            for (const auto& fs : g.frame_sets) {
                groups.push_back(fs.group);
                counts.push_back(fs.frames.size());
            }
            CHECK((groups.empty() || groups == std::vector<std::uint32_t>{0, 1, 2}));
            CHECK(std::adjacent_find(counts.begin(), counts.end(), std::not_equal_to<>()) ==
                  counts.end());
            if (g.shadow) {
                ++shadows;
                CHECK(std::vector<std::uint8_t>(g.shadow->prefix.begin(), g.shadow->prefix.end()) ==
                      std::vector<std::uint8_t>{3, 0, 0, 0, 0, 0, 0, 0});
                std::vector<std::size_t> sc;
                for (const auto& fs : g.shadow->frame_sets) {
                    sc.push_back(fs.frames.size());
                    for (const auto& f : fs.frames) {
                        frames.push_back({fs.group, true, &f});
                    }
                }
                CHECK(sc == counts);
            }
            for (const auto& fs : g.frame_sets) {
                for (const auto& f : fs.frames) {
                    CHECK(f.palette < e.palettes.size());
                    frames.push_back({fs.group, false, &f});
                }
            }
        }
    }
    CHECK(sets == 1288);
    CHECK(shadows == 1047);
    CHECK(frames.size() == 731121);

    REQUIRE(h.volumes.size() == 3);
    for (std::uint32_t g = 0; g < 3; ++g) {
        CHECK(h.volumes[g].group == g);
        CHECK(h.volumes[g].frames == 243707);
        CHECK(std::count_if(frames.begin(), frames.end(),
                            [&](const Ref& r) { return r.group == g; }) == 243707);
    }

    std::map<std::string, std::size_t> formats;
    for (const auto& r : frames) {
        const auto f = r.frame->format();
        REQUIRE(f);
        CHECK(r.shadow == (*f == fig::FrameFormat::Alpha));
        CHECK(r.frame->unknown_06 == 8);
        CHECK(r.frame->unknown_20 == 0);
        ++formats[r.frame->format_name()];
    }
    CHECK(formats == std::map<std::string, std::size_t>{{"BSH_ALPHA", 347058},
                                                        {"BSH_RGBAV", 3837},
                                                        {"BSH_RGBAZ", 504},
                                                        {"BSH_RGBH", 24},
                                                        {"BSH_RGBV", 215178},
                                                        {"BSH_RGBZ", 164520}});

    // Frame ranges tile each group's files exactly; every frame decodes.
    std::uint64_t root_payload = buf.size() - 16;
    std::size_t decoded = 0;
    for (const auto& vh : h.volumes) {
        const Volume v = volume(*dir, vh.group);
        std::uint64_t total = 0;
        for (auto s : v.sizes) {
            total += s;
        }
        root_payload += total;
        CHECK(vh.size == total + 8);
        std::vector<const fig::Frame*> ranges;
        for (const auto& r : frames) {
            if (r.group == vh.group && r.frame->length > 0) {
                ranges.push_back(r.frame);
            }
        }
        auto key = [](const fig::Frame* f) { return std::pair{f->offset, f->length}; };
        std::sort(ranges.begin(), ranges.end(), [&](auto a, auto b) { return key(a) < key(b); });
        ranges.erase(std::unique(ranges.begin(), ranges.end(),
                                 [&](auto a, auto b) { return key(a) == key(b); }),
                     ranges.end());
        std::uint64_t pos = 0;
        for (const fig::Frame* f : ranges) {
            REQUIRE(f->offset == pos);
            pos += f->length;
            CHECK(fig::locate(v.sizes, f->offset, f->length).has_value());
            auto px = fig::decode_frame(*f->format(), Bytes(v.data.data() + f->offset, f->length),
                                        f->width, f->height);
            REQUIRE(px);
            CHECK(px.value().size() == std::size_t{f->width} * f->height);
            ++decoded;
        }
        CHECK(pos == total);
    }
    CHECK(decoded == 655842);
    CHECK(anno::formats::u32_le(h.root_header, 12) == root_payload);
    CHECK(std::count_if(frames.begin(), frames.end(),
                        [](const Ref& r) { return r.frame->length == 0; }) == 5697);
}

TEST_CASE("anlagen corpus") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping anlagen corpus: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto buf = anno::test::read_file(*dir / "Anlagen.dat");
    auto parsed = anno::formats::anlagen::AnlagenFile::parse(buf);
    REQUIRE(parsed);
    const auto& file = parsed.value();
    CHECK(file.colors.size() == 440);
    REQUIRE(file.entries.size() == 921);
    CHECK(file.entries.front().id == 1);
    CHECK(file.entries.back().id == 3998);
    std::map<std::size_t, std::size_t> lists;
    for (std::size_t i = 0; i < file.entries.size(); ++i) {
        const auto& e = file.entries[i];
        const auto enc = e.to_bytes();
        CHECK(std::equal(enc.begin(), enc.end(), file.entry_chunks[i].payload.begin(),
                         file.entry_chunks[i].payload.end()));
        CHECK(e.palettes.empty());
        ++lists[e.palette_ids.size()];
    }
    CHECK(lists == std::map<std::size_t, std::size_t>{{0, 30}, {1, 640}, {2, 195}, {3, 56}});

    std::vector<std::pair<std::uint32_t, const fig::Frame*>> frames;
    std::size_t sets = 0, shadows = 0;
    for (const auto& e : file.entries) {
        for (const auto& g : e.gfx) {
            ++sets;
            for (const auto& fs : g.frame_sets) {
                for (const auto& f : fs.frames) {
                    REQUIRE(f.palette < e.palette_ids.size());
                    const auto id = e.palette_ids[f.palette];
                    CHECK((id >= 1 && id <= 440));
                    frames.emplace_back(fs.group, &f);
                }
            }
            if (g.shadow) {
                ++shadows;
                for (const auto& fs : g.shadow->frame_sets) {
                    for (const auto& f : fs.frames) {
                        frames.emplace_back(fs.group, &f);
                    }
                }
            }
        }
    }
    CHECK(sets == 1031);
    CHECK(shadows == 110);
    CHECK(frames.size() == 3 * 9633);
    std::map<std::string, std::size_t> formats;
    for (const auto& [g, f] : frames) {
        CHECK(f->format().has_value());
        ++formats[f->format_name()];
    }
    CHECK(formats == std::map<std::string, std::size_t>{{"BSH_ALPHA", 981},
                                                        {"BSH_RGBAH", 120},
                                                        {"BSH_RGBAV", 7470},
                                                        {"BSH_RGBAZ", 6669},
                                                        {"BSH_RGBH", 108},
                                                        {"BSH_RGBV", 1899},
                                                        {"BSH_RGBWZ", 3528},
                                                        {"BSH_RGBZ", 7212},
                                                        {"BSH_RGBZM", 912}});

    std::size_t decoded = 0;
    std::map<std::string, std::size_t> clipped;
    for (const auto& v : file.volumes) {
        std::vector<const fig::Frame*> ranges;
        std::size_t n = 0;
        for (const auto& [g, f] : frames) {
            if (g != v.group) {
                continue;
            }
            ++n;
            if (f->length > 0) {
                ranges.push_back(f);
            }
        }
        CHECK(n == v.frames);
        auto key = [](const fig::Frame* f) { return std::pair{f->offset, f->length}; };
        std::sort(ranges.begin(), ranges.end(), [&](auto a, auto b) { return key(a) < key(b); });
        ranges.erase(std::unique(ranges.begin(), ranges.end(),
                                 [&](auto a, auto b) { return key(a) == key(b); }),
                     ranges.end());
        std::size_t pos = 0;
        for (const fig::Frame* f : ranges) {
            REQUIRE(f->offset == pos);
            pos += f->length;
            const Bytes data = v.data.subspan(f->offset, f->length);
            if (!fig::decode_frame(*f->format(), data, f->width, f->height)) {
                auto c = fig::decode_frame_clipped(*f->format(), data, f->width, f->height);
                REQUIRE(c);
                CHECK(c.value().second > 0);
                ++clipped[f->format_name()];
            }
            ++decoded;
        }
        CHECK(pos == v.data.size());
    }
    CHECK(decoded == 6484 + 6452 + 6473);
    CHECK(clipped ==
          std::map<std::string, std::size_t>{
              {"BSH_RGBAZ", 26}, {"BSH_RGBWZ", 398}, {"BSH_RGBZ", 19}, {"BSH_RGBZM", 6}});
}
