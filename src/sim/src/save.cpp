// The save frame (docs/sim/state.md §9.5, stage S5): World::save and World::load.
//
//   "A3SV"                      4 bytes
//   u32 save version            kSaveVersion
//   u32 stream version          kStateHashVersion
//   u8  PRNG table kind         0 = the original msvc_rand_sequence(2048, 1);
//                               1 = custom: u64 n, n x u16
//   5 sections                  u64 byte length, then the section's bytes: the
//                               top-level scopes of the stream, in order
//   terrain blobs               per present island, in slot order: bool present,
//                               then height (u64 n + bytes), gfx (u64 n + u16s),
//                               fertility (u64 n + bytes)
//   u64 FNV-1a 64               of everything before it
//
// The sections are the same field lists the hash runs (state_fields.hpp), each
// into its own archive; the length in front lets a reader bound a section before
// it parses it and skip a section it does not know. The version check is exact:
// a save of version v is rejected by v + 1 with a clear message. Original
// savegames are not read (their layout cannot be validated without an x86
// Windows oracle).
#include <array>
#include <cassert>
#include <string>
#include <utility>
#include <vector>

#include <anno/sim/archive.hpp>
#include <anno/sim/create.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

namespace {

constexpr std::array<std::uint8_t, 4> kMagic = {'A', '3', 'S', 'V'};

/// Whether a generator runs on the original table (identified by size and
/// digest, as the stream identifies it).
bool original_table(const WorldRng& r) {
    const WorldRng original;
    return r.size() == original.size() && r.table_digest() == original.table_digest();
}

} // namespace

std::vector<std::uint8_t> StateIO::save(const World& w) {
    WriteArchive out;
    out.bytes(kMagic);
    out.u32(kSaveVersion);
    out.u32(kStateHashVersion);
    const bool original = original_table(w.rng);
    out.u8(static_cast<std::uint8_t>(original ? 0 : 1));
    if (!original) {
        out.count(w.rng.size());
        for (const std::uint16_t v : w.rng.table()) {
            out.u16(v);
        }
    }
    auto section = [&](auto&& write) {
        WriteArchive s;
        write(s);
        out.u64(s.data().size());
        out.bytes(s.data());
    };
    section([&](WriteArchive& s) { header_section(s, w, w.config_, w.rules_); });
    section([&](WriteArchive& s) { clock_rng_section(s, w.clock, w.rng); });
    section([&](WriteArchive& s) { settings_section(s, w.settings_); });
    section([&](WriteArchive& s) { players_section(s, w.players_, w.config_); });
    section([&](WriteArchive& s) { islands_section(s, w, w.islands_, w.config_, w.settings_); });
    for (const std::optional<Island>& is : w.islands_) {
        if (!is.has_value()) {
            continue;
        }
        out.boolean(is->terrain != nullptr);
        if (is->terrain != nullptr) {
            const Terrain& t = *is->terrain;
            // A world read from the flat stream holds a digest-only placeholder
            // (island.hpp, Terrain): it is for hashing only, and a frame written
            // from it would be refused by load. Debug builds catch the call.
            assert(terrain_matches(t) && "World::save needs the terrain columns");
            out.count(t.height.size());
            for (const std::uint8_t v : t.height) {
                out.u8(v);
            }
            out.count(t.gfx.size());
            for (const std::uint16_t v : t.gfx) {
                out.u16(v);
            }
            out.count(t.fertility.size());
            for (const std::uint8_t v : t.fertility) {
                out.u8(v);
            }
        }
    }
    StateHasher h;
    h.bytes(out.data());
    out.u64(h.value());
    return out.take();
}

formats::Result<World, std::string> StateIO::load(std::span<const std::uint8_t> bytes,
                                                  std::shared_ptr<const TypeRegistry> types,
                                                  Seams seams) {
    if (!types) {
        return std::string("save: the registry is null");
    }
    if (bytes.size() < kMagic.size() + 8) {
        return std::string("save: too short for a save frame");
    }
    // The checksum first: a flipped byte is rejected before any parser runs (a
    // matching checksum proves nothing about a hostile input, so every parser
    // below is bounds-checked all the same).
    const std::span<const std::uint8_t> body = bytes.first(bytes.size() - 8);
    StateHasher hasher;
    hasher.bytes(body);
    std::uint64_t stored = 0;
    {
        ReadArchive tail(bytes.last(8));
        tail.u64(stored);
    }
    if (stored != hasher.value()) {
        return std::string("save: the checksum does not match (the file is damaged)");
    }

    ReadArchive r(body);
    std::array<std::uint8_t, 4> magic{};
    r.bytes(magic);
    if (r.failed() || magic != kMagic) {
        return std::string("save: not a save (the magic is not A3SV)");
    }
    std::uint32_t save_version = 0;
    r.u32(save_version);
    if (!r.failed() && save_version != kSaveVersion) {
        return "save: version " + std::to_string(save_version) +
               " is not supported (this build reads " + std::to_string(kSaveVersion) + ")";
    }
    std::uint32_t stream_version = 0;
    r.u32(stream_version);
    if (!r.failed() && stream_version != kStateHashVersion) {
        return "save: state stream version " + std::to_string(stream_version) +
               " is not supported (this build reads " + std::to_string(kStateHashVersion) + ")";
    }
    std::uint8_t kind = 0;
    r.u8(kind);
    if (!r.failed() && kind > 1) {
        return "save: unknown PRNG table kind " + std::to_string(kind);
    }

    World w;
    w.types_ = RegistryRef(std::move(types));
    w.seams_ = std::move(seams);
    if (!r.failed() && kind == 1) {
        std::uint64_t n = 0;
        r.count(n, 2);
        std::vector<std::uint16_t> table;
        for (std::uint64_t i = 0; i < n && !r.failed(); ++i) {
            table.emplace_back();
            r.u16(table.back());
        }
        if (!r.failed()) {
            std::optional<WorldRng> rng = WorldRng::from_table(std::move(table), 0);
            if (!rng.has_value()) {
                return std::string("save: the PRNG table is empty");
            }
            w.rng = std::move(*rng);
        }
    }
    if (r.failed()) {
        return "save: " + r.error();
    }

    // The sections, in order; each is parsed by its own archive over exactly its
    // bytes and must be consumed whole.
    auto section = [&](const char* name, auto&& read) -> std::string {
        std::uint64_t len = 0;
        r.u64(len);
        if (!r.failed() && len > r.remaining()) {
            r.fail(std::string("section ") + name + " claims " + std::to_string(len) + " bytes, " +
                   std::to_string(r.remaining()) + " remain");
        }
        if (r.failed()) {
            return "save: " + r.error();
        }
        ReadArchive s(
            body.subspan(static_cast<std::size_t>(r.position()), static_cast<std::size_t>(len)));
        read(s);
        if (s.failed()) {
            // The "byte N" inside the message counts from the start of the section,
            // so the file offset of that start goes in front.
            return std::string("save: section ") + name + " (starts at file byte " +
                   std::to_string(r.position()) + "): " + s.error();
        }
        if (s.remaining() != 0) {
            return std::string("save: section ") + name + " has " + std::to_string(s.remaining()) +
                   " bytes after its last field";
        }
        r.skip(len);
        return {};
    };
    std::string problem =
        section("header", [&](ReadArchive& s) { header_section(s, w, w.config_, w.rules_); });
    if (problem.empty()) {
        problem =
            section("clock_rng", [&](ReadArchive& s) { clock_rng_section(s, w.clock, w.rng); });
    }
    if (problem.empty()) {
        problem = section("settings", [&](ReadArchive& s) { settings_section(s, w.settings_); });
    }
    if (problem.empty()) {
        problem =
            section("players", [&](ReadArchive& s) { players_section(s, w.players_, w.config_); });
    }
    if (problem.empty()) {
        problem = section("islands", [&](ReadArchive& s) {
            islands_section(s, w, w.islands_, w.config_, w.settings_);
        });
    }
    if (!problem.empty()) {
        return problem;
    }

    // The terrain blobs replace the placeholders the island scope left; each must
    // match the digest the stream carries.
    for (std::size_t i = 0; i < w.islands_.size(); ++i) {
        if (!w.islands_[i].has_value()) {
            continue;
        }
        Island& is = *w.islands_[i];
        const std::string where = "save: islands[" + std::to_string(i) + "].terrain";
        bool present = false;
        r.boolean(present);
        if (r.failed()) {
            return where + ": " + r.error();
        }
        if (present != (is.terrain != nullptr)) {
            return where + ": the blob is " + (present ? "present" : "absent") +
                   " but the island's terrain digest says otherwise";
        }
        if (!present) {
            continue;
        }
        auto t = std::make_shared<Terrain>();
        ar::seq(r, t->height, 1, [&](auto& v) { r.u8(v); });
        ar::seq(r, t->gfx, 2, [&](auto& v) { r.u16(v); });
        ar::seq(r, t->fertility, 1, [&](auto& v) { r.u8(v); });
        if (r.failed()) {
            return where + ": " + r.error();
        }
        t->digest = terrain_digest(*t);
        if (t->digest != is.terrain->digest) {
            return where + ": the blobs do not match the digest of the state stream";
        }
        is.terrain = std::move(t);
    }
    if (r.remaining() != 0) {
        return "save: " + std::to_string(r.remaining()) + " bytes after the last blob";
    }
    // The derived indexes are not in the save: rebuilt from the lists, then the
    // whole of check_invariants (structure, relations, derived equals recomputed)
    // decides whether the frame describes a world the scheduler can run.
    for (std::optional<Island>& is : w.islands_) {
        if (is.has_value()) {
            rebuild_island_index(*is, w.config_, *w.types_);
        }
    }
    problem = w.check_invariants();
    if (!problem.empty()) {
        return "save: " + problem;
    }
    return w;
}

std::vector<std::uint8_t> World::save() const {
    return StateIO::save(*this);
}

formats::Result<World, std::string> World::load(std::span<const std::uint8_t> bytes,
                                                std::shared_ptr<const TypeRegistry> types,
                                                Seams seams) {
    return StateIO::load(bytes, std::move(types), std::move(seams));
}

} // namespace anno::sim
