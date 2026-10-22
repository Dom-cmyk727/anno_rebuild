// Replay and session (spec: docs/sim/state.md §8.7, §15 S9). The on-disk frame is
// explicit little-endian through the project archives (bounds-checked, no
// reinterpret_cast of log bytes into structs); a truncated or hostile log is a clean
// Result error, never UB.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <anno/sim/archive.hpp>
#include <anno/sim/draw.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/replay.hpp>

namespace anno::sim {

namespace {

// The frame magic: four bytes, not a length-prefixed string (so a reader can tell a
// replay log from any other blob before trusting a single field).
constexpr std::array<std::uint8_t, 4> kMagic{'A', '3', 'R', 'P'};

// The smallest an entry can be on the wire: the kind byte plus the shortest body
// (a Commands entry with an empty blob is u8 + u64). A count larger than the input
// can hold at that size is rejected before the loop (ReadArchive::count).
enum : std::uint64_t { kMinEntryBytes = 9 };

std::uint8_t kind_byte(ReplayEntry::Kind k) {
    return static_cast<std::uint8_t>(k);
}

void apply_entry(World& w, const ReplayEntry& e) {
    switch (e.kind) {
    case ReplayEntry::Kind::Advance:
        w.advance(e.ms);
        break;
    case ReplayEntry::Kind::Commands:
        for (const Command& c : e.commands) {
            w.apply(c);
        }
        break;
    case ReplayEntry::Kind::Checkpoint:
        break;
    }
}

std::string segment(const HashNode& n) {
    std::string s(n.name);
    if (n.indexed) {
        s += '[' + std::to_string(n.index) + ']';
    }
    return s;
}

std::string diff_node(const HashNode& a, const HashNode& b, const std::string& prefix) {
    const std::string here = prefix.empty() ? segment(a) : prefix + "/" + segment(a);
    const std::size_t n = std::min(a.children.size(), b.children.size());
    for (std::size_t i = 0; i < n; ++i) {
        if (a.children[i].hash != b.children[i].hash) {
            return diff_node(a.children[i], b.children[i], here);
        }
    }
    // No recorded child at this depth differs (or the depth stops here): this node is
    // the finest difference the trees can name.
    return here;
}

} // namespace

std::uint64_t seams_digest(const Seams& seams) {
    StateHasher h;
    for (const std::string& name : seam_names(seams)) {
        h.u64(name.size());
        h.bytes(detail::bytes_of(name));
    }
    return h.value();
}

std::vector<std::string> prng_timeline_missing() {
    std::vector<std::string> missing;
    for (std::size_t i = 0; i < kDrawSiteCount; ++i) {
        const auto site = static_cast<DrawSite>(i);
        if (!draw_site_ported(site)) {
            missing.emplace_back(draw_site_name(site));
        }
    }
    return missing;
}

// ---- Recorder ----------------------------------------------------------------------

formats::Result<Recorder, std::string> Recorder::start(World& world) {
    // World::save's precondition (world.hpp): a digest-only terrain placeholder would
    // make the initial save one that World::load refuses. One FNV pass over each
    // island's terrain columns, less than the save itself costs.
    const std::vector<std::optional<Island>>& islands = world.islands();
    for (std::size_t i = 0; i < islands.size(); ++i) {
        if (islands[i].has_value() && islands[i]->terrain != nullptr &&
            !terrain_matches(*islands[i]->terrain)) {
            return std::string("cannot record: islands[" + std::to_string(i) +
                               "].terrain is a digest-only placeholder (a world from the "
                               "flat state stream is for hashing only, World::save)");
        }
    }
    return Recorder(world);
}

Recorder::Recorder(World& world) : world_(world) {
    log_.config_digest = world_.config().digest();
    log_.rules_digest = world_.rules().digest();
    log_.registry_digest = world_.types().digest();
    log_.seams_digest = seams_digest(world_.seams());
    log_.prng_timeline_version = kPrngTimelineVersion;
    log_.prng_timeline_missing = prng_timeline_missing();
    log_.initial_save = world_.save();
}

ApplyResult Recorder::submit(const Command& command) {
    // A command write_log could not write is refused before it is logged or applied:
    // otherwise the world would hold a change the written log does not (§8.7).
    if (!encode_record(command.body).ok()) {
        ApplyResult refused;
        refused.status = Reject::Unencodable;
        return refused;
    }
    ReplayEntry e;
    e.kind = ReplayEntry::Kind::Commands;
    e.commands.push_back(command);
    log_.entries.push_back(std::move(e));
    return world_.apply(command);
}

std::uint64_t Recorder::advance(std::uint32_t ms) {
    const std::uint64_t tick = world_.advance(ms);
    ReplayEntry e;
    e.kind = ReplayEntry::Kind::Advance;
    e.ms = ms;
    e.tick = tick;
    log_.entries.push_back(std::move(e));
    return tick;
}

void Recorder::checkpoint() {
    ReplayEntry e;
    e.kind = ReplayEntry::Kind::Checkpoint;
    e.tick = world_.clock().tick();
    e.hash = world_.state_hash();
    log_.entries.push_back(std::move(e));
}

// ---- session drivers ---------------------------------------------------------------

void run_turn(World& world, Recorder* rec, const Turn& turn, std::uint32_t steps) {
    for (const Command& c : turn.commands) {
        if (rec != nullptr) {
            rec->submit(c);
        } else {
            world.apply(c);
        }
    }
    for (std::uint32_t i = 0; i < steps; ++i) {
        if (rec != nullptr) {
            rec->advance(turn.step_ms);
        } else {
            world.advance(turn.step_ms);
        }
    }
}

void run_frame(World& world, Recorder* rec, std::span<const Command> commands,
               std::uint32_t step_ms) {
    for (const Command& c : commands) {
        if (rec != nullptr) {
            rec->submit(c);
        } else {
            world.apply(c);
        }
    }
    if (rec != nullptr) {
        rec->advance(step_ms);
    } else {
        world.advance(step_ms);
    }
}

// ---- write / read ------------------------------------------------------------------

formats::Result<std::vector<std::uint8_t>, std::string> write_log(const ReplayLog& log) {
    WriteArchive w;
    w.bytes(kMagic);
    w.u32(kReplayLogVersion);
    w.u32(kCommandWireVersion); // once, not per record (§8.5)
    w.u64(log.config_digest);
    w.u64(log.rules_digest);
    w.u64(log.registry_digest);
    w.u64(log.seams_digest);
    w.u32(log.prng_timeline_version);
    w.count(log.prng_timeline_missing.size(), 8);
    for (const std::string& name : log.prng_timeline_missing) {
        w.str(name);
    }
    w.u64(log.initial_save.size());
    w.bytes(log.initial_save);
    w.count(log.entries.size(), kMinEntryBytes);
    for (std::size_t i = 0; i < log.entries.size(); ++i) {
        const ReplayEntry& e = log.entries[i];
        w.u8(kind_byte(e.kind));
        switch (e.kind) {
        case ReplayEntry::Kind::Advance:
            w.u32(e.ms);
            w.u64(e.tick);
            break;
        case ReplayEntry::Kind::Commands: {
            // The commands as a back-to-back TurnEntry stream, framed by record_size
            // on read (decode_turn). seq is the command's ordinal within the entry.
            std::vector<std::uint8_t> blob;
            std::uint32_t seq = 0;
            for (const Command& c : e.commands) {
                auto rec = encode_turn_entry(TurnEntry{c, seq});
                if (!rec.ok()) {
                    // Never drop a command: the log would no longer be what happened.
                    return std::string("entry " + std::to_string(i) + ", command " +
                                       std::to_string(seq) + ": " + rec.error());
                }
                blob.insert(blob.end(), rec.value().begin(), rec.value().end());
                ++seq;
            }
            w.u64(blob.size());
            w.bytes(blob);
            break;
        }
        case ReplayEntry::Kind::Checkpoint:
            w.u64(e.tick);
            w.u64(e.hash);
            break;
        }
    }
    return w.take();
}

formats::Result<ReplayLog, std::string> read_log(std::span<const std::uint8_t> bytes) {
    ReadArchive r(bytes);
    std::array<std::uint8_t, 4> magic{};
    r.bytes(magic);
    if (!r.failed() && magic != kMagic) {
        r.fail("not a replay log (bad magic)");
    }
    std::uint32_t version = 0;
    r.u32(version);
    if (!r.failed() && version != kReplayLogVersion) {
        r.fail("replay-log version " + std::to_string(version) + " is not " +
               std::to_string(kReplayLogVersion));
    }
    std::uint32_t wire = 0;
    r.u32(wire);
    if (!r.failed() && wire != kCommandWireVersion) {
        r.fail("command wire version " + std::to_string(wire) + " is not " +
               std::to_string(kCommandWireVersion));
    }

    ReplayLog log;
    r.u64(log.config_digest);
    r.u64(log.rules_digest);
    r.u64(log.registry_digest);
    r.u64(log.seams_digest);
    r.u32(log.prng_timeline_version);
    {
        std::uint64_t n = 0;
        r.count(n, 8);
        for (std::uint64_t i = 0; i < n && !r.failed(); ++i) {
            std::string name;
            r.str(name);
            log.prng_timeline_missing.push_back(std::move(name));
        }
    }
    {
        std::uint64_t save_len = 0;
        r.u64(save_len);
        if (!r.failed() && save_len > r.remaining()) {
            r.fail("initial save length beyond the input");
        }
        if (!r.failed()) {
            const auto from = static_cast<std::size_t>(r.position());
            log.initial_save.assign(bytes.begin() + static_cast<std::ptrdiff_t>(from),
                                    bytes.begin() + static_cast<std::ptrdiff_t>(from + save_len));
            r.skip(save_len);
        }
    }

    std::uint64_t entry_count = 0;
    r.count(entry_count, kMinEntryBytes);
    for (std::uint64_t i = 0; i < entry_count && !r.failed(); ++i) {
        ReplayEntry e;
        std::uint8_t kind = 0;
        r.u8(kind);
        if (!r.failed() && kind > kind_byte(ReplayEntry::Kind::Checkpoint)) {
            r.fail("unknown replay-entry kind " + std::to_string(kind));
            break;
        }
        e.kind = static_cast<ReplayEntry::Kind>(kind);
        switch (e.kind) {
        case ReplayEntry::Kind::Advance:
            r.u32(e.ms);
            r.u64(e.tick);
            break;
        case ReplayEntry::Kind::Commands: {
            std::uint64_t blob_len = 0;
            r.u64(blob_len);
            if (!r.failed() && blob_len > r.remaining()) {
                r.fail("command block length beyond the input");
            }
            if (!r.failed()) {
                const auto from = static_cast<std::size_t>(r.position());
                const std::span<const std::uint8_t> blob =
                    bytes.subspan(from, static_cast<std::size_t>(blob_len));
                std::vector<TurnEntry> turn;
                auto decoded = decode_turn(blob, turn);
                if (!decoded.ok()) {
                    r.fail("command block: " + decoded.error());
                } else {
                    for (TurnEntry& t : turn) {
                        e.commands.push_back(std::move(t.command));
                    }
                    r.skip(blob_len);
                }
            }
            break;
        }
        case ReplayEntry::Kind::Checkpoint:
            r.u64(e.tick);
            r.u64(e.hash);
            break;
        }
        if (!r.failed()) {
            log.entries.push_back(std::move(e));
        }
    }

    if (r.failed()) {
        return r.error();
    }
    if (r.remaining() != 0) {
        return std::string("trailing bytes after the replay log (byte " +
                           std::to_string(r.position()) + ")");
    }
    return log;
}

// ---- replay / divergence -----------------------------------------------------------

formats::Result<ReplayOutcome, std::string> replay(World& world, const ReplayLog& log) {
    if (log.prng_timeline_version != kPrngTimelineVersion) {
        return std::string("replay refused: PRNG timeline version " +
                           std::to_string(log.prng_timeline_version) + " is not this build's " +
                           std::to_string(kPrngTimelineVersion));
    }
    if (log.prng_timeline_missing != prng_timeline_missing()) {
        // The same version with another set of unported draw sites: a site was ported
        // (or dropped) without a version bump, so the draws would silently differ.
        return std::string("replay refused: the PRNG timeline's missing draw sites differ "
                           "from this build's");
    }
    if (log.config_digest != world.config().digest()) {
        return std::string("replay refused: configuration digest differs from the log's");
    }
    if (log.rules_digest != world.rules().digest()) {
        return std::string("replay refused: rules digest differs from the log's");
    }
    if (log.registry_digest != world.types().digest()) {
        return std::string("replay refused: type registry digest differs from the log's");
    }
    if (log.seams_digest != seams_digest(world.seams())) {
        return std::string("replay refused: seams digest differs from the log's");
    }

    ReplayOutcome out;
    for (std::size_t i = 0; i < log.entries.size(); ++i) {
        const ReplayEntry& e = log.entries[i];
        if (e.kind == ReplayEntry::Kind::Checkpoint) {
            ++out.checkpoints_total;
            const std::uint64_t h = world.state_hash();
            ++out.entries_applied;
            if (h == e.hash) {
                ++out.checkpoints_matched;
            } else {
                out.diverged_entry = i;
                out.expected_hash = e.hash;
                out.actual_hash = h;
                out.final_tick = world.clock().tick();
                return out;
            }
        } else {
            apply_entry(world, e);
            ++out.entries_applied;
        }
    }
    out.final_tick = world.clock().tick();
    return out;
}

std::vector<std::uint64_t> hash_per_entry(World initial, const ReplayLog& log) {
    std::vector<std::uint64_t> hashes;
    hashes.reserve(log.entries.size());
    for (const ReplayEntry& e : log.entries) {
        apply_entry(initial, e);
        hashes.push_back(initial.state_hash());
    }
    return hashes;
}

std::optional<std::size_t> first_divergent_entry(std::span<const std::uint64_t> a,
                                                 std::span<const std::uint64_t> b) {
    const std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            return i;
        }
    }
    if (a.size() != b.size()) {
        return n;
    }
    return std::nullopt;
}

HashTree tree_at(World initial, const ReplayLog& log, std::size_t entry, std::uint32_t depth) {
    for (std::size_t i = 0; i < log.entries.size() && i <= entry; ++i) {
        apply_entry(initial, log.entries[i]);
    }
    HashTree tree;
    static_cast<void>(initial.state_hash(tree, depth));
    return tree;
}

std::string diff_trees(const HashTree& a, const HashTree& b) {
    if (a.root.hash == b.root.hash) {
        return {};
    }
    return diff_node(a.root, b.root, {});
}

} // namespace anno::sim
