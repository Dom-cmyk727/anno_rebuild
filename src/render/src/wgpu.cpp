#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string_view>

#include <anno/render/wgpu.hpp>

namespace anno::render::gpu {

std::string str(WGPUStringView v) {
    if (v.data == nullptr) {
        return {};
    }
    if (v.length == WGPU_STRLEN) {
        return std::string(v.data);
    }
    return std::string(v.data, v.length);
}

namespace {

const char* backend_name(WGPUBackendType b) {
    switch (b) {
    case WGPUBackendType_Metal:
        return "Metal";
    case WGPUBackendType_Vulkan:
        return "Vulkan";
    case WGPUBackendType_D3D12:
        return "DirectX 12";
    case WGPUBackendType_D3D11:
        return "DirectX 11";
    case WGPUBackendType_OpenGL:
        return "OpenGL";
    case WGPUBackendType_OpenGLES:
        return "OpenGL ES";
    default:
        return "other";
    }
}

/// Callback rounds before a wait gives up. wgpu-native answers requests
/// within one ProcessEvents/Poll round in practice; the bound only turns a
/// missing answer into an error instead of a hang.
constexpr int kWaitRounds = 100000;

/// Waits until `done` is set by a callback, processing events and (with a
/// device) polling it.
template <class F> bool wait_for(WGPUInstance instance, WGPUDevice device, F&& done) {
    for (int i = 0; i < kWaitRounds && !done(); ++i) {
        wgpuInstanceProcessEvents(instance);
        if (device != nullptr) {
            wgpuDevicePoll(device, false, nullptr);
        }
    }
    return done();
}

/// Callback state on the heap: freed by whoever comes last, the waiter
/// (answer received) or the callback (after the waiter gave up), so a late
/// callback never writes to a dead stack frame.
template <class T> struct Pending {
    T value{};
    /// 0 waiting, 1 answered, 2 abandoned by the waiter.
    std::atomic<int> state{0};

    /// Called by the callback once `value` is set.
    static void finish(void* p) {
        auto* self = static_cast<Pending*>(p);
        if (self->state.exchange(1) == 2) {
            delete self;
        }
    }
    /// Called by the waiter: true = answered (the caller then owns and frees
    /// it), false = no answer (the callback frees it if it ever comes).
    static bool wait(Pending* self, WGPUInstance instance, WGPUDevice device) {
        if (wait_for(instance, device, [&] { return self->state.load() == 1; })) {
            return true;
        }
        // The callback may have run between the last check and now.
        return self->state.exchange(2) == 1;
    }
};

void on_uncaptured(WGPUDevice const*, WGPUErrorType type, WGPUStringView message, void* u1, void*) {
    if (u1 != nullptr) {
        static_cast<std::atomic<std::uint64_t>*>(u1)->fetch_add(1);
    }
    std::fprintf(stderr, "GPU error (%d): %s\n", static_cast<int>(type), str(message).c_str());
}

void on_lost(WGPUDevice const*, WGPUDeviceLostReason reason, WGPUStringView message, void*, void*) {
    if (reason != WGPUDeviceLostReason_Destroyed &&
        reason != WGPUDeviceLostReason_CallbackCancelled) {
        std::fprintf(stderr, "GPU device lost (%d): %s\n", static_cast<int>(reason),
                     str(message).c_str());
    }
}

/// WGPU_BACKEND as wgpu's from_env (first name of a comma list).
WGPUBackendType backend_from_env() {
    const char* v = std::getenv("WGPU_BACKEND");
    if (v == nullptr) {
        return WGPUBackendType_Undefined;
    }
    const std::string_view s(v);
    const std::string_view first = s.substr(0, s.find(','));
    if (first == "vulkan") {
        return WGPUBackendType_Vulkan;
    }
    if (first == "metal") {
        return WGPUBackendType_Metal;
    }
    if (first == "dx12") {
        return WGPUBackendType_D3D12;
    }
    if (first == "gl") {
        return WGPUBackendType_OpenGL;
    }
    std::fprintf(stderr, "WGPU_BACKEND=%s not known; using the default\n", v);
    return WGPUBackendType_Undefined;
}

} // namespace

formats::Result<Context, std::string> headless_context() {
    Instance instance(wgpuCreateInstance(nullptr));
    if (!instance) {
        return std::string("wgpuCreateInstance failed");
    }
    return context_for(std::move(instance), nullptr);
}

formats::Result<Context, std::string> context_for(Instance instance, WGPUSurface compatible) {
    Context c;
    c.instance = std::move(instance);
    struct AdapterReply {
        WGPUAdapter adapter = nullptr;
        std::string message;
    };
    using PA = Pending<AdapterReply>;
    WGPURequestAdapterOptions opts = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
    opts.powerPreference = WGPUPowerPreference_HighPerformance;
    opts.compatibleSurface = compatible;
    opts.backendType = backend_from_env();
    WGPURequestAdapterCallbackInfo acb = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    acb.mode = WGPUCallbackMode_AllowProcessEvents;
    acb.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message,
                      void* u1, void*) {
        auto* r = static_cast<PA*>(u1);
        if (status == WGPURequestAdapterStatus_Success) {
            r->value.adapter = adapter;
        } else {
            r->value.message = str(message);
        }
        PA::finish(u1);
    };
    auto* ar = new PA;
    acb.userdata1 = ar;
    wgpuInstanceRequestAdapter(c.instance.get(), &opts, acb);
    if (!PA::wait(ar, c.instance.get(), nullptr)) {
        return std::string("no GPU adapter: no answer");
    }
    const std::unique_ptr<PA> a(ar);
    if (a->value.adapter == nullptr) {
        return "no GPU adapter" +
               (a->value.message.empty() ? std::string() : ": " + a->value.message);
    }
    c.adapter = Adapter(a->value.adapter);
    WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT;
    if (wgpuAdapterGetInfo(c.adapter.get(), &info) == WGPUStatus_Success) {
        c.name = str(info.device) + " (" + backend_name(info.backendType) + ")";
        wgpuAdapterInfoFreeMembers(info);
    }
    WGPULimits limits = WGPU_LIMITS_INIT;
    wgpuAdapterGetLimits(c.adapter.get(), &limits);
    WGPUDeviceDescriptor desc = WGPU_DEVICE_DESCRIPTOR_INIT;
    desc.label = sv("anno1503");
    desc.requiredLimits = &limits;
    desc.uncapturedErrorCallbackInfo.callback = on_uncaptured;
    // The counter outlives the device: the Context and every DeviceRef hold it.
    desc.uncapturedErrorCallbackInfo.userdata1 = c.errors.get();
    desc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    desc.deviceLostCallbackInfo.callback = on_lost;
    struct DeviceReply {
        WGPUDevice device = nullptr;
        std::string message;
    };
    using PD = Pending<DeviceReply>;
    WGPURequestDeviceCallbackInfo dcb = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    dcb.mode = WGPUCallbackMode_AllowProcessEvents;
    dcb.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message,
                      void* u1, void*) {
        auto* r = static_cast<PD*>(u1);
        if (status == WGPURequestDeviceStatus_Success) {
            r->value.device = device;
        } else {
            r->value.message = str(message);
        }
        PD::finish(u1);
    };
    auto* dr = new PD;
    dcb.userdata1 = dr;
    wgpuAdapterRequestDevice(c.adapter.get(), &desc, dcb);
    if (!PD::wait(dr, c.instance.get(), nullptr)) {
        return std::string("no GPU device: no answer");
    }
    const std::unique_ptr<PD> d(dr);
    if (d->value.device == nullptr) {
        return "no GPU device: " + d->value.message;
    }
    c.device = Device(d->value.device);
    c.queue = Queue(wgpuDeviceGetQueue(c.device.get()));
    return c;
}

void poll(const Context& c, bool wait) {
    wgpuDevicePoll(c.device.get(), wait ? 1U : 0U, nullptr);
}

formats::Result<ShaderModule, std::string> create_shader(const Context& c, std::string_view wgsl,
                                                         std::string_view label) {
    return create_shader(c.instance.get(), c.device.get(), wgsl, label);
}

formats::Result<ShaderModule, std::string> create_shader(WGPUInstance instance, WGPUDevice device,
                                                         std::string_view wgsl,
                                                         std::string_view label) {
    WGPUShaderSourceWGSL src = WGPU_SHADER_SOURCE_WGSL_INIT;
    src.code = sv(wgsl);
    WGPUShaderModuleDescriptor desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    desc.nextInChain = &src.chain;
    desc.label = sv(label);
    wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
    ShaderModule module(wgpuDeviceCreateShaderModule(device, &desc));
    const std::string err = pop_error_scope(instance, device);
    if (!err.empty() || !module) {
        return std::string(label) + ": " + (err.empty() ? "not created" : err);
    }
    return module;
}

DeviceRef::DeviceRef(WGPUInstance i, WGPUDevice d, WGPUQueue q, ErrorCount e)
    : errors(std::move(e)) {
    wgpuInstanceAddRef(i);
    instance = Instance(i);
    wgpuDeviceAddRef(d);
    device = Device(d);
    wgpuQueueAddRef(q);
    queue = Queue(q);
}

DeviceRef::DeviceRef(const Context& c)
    : DeviceRef(c.instance.get(), c.device.get(), c.queue.get(), c.errors) {}

DeviceRef::DeviceRef(const DeviceRef& o)
    : DeviceRef(o.instance.get(), o.device.get(), o.queue.get(), o.errors) {}

std::string pop_error_scope(WGPUInstance instance, WGPUDevice device) {
    struct Reply {
        WGPUPopErrorScopeStatus status = WGPUPopErrorScopeStatus_Error;
        WGPUErrorType type = WGPUErrorType_NoError;
        std::string message;
    };
    using P = Pending<Reply>;
    WGPUPopErrorScopeCallbackInfo cb = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;
    cb.mode = WGPUCallbackMode_AllowProcessEvents;
    cb.callback = [](WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView message,
                     void* u1, void*) {
        auto* r = static_cast<P*>(u1);
        r->value.status = status;
        r->value.type = type;
        r->value.message = str(message);
        P::finish(u1);
    };
    auto* p = new P;
    cb.userdata1 = p;
    wgpuDevicePopErrorScope(device, cb);
    if (!P::wait(p, instance, device)) {
        return "no answer from the device";
    }
    const std::unique_ptr<P> r(p);
    if (r->value.status != WGPUPopErrorScopeStatus_Success) {
        return "error scope failed: " + r->value.message;
    }
    if (r->value.type != WGPUErrorType_NoError) {
        return r->value.message.empty() ? std::string("error") : r->value.message;
    }
    return {};
}

bool map_read(WGPUInstance instance, WGPUDevice device, WGPUBuffer buffer, std::size_t size) {
    using P = Pending<WGPUMapAsyncStatus>;
    WGPUBufferMapCallbackInfo cb = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    cb.mode = WGPUCallbackMode_AllowProcessEvents;
    cb.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* u1, void*) {
        static_cast<P*>(u1)->value = status;
        P::finish(u1);
    };
    auto* p = new P;
    cb.userdata1 = p;
    wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, size, cb);
    // Waiting for the queue first: the copy into `buffer` must be done.
    wgpuDevicePoll(device, 1U, nullptr);
    if (!P::wait(p, instance, device)) {
        return false;
    }
    const std::unique_ptr<P> r(p);
    return r->value == WGPUMapAsyncStatus_Success;
}

} // namespace anno::render::gpu
