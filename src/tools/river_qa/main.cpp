// river_qa: measures gaps and connectivity of enhanced river water against the
// original's river sprites, per scene and zoom group (QA harness for the
// "river gaps" report of 2026-10-19).
//
//   river_qa <game_dir> [--groups 0,1,2] [--dump <dir>] [--per-island]
//            [--max-mask-px N] <file.sze|.scp>...
//
// For every island that has river, waterfall or river-mouth pieces, the
// islands's rectangle is processed in the viewer's own 512 x 512 chunks
// (render::kChunk) with the viewer's own calls (Scene::ground_with_probe,
// Scene::water_field_from), so the field measured here is exactly the one
// the CPU path and the GPU compositor both draw. Per pixel:
//   O = the original's river sprite is drawn here (Scene::river_sprites,
//       alpha >= 128, depth-tested as the original; the sprites are a 50 %
//       checkerboard, so a pixel counts when any of its 3 x 3 neighbours is drawn),
//   W = enhanced water is drawn here (field cover != 0 and depth >= 0, i.e.
//       the smooth-edge alpha is >= 0.5; river or sea water),
//   R = W and the river flag (bit 1) is set,
//   edge = O and not W, but water within `reach` pixels (th/10 + 1: the soft
//       edges of sprite and water differ by a pixel or two; not a gap),
//   gap = O and no W within `reach` pixels (the owner's "brown bed, no water"),
//   extra = R and not O (enhanced river water outside the original sprites).
// Connectivity (8-neighbourhood) is measured on a mask sampled every `s`
// pixels (s a power of two so that the mask has at most --max-mask-px
// samples; s = 1 for the usual island at groups 1 and 2): components of O,
// components of E = W and (R or O); an O component is "split" when it
// contains pixels of more than one E component, "lost" when it contains
// none.
// Output: one Markdown table row per scene and group (sums over islands),
// optionally per island; --dump writes a colour mask PNG per island (gap red,
// original sprite with water green, enhanced river water only blue; each
// mask pixel is the worst case of its s x s block).
// Overlapping island rectangles count shared pixels twice (rare).
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <anno/app/cli.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/png.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>

namespace {

namespace fs = std::filesystem;
namespace render = anno::render;

constexpr std::array<const char*, 8> kCauseNames = {
    "no level",       "sea/other",    "bed 0..2 above",       "bed 2..4 above",
    "bed 4..8 above", "bed >8 above", "has level, not river", "outside island"};
constexpr std::uint8_t kO = 1, kE = 2, kGap = 4;

struct Totals {
    std::uint64_t o = 0, covered = 0, gap = 0, raw_gap = 0, extra = 0, river = 0;
    std::size_t o_comps = 0, e_comps = 0, split = 0, lost = 0, gap_comps = 0, islands = 0;
    std::size_t gap_max_w = 0;
    /// Gap pixels by cause (kClassNames).
    std::array<std::uint64_t, 8> cause{};
    void add(const Totals& t) {
        o += t.o;
        covered += t.covered;
        gap += t.gap;
        raw_gap += t.raw_gap;
        extra += t.extra;
        river += t.river;
        o_comps += t.o_comps;
        e_comps += t.e_comps;
        split += t.split;
        lost += t.lost;
        gap_comps += t.gap_comps;
        islands += t.islands;
        gap_max_w = std::max(gap_max_w, t.gap_max_w);
        for (std::size_t i = 0; i < cause.size(); ++i) {
            cause[i] += t.cause[i];
        }
    }
};

/// 8-connected components of the samples with `bit` set; labels start at 1.
std::vector<std::int32_t> label(const std::vector<std::uint8_t>& m, std::uint8_t bit, std::size_t w,
                                std::size_t h, std::size_t& count) {
    std::vector<std::int32_t> l(m.size(), 0);
    std::vector<std::size_t> stack;
    count = 0;
    for (std::size_t i = 0; i < m.size(); ++i) {
        if ((m[i] & bit) == 0 || l[i] != 0) {
            continue;
        }
        const auto id = static_cast<std::int32_t>(++count);
        l[i] = id;
        stack.push_back(i);
        while (!stack.empty()) {
            const std::size_t p = stack.back();
            stack.pop_back();
            const std::size_t x = p % w, y = p / w;
            for (std::size_t yy = y == 0 ? 0 : y - 1; yy <= std::min(y + 1, h - 1); ++yy) {
                for (std::size_t xx = x == 0 ? 0 : x - 1; xx <= std::min(x + 1, w - 1); ++xx) {
                    const std::size_t q = yy * w + xx;
                    if ((m[q] & bit) != 0 && l[q] == 0) {
                        l[q] = id;
                        stack.push_back(q);
                    }
                }
            }
        }
    }
    return l;
}

struct Options {
    std::vector<std::size_t> groups = {0, 1, 2};
    fs::path dump;
    bool per_island = false;
    /// Print river piece statistics (heights against the terrain) per island.
    bool pieces = false;
    /// Print the bounding box of every split or lost river component.
    bool list = false;
    std::uint64_t max_mask = 48ULL << 20;
    std::size_t threads = std::max(1U, std::thread::hardware_concurrency());
};

/// Why the enhanced river water is missing at pixel i of a chunk (index into
/// kCauseNames): the same tests as sea::water_field.
std::size_t cause_of(const render::Scene& scene, const render::sea::SceneProbe& probe,
                     std::size_t i) {
    namespace sea = render::sea;
    const std::uint32_t island = probe.island[i];
    if (island == sea::kNoIsland) {
        return 7;
    }
    const sea::IslandWater* iw = scene.island_water_info(island);
    if (iw == nullptr) {
        return 7;
    }
    const auto [x, y] = probe.pos[i];
    const std::size_t tx = std::min(static_cast<std::size_t>(std::max(x, 0.0F)), iw->w - 1);
    const std::size_t ty = std::min(static_cast<std::size_t>(std::max(y, 0.0F)), iw->h - 1);
    const float fx = std::clamp(x - static_cast<float>(tx), 0.0F, 1.0F);
    const float fy = std::clamp(y - static_cast<float>(ty), 0.0F, 1.0F);
    const float smooth = sea::smooth_height(iw->heights, iw->w, iw->h, x, y);
    const auto level = iw->river_level(tx, ty, fx, fy);
    if (!level) {
        return 0;
    }
    if (static_cast<float>(render::kWaterline) - smooth > 0.0F) {
        return 1;
    }
    const float bed = sea::river_bed_height(iw->heights, iw->w, iw->h, x, y) +
                      sea::height_noise(x, y) * sea::kRiverBankNoise;
    const float rd = *level - bed;
    if (rd > -render::kWaves.shore_edge) {
        return 6;
    }
    const float above = -rd;
    return above <= 2.0F ? 2 : above <= 4.0F ? 3 : above <= 8.0F ? 4 : 5;
}

Totals measure_island(const render::Scene& scene, std::size_t group, const render::Placement& pl,
                      const Options& opt, const std::string& tag) {
    Totals tot;
    const render::Rect b = pl.bounds;
    const int cx0 = render::div_euclid(b.x, render::kChunk);
    const int cy0 = render::div_euclid(b.y, render::kChunk);
    const int cx1 = render::div_euclid(b.x + b.w - 1, render::kChunk);
    const int cy1 = render::div_euclid(b.y + b.h - 1, render::kChunk);
    const int ncx = cx1 - cx0 + 1, ncy = cy1 - cy0 + 1;
    const std::uint64_t full = std::uint64_t(ncx) * std::uint64_t(ncy) *
                               static_cast<std::uint64_t>(render::kChunk) *
                               static_cast<std::uint64_t>(render::kChunk);
    int s = 1;
    while (full / (std::uint64_t(s) * std::uint64_t(s)) > opt.max_mask) {
        s *= 2;
    }
    const auto mw = static_cast<std::size_t>(ncx * render::kChunk / s);
    const auto mh = static_cast<std::size_t>(ncy * render::kChunk / s);
    std::vector<std::uint8_t> mask(mw * mh, 0);
    std::atomic<std::size_t> next{0};
    std::mutex mutex;
    // A gap pixel has no enhanced water within `reach` pixels: thinner
    // differences are the sprite's soft edge against the water's soft edge.
    const int reach = static_cast<int>(render::kTile[group].h) / 10 + 1;
    const auto chunks = static_cast<std::size_t>(ncx * ncy);
    auto worker = [&] {
        Totals local;
        for (;;) {
            const std::size_t k = next.fetch_add(1);
            if (k >= chunks) {
                break;
            }
            const int cx = cx0 + static_cast<int>(k % static_cast<std::size_t>(ncx));
            const int cy = cy0 + static_cast<int>(k / static_cast<std::size_t>(ncx));
            const render::Rect rect = render::chunk_rect(cx, cy);
            const auto [ground, probe] = scene.ground_with_probe(rect, group);
            const render::Canvas sprites = scene.river_sprites(rect, probe, group);
            const auto field = scene.water_field_from(rect, probe, group);
            // The river sprites are a 50 % checkerboard (BSH_RGBWZ): a pixel counts as
            // covered when any pixel of its 3 x 3 neighbourhood is drawn.
            // Isolated sprite pixels (fewer than 3 drawn pixels in the 5 x 5
            // neighbourhood, the stray pixels at a bed's rim where the original's depth
            // test passes by chance) are not river and are ignored.
            auto drawn = [&](int x, int y) {
                return x >= 0 && y >= 0 && x < rect.w && y < rect.h &&
                       sprites.pixels[static_cast<std::size_t>(y * rect.w + x) * 4 + 3] >= 128;
            };
            std::vector<std::uint8_t> sprite(field.cover.size(), 0);
            for (int y = 0; y < rect.h; ++y) {
                for (int x = 0; x < rect.w; ++x) {
                    if (!drawn(x, y)) {
                        continue;
                    }
                    int near = 0;
                    for (int yy = y - 2; yy <= y + 2; ++yy) {
                        for (int xx = x - 2; xx <= x + 2; ++xx) {
                            near += drawn(xx, yy) ? 1 : 0;
                        }
                    }
                    if (near < 4) { // itself plus at least three others
                        continue;
                    }
                    for (int yy = std::max(y - 1, 0); yy <= std::min(y + 1, rect.h - 1); ++yy) {
                        for (int xx = std::max(x - 1, 0); xx <= std::min(x + 1, rect.w - 1); ++xx) {
                            sprite[static_cast<std::size_t>(yy * rect.w + xx)] = 1;
                        }
                    }
                }
            }
            std::vector<std::uint8_t> water(field.cover.size(), 0);
            for (std::size_t i = 0; i < water.size(); ++i) {
                const float sd = (static_cast<float>(field.depth[i]) - render::sea::kDepthBias) /
                                 render::sea::kDepthScale;
                water[i] = field.cover[i] != 0 && sd >= 0.0F;
            }
            // Water within `reach` pixels (out of the chunk counts as water).
            auto near_water = [&](int x, int y) {
                for (int yy = y - reach; yy <= y + reach; ++yy) {
                    for (int xx = x - reach; xx <= x + reach; ++xx) {
                        if (xx < 0 || yy < 0 || xx >= rect.w || yy >= rect.h ||
                            water[static_cast<std::size_t>(yy * rect.w + xx)] != 0) {
                            return true;
                        }
                    }
                }
                return false;
            };
            for (int y = 0; y < rect.h; ++y) {
                for (int x = 0; x < rect.w; ++x) {
                    const auto i = static_cast<std::size_t>(y * rect.w + x);
                    const bool O = sprite[i] != 0;
                    const bool W = water[i] != 0;
                    const bool R = W && (field.flags[i] & 2U) != 0;
                    const bool E = W && (R || O);
                    const bool raw_gap = O && !W;
                    const bool gap = raw_gap && !near_water(x, y);
                    local.o += O;
                    local.covered += O && W;
                    if (gap) {
                        local.cause[cause_of(scene, probe, i)] += 1;
                    }
                    local.raw_gap += raw_gap;
                    local.gap += gap;
                    local.extra += R && !O;
                    local.river += R;
                    const std::size_t mx =
                        static_cast<std::size_t>(rect.x - cx0 * render::kChunk + x);
                    const std::size_t my =
                        static_cast<std::size_t>(rect.y - cy0 * render::kChunk + y);
                    std::uint8_t& m = mask[(my / static_cast<std::size_t>(s)) * mw +
                                           mx / static_cast<std::size_t>(s)];
                    if (gap) {
                        m |= kGap;
                    }
                    if (mx % static_cast<std::size_t>(s) == 0 &&
                        my % static_cast<std::size_t>(s) == 0) {
                        m |= (O ? kO : 0) | (E ? kE : 0);
                    }
                }
            }
        }
        const std::lock_guard lock(mutex);
        tot.add(local);
    };
    std::vector<std::thread> pool;
    for (std::size_t t = 0; t < std::min(opt.threads, chunks); ++t) {
        pool.emplace_back(worker);
    }
    for (auto& t : pool) {
        t.join();
    }
    // Connectivity on the sampled mask.
    std::size_t no = 0, ne = 0, ng = 0;
    const auto lo = label(mask, kO, mw, mh, no);
    const auto le = label(mask, kE, mw, mh, ne);
    const auto lg = label(mask, kGap, mw, mh, ng);
    std::vector<std::set<std::int32_t>> touch(no + 1);
    std::vector<std::size_t> size(no + 1, 0);
    for (std::size_t i = 0; i < mask.size(); ++i) {
        if (lo[i] > 0) {
            ++size[static_cast<std::size_t>(lo[i])];
        }
        if (lo[i] > 0 && le[i] > 0) {
            touch[static_cast<std::size_t>(lo[i])].insert(le[i]);
        }
    }
    // Components of fewer than kMinComponent samples are specks (single
    // sprite pixels at the rim of a mouth), not river stretches.
    constexpr std::size_t kMinComponent = 16;
    std::size_t big = 0;
    for (std::size_t c = 1; c <= no; ++c) {
        if (size[c] < kMinComponent) {
            continue;
        }
        ++big;
        tot.split += touch[c].size() > 1;
        tot.lost += touch[c].empty();
        if (opt.list && (touch[c].size() > 1 || touch[c].empty())) {
            // Bounding box of the component in scene pixels, for a look with --rect.
            std::size_t x0 = mw, y0 = mh, x1 = 0, y1 = 0;
            for (std::size_t i = 0; i < lo.size(); ++i) {
                if (lo[i] == static_cast<std::int32_t>(c)) {
                    x0 = std::min(x0, i % mw);
                    x1 = std::max(x1, i % mw);
                    y0 = std::min(y0, i / mw);
                    y1 = std::max(y1, i / mw);
                }
            }
            const auto sc = static_cast<std::size_t>(s);
            std::fprintf(stderr,
                         "  %s: O component %zu (%zu samples) %s in %zu water parts, scene px x "
                         "%d..%d y %d..%d\n",
                         tag.c_str(), c, size[c], touch[c].empty() ? "LOST" : "SPLIT",
                         touch[c].size(), cx0 * render::kChunk + static_cast<int>(x0 * sc),
                         cx0 * render::kChunk + static_cast<int>(x1 * sc),
                         cy0 * render::kChunk + static_cast<int>(y0 * sc),
                         cy0 * render::kChunk + static_cast<int>(y1 * sc));
            // Where the parts nearly touch: sprite pixels without water that have two
            // different water parts within 2 samples.
            std::size_t shown = 0;
            for (std::size_t i = 0; i < lo.size() && shown < 6; ++i) {
                if (lo[i] != static_cast<std::int32_t>(c) || le[i] > 0) {
                    continue;
                }
                std::set<std::int32_t> near;
                const std::size_t px = i % mw, py = i / mw;
                for (std::size_t yy = py < 2 ? 0 : py - 2; yy <= std::min(py + 2, mh - 1); ++yy) {
                    for (std::size_t xx = px < 2 ? 0 : px - 2; xx <= std::min(px + 2, mw - 1);
                         ++xx) {
                        if (le[yy * mw + xx] > 0) {
                            near.insert(le[yy * mw + xx]);
                        }
                    }
                }
                if (near.size() > 1) {
                    std::fprintf(stderr, "    break near scene px (%d, %d)\n",
                                 cx0 * render::kChunk + static_cast<int>(px * sc),
                                 cy0 * render::kChunk + static_cast<int>(py * sc));
                    ++shown;
                    i += 200;
                }
            }
        }
    }
    tot.o_comps = big;
    tot.e_comps = ne;
    tot.gap_comps = ng;
    tot.islands = 1;
    (void)lg;
    if (!opt.dump.empty() && mask.size() <= (16ULL << 20)) {
        std::vector<std::uint8_t> img(mask.size() * 4, 0);
        for (std::size_t i = 0; i < mask.size(); ++i) {
            const std::uint8_t m = mask[i];
            std::uint8_t* p = &img[i * 4];
            if ((m & kGap) != 0) {
                p[0] = 255, p[3] = 255;
            } else if ((m & kO) != 0 && (m & kE) != 0) {
                p[1] = 200, p[3] = 255;
            } else if ((m & kE) != 0) {
                p[2] = 255, p[1] = 120, p[3] = 255;
            } else if ((m & kO) != 0) {
                p[0] = 255, p[1] = 160, p[3] = 255;
            } else {
                p[3] = 255;
            }
        }
        fs::create_directories(opt.dump);
        // The file name carries the scene position of mask sample (0, 0) and the
        // sampling step: x_scene = ox + s * x_mask.
        const fs::path out =
            opt.dump / (tag + "_o" + std::to_string(cx0 * render::kChunk) + "_" +
                        std::to_string(cy0 * render::kChunk) + "_s" + std::to_string(s) + ".png");
        const std::string err = anno::app::write_png(out, static_cast<std::uint32_t>(mw),
                                                     static_cast<std::uint32_t>(mh), img);
        if (!err.empty()) {
            std::fprintf(stderr, "%s\n", err.c_str());
        }
    }
    if (opt.per_island) {
        std::printf("| %s (s=%d) | %llu | %llu | %llu | %llu | %zu | %zu | %zu | %zu | %zu |\n",
                    tag.c_str(), s, static_cast<unsigned long long>(tot.o),
                    static_cast<unsigned long long>(tot.covered),
                    static_cast<unsigned long long>(tot.gap),
                    static_cast<unsigned long long>(tot.extra), tot.o_comps, tot.e_comps, tot.split,
                    tot.lost, tot.gap_comps);
    }
    return tot;
}

/// Heights of the river pieces against the terrain under them (docs/render/iso.md,
/// "Rivers"): z is the piece's height byte, the corner statistics are the
/// terrain heights at the corners of its footprint.
void print_pieces(const render::Scene& scene, const std::string& name) {
    for (std::size_t i = 0; i < scene.islands.size(); ++i) {
        const auto pieces = scene.river_pieces(i);
        const render::sea::IslandWater* iw = scene.island_water_info(i);
        if (pieces.empty() || iw == nullptr) {
            continue;
        }
        std::map<int, int> size_hist, mean_hist, min_hist, max_hist, step_hist;
        for (const auto& [x, y, fw, fh, z] : pieces) {
            ++size_hist[fw * 10 + fh];
            double sum = 0;
            int n = 0, lo = 255, hi = 0;
            for (std::size_t dy = 0; dy <= fh; ++dy) {
                for (std::size_t dx = 0; dx <= fw; ++dx) {
                    const std::size_t cx = std::min<std::size_t>(x + dx, iw->w - 1);
                    const std::size_t cy = std::min<std::size_t>(y + dy, iw->h - 1);
                    const int hgt = iw->heights[cy * iw->w + cx];
                    sum += hgt;
                    ++n;
                    lo = std::min(lo, hgt);
                    hi = std::max(hi, hgt);
                }
            }
            ++mean_hist[static_cast<int>(std::lround(sum / n)) - z];
            ++min_hist[lo - z];
            ++max_hist[hi - z];
        }
        auto dump = [](const char* what, const std::map<int, int>& h) {
            std::printf("  %s:", what);
            for (const auto& [k, v] : h) {
                std::printf(" %d:%d", k, v);
            }
            std::printf("\n");
        };
        std::printf("%s island %zu: %zu river pieces\n", name.c_str(), i, pieces.size());
        dump("footprint (10 w + h)", size_hist);
        dump("mean corner height - z", mean_hist);
        dump("min corner height - z", min_hist);
        dump("max corner height - z", max_hist);
    }
}

/// --rect: writes the masks of one scene rectangle at `group` as an RGBA PNG
/// (R = original river sprite not covered by enhanced water, G = enhanced
/// water, B = original river sprite; alpha 255), for overlays on screenshots.
/// Chunked like the viewer (render::kChunk).
void write_rect_masks(const render::Scene& scene, std::size_t group, render::Rect view,
                      const fs::path& out) {
    std::vector<std::uint8_t> img(
        static_cast<std::size_t>(view.w) * static_cast<std::size_t>(view.h) * 4, 0);
    // Small rectangles are also printed as text: sprite and water (r = river
    // flag) per pixel, with the depth in 1/4 HEIGHT units for water.
    std::vector<std::string> text(static_cast<std::size_t>(view.h),
                                  std::string(static_cast<std::size_t>(view.w) * 4, ' '));
    for (std::size_t i = 3; i < img.size(); i += 4) {
        img[i] = 255;
    }
    const int cx0 = render::div_euclid(view.x, render::kChunk);
    const int cy0 = render::div_euclid(view.y, render::kChunk);
    const int cx1 = render::div_euclid(view.x + view.w - 1, render::kChunk);
    const int cy1 = render::div_euclid(view.y + view.h - 1, render::kChunk);
    for (int cy = cy0; cy <= cy1; ++cy) {
        for (int cx = cx0; cx <= cx1; ++cx) {
            const render::Rect rect = render::chunk_rect(cx, cy);
            const auto [ground, probe] = scene.ground_with_probe(rect, group);
            const render::Canvas sprites = scene.river_sprites(rect, probe, group);
            const auto field = scene.water_field_from(rect, probe, group);
            const auto r = rect.intersect(view);
            if (!r) {
                continue;
            }
            for (int y = r->y; y < r->y + r->h; ++y) {
                for (int x = r->x; x < r->x + r->w; ++x) {
                    const auto i = static_cast<std::size_t>((y - rect.y) * rect.w + (x - rect.x));
                    const float sd =
                        (static_cast<float>(field.depth[i]) - render::sea::kDepthBias) /
                        render::sea::kDepthScale;
                    const bool W = field.cover[i] != 0 && sd >= 0.0F;
                    const bool O = sprites.pixels[i * 4 + 3] >= 128;
                    std::uint8_t* p = &img[(static_cast<std::size_t>(y - view.y) *
                                                static_cast<std::size_t>(view.w) +
                                            static_cast<std::size_t>(x - view.x)) *
                                           4];
                    {
                        const bool river = (field.flags[i] & 2U) != 0;
                        const char c = O && W ? (river ? 'R' : 'S')
                                       : O    ? 'o'
                                       : W    ? (river ? 'r' : 's')
                                              : '.';
                        text[static_cast<std::size_t>(y - view.y)]
                            [static_cast<std::size_t>(x - view.x) * 4] = c;
                        const int d = static_cast<int>(std::lround(sd * 4.0F));
                        text[static_cast<std::size_t>(y - view.y)]
                            [static_cast<std::size_t>(x - view.x) * 4 + 1] =
                                d < -9  ? '-'
                                : d > 9 ? '+'
                                        : static_cast<char>('0' + std::abs(d));
                    }
                    p[0] = O && !W ? 255 : 0;
                    p[1] = W ? 255 : 0;
                    p[2] = O ? 255 : 0;
                }
            }
        }
    }
    const std::string err = anno::app::write_png(out, static_cast<std::uint32_t>(view.w),
                                                 static_cast<std::uint32_t>(view.h), img);
    if (!err.empty()) {
        std::fprintf(stderr, "%s\n", err.c_str());
    }
    if (view.w * view.h <= 4900) {
        for (const auto& row : text) {
            std::fprintf(stderr, "%s\n", row.c_str());
        }
    }
    // Seams: the viewer builds the field per chunk; compare with one field of
    // the whole view (water pixels that differ).
    const auto whole = scene.water_field(view, group);
    std::uint64_t differ = 0, water = 0;
    for (int y = 0; y < view.h; ++y) {
        for (int x = 0; x < view.w; ++x) {
            const auto i = static_cast<std::size_t>(y * view.w + x);
            const float sd = (static_cast<float>(whole.depth[i]) - render::sea::kDepthBias) /
                             render::sea::kDepthScale;
            const bool W = whole.cover[i] != 0 && sd >= 0.0F;
            const bool chunked = img[i * 4 + 1] != 0;
            water += W;
            differ += W != chunked;
        }
    }
    std::fprintf(stderr, "chunked vs whole-view field: %llu of %llu water pixels differ\n",
                 static_cast<unsigned long long>(differ), static_cast<unsigned long long>(water));
    std::uint64_t sprites = 0, uncovered = 0;
    for (std::size_t i = 0; i < img.size(); i += 4) {
        sprites += img[i + 2] != 0;
        uncovered += img[i] != 0;
    }
    std::fprintf(stderr, "river sprite pixels in the view: %llu, without water: %llu\n",
                 static_cast<unsigned long long>(sprites),
                 static_cast<unsigned long long>(uncovered));
}

std::vector<std::string_view> split(std::string_view s, char sep) {
    std::vector<std::string_view> out;
    for (std::size_t start = 0;;) {
        const std::size_t k = s.find(sep, start);
        out.push_back(
            s.substr(start, k == std::string_view::npos ? std::string_view::npos : k - start));
        if (k == std::string_view::npos) {
            return out;
        }
        start = k + 1;
    }
}

} // namespace

int main(int argc, char** argv) {
    Options opt;
    std::optional<render::Rect> rect;
    std::size_t rect_group = 0;
    fs::path rect_out;
    std::vector<fs::path> files;
    fs::path dir;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--groups" && i + 1 < argc) {
            opt.groups.clear();
            for (const auto part : split(argv[++i], ',')) {
                std::size_t g = 0;
                std::from_chars(part.data(), part.data() + part.size(), g);
                opt.groups.push_back(g);
            }
        } else if (a == "--dump" && i + 1 < argc) {
            opt.dump = argv[++i];
        } else if (a == "--rect" && i + 1 < argc) {
            // group,x,y,w,h
            const auto parts = split(argv[++i], ',');
            int v[5] = {};
            for (std::size_t k = 0; k < 5 && k < parts.size(); ++k) {
                std::from_chars(parts[k].data(), parts[k].data() + parts[k].size(), v[k]);
            }
            rect_group = static_cast<std::size_t>(v[0]);
            rect = render::Rect{v[1], v[2], v[3], v[4]};
        } else if (a == "--rect-out" && i + 1 < argc) {
            rect_out = argv[++i];
        } else if (a == "--list") {
            opt.list = true;
        } else if (a == "--pieces") {
            opt.pieces = true;
        } else if (a == "--per-island") {
            opt.per_island = true;
        } else if (a == "--max-mask-px" && i + 1 < argc) {
            opt.max_mask = std::stoull(argv[++i]);
        } else if (dir.empty()) {
            dir = a;
        } else {
            files.emplace_back(a);
        }
    }
    if (dir.empty() || files.empty()) {
        std::fputs("usage: river_qa <game_dir> [--groups 0,1,2] [--dump dir] [--per-island] "
                   "[--max-mask-px N] <file.sze|.scp>...\n",
                   stderr);
        return 2;
    }
    std::printf("| scene | group | islands | O px | covered | edge px | gap px | gap %% | extra px "
                "| O comps | "
                "E comps | split | lost | gap comps "
                "|\n|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
    Totals all;
    for (const auto& file : files) {
        auto loaded = anno::app::load_scene(dir, file);
        if (!loaded) {
            std::fprintf(stderr, "%s: %s\n", file.string().c_str(), loaded.error().c_str());
            continue;
        }
        const render::Scene& scene = *loaded.value().scene;
        const std::string name = anno::app::to_utf8(file.stem());
        if (rect) {
            if (const auto b = scene.bounds(rect_group)) {
                std::fprintf(stderr, "%s group %zu: scene bounds x %d y %d w %d h %d\n",
                             name.c_str(), rect_group, b->x, b->y, b->w, b->h);
            }
            write_rect_masks(scene, rect_group, *rect, rect_out);
            continue;
        }
        if (opt.pieces) {
            print_pieces(scene, name);
        }
        for (const std::size_t g : opt.groups) {
            if (const auto b = scene.bounds(g)) {
                std::fprintf(stderr, "%s group %zu: scene bounds x %d y %d w %d h %d\n",
                             name.c_str(), g, b->x, b->y, b->w, b->h);
            }
            Totals t;
            for (const auto& pl : scene.layouts[g]) {
                if (scene.river_piece_count(pl.index) == 0) {
                    continue;
                }
                t.add(measure_island(scene, g, pl, opt,
                                     name + "_g" + std::to_string(g) + "_i" +
                                         std::to_string(pl.index)));
            }
            all.add(t);
            std::printf("| %s | %zu | %zu | %llu | %llu | %llu | %llu | %.2f | %llu | %zu | %zu | "
                        "%zu | %zu | %zu |\n",
                        name.c_str(), g, t.islands, static_cast<unsigned long long>(t.o),
                        static_cast<unsigned long long>(t.covered),
                        static_cast<unsigned long long>(t.raw_gap - t.gap),
                        static_cast<unsigned long long>(t.gap),
                        t.o ? 100.0 * static_cast<double>(t.gap) / static_cast<double>(t.o) : 0.0,
                        static_cast<unsigned long long>(t.extra), t.o_comps, t.e_comps, t.split,
                        t.lost, t.gap_comps);
            std::fflush(stdout);
            if (t.gap != 0) {
                std::fprintf(stderr, "  %s g%zu gap causes:", name.c_str(), g);
                for (std::size_t c = 0; c < t.cause.size(); ++c) {
                    if (t.cause[c] != 0) {
                        std::fprintf(stderr, " [%s] %llu", kCauseNames[c],
                                     static_cast<unsigned long long>(t.cause[c]));
                    }
                }
                std::fprintf(stderr, "\n");
            }
        }
    }
    return 0;
}
