#include <anno/app/cli.hpp>
#include <anno/app/png.hpp>
#include <anno/app/window_capture.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <utility>
#include <vector>
#endif

namespace anno::app {

#ifdef _WIN32

std::string capture_window_screen(SDL_Window* window, const std::filesystem::path& file, int& w,
                                  int& h) {
    const SDL_PropertiesID props = SDL_GetWindowProperties(window);
    HWND hwnd = static_cast<HWND>(
        SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (hwnd == nullptr) {
        return "the window has no native handle (a headless video driver)";
    }
    SDL_RaiseWindow(window);
    RECT rc{};
    POINT origin{0, 0};
    if (!GetClientRect(hwnd, &rc) || !ClientToScreen(hwnd, &origin)) {
        return "the client area could not be read";
    }
    w = static_cast<int>(rc.right - rc.left);
    h = static_cast<int>(rc.bottom - rc.top);
    if (w <= 0 || h <= 0) {
        return "the window has no pixels";
    }
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, w, h);
    HGDIOBJ old = SelectObject(mem, bitmap);
    std::string error;
    if (!BitBlt(mem, 0, 0, w, h, screen, origin.x, origin.y, SRCCOPY | CAPTUREBLT)) {
        error = "the desktop copy failed";
    } else {
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = w;
        info.bmiHeader.biHeight = -h; // top-down
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        std::vector<std::uint8_t> bgra(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4);
        if (GetDIBits(mem, bitmap, 0, static_cast<UINT>(h), bgra.data(), &info, DIB_RGB_COLORS) ==
            0) {
            error = "the bitmap could not be read";
        } else {
            for (std::size_t i = 0; i + 3 < bgra.size(); i += 4) {
                std::swap(bgra[i], bgra[i + 2]);
                bgra[i + 3] = 255;
            }
            error = write_png(file, static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), bgra);
        }
    }
    SelectObject(mem, old);
    DeleteObject(bitmap);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return error;
}

#else

std::string capture_window_screen(SDL_Window*, const std::filesystem::path&, int&, int&) {
    return "no desktop capture on this platform";
}

#endif

} // namespace anno::app
