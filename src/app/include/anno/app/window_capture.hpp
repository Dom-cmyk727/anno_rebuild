// Saving what a window shows as a PNG, for the scripted sessions' `screenshot` step (W4-OPENING-1).
// It copies the window's client area from the desktop (the compositor's own result, so a GPU window's
// swapchain image is captured too); the window must be visible and not covered. It is evidence for
// a person to inspect, not a pixel-exact reference: the actual captured size is reported.
#pragma once

#include <SDL3/SDL_video.h>
#include <filesystem>
#include <string>

namespace anno::app {

/// Captures the client area of `window` into `file`. Returns an empty string on success (and the
/// size in `w`, `h`), else why not. Windows only; elsewhere it says it is not available.
[[nodiscard]] std::string capture_window_screen(SDL_Window* window, const std::filesystem::path& file,
                                                int& w, int& h);

} // namespace anno::app
