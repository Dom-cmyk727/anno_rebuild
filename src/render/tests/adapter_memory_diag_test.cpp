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
// A follow-up question, after question 3's bisected ceiling was found (the orchestrator's own
// renumbering reply, OQ-727):
// 4. Does the Radeon's heap hold endless02.sze's full pair's bytes once split under the
//    single-resource ceiling (12+5 layers, not tied to either group's own layer count), both
//    resident and byte-exact? And where does the TOTAL heap stop - the largest total that
//    allocates, found by adding more blocks until one is refused, then bisecting the last one?
// CI gate fix (orchestrator, 2026-11-17, matching RENDER-DIRECT-1c's own atlas_upload_test.cpp
// fix): questions 1 and 4 check game_dir() before on_named_adapter (CI's game-file-less runner
// then skips as the allowed `required-asset`, before reaching the disallowed-on-linux-gcc `gpu`
// skip); question 3 has no game-file escape, so it falls back to whatever adapter is available
// via on_preferred_or_any_adapter instead of skipping when "Radeon" is absent (OQ-089's own
// point: the method is what matters, not necessarily this specific adapter, on a runner that
// has no Radeon at all) - EXCEPT question 3's bisection itself, a hardware measurement that is
// skipped outright on a non-Vulkan/Metal/D3D12 backend or a CPU adapter type (the owner's own
// call, 2026-11-17): a software rasterizer's permissive allocator and a CI runner's limited RAM
// make an unbounded-looking probe there meaningless and risky (it hung two CI jobs, exit 143,
// before this skip and a practical 256-layer probe ceiling were both added).
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

/// Opens `preferred_name` when present, else whatever adapter is available (never skips solely
/// because the preferred name is absent) - for question 3's bisection, which measures "this
/// device's own ceiling" generically (the method, not the Radeon specifically, is what the
/// orchestrator's packet asked to find): on the owner's own PC this still opens the Radeon
/// exactly as `on_named_adapter` would; on a CI runner with no Radeon (lavapipe on `linux-gcc`)
/// it falls back to lavapipe rather than skipping as `gpu` (disallowed there, OQ-089) - CI gate
/// fix, orchestrator, 2026-11-17.
void on_preferred_or_any_adapter(const char* preferred_name,
                                 const std::function<void(const gpu::Context&)>& body) {
    const auto adapters = gpu::list_adapters();
    const bool have_preferred = gpu::pick_adapter(preferred_name, adapters).has_value();
    const char* previous = std::getenv("ANNO_GPU_ADAPTER");
    const std::string saved = previous != nullptr ? previous : "";
    if (have_preferred) {
#if defined(_WIN32)
        _putenv_s("ANNO_GPU_ADAPTER", preferred_name);
#else
        setenv("ANNO_GPU_ADAPTER", preferred_name, 1);
#endif
    }
    auto ctx = gpu::headless_context();
    if (have_preferred) {
#if defined(_WIN32)
        _putenv_s("ANNO_GPU_ADAPTER", saved.c_str());
#else
        if (saved.empty()) {
            unsetenv("ANNO_GPU_ADAPTER");
        } else {
            setenv("ANNO_GPU_ADAPTER", saved.c_str(), 1);
        }
#endif
    }
    if (!ctx) {
        if (std::getenv("ANNO_REQUIRE_GPU") != nullptr) {
            FAIL("no GPU device at all (ANNO_REQUIRE_GPU is set): " << ctx.error());
        }
        MESSAGE("skipping: no GPU device: " << ctx.error());
        return;
    }
    MESSAGE("adapter " << ctx.value().name
                       << std::string(have_preferred
                                          ? ""
                                          : " (preferred adapter not found; using whatever is "
                                            "available instead)"));
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

/// `src`'s own layers `[begin, end)`, copied into a new, smaller `AtlasPages` at the same
/// `page_size` - a real-data split of an already-built combined page set at an arbitrary layer
/// boundary (not tied to where one zoom group's own frames end, unlike question 1's per-group
/// split): the orchestrator's follow-up asks for a 12+5 split of `endless02.sze`'s 17-layer
/// pair, which does not line up with either group's own layer count (14 and 3 within the
/// combined set, in collection order).
render::AtlasPages slice_pages(const render::AtlasPages& src, std::size_t begin, std::size_t end) {
    render::AtlasPages out;
    out.page_size = src.page_size;
    out.level0.assign(src.level0.begin() + static_cast<std::ptrdiff_t>(begin),
                      src.level0.begin() + static_cast<std::ptrdiff_t>(end));
    out.level1.assign(src.level1.begin() + static_cast<std::ptrdiff_t>(begin),
                      src.level1.begin() + static_cast<std::ptrdiff_t>(end));
    return out;
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

/// Attempts one more `layers`-layer dummy block ON TOP of whatever is already resident in
/// `held` - every earlier entry in `held` stays alive (held, not released) for the whole
/// attempt, since the orchestrator's own follow-up question is the TOTAL heap's own capacity
/// with other resident textures present, not one resource allocated in isolation (RENDER-
/// DIRECT-1d question 3's own `layers_allocate` always starts from an otherwise-empty device).
/// On success the new texture is appended to `held` (stays resident, counted toward the total);
/// on failure `held` is unchanged.
bool try_hold_another(const gpu::Context& ctx, std::uint32_t page_size, std::uint32_t layers,
                      std::vector<gpu::AtlasTexture>& held) {
    const auto pages = dummy_pages(page_size, layers);
    auto uploaded = gpu::upload_atlas_pages(ctx, pages, gpu::atlas_pages_bytes(pages));
    if (!uploaded) {
        return false;
    }
    held.push_back(std::move(uploaded.value()));
    return true;
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
    // CI gate fix (orchestrator, 2026-11-17, matching RENDER-DIRECT-1c's own atlas_upload_test.cpp
    // fix): game_dir() checked before on_named_adapter, so CI's game-file-less runner skips as
    // the allowed `required-asset` before ever reaching the "no adapter named Radeon" skip
    // (classified `gpu`, disallowed on linux-gcc) - same order play_gpu_test.cpp's own
    // play_gpu_matches_cpu uses.
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

    on_named_adapter("Radeon", [&](const gpu::Context& ctx) {
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
          "allocates on the Radeon iGPU (or, if absent, whatever adapter is available), found "
          "by bisecting the layer count (opt-in: ANNO_MEASURE_TEXTURE_CEILING=1)") {
    // Owner's call, via the orchestrator, 2026-11-17: this bisection is a HARDWARE measurement
    // (a real allocator's own ceiling), not a correctness test, so it is opt-in rather than
    // skipped by adapter type/backend - a CPU-type or non-Vulkan backend check would ALSO catch
    // lavapipe-via-Vulkan on CI's own linux-gcc job (lavapipe's own adapter_type is CPU even
    // though its backend is Vulkan, since it is a software Vulkan implementation), which is the
    // one job `ANNO_REQUIRE_GPU=1` disallows any `gpu`-classified skip on at all (it is meant to
    // run the GPU parity tests for real there, OQ-089) - so that approach skips this case on
    // EVERY CI job, including the one whose policy forbids it. An opt-in switch sidesteps the
    // conflict entirely: `ctest_verdict.py`'s own `OPT_IN` pattern (`\bset ANNO_[A-Z0-9_]+=`) is
    // checked before its `GPU` pattern, and the message below leads with it, so this is
    // classified `opt-in` (allowed on every job, `linux-gcc` included) rather than `gpu`,
    // without touching any other GPU correctness case in this file or atlas_upload_test.cpp -
    // they keep running on lavapipe for real, unaffected, matching RENDER-DIRECT-1c's own
    // adapter-agnostic synthetic case.
    if (std::getenv("ANNO_MEASURE_TEXTURE_CEILING") == nullptr) {
        MESSAGE("skipping: set ANNO_MEASURE_TEXTURE_CEILING=1 to run this hardware probe (a "
                "real allocator's own ceiling - costly, and on a software rasterizer "
                "meaningless, so it is opt-in rather than run automatically)");
        return;
    }
    on_preferred_or_any_adapter("Radeon", [&](const gpu::Context& ctx) {
        WGPULimits limits = WGPU_LIMITS_INIT;
        wgpuDeviceGetLimits(ctx.device.get(), &limits);
        constexpr std::uint32_t kPageSize = 2048;
        // A practical probe ceiling (bounded probe memory), independent of and far tighter than
        // the device's own reported maxTextureArrayLayers: on real hardware the device's own
        // limit stays the real bound in practice (a real GPU refuses long before this ceiling -
        // the ~12-13 layer ceiling already found on the Radeon, this question's own earlier
        // result), but a permissive software rasterizer's own reported limit can be far larger
        // than any real GPU's, letting every layer count the doubling search tries allocate -
        // this ceiling bounds that case too (whoever opts in on a software adapter), at ~5 GiB
        // of dummy probe data at its 256-layer top end rather than the tens of gigabytes that
        // hung a CI job (exit 143, killed on timeout) before the opt-in switch above existed.
        constexpr std::uint32_t kPracticalCeiling = 256;
        const std::uint32_t ceiling = std::min(limits.maxTextureArrayLayers, kPracticalCeiling);

        std::uint32_t lo = 1;
        REQUIRE_MESSAGE(layers_allocate(ctx, kPageSize, lo),
                        "even 1 layer (~21 MB) failed to allocate on " << ctx.name
                                                                       << " - unexpected");
        std::uint32_t hi = std::min<std::uint32_t>(ceiling, 2);
        // Double hi until it refuses (or we hit the practical ceiling): a quick way to bracket
        // the real ceiling before bisecting between the last-known-good and first-known-bad
        // counts.
        while (hi < ceiling && layers_allocate(ctx, kPageSize, hi)) {
            lo = hi;
            hi = std::min(hi * 2, ceiling);
        }
        if (layers_allocate(ctx, kPageSize, hi)) {
            MESSAGE("every layer count up to the practical probe ceiling ("
                    << hi << "; the device's own maxTextureArrayLayers is "
                    << limits.maxTextureArrayLayers << ") allocated on " << ctx.name
                    << " - no ceiling found within the practical probe range");
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

TEST_CASE("RENDER-DIRECT-1d question 4 (orchestrator follow-up): endless02.sze's full pair "
          "split under the single-resource ceiling, both resident and byte-exact, then the "
          "total heap's own stopping point on the Radeon iGPU") {
    // CI gate fix (orchestrator, 2026-11-17): game_dir() checked before on_named_adapter - see
    // question 1's own doc comment above.
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

    on_named_adapter("Radeon", [&](const gpu::Context& ctx) {
        // The same combined 17-layer page set RENDER-DIRECT-1c's worst_pair_scene built: both
        // groups' frames folded into one shared FrameTable/SpriteCache (bench_frame's own
        // collect_static_water_groups, folded here by hand - both groups are wanted regardless
        // of what resident_atlas_groups would return for this adapter, since the question is
        // about the real worst pair specifically, not this adapter's own residency choice).
        render::SpriteCache cache;
        render::FrameTable table;
        std::vector<render::AtlasFrameInput> frames;
        for (const std::uint32_t group : {0U, 1U}) {
            frames = anno::test::collect_static_water(scene, group, cache, table);
        }
        const auto pair_pages = render::build_atlas_pages(frames, 2048, 2, 4);
        REQUIRE_MESSAGE(pair_pages.level0.size() == 17,
                        "endless02.sze's pair is " << pair_pages.level0.size()
                                                   << " layers, not the 17 RENDER-DIRECT-1c "
                                                      "measured - re-check before trusting the "
                                                      "12+5 split below");

        constexpr std::size_t kSplit = 12;
        std::vector<render::AtlasPages> slices;
        slices.push_back(slice_pages(pair_pages, 0, kSplit));
        slices.push_back(slice_pages(pair_pages, kSplit, pair_pages.level0.size()));

        std::vector<gpu::AtlasTexture> held;
        std::uint64_t resident_bytes = 0;
        for (const auto& slice : slices) {
            const auto needed = gpu::atlas_pages_bytes(slice);
            auto uploaded = gpu::upload_atlas_pages(ctx, slice, needed);
            REQUIRE_MESSAGE(bool(uploaded), "endless02.sze's pair split "
                                                << slice.level0.size() << " layers (" << needed
                                                << " bytes) failed to allocate - under "
                                                   "the single-resource ceiling "
                                                   "(question 3), so this would be a "
                                                   "new, different finding, not the "
                                                   "one asked about: "
                                                << (uploaded ? "" : uploaded.error()));
            resident_bytes += needed;
            held.push_back(std::move(uploaded.value()));
        }
        // Byte-exact (first and last layer of each half) while BOTH halves are resident at once.
        for (std::size_t i = 0; i < slices.size(); ++i) {
            const auto l0_first = gpu::read_back_level(ctx, held[i], 0, 0);
            CHECK(l0_first == slices[i].level0.front());
            const auto last = static_cast<std::uint32_t>(slices[i].level0.size() - 1);
            const auto l0_last = gpu::read_back_level(ctx, held[i], last, 0);
            CHECK(l0_last == slices[i].level0.back());
        }
        MESSAGE("endless02.sze's full pair split 12+5 under the single-resource ceiling "
                "(question 3): BOTH resident and byte-exact at once, "
                << resident_bytes << " bytes ("
                << (static_cast<double>(resident_bytes) / (1024.0 * 1024.0))
                << " MiB) - splitting under the per-resource ceiling DOES hold the real worst "
                   "case's full bytes, answering the orchestrator's own question: splitting is "
                   "enough for the bytes themselves.");

        // Where does the TOTAL heap stop? Add more page_size=2048, 12-layer (240 MiB) blocks on
        // top of the 340 MiB already resident (dummy content: there is no more real scene data
        // once the actual worst pair is fully resident, and the question is capacity, not
        // content) until a whole block refuses, then bisect one smaller, partial block for a
        // precise total rather than only knowing it is somewhere within one more 240 MiB step.
        std::uint32_t whole_blocks_added = 0;
        constexpr std::uint32_t kMaxWholeBlocks = 32; // generous: 32 * 240 MiB = 7.5 GiB, far
                                                      // past any plausible total on this PC.
        while (whole_blocks_added < kMaxWholeBlocks && try_hold_another(ctx, 2048, 12, held)) {
            ++whole_blocks_added;
            resident_bytes += gpu::atlas_pages_bytes(dummy_pages(2048, 12));
            gpu::wait_idle(ctx.instance.get(), ctx.device.get(), ctx.queue.get());
        }
        REQUIRE_MESSAGE(whole_blocks_added < kMaxWholeBlocks,
                        "every whole 12-layer block up to " << kMaxWholeBlocks
                                                            << " allocated - no total ceiling "
                                                               "found within a plausible range");
        MESSAGE(whole_blocks_added << " additional whole 12-layer (240 MiB) block(s) also fit on "
                                      "top of the resident 340 MiB pair before a further whole "
                                      "block was refused.");
        std::uint32_t lo = 0, hi = 12;
        while (hi - lo > 1) {
            const std::uint32_t mid = lo + (hi - lo) / 2;
            if (try_hold_another(ctx, 2048, mid, held)) {
                lo = mid;
                resident_bytes += gpu::atlas_pages_bytes(dummy_pages(2048, mid));
            } else {
                hi = mid;
            }
            gpu::wait_idle(ctx.instance.get(), ctx.device.get(), ctx.queue.get());
        }
        MESSAGE("largest TOTAL that allocates on "
                << ctx.name << ": " << resident_bytes << " bytes ("
                << (static_cast<double>(resident_bytes) / (1024.0 * 1024.0))
                << " MiB) = the resident 340 MiB pair + " << whole_blocks_added
                << " whole 12-layer block(s) + a further " << lo
                << "-layer block; the next block size tried (" << hi << " layers) was refused.");
    });
}
