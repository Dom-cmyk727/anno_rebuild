// The live ships as pictures (W4-OPENING-1; docs/app/ship-opening.md): original figure frames of
// every facing, in the owner's colour, turned into render::UnitSprites each frame from an
// OpeningView, and the same sprites' screen boxes for hit testing. The layer is built once at load
// (it reads the figure volumes) and is immutable afterwards: the window thread and the tests share
// it without a lock. It owns no World, scene or generation: a ship is a frame-sized overlay that
// the CPU and GPU frames draw after the scene's own objects, so a sailing tick changes no chunk.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <anno/app/dev_tool.hpp>
#include <anno/app/opening_ui.hpp>
#include <anno/app/opening_view.hpp>
#include <anno/formats/result.hpp>
#include <anno/render/scene.hpp>

namespace anno::app {

class ShipLayer {
  public:
    /// One (figure kind, owner) pair to prepare: the kinds of the world's ships and their owners.
    using Wanted = std::pair<std::uint32_t, std::uint32_t>;

    /// Decodes every facing of each wanted kind in its owner's colour (owner n takes entry n mod 8
    /// of the original player colours: a stand-in for the colour screen, assets/unit.hpp). An error
    /// when a kind has no sprite set.
    [[nodiscard]] static formats::Result<std::shared_ptr<const ShipLayer>, std::string>
    load(const std::filesystem::path& game_dir, const std::vector<Wanted>& wanted);

    /// A layer built from sprites the caller made (tests).
    struct Prepared {
        std::uint32_t kind = 0, owner = 0;
        /// One sprite per facing (render frames per zoom group; x, y are overwritten).
        std::vector<render::UnitSprite> facings;
    };
    [[nodiscard]] static std::shared_ptr<const ShipLayer> from_prepared(std::vector<Prepared> sets);

    /// The sprites of `view`'s ships, ship by ship in FigureId order, each preceded by the
    /// selection ring when it is `selected`. Ships of a (kind, owner) the layer lacks are skipped
    /// and counted in `missing` (when given).
    [[nodiscard]] std::vector<render::UnitSprite> units(const OpeningView& view,
                                                        std::optional<sim::FigureId> selected,
                                                        bool selected_is_own,
                                                        std::size_t* missing = nullptr) const;

    /// Where the ship is drawn in `frame`'s drawable pixels (the frame box of its current facing at
    /// the level shown), or nullopt when the layer has no sprite for it or the frame has no scene.
    [[nodiscard]] std::optional<ScreenBox> box(const ShipView& ship,
                                               const PointerFrame& frame) const;

    [[nodiscard]] std::size_t sets() const { return sets_.size(); }

  private:
    std::map<Wanted, std::vector<render::UnitSprite>> sets_;
    /// The rings of the selected ship, own (index 0) and foreign (1), one frame per zoom group.
    std::vector<render::UnitSprite> rings_;
    void make_rings();
};

/// The CPU renderer's live units: draws `units` over `xrgb`, the finished frame of `view` at
/// built zoom group `group` (0x00RRGGBB, view.w x view.h), after every object of the scene and in
/// the painter's order among themselves (x + y, stable), each unit's shadow and frame through the
/// scene's own UnitSprite::draw on a canvas the size of what is seen. The result is what the
/// scene's own units give when they are in the chunks (the GPU parity test compares both).
void blend_units(std::span<const render::UnitSprite> units, const render::Scene& scene,
                 std::size_t group, render::Rect view, std::uint32_t* xrgb);

} // namespace anno::app
