// Shared corpus-scene loader for the chunk tests (game files, ANNO1503_GAME_DIR):
// Single_01.sze, Single_24.sze and the canyon island D_256x256_0045_5B.scp, with
// their ships and soldiers, exactly as the frame digest tests build them. Used by
// chunk_draws_corpus_test.cpp (CPU replay) and the GPU chunk-build corpus test.
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <anno/assets/figure.hpp>
#include <anno/assets/unit.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/formats/insel.hpp>
#include <anno/formats/sze.hpp>
#include <anno/formats/tex.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>
#include <anno/test/game_dir.hpp>

namespace anno::render::test {

namespace fig = anno::formats::figuren;

/// The shared files of the corpus; scenes are built from them.
struct Game {
    std::vector<std::uint8_t> felder_buf, anl_buf, hbuf;
    fig::FigurenHeader header;
    std::vector<std::vector<std::uint8_t>> vols;

    explicit Game(const std::filesystem::path& dir)
        : felder_buf(anno::test::read_file(dir / "Textures" / "Felder.tex")),
          anl_buf(anno::test::read_file(dir / "Anlagen.dat")),
          hbuf(anno::test::read_file(dir / "FigurenHeader.dat")),
          header(fig::FigurenHeader::parse(hbuf).value()), vols(3) {
        for (std::uint32_t g = 0; g < 3; ++g) {
            for (std::uint32_t part = 0;; ++part) {
                const auto p = dir / fig::volume_file_name(g, part);
                if (!std::filesystem::exists(p)) {
                    break;
                }
                const auto d = anno::test::read_file(p);
                vols[g].insert(vols[g].end(), d.begin(), d.end());
            }
        }
    }

    /// A scene and the file buffer its islands refer to (which must outlive it).
    struct Loaded {
        std::vector<std::uint8_t> island_buf;
        std::unique_ptr<anno::render::Scene> scene;
        anno::render::Scene* operator->() const { return scene.get(); }
        anno::render::Scene& operator*() const { return *scene; }
    };

    /// The scene of `path` with its ships and soldiers, as the digest tests build it.
    [[nodiscard]] Loaded scene(const std::filesystem::path& path) const {
        Loaded loaded;
        loaded.island_buf = anno::test::read_file(path);
        const auto& buf = loaded.island_buf;
        auto anlagen = anno::formats::anlagen::AnlagenFile::parse(anl_buf).value();
        const auto felder = anno::formats::tex::TexFile::parse(felder_buf).value();
        auto textures = anno::assets::island::GroundTextures::load(felder).value();
        if (const auto c = anno::render::water_colour(anlagen)) {
            textures.set_water(*c);
        }
        loaded.scene = std::make_unique<anno::render::Scene>(
            "", anno::formats::insel::islands(buf).value(), std::move(textures), std::move(anlagen),
            anno::render::kHeightLift);
        auto* s = loaded.scene.get();
        if (auto sze = anno::formats::sze::Scene::parse(buf)) {
            const anno::assets::unit::VolumeReader read =
                [this](std::uint32_t g, std::uint64_t o,
                       std::uint64_t l) -> std::optional<std::vector<std::uint8_t>> {
                if (g >= vols.size() || o + l > vols[g].size()) {
                    return std::nullopt;
                }
                const auto b = vols[g].begin() + static_cast<std::ptrdiff_t>(o);
                return std::vector<std::uint8_t>(b, b + static_cast<std::ptrdiff_t>(l));
            };
            const std::array kinds = {anno::assets::unit::UnitKind::Ship,
                                      anno::assets::unit::UnitKind::Soldier};
            auto units =
                anno::assets::unit::units(sze.value(), header, kinds,
                                          anno::assets::figure::kOriginalPlayerColours, read)
                    .value();
            for (auto& u : units.units) {
                std::optional<float> z;
                if (u.kind == anno::assets::unit::UnitKind::Soldier) {
                    z = s->ground_height(u.x, u.y);
                    if (!z) {
                        continue;
                    }
                }
                anno::render::UnitSprite sprite{u.x, u.y, z, {}, {}};
                for (const auto& f : u.frames) {
                    sprite.frames.push_back(
                        f ? std::optional<anno::render::UnitFrame>(
                                anno::render::to_unit_frame(*f, u.figure, false))
                          : std::nullopt);
                }
                for (const auto& f : u.shadows) {
                    sprite.shadows.push_back(
                        f ? std::optional<anno::render::UnitFrame>(
                                anno::render::to_unit_frame(*f, u.figure, true))
                          : std::nullopt);
                }
                s->units.push_back(std::move(sprite));
            }
        }
        return loaded;
    }
};

struct SceneFile {
    const char* name;
    const char* path; ///< below the game directory
};
inline constexpr SceneFile kScenes[] = {
    {"Single_01.sze", "Scenes/0200Single/Single_01.sze"},
    {"Single_24.sze", "Scenes/0200Single/Single_24.sze"},
    {"D_256x256_0045_5B.scp", "Scenes/Islands/Dschungel/D_256x256_0045_5B.scp"},
};

/// The chunks (cx, cy) of `group` that can hold anything: those touching an
/// island's bounds or a unit.
inline std::vector<std::pair<int, int>> busy_chunks(const anno::render::Scene& scene,
                                                    std::size_t group) {
    std::vector<std::pair<int, int>> out;
    const auto b = scene.bounds(group);
    if (!b) {
        return out;
    }
    const int cx0 = anno::render::div_euclid(b->x, anno::render::kChunk),
              cx1 = anno::render::div_euclid(b->x + b->w - 1, anno::render::kChunk);
    const int cy0 = anno::render::div_euclid(b->y, anno::render::kChunk),
              cy1 = anno::render::div_euclid(b->y + b->h - 1, anno::render::kChunk);
    for (int cy = cy0; cy <= cy1; ++cy) {
        for (int cx = cx0; cx <= cx1; ++cx) {
            const anno::render::Rect rect = anno::render::chunk_rect(cx, cy);
            const bool island =
                std::any_of(scene.layouts[group].begin(), scene.layouts[group].end(),
                            [&](const anno::render::Placement& p) {
                                return p.bounds.intersect(rect).has_value();
                            });
            if (island || scene.units_touch(rect, group, true)) {
                out.emplace_back(cx, cy);
            }
        }
    }
    return out;
}

} // namespace anno::render::test
