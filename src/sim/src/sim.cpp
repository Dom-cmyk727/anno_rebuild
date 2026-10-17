#include <algorithm>
#include <utility>

#include <anno/sim/clock.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/rng.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

std::vector<std::uint16_t> msvc_rand_sequence(std::size_t n, std::uint32_t seed) {
    std::vector<std::uint16_t> out;
    out.reserve(n);
    std::uint32_t x = seed;
    for (std::size_t i = 0; i < n; ++i) {
        x = x * 214013U + 2531011U; // wraps mod 2³² (unsigned)
        out.push_back(static_cast<std::uint16_t>((x >> 16) & 0x7fffU));
    }
    return out;
}

WorldRng::WorldRng() : WorldRng(msvc_rand_sequence(kOriginalRngLength, 1)) {}

WorldRng::WorldRng(std::vector<std::uint16_t> table) : table_(std::move(table)) {
    StateHasher h;
    for (const std::uint16_t v : table_) {
        h.u16(v);
    }
    digest_ = h.value();
}

std::optional<WorldRng> WorldRng::from_table(std::vector<std::uint16_t> table,
                                             std::uint32_t cursor) {
    if (table.empty()) {
        return std::nullopt;
    }
    WorldRng r(std::move(table));
    r.set_cursor(cursor);
    return r;
}

std::uint16_t WorldRng::next() {
    cursor_ = static_cast<std::uint32_t>((std::uint64_t{cursor_} + 1) % table_.size());
    return table_[cursor_];
}

void WorldRng::set_cursor(std::uint32_t value) {
    cursor_ = static_cast<std::uint32_t>(value % table_.size());
}

bool is_original_speed(std::uint32_t percent, bool multiplayer) {
    switch (percent) {
    case 0:
        return !multiplayer;
    case 50:
    case 100:
    case 200:
        return true;
    case 400:
        return !multiplayer;
    default:
        return false;
    }
}

std::optional<std::uint32_t> single_player_step_ms(std::uint32_t frame_ms, std::uint32_t percent,
                                                   const Timing& timing) {
    if (!is_original_speed(percent, false)) {
        return std::nullopt;
    }
    const std::uint64_t dt = std::min(frame_ms, timing.max_frame_ms);
    const std::uint64_t game_ms = (dt * percent + 50) / 100;
    if (game_ms < 1) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(game_ms, timing.max_single_step_ms));
}

std::optional<std::uint32_t> multiplayer_step_ms(std::uint32_t percent) {
    if (!is_original_speed(percent, true)) {
        return std::nullopt;
    }
    return percent / 10;
}

std::uint64_t sweep_chunks(std::uint32_t chunks, std::uint16_t delta_tick, std::uint32_t period) {
    if (period == 0) {
        return chunks;
    }
    return std::min<std::uint64_t>(std::uint64_t{chunks} * delta_tick / period + 1, chunks);
}

std::uint64_t World::state_hash() const {
    StateHasher h;
    h.u64(clock.ms);
    h.u64(clock.last_tick);
    h.u64(rng.size());
    h.u64(rng.table_digest());
    h.u32(rng.cursor());
    return h.value();
}

} // namespace anno::sim
