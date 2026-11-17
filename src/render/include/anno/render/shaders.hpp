// The WGSL shaders, embedded at build time from src/render/shaders/ (shared
// with the Rust reference, crates/render; cmake/EmbedFile.cmake).
#pragma once

#include <cstddef>
#include <string_view>

extern const unsigned char kGpuWgsl[];
extern const std::size_t kGpuWgsl_size;
extern const unsigned char kFluidWgsl[];
extern const std::size_t kFluidWgsl_size;
extern const unsigned char kAtlasUploadWgsl[];
extern const std::size_t kAtlasUploadWgsl_size;

namespace anno::render::shaders {

/// The compositor: ground, sprites, sea, objects (gpu.wgsl).
inline std::string_view compositor() {
    return {reinterpret_cast<const char*>(kGpuWgsl), kGpuWgsl_size};
}
/// The coastal wave simulation of the enhanced water (fluid.wgsl).
inline std::string_view fluid() {
    return {reinterpret_cast<const char*>(kFluidWgsl), kFluidWgsl_size};
}
/// RENDER-DIRECT-1c step 3's own sampling test (atlas_upload.wgsl): a new unit, not part of
/// the compositor.
inline std::string_view atlas_upload() {
    return {reinterpret_cast<const char*>(kAtlasUploadWgsl), kAtlasUploadWgsl_size};
}

} // namespace anno::render::shaders
