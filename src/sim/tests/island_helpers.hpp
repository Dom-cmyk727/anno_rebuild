// Helpers shared by the island tests (island_state_test.cpp, tile_hash_test.cpp;
// stage S5): worlds from the synthetic scenarios, a threaded executor, byte
// streams written by hand and the save frame's offsets.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
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

/// The offset in the save frame of a byte at `flat` in the flat stream, which
/// lies in the section with the 0-based index `section` (header 0, clock_rng 1,
/// settings 2, players 3, islands 4): 4 magic + 4 + 4 versions + 1 table kind
/// (the original table), then every section behind a u64 length.
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
/// `at`-th value (counting object, phase, owner, aux columns in order) without
/// going through a setter: a write that skipped the dirty mark.
struct CorruptingArchive {
    static constexpr bool kReading = false;
    std::uint64_t at = 0;
    std::uint64_t seen = 0;
    void u32(std::uint32_t& v) {
        if (seen++ == at) {
            v ^= 1U;
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
