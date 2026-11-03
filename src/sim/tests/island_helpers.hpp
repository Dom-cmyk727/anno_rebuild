// Helpers shared by the island tests (island_state_test.cpp, tile_hash_test.cpp;
// stage S5): worlds from the synthetic scenarios, a threaded executor, byte
// streams written by hand and the save frame's offsets.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/executor.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>

#include "world_access.hpp"

namespace island_test {

namespace sim = anno::sim;

/// A little-endian byte stream written by hand: the expected bytes are
/// transcribed from docs/sim/state.md §9.2/§9.3 independently of the field list.
struct Stream {
    std::vector<std::uint8_t> v;
    void u8(std::uint32_t x) { v.push_back(static_cast<std::uint8_t>(x)); }
    void u16(std::uint32_t x) {
        u8(x);
        u8(x >> 8);
    }
    void u32(std::uint32_t x) {
        u16(x);
        u16(x >> 16);
    }
    void i32(std::int32_t x) { u32(static_cast<std::uint32_t>(x)); }
    void u64(std::uint64_t x) {
        u32(static_cast<std::uint32_t>(x));
        u32(static_cast<std::uint32_t>(x >> 32));
    }
    void boolean(bool b) { u8(b ? 1U : 0U); }
    void str(const std::string& s) {
        u64(s.size());
        for (const char c : s) {
            u8(static_cast<std::uint8_t>(c));
        }
    }
};

/// The stream of an empty city (docs/sim/state.md §9.3, transcribed here from the
/// text, not from the field list): number, owner, tick stamp; the stock (capacity
/// 50 t, 56 goods of 0); 56 goods settings; 5 build goods; 2 empty trade lists; the
/// index (stamp 0, 6 fresh lists of one empty slot: hint -1, cursor 0).
inline void append_empty_city(Stream& s, std::uint32_t number, std::uint32_t owner,
                              const std::string& name = {}) {
    s.u32(number);
    s.u32(owner);
    s.u16(0);
    s.u32(50 * 256);
    s.u64(56);
    for (int i = 0; i < 56; ++i) {
        s.u32(0);
    }
    s.u64(56);
    for (int i = 0; i < 56; ++i) {
        s.boolean(false);
        s.i32(-1);
    }
    s.u64(5);
    for (int i = 0; i < 5; ++i) {
        s.u32(0);
        s.u64(0);
    }
    s.u64(2);
    s.u64(0);
    s.u64(0);
    s.u64(name.size()); // the city name (stream 119): u64 length, then the bytes
    for (const char c : name) {
        s.u8(static_cast<std::uint8_t>(c));
    }
    s.u64(0);  // the player mask (stream 120): an empty set, a u64 count
    s.u32(0);  // the stage mask (stream 128)
    s.u16(0);  // the stage stamp (stream 128)
    s.u64(56); // the billed counters (stream 128, Rules::save_billed_goods default): 56 goods of 0
    for (int i = 0; i < 56; ++i) {
        s.u32(0);
    }
    s.u16(0);
    s.u64(6);
    for (int i = 0; i < 6; ++i) {
        s.u64(0xffffffffffffffffULL); // hint -1
        s.u32(0);                     // cursor
        s.u64(1);                     // slot count
        s.u8(0);                      // the empty slot
    }
}

/// Stage S6: a tile owner is a present city and a city's owner is a player.
/// Gives every owner below `neutral` that an island's owner runs name (a run of
/// length 0 names none) an empty city owned by the scenario's first player when the
/// island has no city with that number, and a player in slot 0 when the scenario
/// has no player. Idempotent; call it again after a test changed the runs.
inline void add_owner_cities(sim::ScenarioSpec& spec, std::uint32_t neutral = 15) {
    if (spec.players.empty()) {
        sim::ScenarioSpec::PlayerSpec p;
        p.id = sim::PlayerId{0};
        spec.players.push_back(p);
    }
    const sim::PlayerId owner = spec.players.front().id;
    for (sim::ScenarioSpec::IslandSpec& is : spec.islands) {
        for (const auto& [run_owner, length] : is.owner_runs) {
            if (run_owner >= neutral || length == 0) {
                continue;
            }
            bool have = false;
            for (const sim::ScenarioSpec::CitySpec& c : is.cities) {
                have = have || c.no.v == run_owner;
            }
            if (!have) {
                sim::ScenarioSpec::CitySpec c;
                c.no = sim::CityNo{run_owner};
                c.owner = owner;
                is.cities.push_back(c);
            }
        }
    }
}

/// World::create on the empty registry (the original goods table, no types),
/// required to succeed.
inline sim::World make_world(const sim::ScenarioSpec& spec, sim::SimConfig config = {},
                             sim::Rules rules = {}) {
    auto r = sim::World::create(spec, sim::TypeRegistry::empty(), std::move(config), rules);
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return std::move(r).value();
}

/// The error of World::create (empty when it succeeded).
inline std::string create_error(const sim::ScenarioSpec& spec, sim::SimConfig config = {},
                                sim::Rules rules = {}, sim::Seams seams = {}) {
    auto r = sim::World::create(spec, sim::TypeRegistry::empty(), std::move(config), rules,
                                std::move(seams));
    return r.ok() ? std::string() : r.error();
}

/// The error of World::load on the empty registry (empty when it succeeded).
inline std::string load_error(const std::vector<std::uint8_t>& bytes) {
    auto r = sim::World::load(bytes, sim::TypeRegistry::empty());
    return r.ok() ? std::string() : r.error();
}

inline sim::World load_world(const std::vector<std::uint8_t>& bytes) {
    auto r = sim::World::load(bytes, sim::TypeRegistry::empty());
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return std::move(r).value();
}

/// The flat stream of the world (what the hash runs), as bytes.
inline std::vector<std::uint8_t> flat_stream(const sim::World& w) {
    sim::WriteArchive wa;
    fields(wa, w);
    return wa.take();
}

/// Reads a flat stream into `target`; empty when it was read whole and valid.
inline std::string read_flat(sim::World& target, const std::vector<std::uint8_t>& in) {
    sim::ReadArchive r(in);
    fields(r, target);
    return r.failed() ? r.error() : (r.remaining() == 0 ? std::string() : "trailing bytes");
}

/// The byte offset in the flat stream at which the `nth` (0-based) scope called
/// `name` opens, by a trace; npos when there is no such scope.
inline std::size_t flat_offset(const sim::World& w, const char* name, std::size_t nth = 0) {
    sim::TraceArchive trace;
    fields(trace, w);
    std::size_t offset = 0;
    for (const sim::TraceEntry& e : trace.entries()) {
        if (e.kind == sim::Prim::ScopeOpen && e.name == name) {
            if (nth == 0) {
                return offset;
            }
            --nth;
        }
        offset += static_cast<std::size_t>(e.width);
    }
    return std::string::npos;
}

/// The size in the flat stream of the timers_monopoly scope of the default
/// configuration (stage S12): u32 last_tick, 5 flags and 5 counters with their counts,
/// 28 entries of a u32 and an absent player = 4 + 8 + 5 + 8 + 20 + 8 + 28 * 5 = 193.
inline constexpr std::size_t kTimersMonopolyBytes = 193;

/// The size in the flat stream of the scopes that follow `islands` in a world
/// without figures and events (stage S7): the figures scope (i64 hint, u32 cursor,
/// u64 slot count, one empty slot byte, u16 stamp, the empty expiry list = 8 + 4 + 8 +
/// 1 + 2 + 8 = 31 bytes; 39 before T22 dropped the deferred list), the events scope
/// (the u64 count 0 = 8 bytes) and the timers_monopoly scope (stage S12).
inline constexpr std::size_t kTailBytes = 31 + 8 + kTimersMonopolyBytes;

/// The flat stream of `w` with the bytes of its islands scope replaced by `table`;
/// the scopes before it and the scopes after it (figures, events) stay as they are.
inline std::vector<std::uint8_t> with_island_table(const sim::World& w,
                                                   const std::vector<std::uint8_t>& table) {
    const std::vector<std::uint8_t> flat = flat_stream(w);
    const std::size_t from = flat_offset(w, "islands");
    const std::size_t to = flat_offset(w, "figures");
    REQUIRE(from != std::string::npos);
    REQUIRE(to != std::string::npos);
    std::vector<std::uint8_t> out(flat.begin(), flat.begin() + static_cast<std::ptrdiff_t>(from));
    out.insert(out.end(), table.begin(), table.end());
    out.insert(out.end(), flat.begin() + static_cast<std::ptrdiff_t>(to), flat.end());
    return out;
}

/// The offset in the save frame of a byte at `flat` in the flat stream, which
/// lies in the section with the 0-based index `section` (header 0, clock_rng 1,
/// settings 2, ids 3, players 4, islands 5, figures 6, events 7): 4 magic + 4 + 4
/// versions + 1 table kind (the original table), then every section behind a u64
/// length.
inline std::size_t frame_offset(std::size_t flat, std::size_t section) {
    return 13 + 8 * (section + 1) + flat;
}

/// Recomputes the trailing checksum of a save after a test patched its bytes.
inline void reseal(std::vector<std::uint8_t>& save) {
    REQUIRE(save.size() > 8);
    sim::StateHasher h;
    h.bytes(std::span<const std::uint8_t>(save.data(), save.size() - 8));
    const std::uint64_t v = h.value();
    for (std::size_t i = 0; i < 8; ++i) {
        save[save.size() - 8 + i] = static_cast<std::uint8_t>(v >> (8 * i));
    }
}

inline void put32(std::vector<std::uint8_t>& b, std::size_t at, std::uint32_t v) {
    REQUIRE(at + 4 <= b.size());
    for (std::size_t i = 0; i < 4; ++i) {
        b[at + i] = static_cast<std::uint8_t>(v >> (8 * i));
    }
}

inline std::uint32_t get32(const std::vector<std::uint8_t>& b, std::size_t at) {
    REQUIRE(at + 4 <= b.size());
    std::uint32_t v = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        v |= static_cast<std::uint32_t>(b[at + i]) << (8 * i);
    }
    return v;
}

/// Unsupported-VERSION rejection of World::load for `good`, a valid save of this build
/// (docs/sim/state.md 9.5; save.cpp). These cases alter only a version number and reseal the frame,
/// so the checksum is valid and the frame is otherwise whole: they show WHICH version check fires.
/// They are not historical payloads (no earlier layout is reconstructed). For each earlier version
/// `v`:
///  - the outer frame's stream version (offset 8) = v      -> "state stream version v is not
///  supported";
///  - the state header's version (first u32 of section 0, outer version current) = v
///                                                          -> "state hash version differs";
///  - the same edit WITHOUT the reseal -> the checksum diagnostic (the cause control: it is what
///  the
///    earlier tests of this kind were silently asserting).
/// A frame save version above this build's is checked once.
inline void check_unsupported_versions(const std::vector<std::uint8_t>& good,
                                       const std::shared_ptr<const sim::TypeRegistry>& types,
                                       std::initializer_list<std::uint32_t> earlier) {
    constexpr std::size_t kSaveVersionAt = 4, kFrameStreamAt = 8;
    const std::size_t header_at = frame_offset(0, 0); // section 0 = header, flat offset 0
    const auto error_of = [&](const std::vector<std::uint8_t>& bytes) {
        auto r = sim::World::load(bytes, types);
        return r.ok() ? std::string() : r.error();
    };
    const auto has = [](const std::string& s, const std::string& part) {
        return s.find(part) != std::string::npos;
    };
    // The valid control reaches every check, and the fields sit where the offsets say.
    REQUIRE(error_of(good).empty());
    REQUIRE(get32(good, 4) == sim::kSaveVersion);
    REQUIRE(get32(good, kFrameStreamAt) == sim::kStateHashVersion);
    REQUIRE(get32(good, header_at) == sim::kStateHashVersion);
    REQUIRE(get32(good, header_at + 4) == sim::kPrngTimelineVersion);
    {
        std::vector<std::uint8_t> bad = good;
        put32(bad, kSaveVersionAt, sim::kSaveVersion + 1);
        reseal(bad);
        const std::string e = error_of(bad);
        MESSAGE("save frame version " << sim::kSaveVersion + 1 << " -> " << e);
        CHECK_MESSAGE(
            has(e, "version " + std::to_string(sim::kSaveVersion + 1) + " is not supported"), e);
        CHECK_MESSAGE(has(e, "this build reads " + std::to_string(sim::kSaveVersion)), e);
    }
    for (const std::uint32_t v : earlier) {
        INFO("earlier version " << v);
        REQUIRE(v != sim::kStateHashVersion);
        {
            std::vector<std::uint8_t> bad = good;
            put32(bad, kFrameStreamAt, v);
            reseal(bad);
            const std::string e = error_of(bad);
            MESSAGE("frame stream version " << v << " -> " << e);
            CHECK_MESSAGE(has(e, "state stream version " + std::to_string(v) + " is not supported"),
                          e);
            CHECK_MESSAGE(has(e, "this build reads " + std::to_string(sim::kStateHashVersion)), e);
            CHECK_FALSE(has(e, "checksum"));
        }
        {
            std::vector<std::uint8_t> bad = good;
            put32(bad, header_at, v); // the outer version stays current
            reseal(bad);
            const std::string e = error_of(bad);
            MESSAGE("state header version " << v << " -> " << e);
            CHECK_MESSAGE(has(e, "state hash version differs"), e);
            CHECK_FALSE(has(e, "checksum"));
            CHECK_FALSE(has(e, "is not supported")); // the outer check passed
        }
        {
            std::vector<std::uint8_t> bad = good;
            put32(bad, header_at, v); // the same edit, not resealed: damage, not a version
            const std::string e = error_of(bad);
            MESSAGE("state header version " << v << " without reseal" << " -> " << e);
            CHECK_MESSAGE(has(e, "the checksum does not match"), e);
        }
    }
}

/// An executor with `threads` threads that hands the jobs out through an atomic
/// counter (any thread, any order) and counts what it was asked to run.
class ThreadExecutor final : public sim::Executor {
  public:
    explicit ThreadExecutor(unsigned threads) : threads_(threads) {}

    void run(std::uint64_t n, Body body, void* ctx) const override {
        calls_.fetch_add(1);
        jobs_.fetch_add(n);
        std::atomic<std::uint64_t> next{0};
        auto worker = [&] {
            for (std::uint64_t i = next.fetch_add(1); i < n; i = next.fetch_add(1)) {
                body(ctx, i);
            }
        };
        std::vector<std::thread> pool;
        for (unsigned t = 1; t < threads_; ++t) {
            pool.emplace_back(worker);
        }
        worker();
        for (std::thread& th : pool) {
            th.join();
        }
    }
    [[nodiscard]] std::uint64_t calls() const { return calls_.load(); }
    [[nodiscard]] std::uint64_t jobs() const { return jobs_.load(); }

  private:
    unsigned threads_;
    mutable std::atomic<std::uint64_t> calls_{0};
    mutable std::atomic<std::uint64_t> jobs_{0};
};

/// An archive that visits the tile columns through TileIO and changes the
/// `at`-th value (counting object, phase, owner, aux, walk_code, walk_mask,
/// foot_dx, foot_dy, foot_rotation, foot_anchor in
/// order) without going through a setter: a write that skipped the dirty mark.
struct CorruptingArchive {
    static constexpr bool kReading = false;
    std::uint64_t at = 0;
    std::uint64_t seen = 0;
    void u32(std::uint32_t& v) {
        if (seen++ == at) {
            v ^= 1U;
        }
    }
    void u16(std::uint16_t& v) {
        if (seen++ == at) {
            v = static_cast<std::uint16_t>(v ^ 1U);
        }
    }
    void u8(std::uint8_t& v) {
        if (seen++ == at) {
            v ^= 1U;
        }
    }
};

/// The hierarchical hash of an island alone.
inline std::uint64_t hash_of(const sim::Island& is) {
    return sim::hash_state(is, nullptr, 0);
}

} // namespace island_test
