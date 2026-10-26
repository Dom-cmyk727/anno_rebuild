#include <algorithm>
#include <cstddef>
#include <utility>

#include <anno/sim/building.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/walk_cells.hpp>
namespace anno::sim {
namespace wf = formats::annowelt;
std::uint64_t walk_tables_digest(const WalkTables& t) {
    StateHasher h;
    h.u32(t.time_date_stamp);
    h.u32(t.size_of_image);
    h.u64(t.mapper.size());
    for (const std::uint16_t v : t.mapper) {
        h.u16(v);
    }
    h.u64(t.layers.size());
    for (const auto& layer : t.layers) {
        h.u64(layer.size());
        h.bytes(layer);
    }
    h.u64(t.goal_order.size());
    for (const auto& row : t.goal_order) {
        for (const auto& e : row) {
            h.u16(e.mask);
            h.u8(e.col);
            h.u8(e.row);
        }
    }
    return h.value();
}
std::string validate_walk_tables(const WalkTables& t) {
    // Nine input mask bits: mapper domain is fixed by the record format.
    if (t.mapper.size() != wf::kMapperCount || t.layers.empty() ||
        t.goal_order.size() < wf::kGoalOrderRows) {
        return "walk tables: missing mapper or layers";
    }
    for (const auto& l : t.layers) {
        if (l.size() < 0x400) {
            return "walk tables: short admission block";
        }
        // Fresh record codes are bytes; marked codes are never admitted.
        for (std::size_t c = 0; c < 256; ++c) {
            if (l[c] != 0 && l.size() < 0x400 + 8 * (c + 1)) {
                return "walk tables: short cost row";
            }
        }
    }
    return {};
}
std::shared_ptr<const WalkTables> synthetic_walk_tables() {
    auto t = std::make_shared<WalkTables>();
    // Minimal own fixture: open/closed mapper plus test layers, not original data.
    // Original layer default5 and goal arrival classes5, configurable by input vectors.
    t->goal_order.resize(wf::kGoalOrderRows);
    for (auto& row : t->goal_order) {
        for (std::size_t k = 0; k < row.size(); ++k) {
            row[k] = {static_cast<std::uint16_t>(1U << k), static_cast<std::uint8_t>(k % 3),
                      static_cast<std::uint8_t>(k / 3)};
        }
    }
    t->mapper.resize(wf::kMapperCount, 0);
    t->mapper[0] = 0xfff;
    t->layers.resize(wf::kLayerCount, std::vector<std::uint8_t>(0x400 + 8 * 8, 0));
    for (std::size_t layer = 0; layer < t->layers.size(); ++layer) {
        for (std::size_t code : {std::size_t{0}, std::size_t{2}, std::size_t{4}}) {
            t->layers[layer][code] = (layer == 3 && code == 2) ? 0 : 0xff;
            for (std::size_t j = 0; j < 8; ++j) {
                t->layers[layer][0x400 + 8 * code + j] = 1;
            }
        }
    }
    return t;
}
std::uint8_t admission(const WalkTables& t, std::uint32_t layer, std::uint16_t code) {
    // D AnnoWelt 0x10006ab0: high word's low ten bits index admission.
    if (layer >= t.layers.size()) {
        return 0;
    }
    const std::size_t i = code & 0x3ffU;
    return i < t.layers[layer].size() ? t.layers[layer][i] : 0;
}
std::uint8_t cost(const WalkTables& t, std::uint32_t layer, std::uint16_t code, std::uint32_t j) {
    // D AnnoWelt 0x10006ab9. Malformed/missing rows fail closed, no indexing UB.
    if (layer >= t.layers.size() || j >= 8) {
        return 0;
    }
    const std::size_t i = 0x400 + 8 * std::size_t{code & 0x3ffU} + j;
    return i < t.layers[layer].size() ? t.layers[layer][i] : 0;
}
std::uint16_t walk_code(const BuildingType& t, std::uint32_t rotation) {
    // D AnnoGame 0x1000940b..0x1000942b, encoder0x100091f0, default0x1000b555.
    const auto byte = [&](std::size_t i) {
        return i < t.head_raw.size() ? t.head_raw[i] : std::uint8_t{0};
    };
    const auto first = byte(0x2f), shape = byte(0x30);
    std::uint32_t c = 4;
    if (first != 0 || shape != 0) {
        c = first < 8 ? first : ((first - 8U) | 0x80U);
        if (first >= 8) {
            // These are branch constants of the encoder, not original table bytes.
            switch (shape) {
            case 0:
                c |= 0x50;
                break;
            case 1:
                c |= 0x48;
                break;
            case 2:
                c |= 0x58;
                break;
            case 3:
                c |= 0x78;
                break;
            case 4:
                c |= 0x10;
                break;
            default:
                break;
            }
        }
    }
    c &= 0xff;
    const auto n = (c >> 3) & 15;
    // D AnnoGame 0x10007dc0: only these canonical high-bit connection cases rotate.
    if ((c & 0x80) != 0 && (n == 2 || n == 9 || n == 10 || n == 11)) {
        std::uint32_t bits = n;
        for (std::uint32_t r = 0; r < (rotation & 3); ++r) {
            bits = ((bits << 1) | (bits >> 3)) & 15;
        }
        c = (c & 0x87) | (bits << 3);
    }
    return static_cast<std::uint16_t>(c);
}
std::uint16_t walk_mask(const BuildingType& t, std::uint32_t dx, std::uint32_t dy,
                        std::uint32_t rotation) {
    // D AnnoWelt 0x1007b5e0..0x1007b837: inverse-rotate each output subcell.
    const std::uint64_t w = 3ULL * t.width, h = 3ULL * t.height;
    std::uint16_t mask = 0;
    for (std::uint32_t v = 0; v < 3; ++v) {
        for (std::uint32_t u = 0; u < 3; ++u) {
            const std::uint64_t x = 3ULL * dx + u, y = 3ULL * dy + v;
            std::uint64_t sx = x, sy = y;
            switch (rotation & 3) {
            case 1:
                if (y >= w) {
                    continue;
                }
                sx = w - 1 - y;
                sy = x;
                break;
            case 2:
                if (x >= w || y >= h) {
                    continue;
                }
                sx = w - 1 - x;
                sy = h - 1 - y;
                break;
            case 3:
                if (x >= h) {
                    continue;
                }
                sx = y;
                sy = h - 1 - x;
                break;
            default:
                break;
            }
            if (sx < w && sy < h && sx < 32 && sy < t.walk_mask.size() &&
                ((t.walk_mask[static_cast<std::size_t>(sy)] >> sx) & 1U) != 0) {
                mask = static_cast<std::uint16_t>(mask | (1U << (3 * v + u)));
            }
        }
    }
    return mask;
}
void write_walk_record(TileLayer& layer, const BuildingType& type, std::int32_t x0, std::int32_t y0,
                       std::uint32_t rotation) {
    // D AnnoWelt 0x100825af..0x100825d9: code store 0x100825af..0x100825b7,
    // mask store 0x100825c7..0x100825d9. The overlay test is the type's HEAD
    // flags bit 0x10 (slot +0x60, 0x10082184..0x1008218f), not the walk code;
    // that branch ORs the sampled mask (0x1008239d..0x100823b0).
    const std::uint16_t code = walk_code(type, rotation);
    const bool overlay = (type.flags_word & 0x10U) != 0;
    const auto [fw, fh] = footprint(type, static_cast<std::uint8_t>(rotation));
    for (std::uint32_t dy = 0; dy < fh; ++dy) {
        for (std::uint32_t dx = 0; dx < fw; ++dx) {
            const std::int64_t px = std::int64_t{x0} + std::int64_t{dx};
            const std::int64_t py = std::int64_t{y0} + std::int64_t{dy};
            const std::uint64_t i =
                layer.index_of(static_cast<std::int32_t>(px), static_cast<std::int32_t>(py));
            if (i == kNoSlot) {
                continue; // outside the island: skipped
            }
            layer.set_walk_code(i, code);
            const std::uint16_t mask = walk_mask(type, dx, dy, rotation);
            if (overlay) {
                layer.set_walk_mask(i, static_cast<std::uint16_t>(layer.walk_mask(i) | mask));
            } else {
                layer.set_walk_mask(i, mask);
            }
        }
    }
}
namespace {
/// D AnnoWelt 0x10082109..0x10082116: MSVC's signed remainder by 8, then
/// subtract. C++ toward-zero division matches it, including a negative origin.
std::int32_t placement_block_origin(std::int32_t v) {
    const std::int64_t n = v;
    const std::int64_t edge = kPlacementBlockEdge;
    return static_cast<std::int32_t>(n - n % edge);
}
} // namespace
std::uint32_t place_object_record(TileLayer& layer, const TypeRegistry& types,
                                  const BuildingType& type, std::int32_t x0, std::int32_t y0,
                                  std::uint32_t rotation, std::uint8_t phase) {
    // D AnnoWelt 0x10082040..0x10082637. Y blocks outer (0x1008262e), X blocks
    // inner (0x1008260c); inside a block, rows outer and columns inner.
    // Ordinary stores: id 0x1008244a, dx 0x1008245c, dy 0x10082474, rotation
    // 0x10082480, phase 0x100824c5, anchor 0x100824af..0x100824c8. Overlay
    // stores: 0x10082215..0x10082286, skip 0x100821e6..0x1008220c, anchor clear
    // only after a write (0x10082289). The code and the mask then cover every
    // footprint tile (0x10082342..0x100823e2), a skipped tile included.
    // Byte 3 bit 6 is not modelled. deviation: the height byte (0x10082260,
    // 0x10082494) is not a column (Terrain, OQ-060). A tile outside the layer
    // is skipped and does not clear the anchor: the original indexes the
    // island grid, and this layer has no record there. A missing type does not
    // satisfy the flag test (the catalogue load at 0x100821fa is not given a
    // null in the range read). The largest shipped edge is 8 (T33.5 step 1,
    // type 1021), so dx and dy fit the original's 3 bits and this u16.
    const bool overlay = (type.flags_word & 0x10U) != 0;
    const auto [fw, fh] = footprint(type, static_cast<std::uint8_t>(rotation));
    const std::int64_t x_end = std::int64_t{x0} + fw;
    const std::int64_t y_end = std::int64_t{y0} + fh;
    std::uint32_t skipped = 0;
    for (std::int64_t by = placement_block_origin(y0); by < y_end; by += kPlacementBlockEdge) {
        for (std::int64_t bx = placement_block_origin(x0); bx < x_end; bx += kPlacementBlockEdge) {
            bool anchor = true;
            const std::int64_t y_lo = std::max(by, std::int64_t{y0});
            const std::int64_t y_hi = std::min(by + kPlacementBlockEdge, y_end);
            const std::int64_t x_lo = std::max(bx, std::int64_t{x0});
            const std::int64_t x_hi = std::min(bx + kPlacementBlockEdge, x_end);
            for (std::int64_t y = y_lo; y < y_hi; ++y) {
                for (std::int64_t x = x_lo; x < x_hi; ++x) {
                    const std::uint64_t i =
                        layer.index_of(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y));
                    if (i == kNoSlot) {
                        continue;
                    }
                    if (overlay) {
                        const std::uint32_t id = layer.object(i);
                        const BuildingType* cur = id == 0 ? nullptr : types.find(TypeId{id});
                        if (cur != nullptr && (cur->flags_word & 0x10U) != 0 &&
                            layer.foot_anchor(i) != 0) {
                            ++skipped;
                            continue;
                        }
                    }
                    layer.set_object(i, type.id.v);
                    layer.set_foot_dx(i, static_cast<std::uint16_t>(x - x0));
                    layer.set_foot_dy(i, static_cast<std::uint16_t>(y - y0));
                    layer.set_foot_rotation(i, static_cast<std::uint8_t>(rotation));
                    layer.set_phase(i, phase);
                    layer.set_foot_anchor(i, anchor ? std::uint8_t{1} : std::uint8_t{0});
                    anchor = false;
                }
            }
        }
    }
    write_walk_record(layer, type, x0, y0, rotation);
    return skipped;
}
void clear_object_record(TileLayer& layer, const TypeRegistry& types, std::int32_t x,
                         std::int32_t y) {
    // D AnnoWelt 0x10082c30, the clear arm 0x10082f00..0x10082f1f. deviation:
    // the replacement arm (island +0x18 high nibble, 0x10082e43..0x10082efc)
    // is not modelled (OQ-201). The arm keeps the height byte and bits 25–31;
    // those are not columns (OQ-060, OQ-204, OQ-205).
    const std::uint64_t clicked = layer.index_of(x, y);
    if (clicked == kNoSlot) {
        return;
    }
    const std::uint32_t id = layer.object(clicked);
    if (id == 0) {
        return; // 0x10082c6a: a zero id writes nothing
    }
    const BuildingType* type = types.find(TypeId{id});
    if (type == nullptr) {
        return; // no footprint without a type; the catalogue load is not given a null
    }
    const std::uint8_t rotation = layer.foot_rotation(clicked);
    // Widened byte 2 (T33.5 step 1: no shipped edge above 8, so the low 3 + 3
    // bits are the whole offset). The origin is clicked − (dx, dy) (0x10082ca5).
    const std::int64_t x0 = std::int64_t{x} - std::int64_t{layer.foot_dx(clicked)};
    const std::int64_t y0 = std::int64_t{y} - std::int64_t{layer.foot_dy(clicked)};
    const auto [fw, fh] = footprint(*type, rotation);
    for (std::uint32_t dy = 0; dy < fh; ++dy) {
        for (std::uint32_t dx = 0; dx < fw; ++dx) {
            const std::int64_t px = x0 + std::int64_t{dx};
            const std::int64_t py = y0 + std::int64_t{dy};
            if (px < INT32_MIN || px > INT32_MAX || py < INT32_MIN || py > INT32_MAX) {
                continue;
            }
            const std::uint64_t i =
                layer.index_of(static_cast<std::int32_t>(px), static_cast<std::int32_t>(py));
            if (i == kNoSlot) {
                continue;
            }
            // 0x10082d98..0x10082de7: id, rotation and the in-footprint offset.
            if (layer.object(i) != id || layer.foot_rotation(i) != rotation ||
                layer.foot_dx(i) != dx || layer.foot_dy(i) != dy) {
                continue;
            }
            layer.set_object(i, 0);
            layer.set_foot_dx(i, 0);
            layer.set_foot_dy(i, 0);
            layer.set_foot_rotation(i, 0);
            layer.set_foot_anchor(i, 0); // and al, 0x3f (0x10082f11); phase stays
            layer.set_walk_code(i, 2);
            layer.set_walk_mask(i, 0);
        }
    }
}
WalkRecord walk_record(const Island& island, const TypeRegistry& types, TileXY p) {
    static_cast<void>(types); // kept for API compatibility; the record is read from the tile layer
    const auto i = island.tiles.index_of(p.x, p.y);
    if (i == kNoSlot) {
        return {0, 0x1ff};
    }
    return {island.tiles.walk_code(i), island.tiles.walk_mask(i), true};
}
WalkCells::WalkCells(const Island& island, const TypeRegistry& types, const WalkTables& tables,
                     TileRect rect, std::uint32_t edge, std::optional<SearchArea> disc)
    : island_(island), types_(types), tables_(tables), edge_(edge), disc_(std::move(disc)) {
    // D0x10002bb5..0x10002c1b: clip in world tiles, not island-relative origin.
    const std::int64_t x0 = std::max(rect.x0, island.tiles.x0()),
                       y0 = std::max(rect.y0, island.tiles.y0());
    const auto x1 = std::max(x0, std::min(std::int64_t{rect.x0} + rect.w,
                                          std::int64_t{island.tiles.x0()} + island.tiles.width()));
    const auto y1 = std::max(y0, std::min(std::int64_t{rect.y0} + rect.h,
                                          std::int64_t{island.tiles.y0()} + island.tiles.height()));
    rect_ = {static_cast<std::int32_t>(x0), static_cast<std::int32_t>(y0),
             static_cast<std::uint32_t>(x1 - x0), static_cast<std::uint32_t>(y1 - y0)};
}
std::vector<WalkCell>* WalkCells::chunk(TileXY p) {
    const std::int64_t x = std::int64_t{p.x} - rect_.x0, y = std::int64_t{p.y} - rect_.y0;
    if (edge_ == 0 || edge_ > kWorldEdgeBound || x < 0 || y < 0 || x >= rect_.w || y >= rect_.h) {
        return nullptr;
    }
    const TileXY key{static_cast<std::int32_t>(x / edge_), static_cast<std::int32_t>(y / edge_)};
    auto [it, fresh] = chunks_.try_emplace(key);
    if (fresh) {
        const auto width = std::min(edge_, rect_.w), height = std::min(edge_, rect_.h);
        it->second.resize(std::size_t{width} * height);
        for (std::uint32_t dy = 0; dy < height; ++dy) {
            for (std::uint32_t dx = 0; dx < width; ++dx) {
                const std::int64_t px = std::int64_t{rect_.x0} + std::int64_t{key.x} * edge_ + dx;
                const std::int64_t py = std::int64_t{rect_.y0} + std::int64_t{key.y} * edge_ + dy;
                if (px >= std::int64_t{rect_.x0} + rect_.w ||
                    py >= std::int64_t{rect_.y0} + rect_.h) {
                    continue;
                }
                const TileXY world{static_cast<std::int32_t>(px), static_cast<std::int32_t>(py)};
                const auto r = walk_record(island_, types_, world);
                // walk_record always returns supported = true now (task T33.4 fix-up F5):
                // the per-tile walk record is read from the tile layer's walk_code /
                // walk_mask columns, which the placement writers fill for every tile
                // of an object's footprint. Nothing sets complete_ false any more.
                complete_ = true;
                WalkCell c{r.mask9 < tables_.mapper.size() ? tables_.mapper[r.mask9]
                                                           : std::uint16_t{0},
                           r.code};
                // D0x100037e5..0x10003a84: outside disc closes high word, low unchanged.
                if (disc_ && !disc_->contains(world.x, world.y)) {
                    c.high = static_cast<std::uint16_t>((c.high & 0xfc07) | 7);
                }
                it->second[std::size_t{dy} * width + dx] = c;
            }
        }
    }
    return &it->second;
}
WalkCell WalkCells::get(TileXY p) {
    auto* c = chunk(p);
    if (!c) {
        return {};
    }
    const auto x = static_cast<std::uint32_t>(std::int64_t{p.x} - rect_.x0) % edge_;
    const auto y = static_cast<std::uint32_t>(std::int64_t{p.y} - rect_.y0) % edge_;
    return (*c)[std::size_t{y} * std::min(edge_, rect_.w) + x];
}
bool WalkCells::set(TileXY p, WalkCell value) {
    auto* c = chunk(p);
    if (!c) {
        return false;
    }
    const auto x = static_cast<std::uint32_t>(std::int64_t{p.x} - rect_.x0) % edge_;
    const auto y = static_cast<std::uint32_t>(std::int64_t{p.y} - rect_.y0) % edge_;
    (*c)[std::size_t{y} * std::min(edge_, rect_.w) + x] = value;
    return true;
}
} // namespace anno::sim
