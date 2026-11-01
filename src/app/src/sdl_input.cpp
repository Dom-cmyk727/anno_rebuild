// The play window's SDL events for the developer tool (see the header).
#include <anno/app/sdl_input.hpp>

namespace anno::app {

std::optional<ToolInput> tool_input_from_sdl(const SDL_Event& e, float density, bool enabled,
                                             bool pending) {
    using K = ToolInput::Kind;
    const double d = density > 0.0F ? static_cast<double>(density) : 1.0;
    switch (e.type) {
    case SDL_EVENT_KEY_DOWN:
        if (e.key.repeat) {
            return std::nullopt;
        }
        if (e.key.key == SDLK_B) {
            return ToolInput{K::Toggle, 0, 0};
        }
        if (!enabled) {
            return std::nullopt;
        }
        switch (e.key.key) {
        case SDLK_N:
            return ToolInput{K::NextType, 0, 0};
        case SDLK_P:
            return ToolInput{K::PrevType, 0, 0};
        case SDLK_T:
            return ToolInput{K::Rotate, 0, 0};
        case SDLK_X:
            return ToolInput{K::ToggleDemolish, 0, 0};
        case SDLK_ESCAPE:
            return pending ? std::optional<ToolInput>(ToolInput{K::Cancel, 0, 0}) : std::nullopt;
        default:
            return std::nullopt;
        }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (!enabled || e.button.button != SDL_BUTTON_LEFT) {
            return std::nullopt;
        }
        return ToolInput{e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? K::Press : K::Release,
                         static_cast<double>(e.button.x) * d, static_cast<double>(e.button.y) * d};
    case SDL_EVENT_MOUSE_MOTION:
        if (!enabled) {
            return std::nullopt;
        }
        return ToolInput{K::Move, static_cast<double>(e.motion.x) * d,
                         static_cast<double>(e.motion.y) * d};
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
    case SDL_EVENT_WINDOW_HIDDEN:
    case SDL_EVENT_WINDOW_MINIMIZED:
        if (!enabled) {
            return std::nullopt;
        }
        return ToolInput{K::Abort, 0, 0};
    default:
        return std::nullopt;
    }
}

ToolStep dev_tool_step(DevTool& tool, const SDL_Event& event, float density,
                       const PointerFrame* frame) {
    ToolStep step;
    const auto input = tool_input_from_sdl(event, density, tool.enabled(), tool.has_pending());
    if (!input) {
        return step;
    }
    step.taken = true;
    step.commands = tool.handle(*input, frame);
    return step;
}

} // namespace anno::app
