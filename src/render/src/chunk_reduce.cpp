#include <algorithm>
#include <cstdint>

#include <anno/render/chunk_reduce.hpp>

namespace anno::render {

namespace {

constexpr std::size_t kN = static_cast<std::size_t>(kChunk);
constexpr std::size_t kHalf = kN / 2;

/// round(sum / count) to nearest, ties up.
constexpr std::uint32_t mean(std::uint32_t sum, std::uint32_t count) {
    return (2U * sum + count) / (2U * count);
}

/// Index of texel (x, y) of a chunk plane.
constexpr std::size_t at(std::size_t x, std::size_t y) {
    return y * kN + x;
}

} // namespace

Chunk reduce_chunks_2x(const ChunkQuad& quad, Px open_water) {
    auto some = [&](auto member) {
        return std::any_of(quad.begin(), quad.end(),
                           [&](const Chunk* c) { return c != nullptr && !(c->*member).empty(); });
    };
    const bool has_ground = some(&Chunk::ground), has_objects = some(&Chunk::objects),
               has_field = some(&Chunk::field), has_terrain = some(&Chunk::terrain);

    Chunk out;
    out.ships = true;
    for (const Chunk* c : quad) {
        if (c != nullptr) {
            out.water_below = out.water_below || c->water_below;
            out.water_above = out.water_above || c->water_above;
            out.ships = out.ships && c->ships;
        }
    }
    const std::uint32_t water_rgb =
        std::uint32_t{open_water[0]} << 16U | std::uint32_t{open_water[1]} << 8U | open_water[2];
    if (has_ground) {
        out.ground.assign(kN * kN, water_rgb);
    }
    if (has_objects) {
        out.objects.assign(kN * kN, Px{0, 0, 0, 0});
    }
    if (has_field) {
        out.field.resize(kN * kN * 4);
        for (std::size_t i = 0; i < kN * kN; ++i) {
            out.field[i * 4 + 0] = 255;
            out.field[i * 4 + 1] = 255;
            out.field[i * 4 + 2] = 255;
            out.field[i * 4 + 3] = 1;
        }
    }
    if (has_terrain) {
        out.terrain.assign(kN * kN, kNoTerrain);
    }

    // A source without a plane (or no source) leaves the defaults set above in
    // its quadrant: open sea.
    for (std::size_t q = 0; q < 4; ++q) {
        const Chunk* c = quad[q];
        if (c == nullptr) {
            continue;
        }
        const std::size_t ox = (q % 2) * kHalf, oy = (q / 2) * kHalf;
        for (std::size_t y = 0; y < kHalf; ++y) {
            for (std::size_t x = 0; x < kHalf; ++x) {
                const std::size_t d = at(ox + x, oy + y);
                // The four source texels of this one.
                const std::array<std::size_t, 4> s = {at(2 * x, 2 * y), at(2 * x + 1, 2 * y),
                                                      at(2 * x, 2 * y + 1),
                                                      at(2 * x + 1, 2 * y + 1)};
                std::array<bool, 4> land{};
                std::uint32_t lands = 0;
                for (std::size_t k = 0; k < 4; ++k) {
                    land[k] =
                        c->terrain.empty() ? !c->ground.empty() : c->terrain[s[k]] != kNoTerrain;
                    lands += land[k] ? 1U : 0U;
                }
                if (lands > 0) {
                    if (!c->terrain.empty()) {
                        std::uint32_t sum = 0;
                        for (std::size_t k = 0; k < 4; ++k) {
                            sum += land[k] ? c->terrain[s[k]] : 0U;
                        }
                        out.terrain[d] = static_cast<std::uint16_t>(mean(sum, lands));
                    }
                    if (!c->ground.empty()) {
                        std::uint32_t rgb = 0;
                        for (unsigned shift = 0; shift <= 16; shift += 8) {
                            std::uint32_t sum = 0;
                            for (std::size_t k = 0; k < 4; ++k) {
                                sum += land[k] ? (c->ground[s[k]] >> shift) & 0xFFU : 0U;
                            }
                            rgb |= mean(sum, lands) << shift;
                        }
                        out.ground[d] = rgb;
                    }
                }
                if (!c->objects.empty()) {
                    std::uint32_t a = 0, r = 0, g = 0, b = 0;
                    for (const std::size_t i : s) {
                        const Px& p = c->objects[i];
                        a += p[3];
                        r += std::uint32_t{p[0]} * p[3];
                        g += std::uint32_t{p[1]} * p[3];
                        b += std::uint32_t{p[2]} * p[3];
                    }
                    if (a > 0) {
                        out.objects[d] = {static_cast<std::uint8_t>(mean(r, a)),
                                          static_cast<std::uint8_t>(mean(g, a)),
                                          static_cast<std::uint8_t>(mean(b, a)),
                                          static_cast<std::uint8_t>(mean(a, 4))};
                    }
                }
                if (!c->field.empty()) {
                    std::uint32_t cover = 0, shore = 0, depth = 0, flag_bits = 0, slope = 0,
                                  water = 0;
                    for (const std::size_t i : s) {
                        const std::uint8_t* f = &c->field[i * 4];
                        cover += f[0];
                        shore += f[1];
                        depth += f[2];
                        flag_bits |= f[3] & 3U;
                        if (f[0] > 0) {
                            slope += f[3] >> 2U;
                            ++water;
                        }
                    }
                    std::uint8_t* o = &out.field[d * 4];
                    o[0] = static_cast<std::uint8_t>(mean(cover, 4));
                    o[1] = static_cast<std::uint8_t>(mean(shore, 4));
                    o[2] = static_cast<std::uint8_t>(mean(depth, 4));
                    o[3] = static_cast<std::uint8_t>(flag_bits |
                                                     (water > 0 ? mean(slope, water) << 2U : 0U));
                }
            }
        }
    }
    return out;
}

} // namespace anno::render
