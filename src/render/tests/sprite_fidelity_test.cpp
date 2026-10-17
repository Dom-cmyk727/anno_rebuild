// Asset fidelity at single-pixel level (PLAN §0.4 (1)): every frame of every
// Anlagen.dat and FigurenHeader.dat sprite set and shadow, at all three zoom
// groups, keeps every covered pixel when decoded, and the blitter
// reproduces the decoded image exactly (shadows as their coverage mask in
// the shadow colour). Counts: docs/render/iso.md, "Fidelity". Needs
// ANNO1503_GAME_DIR; skips otherwise.
#include <map>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/assets/building.hpp>
#include <anno/assets/figure.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/render/iso.hpp>
#include <anno/test/game_dir.hpp>

namespace render = anno::render;
namespace fig = anno::formats::figuren;

namespace {

/// Draws `img` with the blitter onto a transparent canvas covering exactly
/// the image and compares pixel by pixel; the number of covered pixels, or
/// −1 on the first mismatch.
long check_blit(const anno::assets::Rgba& img, const fig::Frame& frame, bool shadow) {
    const std::pair<int, int> anchor{1000, 2000};
    render::Canvas canvas;
    canvas.width = img.width;
    canvas.height = img.height;
    canvas.origin = {anchor.first - frame.anchor_x, anchor.second - frame.anchor_y};
    canvas.pixels.assign(canvas.width * canvas.height * 4, 0);
    render::blit_sprite(canvas, img, frame, anchor, shadow,
                        [](int, int, render::Px p) -> std::optional<render::Px> { return p; });
    long covered = 0;
    for (std::size_t i = 0; i < canvas.width * canvas.height; ++i) {
        const std::uint8_t* want = &img.pixels[i * 4];
        render::Px expect{0, 0, 0, 0};
        if (want[3] != 0) {
            expect = shadow ? render::kShadow : render::Px{want[0], want[1], want[2], want[3]};
            ++covered;
        }
        const std::uint8_t* got = &canvas.pixels[i * 4];
        if (render::Px{got[0], got[1], got[2], got[3]} != expect) {
            return -1;
        }
    }
    return covered;
}

struct Tally {
    std::size_t frames = 0, pixels = 0, widened = 0, restored = 0;
    int max_overrun = 0;
};

std::vector<std::pair<const fig::GfxSet*, bool>> sets(const std::vector<fig::GfxSet>& gfx) {
    std::vector<std::pair<const fig::GfxSet*, bool>> out;
    for (const auto& s : gfx) {
        out.emplace_back(&s, false);
        if (s.shadow) {
            out.emplace_back(s.shadow.get(), true);
        }
    }
    return out;
}

} // namespace

TEST_CASE("Anlagen.dat frames are pixel exact") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto buf = anno::test::read_file(*dir / "Anlagen.dat");
    const auto file = anno::formats::anlagen::AnlagenFile::parse(buf).value();
    std::map<std::uint32_t, Tally> tally;
    std::size_t failed = 0, mismatched = 0;
    for (const auto& e : file.entries) {
        for (const auto& [set, shadow] : sets(e.gfx)) {
            for (const auto& fs : set->frame_sets) {
                for (const auto& f : fs.frames) {
                    if (f.length == 0) {
                        continue;
                    }
                    auto img = anno::assets::building::to_rgba(file, e, fs.group, f);
                    if (!img) {
                        ++failed;
                        continue;
                    }
                    Tally& t = tally[fs.group];
                    const int over = int{img.value().width} - int{f.width};
                    REQUIRE(over >= 0);
                    REQUIRE(img.value().height == f.height);
                    const auto data = file.volume(fs.group)->data.subspan(f.offset, f.length);
                    const auto ext =
                        fig::decode_frame_extended(*f.format(), data, f.width, f.height).value();
                    std::size_t coverage = 0;
                    for (const auto& p : ext.first) {
                        coverage += p ? 1 : 0;
                    }
                    const long covered = check_blit(img.value(), f, shadow);
                    if (covered < 0 || static_cast<std::size_t>(covered) != coverage) {
                        ++mismatched;
                        continue;
                    }
                    ++t.frames;
                    t.pixels += coverage;
                    t.widened += over > 0 ? 1 : 0;
                    t.max_overrun = std::max(t.max_overrun, over);
                    t.restored += fig::decode_frame_clipped(*f.format(), data, f.width, f.height)
                                      .value()
                                      .second;
                }
            }
        }
    }
    CHECK(failed == 0);
    CHECK(mismatched == 0);
    // (frames, covered pixels, widened frames, largest overrun, restored
    // pixels) per zoom group 0, 1, 2.
    const std::array<std::tuple<std::size_t, std::size_t, std::size_t, int, std::size_t>, 3>
        expect = {{{9506, 78213833, 154, 16, 868},
                   {9506, 19955171, 179, 9, 587},
                   {9506, 4948688, 129, 4, 240}}};
    int max = 0;
    for (std::uint32_t g = 0; g < 3; ++g) {
        const Tally& t = tally[g];
        CHECK(std::tuple{t.frames, t.pixels, t.widened, t.max_overrun, t.restored} == expect[g]);
        max = std::max(max, t.max_overrun);
    }
    // kOverrunSlack is the corpus maximum.
    CHECK(max == render::kOverrunSlack);
}

TEST_CASE("figure frames are pixel exact") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto buf = anno::test::read_file(*dir / "FigurenHeader.dat");
    const auto header = fig::FigurenHeader::parse(buf).value();
    std::vector<std::vector<std::uint8_t>> vols(3);
    for (std::uint32_t g = 0; g < 3; ++g) {
        for (std::uint32_t part = 0;; ++part) {
            const auto p = *dir / fig::volume_file_name(g, part);
            if (!std::filesystem::exists(p)) {
                break;
            }
            const auto d = anno::test::read_file(p);
            vols[g].insert(vols[g].end(), d.begin(), d.end());
        }
    }
    std::map<std::uint32_t, Tally> tally;
    std::size_t failed = 0, mismatched = 0;
    for (const auto& e : header.entries) {
        for (const auto& [set, shadow] : sets(e.gfx)) {
            for (const auto& fs : set->frame_sets) {
                for (const auto& f : fs.frames) {
                    if (f.length == 0) {
                        continue;
                    }
                    auto img = anno::assets::figure::to_rgba(e, f, vols.at(fs.group));
                    if (!img) {
                        ++failed;
                        continue;
                    }
                    REQUIRE(img.value().width == f.width);
                    REQUIRE(img.value().height == f.height);
                    const long covered = check_blit(img.value(), f, shadow);
                    if (covered < 0) {
                        ++mismatched;
                        continue;
                    }
                    tally[fs.group].pixels += static_cast<std::size_t>(covered);
                    ++tally[fs.group].frames;
                }
            }
        }
    }
    CHECK(failed == 0);
    CHECK(mismatched == 0);
    const std::array<std::pair<std::size_t, std::size_t>, 3> expect = {
        {{241808, 145609961}, {241808, 39471049}, {241808, 9570884}}};
    for (std::uint32_t g = 0; g < 3; ++g) {
        CHECK(std::pair{tally[g].frames, tally[g].pixels} == expect[g]);
    }
}
