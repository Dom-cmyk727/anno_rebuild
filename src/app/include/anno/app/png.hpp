// Minimal PNG writer for headless renders: 8-bit RGBA, no interlace, image
// data in stored (uncompressed) deflate blocks. Files are large but need no
// compression library; any PNG reader opens them (PNG spec, ISO/IEC
// 15948:2004; zlib RFC 1950, deflate RFC 1951 §3.2.4).
#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace anno::app {

/// Writes `rgba` (width × height × 4 bytes, rows top to bottom) to `path`;
/// an empty string on success, else the error.
std::string write_png(const std::filesystem::path& path, std::uint32_t width, std::uint32_t height,
                      std::span<const std::uint8_t> rgba);

/// CRC-32 (ISO 3309, as PNG chunks use) of `data`, continuing from `crc`.
std::uint32_t crc32(std::span<const std::uint8_t> data, std::uint32_t crc = 0);

} // namespace anno::app
