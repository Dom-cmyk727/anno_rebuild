// The last-owner release paths allocate nothing (round dy, A6). This executable replaces the global
// allocation functions with ones that count, and optionally refuse, allocations made by the one
// thread that asks for it (a thread_local switch); every other thread, and this thread outside the
// switched window, allocates normally. Inside the window the test releases the last owner of a
// generation (the hand-off to the process reclaimer), a build credit and an accounted payload. A
// path that allocated would be counted, and with refusal on would throw bad_alloc inside a noexcept
// release, which ends the process: the case fails either way.
#include <cstddef>
#include <cstdlib>
#include <new>
#if defined(_MSC_VER)
#include <malloc.h>
#endif

// <cfloat> before doctest: with libc++ 22 doctest's own include order leaves DBL_EPSILON undefined
// in its implementation.
#include <cfloat>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/credit.hpp>
#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/scene.hpp>

namespace {

thread_local bool t_counting = false;
thread_local bool t_refusing = false;
thread_local std::size_t t_allocations = 0;

/// Counts an allocation of this thread inside the window, and refuses it when asked.
void note_allocation() {
    if (t_counting) {
        ++t_allocations;
        if (t_refusing) {
            throw std::bad_alloc();
        }
    }
}

// Each replaced allocation function is paired with its own release: the plain forms with malloc
// and free, the aligned forms with the platform's aligned allocator and its own free
// (_aligned_malloc and _aligned_free on MSVC, where free must not release aligned memory;
// aligned_alloc and free elsewhere). An aligned form always takes the aligned allocator, whatever
// the alignment, so its delete always matches.
void* allocate_plain(std::size_t size) {
    note_allocation();
    void* p = std::malloc(size == 0 ? 1 : size);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

void* allocate_aligned(std::size_t size, std::align_val_t a) {
    note_allocation();
    const auto align = static_cast<std::size_t>(a);
    const std::size_t n = size == 0 ? 1 : size;
#if defined(_MSC_VER)
    void* p = _aligned_malloc(n, align);
#else
    // aligned_alloc takes a size that is a multiple of the alignment.
    void* p = std::aligned_alloc(align, (n + align - 1) / align * align);
#endif
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

void release_plain(void* p) noexcept {
    std::free(p);
}

void release_aligned(void* p) noexcept {
#if defined(_MSC_VER)
    _aligned_free(p);
#else
    std::free(p);
#endif
}

/// The window in which this thread's allocations are counted (and refused when `refuse`).
class Window {
  public:
    explicit Window(bool refuse) {
        t_allocations = 0;
        t_refusing = refuse;
        t_counting = true;
    }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    ~Window() { close(); }
    std::size_t close() {
        t_counting = false;
        t_refusing = false;
        return t_allocations;
    }
};

} // namespace

void* operator new(std::size_t size) {
    return allocate_plain(size);
}
void* operator new[](std::size_t size) {
    return allocate_plain(size);
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return allocate_plain(size);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return allocate_plain(size);
    } catch (...) {
        return nullptr;
    }
}
void* operator new(std::size_t size, std::align_val_t a) {
    return allocate_aligned(size, a);
}
void* operator new[](std::size_t size, std::align_val_t a) {
    return allocate_aligned(size, a);
}
void* operator new(std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept {
    try {
        return allocate_aligned(size, a);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept {
    try {
        return allocate_aligned(size, a);
    } catch (...) {
        return nullptr;
    }
}
void operator delete(void* p) noexcept {
    release_plain(p);
}
void operator delete[](void* p) noexcept {
    release_plain(p);
}
void operator delete(void* p, std::size_t) noexcept {
    release_plain(p);
}
void operator delete[](void* p, std::size_t) noexcept {
    release_plain(p);
}
void operator delete(void* p, const std::nothrow_t&) noexcept {
    release_plain(p);
}
void operator delete[](void* p, const std::nothrow_t&) noexcept {
    release_plain(p);
}
void operator delete(void* p, std::align_val_t) noexcept {
    release_aligned(p);
}
void operator delete[](void* p, std::align_val_t) noexcept {
    release_aligned(p);
}
void operator delete(void* p, std::size_t, std::align_val_t) noexcept {
    release_aligned(p);
}
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept {
    release_aligned(p);
}
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept {
    release_aligned(p);
}
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept {
    release_aligned(p);
}

namespace render = anno::render;
namespace app = anno::app;

namespace {

std::shared_ptr<render::Scene> scene_of(std::size_t islands) {
    auto bytes = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
    std::vector<anno::formats::insel::Island> list(islands);
    for (std::size_t i = 0; i < islands; ++i) {
        bytes->push_back(std::vector<std::uint8_t>(64, 0));
    }
    for (std::size_t i = 0; i < islands; ++i) {
        list[i].head = (*bytes)[i];
        list[i].number = static_cast<std::uint32_t>(i);
        list[i].width = 8;
        list[i].height = 8;
    }
    return render::Scene::create("alloc", std::move(list), anno::assets::island::GroundTextures{},
                                 anno::formats::anlagen::AnlagenFile{}, render::kHeightLift, bytes);
}

bool settled() {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    for (;;) {
        const auto s = render::SceneReclaimer::shared().stats();
        if (s.waiting == 0 && s.in_flight == 0) {
            return true;
        }
        if (std::chrono::steady_clock::now() > until) {
            return false;
        }
        std::this_thread::yield();
    }
}

} // namespace

TEST_CASE("alloc: the window counts this thread's allocations and refuses them when asked" *
          doctest::test_suite("alloc")) {
    {
        Window w(false);
        auto p = std::make_unique<int>(1);
        CHECK(w.close() == 1);
    }
    {
        Window w(true);
        CHECK_THROWS_AS((void)std::make_unique<int>(1), std::bad_alloc);
        CHECK(w.close() == 1);
    }
    // The nothrow forms report a refusal as a null pointer; the aligned forms count and refuse too.
    {
        Window w(true);
        int* p = new (std::nothrow) int(1);
        CHECK(p == nullptr);
        CHECK(w.close() == 1);
    }
    struct alignas(64) Wide {
        unsigned char bytes[64];
    };
    {
        Window w(true);
        CHECK_THROWS_AS((void)std::make_unique<Wide>(), std::bad_alloc);
        CHECK(w.close() == 1);
    }
    {
        Window w(false);
        auto wide = std::make_unique<Wide>();
        CHECK(reinterpret_cast<std::uintptr_t>(wide.get()) % 64 == 0);
        CHECK(w.close() == 1);
    }
}

TEST_CASE("alloc: the last owner's hand-off of a generation allocates nothing" *
          doctest::test_suite("alloc")) {
    // Generations of 1 and of 64 islands: the hand-off is the same constant work for both (it
    // reads the byte count fixed at construction and links the scene into the reclaimer's list).
    for (const std::size_t islands : {std::size_t{1}, std::size_t{64}}) {
        CAPTURE(islands);
        auto base = scene_of(islands);
        auto derived = base->with_islands({});
        REQUIRE(settled());
        std::size_t counted = 0;
        {
            Window w(true);
            derived.reset(); // the last owner: hands the scene to the reclaimer
            counted = w.close();
        }
        CHECK(counted == 0);
        REQUIRE(settled());
        {
            Window w(true);
            base.reset(); // generation 0, with its baseline lists and backing
            counted = w.close();
        }
        CHECK(counted == 0);
        REQUIRE(settled());
    }
}

TEST_CASE("alloc: releasing a credit and an accounted payload allocates nothing" *
          doctest::test_suite("alloc")) {
    auto ledger = std::make_shared<app::Ledger>();
    struct Payload {
        std::vector<int> data;
    };
    auto held = std::make_unique<app::Accounted<Payload>>();
    held->data.assign(1024, 1);
    held->credit = app::Credit(ledger, 4096);
    render::Buffers buffers;
    buffers.push_back(std::vector<std::uint8_t>(256, 1));
    auto storage = render::accounted_buffers(std::move(buffers), render::PayloadKind::Incremental);
    std::size_t counted = 0;
    {
        Window w(true);
        held->credit.resize(2048);
        held.reset();    // the payload, then its credit
        storage.reset(); // the last owner of an accounted allocation
        counted = w.close();
    }
    CHECK(counted == 0);
    CHECK(ledger->bytes.load() == 0);
    CHECK(ledger->count.load() == 0);
}
