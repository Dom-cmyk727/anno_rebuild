// The counting operator new of the test binary (alloc_probe.hpp). Test-only global
// state: the lint of the determinism rules scans src/sim/include and src/sim/src.
#include "alloc_probe.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

namespace {

std::atomic<bool> g_armed{false};
std::atomic<std::uint64_t> g_bytes{0};

} // namespace

namespace anno::test {

AllocScope::AllocScope() {
    g_bytes.store(0, std::memory_order_relaxed);
    g_armed.store(true, std::memory_order_relaxed);
}

AllocScope::~AllocScope() {
    g_armed.store(false, std::memory_order_relaxed);
}

std::uint64_t AllocScope::bytes() const {
    return g_bytes.load(std::memory_order_relaxed);
}

void* keep(void* p) {
    return p;
}

} // namespace anno::test

void* operator new(std::size_t n) {
    if (g_armed.load(std::memory_order_relaxed)) {
        g_bytes.fetch_add(n, std::memory_order_relaxed);
    }
    void* p = std::malloc(n != 0 ? n : 1);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

void* operator new[](std::size_t n) {
    return operator new(n);
}

// The nothrow forms are replaced too: their pointers reach the replaced
// operator delete (std::stable_sort frees its get_temporary_buffer storage,
// taken with new(nothrow), through plain delete), so they must come from the
// same malloc. Left to the runtime, ASan reports the pair as an
// alloc-dealloc mismatch (operator new vs free).
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
    if (g_armed.load(std::memory_order_relaxed)) {
        g_bytes.fetch_add(n, std::memory_order_relaxed);
    }
    return std::malloc(n != 0 ? n : 1);
}

void* operator new[](std::size_t n, const std::nothrow_t& tag) noexcept {
    return operator new(n, tag);
}

void operator delete(void* p) noexcept {
    std::free(p);
}

void operator delete[](void* p) noexcept {
    std::free(p);
}

void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}

void operator delete[](void* p, std::size_t) noexcept {
    std::free(p);
}

void operator delete(void* p, const std::nothrow_t&) noexcept {
    std::free(p);
}

void operator delete[](void* p, const std::nothrow_t&) noexcept {
    std::free(p);
}
