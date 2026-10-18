#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>

#include <anno/test/game_dir.hpp>

namespace anno::test {

std::optional<std::filesystem::path> game_dir() {
    const char* dir = std::getenv("ANNO1503_GAME_DIR");
    if (dir == nullptr || *dir == '\0') {
        return std::nullopt;
    }
    const std::filesystem::path path(dir);
    if (const std::string problem = game_dir_problem(path); !problem.empty()) {
        throw std::runtime_error("ANNO1503_GAME_DIR=" + path.string() + ": " + problem +
                                 ". Point it at the installed game (the folder with "
                                 "Anlagen.dat), or unset it to skip the game-file tests.");
    }
    return path;
}

std::string game_dir_problem(const std::filesystem::path& dir) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) {
        return "the folder does not exist";
    }
    if (!std::filesystem::is_directory(dir, ec)) {
        return "not a folder";
    }
    if (std::filesystem::is_regular_file(dir / "Anlagen.dat", ec)) {
        return {};
    }
    // Installers sometimes add a folder level; name it.
    for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
        if (e.is_directory(ec) && std::filesystem::is_regular_file(e.path() / "Anlagen.dat", ec)) {
            return "no Anlagen.dat here, but in " + e.path().string();
        }
    }
    return "no Anlagen.dat: not the game folder";
}

namespace {
bool iequal(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; };
               return lower(x) == lower(y);
           });
}
} // namespace

std::vector<std::filesystem::path> files_with_ext(const std::filesystem::path& dir,
                                                  std::string_view ext) {
    std::vector<std::filesystem::path> out;
    for (const auto& e : std::filesystem::recursive_directory_iterator(dir)) {
        if (!e.is_regular_file()) {
            continue;
        }
        const std::string x = e.path().extension().string();
        if (x.size() > 1 && iequal(std::string_view(x).substr(1), ext)) {
            out.push_back(e.path());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

} // namespace anno::test
