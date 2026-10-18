#include <cstdio>

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

/// Waits until `done` is set by a callback, processing events; gives up
/// after `rounds` polls (callbacks of wgpu-native mostly fire at once).
template <class F> bool wait_for(WGPUInstance instance, WGPUDevice device, F&& done) {
    for (int i = 0; i < 100000 && !done(); ++i) {
        wgpuInstanceProcessEvents(instance);
        if (device != nullptr) {
            wgpuDevicePoll(device, false, nullptr);
        }
    }
    return done();
}

void on_uncaptured(WGPUDevice const*, WGPUErrorType type, WGPUStringView message, void*, void*) {
    std::fprintf(stderr, "GPU error (%d): %s\n", static_cast<int>(type), str(message).c_str());
}

} // namespace

formats::Result<Context, std::string> headless_context() {
    Context c;
    c.instance = Instance(wgpuCreateInstance(nullptr));
    if (!c.instance) {
        return std::string("wgpuCreateInstance failed");
    }
    struct AdapterReply {
        bool done = false;
        WGPUAdapter adapter = nullptr;
        std::string message;
    } ar;
    WGPURequestAdapterOptions opts = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
    opts.powerPreference = WGPUPowerPreference_HighPerformance;
    WGPURequestAdapterCallbackInfo acb = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    acb.mode = WGPUCallbackMode_AllowProcessEvents;
    acb.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message,
                      void* u1, void*) {
        auto* r = static_cast<AdapterReply*>(u1);
        if (status == WGPURequestAdapterStatus_Success) {
            r->adapter = adapter;
        } else {
            r->message = str(message);
        }
        r->done = true;
    };
    acb.userdata1 = &ar;
    wgpuInstanceRequestAdapter(c.instance.get(), &opts, acb);
    if (!wait_for(c.instance.get(), nullptr, [&] { return ar.done; }) || ar.adapter == nullptr) {
        return "no GPU adapter" + (ar.message.empty() ? std::string() : ": " + ar.message);
    }
    c.adapter = Adapter(ar.adapter);
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
    struct DeviceReply {
        bool done = false;
        WGPUDevice device = nullptr;
        std::string message;
    } dr;
    WGPURequestDeviceCallbackInfo dcb = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    dcb.mode = WGPUCallbackMode_AllowProcessEvents;
    dcb.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message,
                      void* u1, void*) {
        auto* r = static_cast<DeviceReply*>(u1);
        if (status == WGPURequestDeviceStatus_Success) {
            r->device = device;
        } else {
            r->message = str(message);
        }
        r->done = true;
    };
    dcb.userdata1 = &dr;
    wgpuAdapterRequestDevice(c.adapter.get(), &desc, dcb);
    if (!wait_for(c.instance.get(), nullptr, [&] { return dr.done; }) || dr.device == nullptr) {
        return "no GPU device: " + dr.message;
    }
    c.device = Device(dr.device);
    c.queue = Queue(wgpuDeviceGetQueue(c.device.get()));
    return c;
}

void poll(const Context& c, bool wait) {
    wgpuDevicePoll(c.device.get(), wait ? 1U : 0U, nullptr);
}

formats::Result<ShaderModule, std::string> create_shader(const Context& c, std::string_view wgsl,
                                                         std::string_view label) {
    WGPUShaderSourceWGSL src = WGPU_SHADER_SOURCE_WGSL_INIT;
    src.code = sv(wgsl);
    WGPUShaderModuleDescriptor desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    desc.nextInChain = &src.chain;
    desc.label = sv(label);
    wgpuDevicePushErrorScope(c.device.get(), WGPUErrorFilter_Validation);
    ShaderModule module(wgpuDeviceCreateShaderModule(c.device.get(), &desc));
    struct Reply {
        bool done = false;
        WGPUErrorType type = WGPUErrorType_NoError;
        std::string message;
    } r;
    WGPUPopErrorScopeCallbackInfo cb = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;
    cb.mode = WGPUCallbackMode_AllowProcessEvents;
    cb.callback = [](WGPUPopErrorScopeStatus, WGPUErrorType type, WGPUStringView message, void* u1,
                     void*) {
        auto* rr = static_cast<Reply*>(u1);
        rr->type = type;
        rr->message = str(message);
        rr->done = true;
    };
    cb.userdata1 = &r;
    wgpuDevicePopErrorScope(c.device.get(), cb);
    if (!wait_for(c.instance.get(), c.device.get(), [&] { return r.done; })) {
        return std::string(label) + ": no answer from the shader compiler";
    }
    if (r.type != WGPUErrorType_NoError || !module) {
        return std::string(label) + ": " + r.message;
    }
    return module;
}

} // namespace anno::render::gpu
