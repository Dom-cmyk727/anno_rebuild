// Still pictures of a scene for --png: the built zoom groups through Scene::render, and the
// derived levels (group 3 and up) through the viewer's chunk pipeline, so that a derived
// picture is the one the viewer draws on the CPU (docs/render/iso.md, "Choosing the level").
#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>

namespace anno::app {

/// Sets the zoom levels that `group` needs: the built ones and, for group 3 and up, the
/// derived levels reduced from group 2 up to it. `max_derived` bounds how many (the
/// --max-derived-levels option). Returns an error message, empty when the group is in range.
std::string set_derived_levels(render::Scene& scene, std::size_t group, std::size_t max_derived);

/// The picture of `view` at a derived zoom level (`group` >= render::kGroups, the scene's
/// levels set first): FrameRenderer on the CPU with the ships in its chunks. The chunks it
/// needs are built here, on this thread, before the picture is drawn; `frames` gets the
/// stats of the draw. nullopt if the view's chunks do not all get built within the round
/// bound (kRounds in still.cpp: a guard against a chunk that never completes).
[[nodiscard]] std::optional<render::Canvas> derived_view(const render::Scene& scene,
                                                         render::Rect view, std::size_t group,
                                                         const render::Layers& layers,
                                                         render::FrameStats& frames);

} // namespace anno::app
