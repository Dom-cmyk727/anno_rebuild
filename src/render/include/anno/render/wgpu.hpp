// A thin C++ layer over webgpu.h (wgpu-native, ADR 0001): owning handles,
// a headless device, and shader modules whose WGSL errors are reported.
// Rendering logic lives in gpu.hpp; this file only manages the API.
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

#include <anno/formats/result.hpp>

namespace anno::render::gpu {

/// An owned WebGPU object: released when destroyed, movable, not copyable.
template <class T, void (*Release)(T)> class Handle {
  public:
    Handle() = default;
    explicit Handle(T h) : h_(h) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& o) noexcept : h_(std::exchange(o.h_, nullptr)) {}
    Handle& operator=(Handle&& o) noexcept {
        if (this != &o) {
            reset();
            h_ = std::exchange(o.h_, nullptr);
        }
        return *this;
    }
    ~Handle() { reset(); }
    void reset() {
        if (h_ != nullptr) {
            Release(h_);
            h_ = nullptr;
        }
    }
    [[nodiscard]] T get() const { return h_; }
    explicit operator bool() const { return h_ != nullptr; }

  private:
    T h_ = nullptr;
};

using Instance = Handle<WGPUInstance, wgpuInstanceRelease>;
using Adapter = Handle<WGPUAdapter, wgpuAdapterRelease>;
using Device = Handle<WGPUDevice, wgpuDeviceRelease>;
using Queue = Handle<WGPUQueue, wgpuQueueRelease>;
using ShaderModule = Handle<WGPUShaderModule, wgpuShaderModuleRelease>;
using Buffer = Handle<WGPUBuffer, wgpuBufferRelease>;
using Texture = Handle<WGPUTexture, wgpuTextureRelease>;
using TextureView = Handle<WGPUTextureView, wgpuTextureViewRelease>;
using BindGroup = Handle<WGPUBindGroup, wgpuBindGroupRelease>;
using BindGroupLayout = Handle<WGPUBindGroupLayout, wgpuBindGroupLayoutRelease>;
using PipelineLayout = Handle<WGPUPipelineLayout, wgpuPipelineLayoutRelease>;
using RenderPipeline = Handle<WGPURenderPipeline, wgpuRenderPipelineRelease>;
using ComputePipeline = Handle<WGPUComputePipeline, wgpuComputePipelineRelease>;
using CommandEncoder = Handle<WGPUCommandEncoder, wgpuCommandEncoderRelease>;
using CommandBuffer = Handle<WGPUCommandBuffer, wgpuCommandBufferRelease>;
using Surface = Handle<WGPUSurface, wgpuSurfaceRelease>;

/// A WGPUStringView of `s` (not NUL-terminated; valid while `s` is).
inline WGPUStringView sv(std::string_view s) {
    return WGPUStringView{s.data(), s.size()};
}
/// A string from a WGPUStringView (NULL data = empty; SIZE_MAX = NUL-terminated).
std::string str(WGPUStringView v);

/// Uncaptured device errors so far (validation, out of memory, internal);
/// shared by everything that uses the device, so tests and checks can
/// assert that none occurred.
using ErrorCount = std::shared_ptr<std::atomic<std::uint64_t>>;

/// An instance, adapter, device and queue.
struct Context {
    Instance instance;
    Adapter adapter;
    Device device;
    Queue queue;
    /// "<adapter name> (<backend>)".
    std::string name;
    ErrorCount errors = std::make_shared<std::atomic<std::uint64_t>>(0);
};

/// Owned references to a context's instance, device and queue (and its
/// error count), so a renderer stays valid when the Context is moved.
struct DeviceRef {
    Instance instance;
    Device device;
    Queue queue;
    ErrorCount errors;
    explicit DeviceRef(const Context& c);
    DeviceRef(const DeviceRef& o);
    DeviceRef& operator=(const DeviceRef&) = delete;
    DeviceRef(DeviceRef&&) noexcept = default;
    DeviceRef& operator=(DeviceRef&&) noexcept = default;
    ~DeviceRef() = default;

  private:
    DeviceRef(WGPUInstance i, WGPUDevice d, WGPUQueue q, ErrorCount e);
};

/// Opens the high-performance adapter without a surface and a device with
/// the adapter's limits (as the Rust viewer). Uncaptured device errors are
/// printed to stderr and counted (Context::errors; wgpu in Rust panics
/// instead), a lost device is printed. WGPU_BACKEND (vulkan, metal, dx12,
/// gl) picks the backend, as wgpu's *_from_env. The error names what failed
/// (e.g. no adapter).
formats::Result<Context, std::string> headless_context();

/// As headless_context, on `instance` with an adapter that can present to
/// `compatible` (a window surface; null = none).
formats::Result<Context, std::string> context_for(Instance instance, WGPUSurface compatible);

/// Runs the device's pending callbacks; with `wait`, blocks until the
/// submitted work is done (wgpu-native wgpuDevicePoll).
void poll(const Context& c, bool wait);

/// A shader module from WGSL; the error holds the compiler's message.
formats::Result<ShaderModule, std::string> create_shader(const Context& c, std::string_view wgsl,
                                                         std::string_view label);
formats::Result<ShaderModule, std::string> create_shader(WGPUInstance instance, WGPUDevice device,
                                                         std::string_view wgsl,
                                                         std::string_view label);

/// Pops the device's error scope and waits for the answer: empty if the
/// scope saw no error, else the message (also when no answer came).
std::string pop_error_scope(WGPUInstance instance, WGPUDevice device);

/// Maps `buffer` for reading and waits until it is mapped (or failed);
/// false on failure.
bool map_read(WGPUInstance instance, WGPUDevice device, WGPUBuffer buffer, std::size_t size);

} // namespace anno::render::gpu
