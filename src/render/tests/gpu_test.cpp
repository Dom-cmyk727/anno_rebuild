// GPU device and shaders (wgpu-native). Skips when the machine has no GPU
// adapter (e.g. CI runners without one).
#include <doctest/doctest.h>

#include <anno/render/shaders.hpp>
#include <anno/render/wgpu.hpp>

namespace gpu = anno::render::gpu;

TEST_CASE("the shared WGSL shaders compile on the device") {
    auto c = gpu::headless_context();
    if (!c) {
        MESSAGE("skipping: " << c.error());
        return;
    }
    MESSAGE("adapter: " << c.value().name);
    CHECK(anno::render::shaders::compositor().size() > 1000);
    auto a = gpu::create_shader(c.value(), anno::render::shaders::compositor(), "gpu.wgsl");
    CHECK_MESSAGE(a, (a ? "" : a.error()));
    auto b = gpu::create_shader(c.value(), anno::render::shaders::fluid(), "fluid.wgsl");
    CHECK_MESSAGE(b, (b ? "" : b.error()));
    // A broken shader reports the compiler's error instead of failing later.
    auto bad = gpu::create_shader(c.value(), "fn main( {", "broken.wgsl");
    REQUIRE_FALSE(bad);
    CHECK(bad.error().find("broken.wgsl") == 0);
}
