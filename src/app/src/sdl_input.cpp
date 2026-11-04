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
            return ToolInput{(e.key.mod & SDL_KMOD_SHIFT) != 0 ? K::NextTypeFast : K::NextType, 0,
                             0};
        case SDLK_P:
            return ToolInput{(e.key.mod & SDL_KMOD_SHIFT) != 0 ? K::PrevTypeFast : K::PrevType, 0,
                             0};
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
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.repeat) {
        // A held key of the tool's does nothing, and it is still the tool's: a held Esc after a
        // cancel must not reach the window's own Esc (quit; integration review of round ee, P2-1).
        const SDL_Keycode k = event.key.key;
        step.taken = k == SDLK_B || (tool.enabled() && (k == SDLK_N || k == SDLK_P || k == SDLK_T ||
                                                        k == SDLK_X || k == SDLK_ESCAPE));
        return step;
    }
    const auto input = tool_input_from_sdl(event, density, tool.enabled(), tool.has_pending());
    if (!input) {
        return step;
    }
    step.taken = true;
    step.commands = tool.handle(*input, frame);
    return step;
}

std::optional<OpeningKey> opening_key_from_sdl(SDL_Keycode key) {
    switch (key) {
    case SDLK_F:
        return OpeningKey::Found;
    case SDLK_U:
        return OpeningKey::Unload;
    case SDLK_H:
        return OpeningKey::Halt;
    case SDLK_T:
    case SDLK_COMMA:
        return OpeningKey::Rotate;
    case SDLK_PERIOD:
        return OpeningKey::RotateBack;
    case SDLK_TAB:
        return OpeningKey::NextHold;
    // [ ] are AltGr+8 / AltGr+9 on a German keyboard: minus / plus and Page Down / Page Up do
    // the same, with the same Shift for ten tons.
    case SDLK_LEFTBRACKET:
    case SDLK_MINUS:
    case SDLK_KP_MINUS:
    case SDLK_PAGEDOWN:
        return OpeningKey::QtyDown;
    case SDLK_RIGHTBRACKET:
    case SDLK_PLUS:
    case SDLK_EQUALS: // the + key of a US layout, unshifted
    case SDLK_KP_PLUS:
    case SDLK_PAGEUP:
        return OpeningKey::QtyUp;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        return OpeningKey::Send;
    case SDLK_F5:
        return OpeningKey::Save;
    default:
        return std::nullopt;
    }
}

} // namespace anno::app
