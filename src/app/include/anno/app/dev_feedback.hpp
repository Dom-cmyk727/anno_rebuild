// The developer tool's preview and feedback (W4.4 round ee, package C): the selected type's own
// sprite at the hovered tile, what the window knows about that placement from published data,
// and one feedback line (the type and its costs, the money and the type's goods, the preview's
// knowledge, and the outcome of the tool's latest command by its receipt). The window never reads
// or changes the World: the picture's published buildings, the latest PlaySnapshot and the
// runner's outcome log are its only sources. A geometric preview promises nothing: the sim
// revalidates every command when it runs it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <anno/app/command_outcome.hpp>
#include <anno/app/dev_tool.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/play.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/scene.hpp>

namespace anno::app {

/// The preview of the tool's selected type at the hovered tile: the city island's scene index,
/// the object as the scene would hold it, and its sprites (Scene::preview_sprites).
struct Preview {
    std::size_t island = 0;
    render::Prepared object;
    std::vector<render::SpriteDraw> sprites;
};

/// Why the hovered placement has no preview in `scene` at zoom level `group` (round ee D3), ""
/// when it has one or none is due (the tool off, the demolish mode, no type or hovered tile):
/// an id the picture cannot draw (kPictureAnlageIds), no Anlagen entry for the type, a derived
/// level (the sprites exist at the built groups only), the city's island not in the scene.
[[nodiscard]] std::string preview_note(const DevTool& tool, const render::Scene& scene,
                                       std::size_t group);

/// The preview for `scene` at zoom level `group` and `time_ms`: none while the tool is off, in the
/// demolish mode, without a type or a hovered tile, or when preview_note names a reason. The object
/// stands at the ground height under its origin tile's corner (the sim's own height for the
/// building may differ).
[[nodiscard]] std::optional<Preview> make_preview(const DevTool& tool, const render::Scene& scene,
                                                  std::size_t group, std::uint64_t time_ms);

/// The CPU renderer's preview: draws it (draw_prepared, the scene's sprite cache) and blends it
/// onto `xrgb`, the frame of `view` (0x00RRGGBB, view.w x view.h), over what is there.
void blend_preview(const render::Scene& scene, const Preview& preview, std::size_t group,
                   std::uint64_t time_ms, render::Rect view, std::uint32_t* xrgb);

/// What the window knows about the hovered placement (round ee, C2). Its sources:
/// - the city's island tiles (DevToolSetup);
/// - the tiles the picture's published buildings cover, indexed once per change of that list
///   (ScenePicture::buildings_version), never scanned on a pointer move;
/// - the latest snapshot's money, goods and cost waiver against the type's costs.
/// A refusal is reported only with one of those as its basis; anything else is unchecked, and the
/// text names what was not checked (the sim's placement rule always, which does not check terrain
/// yet, OQ-052; occupancy without an
/// index, costs without a snapshot, goods outside the published stock).
class PlacementKnowledge {
  public:
    explicit PlacementKnowledge(DevToolSetup setup);
    /// Re-indexes when `version` differs from the last one indexed; returns whether it did.
    /// `scene` names the city island's number (the buildings' island key).
    bool update(const std::vector<SceneBuilding>& buildings, std::uint64_t version,
                const render::Scene& scene);
    struct Verdict {
        /// A known refusal (its basis is in `text`); false: unchecked or nothing to check.
        bool refused = false;
        std::string text;
    };
    [[nodiscard]] Verdict check(const DevTool& tool, const PlaySnapshot* snapshot) const;
    /// How many times the index was rebuilt (a test counts them).
    [[nodiscard]] std::size_t rebuilds() const { return rebuilds_; }
    [[nodiscard]] bool occupied(sim::TileXY tile) const;

  private:
    DevToolSetup setup_;
    std::optional<std::uint64_t> version_;
    std::set<std::pair<std::int32_t, std::int32_t>> occupied_;
    /// Whether the last update built the index (the city's island in the scene, a registry).
    bool indexed_ = false;
    std::size_t rebuilds_ = 0;
};

/// The feedback line for the title: the tool's state (dev_tool_status), the money and the
/// selected type's cost goods from `snapshot` (with its turn), the preview's knowledge, and the
/// outcome of the latest command the tool sent (`latest`, looked up by its receipt): Queued,
/// Applied (and how many of its records did something), Refused with the sim's reason, or Lost.
/// `note` is preview_note's reason ("" when there is a preview).
[[nodiscard]] std::string feedback_line(const DevTool& tool,
                                        const PlacementKnowledge::Verdict& verdict,
                                        const PlaySnapshot* snapshot,
                                        const std::optional<CommandOutcome>& latest,
                                        std::string_view note = {});

/// Whether the picture shows what an applied command built or removed (round ee, E4): "" when it
/// does or the command was not applied; "not drawn yet: ..." when the command's turn is not in
/// the picture's desired state (`followed_turn`, PlayFollower), or when the picture is still
/// rebuilding (`pending` islands, ScenePicture::pending: the island of the change may be one).
/// The sim's outcome stands either way; only the picture is behind.
[[nodiscard]] std::string drawn_note(const std::optional<CommandOutcome>& latest,
                                     std::optional<std::uint64_t> followed_turn,
                                     std::size_t pending);

/// The window title's text after its name while the tool is on, from one snapshot (round ee
/// review of C, P2-1): the runner's line without the money (PlayRunner::title_of; its refusal
/// names its command), then feedback_line, which shows the money once.
[[nodiscard]] std::string dev_title(const PlaySnapshot& snapshot, const DevTool& tool,
                                    const PlacementKnowledge::Verdict& verdict,
                                    const std::optional<CommandOutcome>& latest,
                                    std::string_view note = {});

} // namespace anno::app
