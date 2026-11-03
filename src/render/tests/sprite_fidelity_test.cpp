// Asset fidelity at single-pixel level (PLAN §0.4 (1)): every frame of every
// Anlagen.dat and FigurenHeader.dat sprite set and shadow, at all three zoom
// groups, keeps every covered pixel when decoded, and the blitter
// reproduces the decoded image exactly (shadows as their coverage mask in
// the shadow colour). Counts: docs/render/iso.md, "Fidelity". Needs
// ANNO1503_GAME_DIR; skips otherwise.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/assets/building.hpp>
#include <anno/assets/figure.hpp>
#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/formats/insel.hpp>
#include <anno/formats/tex.hpp>
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
                        coverage += p ? 1U : 0U;
                    }
                    const long covered = check_blit(img.value(), f, shadow);
                    if (covered < 0 || static_cast<std::size_t>(covered) != coverage) {
                        ++mismatched;
                        continue;
                    }
                    ++t.frames;
                    t.pixels += coverage;
                    t.widened += over > 0 ? 1U : 0U;
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

TEST_CASE("non-square buildings: every rotation block fits its footprint in the original's "
          "handedness") {
    // P (docs/render/iso.md, "Tiles"; tools/footprint_handedness.py is the
    // Python oracle with the same counts): for every Anlagen.dat entry with a
    // non-square footprint w x h (HEAD bytes 0-1) and four rotation blocks,
    // the first frame of each block is placed by place_prepared at rotation r
    // with the footprint the sim uses ((w, h) for even r, (h, w) for odd r) at
    // zoom group 2. The lowest opaque row of the sprite is the footprint's
    // bottom vertex, corner (x + w', y + h'): its mean column must lie nearer
    // tile_top's projection of that corner than the mirrored projection's
    // (the anchor minus the same offset), which this renderer used before
    // 2026-11-03.
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto buf = anno::test::read_file(*dir / "Anlagen.dat");
    const auto anlagen = anno::formats::anlagen::AnlagenFile::parse(buf).value();
    constexpr std::size_t kGroup = 2;
    constexpr std::uint16_t kAt = 10;
    render::SpriteCache cache;
    std::size_t types = 0, unanimous = 0, majority = 0, blocks = 0, fits = 0, mirrored_fits = 0;
    for (const auto& e : anlagen.entries) {
        const auto fp = anlagen.footprint(e.id);
        if (!fp || fp->first == fp->second || e.gfx.empty()) {
            continue;
        }
        const fig::GfxSet& set = e.gfx.front();
        const std::size_t anim = render::animation(set).first;
        const fig::FrameSet* frames = nullptr;
        for (const auto& fs : set.frame_sets) {
            if (fs.group == kGroup) {
                frames = &fs;
            }
        }
        if (frames == nullptr || anim == 0 || frames->frames.size() != 4 * anim) {
            continue;
        }
        ++types;
        std::size_t type_fits = 0;
        for (std::uint8_t r = 0; r < 4; ++r) {
            anno::formats::insel::PlacedObject o;
            o.anlage = static_cast<std::uint16_t>(e.id);
            o.rotation = r;
            o.x = kAt;
            o.y = kAt;
            const render::Prepared p{o, o.footprint(fp->first, fp->second)};
            std::vector<render::SpriteDraw> draws;
            render::place_prepared(
                std::span<const render::Prepared>(&p, 1), anlagen, kGroup, render::kNoLift, 0,
                [](int, int, int, int) { return true; }, draws);
            REQUIRE(!draws.empty());
            const render::SpriteDraw& d = draws.back();
            REQUIRE(!d.is_shadow());
            const auto img = cache.get(anlagen, d.key, *d.frame);
            REQUIRE(img);
            // Mean column of the lowest opaque row.
            std::size_t low = 0, sum = 0, n = 0;
            for (std::size_t i = 0; i < img->pixels.size() / 4; ++i) {
                if (img->pixels[i * 4 + 3] == 0) {
                    continue;
                }
                const std::size_t row = i / img->width, col = i % img->width;
                if (row > low || n == 0) {
                    low = row, sum = 0, n = 0;
                }
                if (row == low) {
                    sum += col, ++n;
                }
            }
            REQUIRE(n > 0);
            const double measured = d.anchor.first - d.frame->anchor_x +
                                    static_cast<double>(sum) / static_cast<double>(n);
            const auto [w, h] = p.second;
            const double vertex = render::tile_top(kAt + w, kAt + h, kGroup).first;
            const double mirrored = 2.0 * d.anchor.first - vertex;
            ++blocks;
            if (std::fabs(measured - vertex) < std::fabs(measured - mirrored)) {
                ++fits, ++type_fits;
            } else if (std::fabs(measured - mirrored) < std::fabs(measured - vertex)) {
                ++mirrored_fits;
            }
        }
        unanimous += type_fits == 4 ? 1 : 0;
        majority += type_fits >= 3 ? 1 : 0;
    }
    // The misses are types with |w - h| = 1 (a 5 px difference at group 2: 1416,
    // 1603, 1604, 2907, 2908, 3510, one block each) and 3405 (7 x 6, all four).
    CHECK(types == 69);
    CHECK(blocks == 276);
    CHECK(fits == 266);
    CHECK(mirrored_fits == 10);
    CHECK(unanimous == 62);
    CHECK(majority == 68);
}

TEST_CASE("ground texels: the edge-colour error selects GroundCell::texel among the 16 "
          "orientations") {
    // P (docs/render/iso.md, "Tiles", re-run 2026-11-03 with the projection
    // corrected): of the 8 assignments of the tile axes to the texture axes
    // (the dihedral group of the square) x 2 directions of the cell's
    // quarter turns, the one GroundCell::texel implements has the lowest
    // colour difference across the edges that neighbouring tiles share. The
    // measure pairs texels of tiles (x, y) and (x + 1, y) or (x, y + 1) along
    // their shared edge, so no projection enters it: it is the same in the
    // mirrored projection and in the original's. The D evidence for the
    // mapping (AnnoWelt.dll mesh builder, docs/re/annowelt.md) is independent.
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    namespace insel = anno::formats::insel;
    const auto felder_buf = anno::test::read_file(*dir / "Textures" / "Felder.tex");
    const auto felder = anno::formats::tex::TexFile::parse(felder_buf).value();
    const auto textures = anno::assets::island::GroundTextures::load(felder).value();
    constexpr std::size_t n = 32;
    // (s, t) -> the candidate's position before the cell's quarter turns.
    auto axes = [](int g, std::size_t s, std::size_t t) {
        const std::size_t is = n - 1 - s, it = n - 1 - t;
        switch (g) {
        case 0:
            return std::pair{s, t};
        case 1:
            return std::pair{t, s};
        case 2:
            return std::pair{is, t};
        case 3:
            return std::pair{s, it};
        case 4:
            return std::pair{is, it};
        case 5:
            return std::pair{t, is};
        case 6:
            return std::pair{it, s};
        default:
            return std::pair{it, is};
        }
    };
    for (const char* climate : {"Polar", "Nord"}) {
        std::vector<std::filesystem::path> files;
        for (const auto& f :
             std::filesystem::directory_iterator(*dir / "Scenes" / "Islands" / climate)) {
            files.push_back(f.path());
        }
        std::sort(files.begin(), files.end());
        files.resize(std::min<std::size_t>(files.size(), 6));
        std::array<double, 16> error{};
        std::size_t pairs = 0;
        for (const auto& path : files) {
            const auto file = anno::test::read_file(path);
            const auto islands = insel::islands(file).value();
            REQUIRE(!islands.empty());
            const insel::Island& island = islands.front();
            const auto gfx = island.gfx_ids().value();
            const std::size_t w = island.width, h = island.height;
            auto cell = [&](std::size_t x, std::size_t y) {
                return insel::GroundCell::from_gfx_id(gfx[y * w + x]);
            };
            for (std::size_t y = 0; y < h; ++y) {
                for (std::size_t x = 0; x < w; ++x) {
                    const auto a = cell(x, y);
                    for (const auto [nx, ny] : {std::pair{x + 1, y}, std::pair{x, y + 1}}) {
                        if (nx >= w || ny >= h || a.texture == 0) {
                            continue;
                        }
                        const auto b = cell(nx, ny);
                        if (b.texture == 0) {
                            continue;
                        }
                        ++pairs;
                        for (int c = 0; c < 16; ++c) {
                            auto turned = [c](insel::GroundCell k) {
                                if (c >= 8) {
                                    k.rotation = static_cast<std::uint8_t>((4 - k.rotation) % 4);
                                }
                                return k;
                            };
                            double sum = 0;
                            for (std::size_t k = 0; k < n; ++k) {
                                // Along the shared edge: A's far side, B's near side.
                                const auto [as, at] =
                                    nx > x ? std::pair{n - 1, k} : std::pair{k, n - 1};
                                const auto [bs, bt] = nx > x ? std::pair{std::size_t{0}, k}
                                                             : std::pair{k, std::size_t{0}};
                                const auto pa = axes(c % 8, as, at), pb = axes(c % 8, bs, bt);
                                const auto ca =
                                    textures.cell_pixel(turned(a), pa.first, pa.second, n);
                                const auto cb =
                                    textures.cell_pixel(turned(b), pb.first, pb.second, n);
                                for (std::size_t ch = 0; ch < 3; ++ch) {
                                    sum += std::abs(int{ca[ch]} - int{cb[ch]});
                                }
                            }
                            error[static_cast<std::size_t>(c)] += sum / static_cast<double>(n);
                        }
                    }
                }
            }
        }
        REQUIRE(pairs > 1000);
        for (double& e : error) {
            e /= static_cast<double>(pairs);
        }
        const double best = *std::min_element(error.begin() + 1, error.end());
        MESSAGE(std::string(climate) << ": " << pairs << " edges, GroundCell::texel " << error[0]
                                     << ", best other " << best);
        CHECK(error[0] < best);
    }
}
