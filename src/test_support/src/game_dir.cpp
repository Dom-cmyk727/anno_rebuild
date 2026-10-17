#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>

#include <anno/test/game_dir.hpp>

namespace anno::test {

std::optional<std::filesystem::path> game_dir() {
    const char* dir = std::getenv("ANNO1503_GAME_DIR");
    if (dir == nullptr || *dir == '\0') {
        return std::nullopt;
    }
    return std::filesystem::path(dir);
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
