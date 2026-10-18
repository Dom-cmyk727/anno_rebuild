// The anno1503 command line (same options as the Rust viewer,
// crates/app/src/main.rs). Strings are UTF-8 on every platform; paths are
// built with utf8_path so non-ASCII file names work on Windows too.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <anno/render/iso.hpp>

namespace anno::app {

inline constexpr const char* kUsage =
    "usage: anno1503 --view <file.sze|file.scp> [--game-dir <dir>] [--group <0|1|2>] [--png "
    "<out.png> [--time <ms>] [--at <x>,<y> [--size <w>x<h>]]] [--water original|enhanced] "
    "[--renderer auto|gpu|cpu]\n";

/// World tiles accepted by --at: INSEL5 positions are u16 (docs/formats/insel.md);
/// the margin keeps every projected coordinate inside int range.
inline constexpr int kMaxAtTile = 65536;
/// Pixels per dimension accepted by --size; larger regions exceed the 4 GiB
/// canvas limit anyway and would overflow screen coordinates.
inline constexpr int kMaxSizePx = 65535;

enum class Renderer { Auto, Gpu, Cpu };

struct Args {
    std::filesystem::path file;
    std::optional<std::filesystem::path> game_dir;
    std::optional<std::size_t> group;
    std::optional<std::filesystem::path> png;
    std::uint64_t time = 0;
    std::optional<std::pair<int, int>> at;
    std::pair<int, int> size{1600, 900};
    render::WaterStyle water = render::WaterStyle::Original;
    Renderer renderer = Renderer::Auto;
};

/// Parses option/value pairs; nullopt for anything invalid (print kUsage,
/// exit 2). Numbers may carry a leading '+', as Rust's parse accepts.
std::optional<Args> parse_args(const std::vector<std::string>& args);

/// A path from a UTF-8 string.
std::filesystem::path utf8_path(const std::string& s);
/// A path as a UTF-8 string.
std::string to_utf8(const std::filesystem::path& p);

} // namespace anno::app
