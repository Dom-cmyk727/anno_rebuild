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
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/credit.hpp>
#include <anno/app/dev_feedback.hpp>
#include <anno/app/dev_tool.hpp>
#include <anno/app/loader.hpp>
#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/scene.hpp>
#include <anno/sim/types.hpp>

namespace {

thread_local bool t_counting = false;
thread_local bool t_refusing = false;
thread_local std::size_t t_allocations = 0;
/// A single allocation above this many bytes is refused (a work guard: a test that must not make a
/// large allocation fails with bad_alloc instead of exhausting the host). SIZE_MAX: no limit.
thread_local std::size_t t_limit = static_cast<std::size_t>(-1);

/// Counts an allocation of this thread inside the window, and refuses it when asked or when it is
/// larger than the window's limit.
void note_allocation(std::size_t size) {
    if (t_counting) {
        ++t_allocations;
        if (t_refusing || size > t_limit) {
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
    note_allocation(size);
    void* p = std::malloc(size == 0 ? 1 : size);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

void* allocate_aligned(std::size_t size, std::align_val_t a) {
    note_allocation(size);
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
    /// `limit`: the largest single allocation allowed (bytes); a larger one throws bad_alloc.
    explicit Window(bool refuse, std::size_t limit = static_cast<std::size_t>(-1)) {
        t_allocations = 0;
        t_refusing = refuse;
        t_limit = limit;
        t_counting = true;
    }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    ~Window() { close(); }
    std::size_t close() {
        t_counting = false;
        t_refusing = false;
        t_limit = static_cast<std::size_t>(-1);
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

namespace sim = anno::sim;

namespace {

// The developer tool's feedback against footprint definitions it must refuse cheaply (round ee
// review, R3). A synthetic 30 x 30 island at world tile (100, 40), flat; the registry holds an
// original-sized type, a legitimate larger one and two absurd ones. No game files.
constexpr std::int32_t kX0 = 100, kY0 = 40, kSize = 30;
constexpr sim::TypeId kHouse{501}, kBig{502}, kWide{503}, kHuge{504};
constexpr std::uint32_t kListed = 161;

std::shared_ptr<const render::Scene> flat_scene() {
    auto bytes = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
    bytes->push_back(std::vector<std::uint8_t>(64, 0));
    std::vector<anno::formats::insel::Island> list(1);
    list[0].head = (*bytes)[0];
    list[0].number = 0;
    list[0].x = static_cast<std::uint16_t>(kX0);
    list[0].y = static_cast<std::uint16_t>(kY0);
    list[0].width = kSize;
    list[0].height = kSize;
    return render::Scene::create("alloc-r3", std::move(list),
                                 anno::assets::island::GroundTextures{},
                                 anno::formats::anlagen::AnlagenFile{}, render::kHeightLift, bytes);
}

sim::BuildingType definition(sim::TypeId id, std::uint32_t w, std::uint32_t h) {
    sim::BuildingType t;
    t.id = id;
    t.prodtype = kListed;
    t.width = w;
    t.height = h;
    return t;
}

app::DevToolSetup tool_setup(sim::TypeId type) {
    sim::TypeRegistry::Builder b;
    b.add(definition(kHouse, 2, 2))
        .add(definition(kBig, 12, 7))
        .add(definition(kWide, 1000000, 1))
        .add(definition(kHuge, 0xFFFFFFFFU, 0xFFFFFFFFU));
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    app::DevToolSetup s;
    s.registry = built.value();
    s.types = {type};
    s.city = sim::CityRef{sim::IslandId{0}, sim::CityNo{0}};
    s.island = app::TileRect{kX0, kY0, static_cast<std::uint32_t>(kSize),
                             static_cast<std::uint32_t>(kSize)};
    s.issuer = sim::PlayerId{1};
    return s;
}

/// A tool with `type` selected (rotated `rotations` times) hovering world tile (x, y).
app::DevTool hovering(const std::shared_ptr<const render::Scene>& scene, sim::TypeId type,
                      std::int32_t x, std::int32_t y, int rotations = 0) {
    app::DevTool tool(tool_setup(type));
    static_cast<void>(tool.handle({app::ToolInput::Kind::Toggle, 0, 0}, nullptr));
    for (int i = 0; i < rotations; ++i) {
        static_cast<void>(tool.handle({app::ToolInput::Kind::Rotate, 0, 0}, nullptr));
    }
    const auto [tw, th] = render::kTile[1];
    const int cx = (x - y) * tw / 2, cy = (x + y) * th / 2 + th / 2;
    const render::Rect view{cx - 800, cy - 500, 1600, 1000};
    const app::PointerFrame frame = app::pointer_frame(view, nullptr, 1, tw, th, scene, 1600, 1000);
    static_cast<void>(tool.handle({app::ToolInput::Kind::Move, 800, 500}, &frame));
    REQUIRE(tool.hover() == std::optional<sim::TileXY>{sim::TileXY{x, y}});
    return tool;
}

/// The verdict of PlacementKnowledge::check with a window that refuses any single allocation over
/// 64 KiB: an implementation that builds the footprint's tile list (8 MB for 1,000,000 tiles, a
/// bad_alloc for the absurd one) throws here instead of exhausting the host.
app::PlacementKnowledge::Verdict guarded_check(const app::DevTool& tool,
                                               const app::PlacementKnowledge& knows) {
    Window w(false, std::size_t{64} << 10);
    auto v = knows.check(tool, nullptr);
    w.close();
    return v;
}

} // namespace

TEST_CASE("footprint feedback: an oversized or absurd definition is refused before any tile is "
          "enumerated, and a legitimate larger one is not (round ee review, R3)" *
          doctest::test_suite("alloc")) {
    const auto scene = flat_scene();
    for (const sim::TypeId id : {kWide, kHuge}) {
        CAPTURE(id.v);
        const app::DevTool tool = hovering(scene, id, kX0 + 10, kY0 + 10);
        const app::PlacementKnowledge knows(tool.setup());
        app::PlacementKnowledge::Verdict v;
        CHECK_NOTHROW(v = guarded_check(tool, knows));
        CHECK(v.refused);
        CHECK(v.text.find("leaves the city's island") != std::string::npos);
    }
    // The valid control: a 12 x 7 definition inside the island is not refused. An implementation
    // that refuses everything it cannot cheaply enumerate fails here.
    {
        const app::DevTool tool = hovering(scene, kBig, kX0 + 10, kY0 + 10);
        const app::PlacementKnowledge knows(tool.setup());
        const auto v = guarded_check(tool, knows);
        CHECK_FALSE(v.refused);
        CHECK(v.text.find("unchecked") != std::string::npos);
    }
    // Island boundaries, from independently known numbers: 12 wide from local x 18 ends at 30
    // (the island's edge, allowed), from 19 it leaves; turned, it is 7 wide and 12 tall, so from
    // local y 18 it ends at 30 and from 19 it leaves. (The pick only names tiles of an island, so
    // an origin before the island cannot be hovered; the check clamps at the island's origin too.)
    const struct {
        std::int32_t lx, ly;
        int rotations;
        bool refused;
    } edges[] = {{18, 5, 0, false}, {19, 5, 0, true}, {5, 18, 1, false},
                 {5, 19, 1, true},  {0, 0, 0, false}, {29, 29, 0, true}};
    for (const auto& e : edges) {
        CAPTURE(e.lx);
        CAPTURE(e.ly);
        CAPTURE(e.rotations);
        const app::DevTool tool = hovering(scene, kBig, kX0 + e.lx, kY0 + e.ly, e.rotations);
        const app::PlacementKnowledge knows(tool.setup());
        CHECK(guarded_check(tool, knows).refused == e.refused);
    }
}

TEST_CASE("footprint feedback: the extent is exact in 64-bit coordinates and the tile list is "
          "bounded by the region the caller admits (round ee review, R3)" *
          doctest::test_suite("alloc")) {
    const auto scene = flat_scene();
    const app::DevTool wide = hovering(scene, kWide, kX0 + 10, kY0 + 10);
    const auto extent = wide.input().footprint_extent({kX0 + 10, kY0 + 10});
    REQUIRE(extent.has_value());
    CHECK(extent->w == 1000000);
    CHECK(extent->h == 1);
    CHECK(extent->area() == 1000000);
    // The absurd definition's area needs 64 bits and no tile is made for it.
    const app::DevTool huge = hovering(scene, kHuge, kX0 + 10, kY0 + 10);
    const auto big = huge.input().footprint_extent({std::numeric_limits<std::int32_t>::max(), 0});
    REQUIRE(big.has_value());
    CHECK(big->area() == std::uint64_t{0xFFFFFFFFU} * 0xFFFFFFFFU);
    CHECK(big->x0 + big->w > std::int64_t{std::numeric_limits<std::int32_t>::max()});
    const app::DevTool house = hovering(scene, kHouse, kX0 + 10, kY0 + 10);
    Window w(false, std::size_t{64} << 10);
    std::vector<sim::TileXY> none{sim::TileXY{}};
    CHECK_NOTHROW(none = huge.input().footprint_at({0, 0}, 900));
    // A footprint that fits the admitted region is listed, row by row...
    const auto tiles = house.input().footprint_at({kX0 + 10, kY0 + 10}, 900);
    // ... one that reaches past the 32-bit tile coordinates is not.
    const auto past =
        house.input().footprint_at({std::numeric_limits<std::int32_t>::max(), 0}, 900);
    w.close();
    CHECK(none.empty());
    REQUIRE(tiles.size() == 4);
    CHECK(tiles[3] == sim::TileXY{kX0 + 11, kY0 + 11});
    CHECK(past.empty());
}

TEST_CASE("footprint feedback: indexing a published building with an absurd definition stays "
          "inside the island (round ee review, R3)" *
          doctest::test_suite("alloc")) {
    const auto scene = flat_scene();
    const app::DevTool tool = hovering(scene, kBig, kX0 + 10, kY0 + 10);
    app::PlacementKnowledge knows(tool.setup());
    const std::vector<app::SceneBuilding> buildings{app::SceneBuilding{0, kWide.v, 5, 5, 0, 0},
                                                    app::SceneBuilding{0, kHuge.v, 0, 0, 0, 0}};
    std::size_t counted = 0;
    {
        Window w(false, std::size_t{64} << 10);
        CHECK_NOTHROW(knows.update(buildings, 1, *scene));
        counted = w.close();
    }
    // Two buildings cover at most the island (30 x 30 tiles) each; a tile per set node.
    CHECK(counted <= 2 * static_cast<std::size_t>(kSize) * kSize + 16);
    CHECK(knows.occupied({kX0 + 5, kY0 + 5}));
    CHECK(knows.occupied({kX0, kY0}));
    CHECK_FALSE(knows.occupied({kX0 - 1, kY0}));
    // The huge building covers the whole island, so any placement of the 12 x 7 type is refused
    // as occupied, in a count of lookups bounded by its width, not its area.
    CHECK(guarded_check(tool, knows).text.find("occupied") != std::string::npos);
}
