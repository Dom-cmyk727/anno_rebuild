#include <algorithm>
#include <memory>
#include <utility>

#include <anno/sim/archive.hpp>
#include <anno/sim/clock.hpp>
#include <anno/sim/executor.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/player.hpp>
#include <anno/sim/rng.hpp>
#include <anno/sim/state_fields.hpp>
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

namespace {

std::shared_ptr<const RngTable> make_table(std::vector<std::uint16_t> values) {
    auto t = std::make_shared<RngTable>();
    StateHasher h;
    for (const std::uint16_t v : values) {
        h.u16(v);
    }
    t->digest = h.value();
    t->values = std::move(values);
    return t;
}

/// The original table, built once and shared by every default generator
/// (immutable after construction; function-local static initialisation is
/// thread safe).
const std::shared_ptr<const RngTable>& original_table() {
    static const std::shared_ptr<const RngTable> table =
        make_table(msvc_rand_sequence(kOriginalRngLength, 1));
    return table;
}

/// The one body behind both World::island_at overloads: the island whose rectangle holds
/// the world tile, or nullptr. `Islands` deduces to the const or the non-const island
/// vector, so the const and the mutable lookup share this scan. The table is scanned (a
/// derived grid is a later optimisation, §4.14): the cost is the island count, never the
/// building or player count.
template <class Islands>
auto island_at_impl(Islands& islands, std::int32_t x, std::int32_t y)
    -> decltype(&*islands.front()) {
    for (auto& is : islands) {
        if (is.has_value() && is->tiles.contains(x, y)) {
            return &*is;
        }
    }
    return nullptr;
}

} // namespace

WorldRng::WorldRng() : table_(original_table()) {}

// GCC 13 at -O3 once reported a "potential null pointer dereference" for the
// WorldRng built here and moved into the optional (at that time the move
// copied a std::vector table). WorldRng now holds a shared_ptr that is never
// null, so the report may be gone; the suppression stays, for this function
// only, until the linux-gcc-release job shows it unnecessary (it cannot be
// checked on the MSVC host).
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
std::optional<WorldRng> WorldRng::from_table(std::vector<std::uint16_t> table,
                                             std::uint32_t cursor) {
    if (table.empty()) {
        return std::nullopt;
    }
    WorldRng r(make_table(std::move(table)));
    r.set_cursor(cursor);
    return r;
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

std::uint16_t WorldRng::next() {
    const std::vector<std::uint16_t>& v = table_->values;
    cursor_ = static_cast<std::uint32_t>((std::uint64_t{cursor_} + 1) % v.size());
    return v[cursor_];
}

void WorldRng::set_cursor(std::uint32_t value) {
    cursor_ = static_cast<std::uint32_t>(value % table_->values.size());
}

std::uint32_t rng_mix(std::uint64_t seed, std::uint32_t slot) {
    // splitmix64: state step 0x9e3779b97f4a7c15 per slot, then the finaliser.
    std::uint64_t x = seed + 0x9e3779b97f4a7c15ULL * (std::uint64_t{slot} + 1);
    std::uint64_t z = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    z = z ^ (z >> 31);
    return static_cast<std::uint32_t>(z);
}

WorldRng WorldRng::stream(std::uint64_t seed, std::uint32_t slot) const {
    WorldRng r(table_);
    r.set_cursor(rng_mix(seed, slot));
    return r;
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

World::World()
    : types_(TypeRegistry::empty()), players_(make_player_table(config_)),
      figures_(make_figure_registry(config_)), timers_(make_script_timers(config_)),
      monopoly_(make_monopoly_table(config_)) {}

World::World(std::uint32_t seed)
    : types_(TypeRegistry::empty()), players_(make_player_table(config_)),
      figures_(make_figure_registry(config_)), timers_(make_script_timers(config_)),
      monopoly_(make_monopoly_table(config_)) {
    rng_.set_cursor(seed);
}

const Island* World::island_at(std::int32_t x, std::int32_t y) const {
    return island_at_impl(islands_, x, y);
}

Island* World::island_at(std::int32_t x, std::int32_t y) {
    return island_at_impl(islands_, x, y);
}

void World::prepare_hash(const Executor& ex) {
    for (std::optional<Island>& is : islands_) {
        if (is.has_value()) {
            is->tiles.refresh(ex);
        }
    }
}

const Executor& serial_executor() {
    // Stateless and immutable: safe to share between threads and worlds.
    static const SerialExecutor executor;
    return executor;
}

std::uint64_t World::state_hash() const {
    return hash_state(*this, nullptr, 0);
}

std::uint64_t World::state_hash(HashTree& tree, std::uint32_t depth) const {
    return hash_state(*this, &tree, depth);
}

std::uint64_t World::core_hash() const {
    StateHasher h;
    h.u64(clock_.ms);
    h.u64(clock_.last_tick);
    h.u64(rng_.size());
    h.u64(rng_.table_digest());
    h.u32(rng_.cursor());
    return h.value();
}

} // namespace anno::sim
