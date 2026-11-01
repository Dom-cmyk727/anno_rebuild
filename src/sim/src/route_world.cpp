// The route engine's terrain adapter over a World (see route_world.hpp).
#include <algorithm>
#include <utility>

#include <anno/sim/route_world.hpp>

namespace anno::sim {

namespace {

// f32 fields: sign, biased exponent, mantissa. value = (-1)^s * m * 2^exp with m an integer.
struct F32Parts {
    bool negative = false;
    bool finite = true;
    std::uint64_t m = 0;
    std::int32_t exp = 0;
};

F32Parts split_f32(std::uint32_t bits) {
    F32Parts p;
    p.negative = (bits >> 31) != 0;
    const std::uint32_t e = (bits >> 23) & 0xffu;
    const std::uint32_t frac = bits & 0x7fffffu;
    if (e == 0xffu) {
        p.finite = false;
    } else if (e == 0) {
        p.m = frac;
        p.exp = -149;
    } else {
        p.m = frac | 0x800000u;
        p.exp = static_cast<std::int32_t>(e) - 150;
    }
    return p;
}

// 0.04f = 0x3d23d70a: the integer mantissa and exponent.
const std::uint64_t kFortieth = 0xa3d70au;
const std::int32_t kFortiethExp = -28;

} // namespace

std::int64_t object_height_units(std::uint32_t f32_bits) {
    const F32Parts p = split_f32(f32_bits);
    if (!p.finite || p.m == 0) {
        return 0;
    }
    std::uint64_t r = 0;
    const std::uint64_t num = p.m * 25u; // < 2^29
    if (p.exp >= 0) {
        r = p.exp >= 30 ? (std::uint64_t{1} << 40)
                        : (num << p.exp); // saturates far above any height
    } else {
        r = -p.exp >= 40 ? 0 : (num >> (-p.exp));
    }
    const auto v = static_cast<std::int64_t>(std::min<std::uint64_t>(r, std::uint64_t{1} << 40));
    return p.negative ? -v : v;
}

std::int64_t object_top_units(std::uint32_t f32_bits) {
    const F32Parts p = split_f32(f32_bits);
    if (!p.finite || p.m == 0) {
        return 0;
    }
    // (m * 2^exp) / (kFortieth * 2^kFortiethExp) = m * 2^k / kFortieth with k = exp - kFortiethExp.
    const std::int32_t k = p.exp - kFortiethExp;
    std::uint64_t r = 0;
    if (k >= 0) {
        r = k >= 30 ? (std::uint64_t{1} << 40) : ((p.m << k) / kFortieth);
    } else {
        r = -k >= 40 ? 0 : p.m / (kFortieth << (-k));
    }
    const auto v = static_cast<std::int64_t>(std::min<std::uint64_t>(r, std::uint64_t{1} << 40));
    return p.negative ? -v : v;
}

bool object_tile_blocks(std::int64_t h, std::uint32_t base) {
    // An 8-bit unsigned add (add bl, dl at 0x100904e7): the sum wraps, so a negative h blocks.
    const std::uint32_t sum =
        static_cast<std::uint32_t>(static_cast<std::uint64_t>(h) + base) & 0xffu;
    return sum > 19;
}

WorldTileSource::WorldTileSource(const World& world, RouteTerrainOptions options)
    : types_(&world.types()), opt_(std::move(options)) {
    std::vector<const Island*> islands;
    for (const auto& slot : world.islands()) { // island-slot order
        islands.push_back(slot.has_value() ? &*slot : nullptr);
    }
    init(islands,
         static_cast<std::int32_t>(
             std::min<std::uint32_t>(world.settings().world_width, 0x7fffffffu)),
         static_cast<std::int32_t>(
             std::min<std::uint32_t>(world.settings().world_height, 0x7fffffffu)));
}

WorldTileSource::WorldTileSource(std::span<const Island* const> islands, const TypeRegistry& types,
                                 std::int32_t width, std::int32_t height,
                                 RouteTerrainOptions options)
    : types_(&types), opt_(std::move(options)) {
    init(islands, width, height);
}

void WorldTileSource::init(std::span<const Island* const> islands, std::int32_t width,
                           std::int32_t height) {
    w_ = std::max(width, 0);
    h_ = std::max(height, 0);
    cells_x_ = (w_ + 7) / 8;
    cells_z_ = (h_ + 7) / 8;
    cell_entry_.assign(static_cast<std::uint64_t>(cells_x_) * static_cast<std::uint64_t>(cells_z_),
                       -1);
    for (const Island* island : islands) {
        if (island == nullptr) {
            continue;
        }
        Entry e;
        e.island = island;
        e.grid = VertexGrid::of(*island);
        const TileLayer& t = island->tiles;
        e.aligned = t.width() > 0 && t.height() > 0 && t.x0() % 8 == 0 && t.y0() % 8 == 0 &&
                    t.width() % 8 == 0 && t.height() % 8 == 0;
        entries_.push_back(e);
        const std::int32_t index = static_cast<std::int32_t>(entries_.size()) - 1;
        // Cells entirely inside this island's rectangle (later islands overwrite: rectangles are
        // disjoint in a valid world).
        const std::int64_t x0 = t.x0(), y0 = t.y0();
        const std::int64_t x1 = x0 + t.width(), y1 = y0 + t.height();
        for (std::int64_t cz = std::max<std::int64_t>((y0 + 7) / 8, 0);
             cz * 8 + 8 <= y1 && cz < cells_z_; ++cz) {
            for (std::int64_t cx = std::max<std::int64_t>((x0 + 7) / 8, 0);
                 cx * 8 + 8 <= x1 && cx < cells_x_; ++cx) {
                cell_entry_[static_cast<std::uint64_t>(cz * cells_x_ + cx)] = index;
            }
        }
    }
}

const WorldTileSource::Entry* WorldTileSource::entry_at(std::int32_t x, std::int32_t z) const {
    const std::int32_t cx = x / 8;
    const std::int32_t cz = z / 8;
    const std::int32_t hit =
        cell_entry_[static_cast<std::uint64_t>(cz) * static_cast<std::uint64_t>(cells_x_) +
                    static_cast<std::uint64_t>(cx)];
    if (hit >= 0) {
        return &entries_[static_cast<std::uint64_t>(hit)];
    }
    for (const Entry& e : entries_) {
        if (e.island->tiles.contains(x, z)) {
            return &e;
        }
    }
    return nullptr;
}

std::uint32_t WorldTileSource::head_f32(std::uint32_t object) const {
    const BuildingType* t = types_->find(TypeId{object});
    if (t == nullptr || t->head_raw.size() < 0x18) {
        return 0;
    }
    return static_cast<std::uint32_t>(t->head_raw[0x14]) |
           (static_cast<std::uint32_t>(t->head_raw[0x15]) << 8) |
           (static_cast<std::uint32_t>(t->head_raw[0x16]) << 16) |
           (static_cast<std::uint32_t>(t->head_raw[0x17]) << 24);
}

bool WorldTileSource::object_blocks(const Entry& e, std::int32_t x, std::int32_t z) const {
    const TileLayer& tiles = e.island->tiles;
    const Slot idx = tiles.index_of(x, z);
    if (idx == kNoSlot) {
        return false;
    }
    const std::uint32_t object = tiles.object(idx);
    if (object == 0) {
        return false;
    }
    const BuildingType* t = types_->find(TypeId{object});
    if (t == nullptr) {
        return false;
    }
    const std::int64_t h = object_height_units(head_f32(object));
    const std::uint32_t base = opt_.item_base ? opt_.item_base(*e.island, idx) : 0u;
    return object_tile_blocks(h, base);
}

bool WorldTileSource::tile_is_water(std::int32_t x, std::int32_t z) const {
    if (x < 0 || z < 0 || x >= w_ || z >= h_) {
        return false;
    }
    const Entry* e = entry_at(x, z);
    if (e == nullptr) {
        return true; // open sea
    }
    if (e->grid.ok()) {
        const auto v = [&](std::int32_t vx, std::int32_t vz) {
            return std::uint32_t{e->grid.at(vx, vz).value_or(0)};
        };
        const std::uint32_t sum = v(x, z) + v(x + 1, z) + v(x, z + 1) + v(x + 1, z + 1);
        if ((sum >> 2) > 19) {
            return false; // terrain land
        }
    }
    return !(opt_.objects_block && object_blocks(*e, x, z));
}

bool WorldTileSource::cell_uses_shortcut(std::int32_t cx, std::int32_t cz) const {
    if (!opt_.block_shortcut || cx < 0 || cz < 0 || cx >= cells_x_ || cz >= cells_z_) {
        return false;
    }
    const std::int32_t hit =
        cell_entry_[static_cast<std::uint64_t>(cz) * static_cast<std::uint64_t>(cells_x_) +
                    static_cast<std::uint64_t>(cx)];
    return hit >= 0 && entries_[static_cast<std::uint64_t>(hit)].aligned &&
           entries_[static_cast<std::uint64_t>(hit)].grid.ok();
}

bool WorldTileSource::block_range(std::int32_t cx, std::int32_t cz, std::uint8_t& min_half,
                                  std::uint8_t& max_half) const {
    if (!cell_uses_shortcut(cx, cz)) {
        return false;
    }
    const Entry& e = entries_[static_cast<std::uint64_t>(
        cell_entry_[static_cast<std::uint64_t>(cz) * static_cast<std::uint64_t>(cells_x_) +
                    static_cast<std::uint64_t>(cx)])];
    std::uint32_t mn = 255;
    std::uint32_t mx = 0;
    for (std::int32_t j = 0; j <= 8; ++j) {
        for (std::int32_t i = 0; i <= 8; ++i) {
            const std::uint32_t v = e.grid.at(cx * 8 + i, cz * 8 + j).value_or(0);
            mn = std::min(mn, v);
            mx = std::max(mx, v);
        }
    }
    if (opt_.objects_block) {
        const TileLayer& tiles = e.island->tiles;
        for (std::int32_t j = 0; j < 8; ++j) {
            for (std::int32_t i = 0; i < 8; ++i) {
                const Slot idx = tiles.index_of(cx * 8 + i, cz * 8 + j);
                if (idx == kNoSlot || tiles.foot_anchor(idx) == 0 || tiles.object(idx) == 0) {
                    continue;
                }
                const std::uint32_t base = opt_.item_base ? opt_.item_base(*e.island, idx) : 0u;
                const std::int64_t top = (object_top_units(head_f32(tiles.object(idx))) +
                                          static_cast<std::int64_t>(base)) &
                                         0xff;
                mx = std::max(mx, static_cast<std::uint32_t>(top));
            }
        }
    }
    min_half = static_cast<std::uint8_t>(mn >> 1);
    max_half = static_cast<std::uint8_t>(std::min<std::uint32_t>(mx, 255) >> 1);
    return true;
}

CellClass WorldTileSource::classify_cell(std::int32_t cx, std::int32_t cz) const {
    std::uint8_t mn = 0, mx = 0;
    if (!block_range(cx, cz, mn, mx)) {
        return classify_cell_by_tiles(*this, cx, cz);
    }
    // The builder tests 19 >= 2 max (all water), then 19 < 2 min (all land) on the halved records.
    if (19 >= 2 * static_cast<std::uint32_t>(mx)) {
        return CellClass::AllWater;
    }
    if (19 < 2 * static_cast<std::uint32_t>(mn)) {
        return CellClass::AllLand;
    }
    return CellClass::Mixed;
}

} // namespace anno::sim
