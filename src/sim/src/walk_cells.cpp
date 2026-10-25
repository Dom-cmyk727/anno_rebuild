#include <algorithm>
#include <cstddef>
#include <utility>

#include <anno/sim/building.hpp>
#include <anno/sim/walk_cells.hpp>
namespace anno::sim {
namespace wf = formats::annowelt;
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
WalkRecord walk_record(const Island& island, const TypeRegistry& types, TileXY p) {
    const auto i = island.tiles.index_of(p.x, p.y);
    if (i == kNoSlot) {
        return {0, 0x1ff};
    }
    const auto* type = types.find(TypeId{island.tiles.object(i)});
    if (!type) {
        // deviation OQ-200: HEIGHT-to-runtime corner grid unresolved; no water guess.
        // deviation OQ-170: no per-subcell coast pass0x10081ce0.
        return {2, 0};
    }
    for (const auto& city : island.cities) {
        if (!city) {
            continue;
        }
        for (const auto& list : city->index.lists) {
            for (const auto& slot : list.slots) {
                if (!slot || slot->type != type->id) {
                    continue;
                }
                const auto& b = *slot;
                const auto [w, h] = footprint(*type, b.rotation);
                const std::int64_t dx = std::int64_t{p.x} - b.x, dy = std::int64_t{p.y} - b.y;
                if (dx >= 0 && dy >= 0 && dx < w && dy < h) {
                    return {walk_code(*type, b.rotation),
                            walk_mask(*type, static_cast<std::uint32_t>(dx),
                                      static_cast<std::uint32_t>(dy), b.rotation)};
                }
            }
        }
    }
    // Owner-authorized stop: tile-only placements dropped rotation/offsets.
    // Only a 1x1 object invariant under ALL rotations is known from retained state.
    const auto code = walk_code(*type, 0), mask = walk_mask(*type, 0, 0, 0);
    bool known = type->width == 1 && type->height == 1;
    for (std::uint32_t r = 1; r < 4; ++r) {
        known = known && walk_code(*type, r) == code && walk_mask(*type, 0, 0, r) == mask;
    }
    return known ? WalkRecord{code, mask, true} : WalkRecord{0, 0x1ff, false};
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
                complete_ = complete_ && r.supported;
                WalkCell c{r.mask9 < tables_.mapper.size() ? tables_.mapper[r.mask9]
                                                           : std::uint16_t{0},
                           r.code};
                if (!r.supported) {
                    c = {};
                }
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
