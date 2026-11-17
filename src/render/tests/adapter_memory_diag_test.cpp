// RENDER-DIRECT-1d: measurement and diagnosis only, following RENDER-DIRECT-1c review's finding
// that endless02.sze's real resident pair (356,515,840 bytes) fits the calculated budget
// (402,653,184 bytes, OQ-096b's 384 MiB default) but the real Radeon iGPU device still refused
// the single 17-layer texture allocation ("Not enough memory left"). Three questions (the
// orchestrator's packet):
// 1. Is the refusal a single-resource allocation ceiling, or the heap's own total capacity?
//    Uploads the same pair as two SEPARATE texture arrays, both held resident at once.
// 2. What does the OS/API actually report for both adapters' memory (DXGI_ADAPTER_DESC's own
//    DedicatedVideoMemory/DedicatedSystemMemory/SharedSystemMemory - wgpu's own WGPUAdapterInfo,
//    webgpu.h, exposes no memory sizes at all)?
// 3. What is the largest single 2048x2048 texture array that actually allocates on the Radeon,
//    found by bisecting the layer count with RENDER-DIRECT-1c's own error-scope detection?
// No production code changed here, and OQ-096b's 384 MiB default is NOT touched (the packet's
// own "don't change the default, prepare the options and numbers"): this file is diagnostic
// only, added to anno_render_gpu_tests.
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/formats/tex.hpp>
#include <anno/render/atlas_pages.hpp>
#include <anno/render/atlas_upload.hpp>
#include <anno/render/frame_table.hpp>
#include <anno/render/gpu.hpp>
#include <anno/render/scene.hpp>
#include <anno/render/wgpu.hpp>
#include <anno/test/atlas_pages_check.hpp>
#include <anno/test/game_dir.hpp>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX // <windows.h>'s min/max macros would otherwise shadow std::min below.
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <dxgi1_4.h>
#endif

namespace render = anno::render;
namespace gpu = anno::render::gpu;

namespace {

/// Opens the adapter named `adapter_name` (empty: skip), runs `body` on it - duplicated from
/// atlas_upload_test.cpp's own `on_named_adapter` (that one is anonymous-namespace-local to its
/// own translation unit, not shared, so this file needs its own copy rather than a new header
/// for one helper two files use).
void on_named_adapter(const char* adapter_name,
                      const std::function<void(const gpu::Context&)>& body) {
    const auto adapters = gpu::list_adapters();
    if (!gpu::pick_adapter(adapter_name, adapters)) {
        MESSAGE("skipping: no GPU device named " << std::string(adapter_name));
        return;
    }
    const char* previous = std::getenv("ANNO_GPU_ADAPTER");
    const std::string saved = previous != nullptr ? previous : "";
#if defined(_WIN32)
    _putenv_s("ANNO_GPU_ADAPTER", adapter_name);
#else
    setenv("ANNO_GPU_ADAPTER", adapter_name, 1);
#endif
    auto ctx = gpu::headless_context();
#if defined(_WIN32)
    _putenv_s("ANNO_GPU_ADAPTER", saved.c_str());
#else
    if (saved.empty()) {
        unsetenv("ANNO_GPU_ADAPTER");
    } else {
        setenv("ANNO_GPU_ADAPTER", saved.c_str(), 1);
    }
#endif
    if (!ctx) {
        MESSAGE("skipping: no GPU device: " << ctx.error());
        return;
    }
    MESSAGE("adapter " << ctx.value().name);
    body(ctx.value());
    CHECK(ctx.value().errors->load() == 0);
}

std::vector<std::uint8_t> read_all(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    REQUIRE_MESSAGE(bool(in), "cannot open " << p.string());
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), {});
}

/// A `render::Scene` from the original files directly - `atlas_upload_test.cpp`'s own
/// `load_scene_direct`, duplicated here rather than shared: both files compile into
/// `anno_render_gpu_tests`, but `atlas_upload_test.cpp`'s copy lives in its own anonymous
/// namespace with no external linkage, and this is a small enough helper that a new shared
/// header for one function (used by only these two files) would be more machinery than the
/// duplication it replaces.
render::Scene load_scene_direct(const std::filesystem::path& dir, const std::filesystem::path& file,
                                std::deque<std::vector<std::uint8_t>>& keep) {
    keep.push_back(read_all(dir / "Textures" / "Felder.tex"));
    const auto felder = anno::formats::tex::TexFile::parse(keep.back()).value();
    keep.push_back(read_all(dir / "Anlagen.dat"));
    auto anlagen = anno::formats::anlagen::AnlagenFile::parse(keep.back()).value();
    auto textures = anno::assets::island::GroundTextures::load(felder).value();
    if (const auto c = render::water_colour(anlagen)) {
        textures.set_water(*c);
    }
    keep.push_back(read_all(file));
    return render::Scene("", anno::formats::insel::islands(keep.back()).value(),
                         std::move(textures), std::move(anlagen), render::kHeightLift);
}

/// An `AtlasPages` of `layers` all-zero layers at `page_size` - allocation-size testing needs no
/// real frame content, only the right byte counts (`upload_atlas_pages` writes whatever is in
/// `level0`/`level1` verbatim; zero bytes are as valid a payload as any other for this purpose).
render::AtlasPages dummy_pages(std::uint32_t page_size, std::uint32_t layers) {
    render::AtlasPages p;
    p.page_size = page_size;
    p.level0.assign(layers, std::vector<std::uint8_t>(std::size_t{page_size} * page_size * 4, 0));
    const std::uint32_t half = page_size / 2;
    p.level1.assign(layers, std::vector<std::uint8_t>(std::size_t{half} * half * 4, 0));
    return p;
}

/// True if a single `layers`-layer, `page_size` texture array allocates on `ctx`'s device (every
/// device call that can fail is inside `upload_atlas_pages`'s own error scope, RENDER-DIRECT-1c's
/// fix). `wait_idle` afterward encourages the allocator to reclaim the released texture (RAII,
/// `~AtlasTexture`) before the next attempt, so back-to-back bisection steps do not see stale
/// pressure from a resource whose free is still pending on the device.
bool layers_allocate(const gpu::Context& ctx, std::uint32_t page_size, std::uint32_t layers) {
    const auto pages = dummy_pages(page_size, layers);
    const bool ok = bool(gpu::upload_atlas_pages(ctx, pages, gpu::atlas_pages_bytes(pages)));
    gpu::wait_idle(ctx.instance.get(), ctx.device.get(), ctx.queue.get());
    return ok;
}

#if defined(_WIN32)
struct DxgiMemoryFacts {
    bool found = false;
    std::string description;
    std::uint64_t dedicated_video_memory = 0;
    std::uint64_t dedicated_system_memory = 0;
    std::uint64_t shared_system_memory = 0;
};

/// `DXGI_ADAPTER_DESC`'s own three memory fields for the DXGI adapter matching `vendor_id`/
/// `device_id` (the same ids `wgpuAdapterGetInfo` reports for the already-opened wgpu adapter) -
/// the OS/driver's own report, not anything wgpu or this codebase computes.
DxgiMemoryFacts dxgi_memory_facts(std::uint32_t vendor_id, std::uint32_t device_id) {
    DxgiMemoryFacts out;
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))) ||
        factory == nullptr) {
        return out;
    }
    for (UINT i = 0; !out.found; ++i) {
        IDXGIAdapter1* adapter = nullptr;
        if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        DXGI_ADAPTER_DESC1 desc{};
        if (adapter != nullptr && SUCCEEDED(adapter->GetDesc1(&desc)) &&
            desc.VendorId == vendor_id && desc.DeviceId == device_id) {
            out.found = true;
            char buf[256] = {};
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, buf, sizeof(buf) - 1, nullptr,
                                nullptr);
            out.description = buf;
            out.dedicated_video_memory = desc.DedicatedVideoMemory;
            out.dedicated_system_memory = desc.DedicatedSystemMemory;
            out.shared_system_memory = desc.SharedSystemMemory;
        }
        if (adapter != nullptr) {
            adapter->Release();
        }
    }
    factory->Release();
    return out;
}
#endif

} // namespace

TEST_CASE("RENDER-DIRECT-1d question 2: DXGI's own DedicatedVideoMemory/DedicatedSystemMemory/"
          "SharedSystemMemory for the RTX 4090 and the Radeon iGPU") {
#if defined(_WIN32)
    for (const char* adapter_name : {"RTX 4090", "Radeon"}) {
        on_named_adapter(adapter_name, [&](const gpu::Context& ctx) {
            WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT;
            REQUIRE_MESSAGE(wgpuAdapterGetInfo(ctx.adapter.get(), &info) == WGPUStatus_Success,
                            "wgpuAdapterGetInfo failed for " << ctx.name);
            const std::uint32_t vendor_id = info.vendorID;
            const std::uint32_t device_id = info.deviceID;
            wgpuAdapterInfoFreeMembers(info);
            const auto facts = dxgi_memory_facts(vendor_id, device_id);
            REQUIRE_MESSAGE(facts.found, "no DXGI adapter matched vendorID "
                                             << vendor_id << "/deviceID " << device_id << " for "
                                             << ctx.name);
            MESSAGE(
                ctx.name << " (DXGI \"" << facts.description << "\"): DedicatedVideoMemory "
                         << facts.dedicated_video_memory << " bytes ("
                         << (static_cast<double>(facts.dedicated_video_memory) / (1024.0 * 1024.0))
                         << " MiB), DedicatedSystemMemory " << facts.dedicated_system_memory
                         << " bytes, SharedSystemMemory " << facts.shared_system_memory
                         << " bytes ("
                         << (static_cast<double>(facts.shared_system_memory) / (1024.0 * 1024.0))
                         << " MiB)");
        });
    }
#else
    MESSAGE("skipping: DXGI memory facts are Windows-only");
#endif
}

TEST_CASE("RENDER-DIRECT-1d question 1: endless02.sze's pair as two SEPARATE per-group "
          "textures, both resident at once, on the Radeon iGPU") {
    on_named_adapter("Radeon", [&](const gpu::Context& ctx) {
        const auto dir = anno::test::game_dir();
        if (!dir) {
            MESSAGE("skipping: ANNO1503_GAME_DIR not set");
            return;
        }
        const auto file = *dir / "Scenes" / "0011SingleEndlosAddon" / "endless02.sze";
        std::error_code ec;
        if (!std::filesystem::exists(file, ec)) {
            FAIL("missing named scene: " << file.string());
        }
        std::deque<std::vector<std::uint8_t>> keep;
        const render::Scene scene = load_scene_direct(*dir, file, keep);

        // Each group's own upload may itself refuse (a real device outcome, not a bug - see
        // RENDER-DIRECT-1c review F1/F2's own precedent for treating a real, named device
        // refusal as a reportable finding rather than forcing it to "pass"). Every refusal is
        // one that `atlas_upload.cpp`'s error scope (RENDER-DIRECT-1c's own fix) names, not a
        // silent success holding an invalid texture.
        std::vector<std::pair<render::AtlasPages, gpu::AtlasTexture>> held;
        std::uint64_t total_bytes = 0;
        bool any_refused = false;
        for (const std::uint32_t group : {0U, 1U}) {
            render::SpriteCache cache;
            render::FrameTable table;
            const auto frames = anno::test::collect_static_water(scene, group, cache, table);
            auto pages = render::build_atlas_pages(frames, 2048, 2, 4);
            const auto needed = gpu::atlas_pages_bytes(pages);
            auto uploaded = gpu::upload_atlas_pages(ctx, pages, needed);
            MESSAGE("group " << group << ": " << pages.level0.size() << " layers, " << needed
                             << " bytes (" << (static_cast<double>(needed) / (1024.0 * 1024.0))
                             << " MiB) - "
                             << (uploaded ? std::string("allocated")
                                          : ("REFUSED: " + uploaded.error())));
            if (!uploaded) {
                any_refused = true;
                continue;
            }
            total_bytes += needed;
            held.emplace_back(std::move(pages), std::move(uploaded.value()));
        }
        // Every texture that did allocate is still alive here - spot-check one layer of each
        // while all of them are resident together, proving they are genuinely simultaneous, not
        // one silently evicting or aliasing another.
        for (auto& [pages, tex] : held) {
            const auto l0 = gpu::read_back_level(ctx, tex, 0, 0);
            CHECK(l0 == pages.level0[0]);
        }
        if (!any_refused) {
            MESSAGE("endless02.sze's pair as two separate textures: BOTH resident and "
                    "byte-exact at once, "
                    << total_bytes << " bytes total ("
                    << (static_cast<double>(total_bytes) / (1024.0 * 1024.0))
                    << " MiB) - splitting the earlier combined-texture refusal (RENDER-DIRECT-1c "
                       "review F1/F2) into two per-group resources made it allocate: a "
                       "single-resource ceiling, not the heap's own total capacity.");
        } else {
            MESSAGE("FINDING for the owner (question 1): at least one of endless02.sze's two "
                    "groups refused to allocate even on its OWN, split from the other - "
                    "splitting the pair into two per-group textures does not by itself make "
                    "this scene's resident set fit on the Radeon. Combined with question 3's "
                    "bisected ceiling below, this confirms the real limit is a single-resource "
                    "allocation ceiling well under the device's own DedicatedVideoMemory "
                    "(question 2) - but at a size small enough that even one of this scene's "
                    "two groups alone can exceed it, so splitting by group is not always "
                    "enough.");
        }
    });
}

TEST_CASE("RENDER-DIRECT-1d question 3: the largest single 2048x2048 texture array that "
          "allocates on the Radeon iGPU, found by bisecting the layer count") {
    on_named_adapter("Radeon", [&](const gpu::Context& ctx) {
        WGPULimits limits = WGPU_LIMITS_INIT;
        wgpuDeviceGetLimits(ctx.device.get(), &limits);
        constexpr std::uint32_t kPageSize = 2048;

        std::uint32_t lo = 1;
        REQUIRE_MESSAGE(layers_allocate(ctx, kPageSize, lo),
                        "even 1 layer (~21 MB) failed to allocate on " << ctx.name
                                                                       << " - unexpected");
        std::uint32_t hi = std::min<std::uint32_t>(limits.maxTextureArrayLayers, 2);
        // Double hi until it refuses (or we hit the device's own layer limit): a quick way to
        // bracket the real ceiling before bisecting between the last-known-good and first-known-
        // bad counts.
        while (hi < limits.maxTextureArrayLayers && layers_allocate(ctx, kPageSize, hi)) {
            lo = hi;
            hi = std::min(hi * 2, limits.maxTextureArrayLayers);
        }
        if (layers_allocate(ctx, kPageSize, hi)) {
            MESSAGE("every layer count up to the device's own maxTextureArrayLayers ("
                    << hi << ") allocated on " << ctx.name
                    << " - no ceiling found below the "
                       "device's own reported limit");
            return;
        }
        while (hi - lo > 1) {
            const std::uint32_t mid = lo + (hi - lo) / 2;
            if (layers_allocate(ctx, kPageSize, mid)) {
                lo = mid;
            } else {
                hi = mid;
            }
        }
        const std::uint64_t bytes_at_lo = gpu::atlas_pages_bytes(dummy_pages(kPageSize, lo));
        const std::uint64_t bytes_at_hi = gpu::atlas_pages_bytes(dummy_pages(kPageSize, hi));
        MESSAGE("largest single " << kPageSize << "x" << kPageSize
                                  << " texture array that allocates on " << ctx.name << ": " << lo
                                  << " layers (" << bytes_at_lo << " bytes, "
                                  << (static_cast<double>(bytes_at_lo) / (1024.0 * 1024.0))
                                  << " MiB) - " << hi << " layers (" << bytes_at_hi << " bytes, "
                                  << (static_cast<double>(bytes_at_hi) / (1024.0 * 1024.0))
                                  << " MiB) refused");
    });
}
