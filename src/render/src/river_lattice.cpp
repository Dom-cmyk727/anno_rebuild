// The flow lattice of enhanced rivers: the shading configuration and the
// baked texel plane a pixel reads (docs/render/rivers.md, 3.3 and 8; stage S5).
#include <algorithm>
#include <cmath>

#include <anno/render/river.hpp>

namespace anno::render::river {

namespace {

/// The hour of the animation clock (iso.cpp time_f takes the time modulo it).
constexpr std::uint64_t kHourMs = 3600000;

} // namespace

std::vector<LayerConfig> default_layers() {
    // rivers.md 2.8, tuning values. Scale (1/tile), period (ms; all divide the
    // hour), amplitude, fades with the zoom group's wave detail, rapids layer.
    // The amplitudes of the three ripple layers are the table's 0.045 / 0.060 /
    // 0.040 times kLightGain; kLightGain makes the light variance of the flow
    // shading equal the legacy shimmer's for a uniform flow at full wave detail
    // (the calibration test); in real scenes the detail fade of the zoom groups
    // and the shear fade leave it below that (rivers.md 15.3, OQ-083). No
    // stretch along the flow: the design's S(u) applied to the island's tile
    // coordinates aliased (rivers.md 15.7).
    constexpr float kLightGain = 0.52F;
    return {
        {0.40F, 2400.0F, 0.045F * kLightGain, false, false},
        {1.15F, 1600.0F, 0.060F * kLightGain, false, false},
        {3.10F, 800.0F, 0.040F * kLightGain, true, false},
        // Whitewater streaks of the falls: 0.05 + 0.11 noise, times the rapids factor.
        {7.30F, 640.0F, 0.11F, true, true},
    };
}

std::vector<std::uint32_t> default_obstacle_ids() {
    // Reviewed on sprite sheets and in-scene crops of every id that has at least
    // half of a footprint of at most 16 tiles in river water somewhere in the
    // corpus (river_qa --obstacles, rivers.md 17.2): the rocks and cliffs. The
    // design's candidates (1502-1508, 1549-1551, 1572-1577, 1580, 1581) all are
    // rocks; the review adds 1535, 1548, 1553, 1555, 1557, 1558, 1560, 1561 and
    // drops nothing except what is not a rock: vegetation (ids below 1000, 1x1),
    // the bridges 3956 / 3961, the lookouts 2536 / 2537, the crate 2530 and the
    // block 3974, which water passes under or that are not in the water. No
    // category of the data says so (OQ-073).
    return {1502, 1503, 1505, 1506, 1507, 1508, 1535, 1548, 1549, 1550, 1551, 1553, 1555,
            1557, 1558, 1560, 1561, 1572, 1573, 1574, 1575, 1576, 1577, 1580, 1581};
}

std::string RiverConfig::validate(std::uint32_t chunk_px, std::size_t groups) const {
    if (layers.size() > kMaxRiverLayers) {
        return "more than " + std::to_string(kMaxRiverLayers) + " river layers";
    }
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const LayerConfig& l = layers[i];
        const std::string name = "river layer " + std::to_string(i);
        if (!(l.period_ms >= 1.0F) || l.period_ms != std::floor(l.period_ms) ||
            kHourMs % static_cast<std::uint64_t>(l.period_ms) != 0) {
            return name + ": the period must be a whole number of milliseconds that divides "
                          "3,600,000 (the hourly time wrap)";
        }
        if (!std::isfinite(l.scale) || !(l.scale > 0.0F)) {
            return name + ": the scale must be positive";
        }
        if (!std::isfinite(l.amplitude)) {
            return name + ": the amplitude must be finite";
        }
    }
    if (lattice_stride.size() != groups) {
        return "one lattice stride per zoom group";
    }
    for (const std::uint32_t s : lattice_stride) {
        if (s == 0 || chunk_px % s != 0) {
            return "every lattice stride must divide the chunk size";
        }
        if (s > kMaxLatticeStride) {
            return "every lattice stride must be at most " + std::to_string(kMaxLatticeStride) +
                   " pixels (the GPU chunk flags hold it in 8 bits)";
        }
    }
    // The int16 lattice holds +-32767 / kVelocityScale.
    if (!(velocity_max > 0.0F) || velocity_max * kVelocityScale > 32768.0F) {
        return "velocity_max must be positive and fit the int16 lattice";
    }
    if (!(extend_tiles >= 0.0F && extend_tiles <= kMaxExtendTiles) ||
        !(min_valid >= 0.0F && min_valid <= 1.0F)) {
        return "extend_tiles must be 0.." + std::to_string(static_cast<int>(kMaxExtendTiles)) +
               " and min_valid 0..1";
    }
    if (!(shear_probe_tiles > 0.0F) || !std::isfinite(shear_probe_tiles)) {
        return "shear_probe_tiles must be positive";
    }
    if (flow_threads > 64) {
        return "flow_threads must be 0 (automatic) .. 64";
    }
    if (!(light_gain >= 0.0F) || !std::isfinite(light_gain)) {
        return "light_gain must be a finite number, not negative";
    }
    if (!(shear_to > shear_from)) {
        return "shear_to must exceed shear_from";
    }
    // Stage S7.
    auto finite_in = [](float v, float lo, float hi) {
        return std::isfinite(v) && v >= lo && v <= hi;
    };
    if (!(std::isfinite(gravity) && gravity > 0.0F)) {
        return "gravity must be positive";
    }
    if (!(froude_to > froude_from) || !finite_in(froude_from, 0.0F, 100.0F) ||
        !finite_in(froude_to, 0.0F, 100.0F)) {
        return "froude_to must exceed froude_from (both non-negative)";
    }
    if (!(turb_fr_to > turb_fr_from) || !(turb_bank_tiles > 0.0F)) {
        return "turb_fr_to must exceed turb_fr_from and turb_bank_tiles must be positive";
    }
    if (!finite_in(aeration, 0.0F, 1.0F) || !finite_in(wake_dark, 0.0F, 1.0F) ||
        !finite_in(wake_boost, 0.0F, 100.0F) || !finite_in(wake_slow, 0.0F, 1.0F)) {
        return "aeration, wake_dark and wake_slow must be 0..1, wake_boost non-negative";
    }
    if (!(wake_length > 0.0F) || !std::isfinite(wake_length)) {
        return "wake_length must be positive";
    }
    if (!finite_in(obstacle_min_wet, 0.0F, 1.0F) || !finite_in(obstacle_shrink, 0.0F, 0.9F) ||
        obstacle_gap_cells > 64) {
        return "obstacle_min_wet must be 0..1, obstacle_shrink 0..0.9, obstacle_gap_cells 0..64";
    }
    if (!(standing_to > standing_from) || !(standing_min > 0.0F) ||
        !(standing_max >= standing_min) || !std::isfinite(standing_amp)) {
        return "standing waves: standing_to must exceed standing_from, 0 < standing_min <= "
               "standing_max";
    }
    if (picard_iterations > 64 || !finite_in(picard_floor, 1e-6F, 1.0F) ||
        !finite_in(picard_relax, 0.05F, 1.0F) || !(picard_tolerance > 0.0F)) {
        return "picard: at most 64 iterations, picard_floor 1e-6..1, picard_relax 0.05..1";
    }
    if (!finite_in(thalweg_beta, 0.0F, 0.9F)) {
        return "thalweg_beta must be 0..0.9 (a larger shift would reverse the flow at the inner "
               "bank)";
    }
    return {};
}

RiverParams RiverConfig::params() const {
    RiverParams p;
    p.rapids_base = rapids_base;
    p.shear_from = shear_from;
    p.shear_to = shear_to;
    p.froude_from = froude_from;
    p.froude_to = froude_to;
    p.gravity = gravity;
    p.depth_min = h_min;
    p.aeration = aeration;
    p.wake_dark = wake_dark;
    p.wake_boost = wake_boost;
    p.standing = standing_waves ? 1.0F : 0.0F;
    p.standing_amp = standing_amp;
    p.standing_from = standing_from;
    p.standing_to = standing_to;
    p.standing_min = standing_min;
    p.standing_max = standing_max;
    p.count = static_cast<std::uint32_t>(std::min(layers.size(), kMaxRiverLayers));
    for (std::size_t i = 0; i < p.count; ++i) {
        const LayerConfig& l = layers[i];
        p.layers[i] = {l.scale, l.period_ms, l.rapids ? l.amplitude : l.amplitude * light_gain,
                       l.detail_fades ? 1.0F : 0.0F, l.rapids ? 1.0F : 0.0F};
    }
    return p;
}

void set_texel(FlowPlane& plane, std::uint32_t i, std::uint32_t j, float ux, float uy, float valid,
               float turb, float wake, float level, float velocity_max, float shear) {
    const std::size_t t = plane.texel(i, j);
    // The int16 range limits the velocity whatever the caller configured.
    const float vmax = std::min(velocity_max, 32767.0F / kVelocityScale);
    auto quant = [&](float v) {
        return sat_cast<std::int16_t>(roundf(clampf(v, -vmax, vmax) * kVelocityScale));
    };
    auto unit = [](float v) {
        return sat_cast<std::uint8_t>(roundf(clampf(v, 0.0F, 1.0F) * 255.0F));
    };
    plane.uv[t * 2] = quant(ux);
    plane.uv[t * 2 + 1] = quant(uy);
    plane.aux[t * 4] = unit(valid);
    plane.aux[t * 4 + 1] = unit(turb);
    plane.aux[t * 4 + 2] = unit(wake);
    plane.aux[t * 4 + 3] = sat_cast<std::uint8_t>(roundf(clampf(level / kLevelStep, 0.0F, 255.0F)));
    if (plane.shear.size() > t) {
        plane.shear[t] = sat_cast<std::uint8_t>(roundf(clampf(shear / kShearStep, 0.0F, 255.0F)));
    }
}

FlowSample flow_at(const FlowPlane& p, int sx, int sy) {
    FlowSample out;
    if (p.empty()) {
        return out;
    }
    const float stride = static_cast<float>(p.stride_px);
    const float fx = (static_cast<float>(sx - p.x0) + 0.5F) / stride;
    const float fy = (static_cast<float>(sy - p.y0) + 0.5F) / stride;
    const float flx = std::floor(fx), fly = std::floor(fy);
    if (!(flx >= 0.0F) || !(fly >= 0.0F) || flx + 1.0F >= static_cast<float>(p.nx) ||
        fly + 1.0F >= static_cast<float>(p.ny)) {
        return out;
    }
    const auto i0 = static_cast<std::uint32_t>(flx), j0 = static_cast<std::uint32_t>(fly);
    const float a = fx - flx, b = fy - fly;
    const bool has_shear = p.shear.size() == p.uv.size() / 2;
    float sum = 0.0F, ux = 0.0F, uy = 0.0F, turb = 0.0F, wake = 0.0F, level = 0.0F, shear = 0.0F;
    for (std::uint32_t dy = 0; dy < 2; ++dy) {
        for (std::uint32_t dx = 0; dx < 2; ++dx) {
            const std::size_t t = p.texel(i0 + dx, j0 + dy);
            const float v = static_cast<float>(p.aux[t * 4]) / 255.0F;
            const float w = (dx != 0 ? a : 1.0F - a) * (dy != 0 ? b : 1.0F - b) * v;
            sum += w;
            ux += w * static_cast<float>(p.uv[t * 2]) / kVelocityScale;
            uy += w * static_cast<float>(p.uv[t * 2 + 1]) / kVelocityScale;
            turb += w * static_cast<float>(p.aux[t * 4 + 1]) / 255.0F;
            wake += w * static_cast<float>(p.aux[t * 4 + 2]) / 255.0F;
            level += w * static_cast<float>(p.aux[t * 4 + 3]) * kLevelStep;
            if (has_shear) {
                shear += w * static_cast<float>(p.shear[t]) * kShearStep;
            }
        }
    }
    out.valid = sum;
    if (!(sum > 0.0F)) {
        return out;
    }
    out.vx = ux / sum;
    out.vy = uy / sum;
    out.turb = turb / sum;
    out.wake = wake / sum;
    out.level = level / sum;
    // The shear |grad u| (1/s) is a baked channel (Scene::flow_plane): the
    // gradient of the island's flow over a fraction of a tile, interpolated
    // like the velocity, so the shear fade has no steps at texel borders.
    out.shear = shear / sum;
    return out;
}

} // namespace anno::render::river
