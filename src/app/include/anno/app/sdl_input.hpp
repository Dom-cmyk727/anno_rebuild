// The play window's SDL events for the developer tool (W4.4 round ee, B3): the one mapping the
// live window and the tests use. Keys: B turns the tool on and off; with it on, N and P cycle the
// buildable types, T rotates, X toggles the demolish mode, Esc cancels a press or the demolish
// mode (with nothing to cancel the window keeps Esc: quit). The left button presses and releases
// at the pointer; motion only moves the hover; losing focus, leaving the window or being hidden
// drops a press. Pointer coordinates are window points, converted to drawable pixels with the
// window's pixel density. A test tool, not UI.2's bindings (round dr).
#pragma once

#include <SDL3/SDL_events.h>
#include <optional>
#include <vector>

#include <anno/app/dev_tool.hpp>
#include <anno/sim/command.hpp>

namespace anno::app {

/// The tool input of `event`, or nullopt when the tool does not take it. `enabled`: the tool is
/// on (only B is taken while it is off); `pending`: it has something Esc cancels.
[[nodiscard]] std::optional<ToolInput> tool_input_from_sdl(const SDL_Event& event, float density,
                                                           bool enabled, bool pending);

/// The window's whole step for one event: the mapping, then DevTool::handle with the frame that
/// was drawn. Returns whether the tool took the event and the commands it made (the window posts
/// them). The live window calls exactly this.
struct ToolStep {
    bool taken = false;
    std::vector<sim::Command> commands;
};
[[nodiscard]] ToolStep dev_tool_step(DevTool& tool, const SDL_Event& event, float density,
                                     const PointerFrame* frame);

} // namespace anno::app
