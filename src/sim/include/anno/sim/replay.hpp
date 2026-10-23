// Replay and session: the recorder that is the one door a session drives a World
// through, the on-disk replay log, the deterministic re-run and the divergence
// finder (spec: docs/sim/state.md §8.7, §15 S9).
//
// Every command a Recorder applies and every advance it makes becomes a ReplayEntry
// (a command is logged, then applied; an advance is made, then logged with the tick
// it reached), with a checkpoint entry holding the state hash wherever the session
// asks; a command the log cannot hold is refused before it is applied. write_log /
// read_log move the log through an explicit little-endian frame (bounds-checked, no
// reinterpret_cast; a truncated or hostile log, or a log holding a command without a
// wire form, is a clean Result error, never UB). replay() re-runs a log against a world
// built with the matching registry and seams and stops at the first checkpoint whose
// hash differs; hash_per_entry / first_divergent_entry / tree_at / diff_trees narrow
// a two-build disagreement to one scope path. PrintArchive dumps a scope's fields as
// text through the one field list of state_fields.hpp.
//
// The replay log is a session artifact, not world state: nothing here is hashed or
// saved, and S9 adds no checksummed field (kStateHashVersion stays 105).
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/sim/archive.hpp>
#include <anno/sim/command.hpp>
#include <anno/sim/hash_tree.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

/// The version of the replay-log frame (a format version, like kCommandWireVersion
/// and kSaveVersion; a reader refuses a log written with another one).
inline constexpr std::uint32_t kReplayLogVersion = 1;

/// One recorded step of a session: an advance by `ms`, a block of commands applied
/// in stream order, or a checkpoint carrying the tick and the state hash at that
/// point. The fields a kind uses: Advance {ms, tick = the new tick}; Commands
/// {commands}; Checkpoint {tick, hash}.
struct ReplayEntry {
    enum class Kind : std::uint8_t { Advance, Commands, Checkpoint };
    Kind kind = Kind::Advance;
    std::uint32_t ms = 0;
    std::vector<Command> commands;
    std::uint64_t tick = 0;
    std::uint64_t hash = 0;
    friend bool operator==(const ReplayEntry&, const ReplayEntry&) = default;
};

/// A recorded session: the digests that pin the world it was recorded against (a
/// replay refuses a world whose digests differ), the PRNG timeline (§6: the version
/// and the names of the original draw sites the sim does not reproduce yet, a state
/// input recorded so porting a site is never a silent desync), the save of the world
/// before entry 0, and the entries in order.
struct ReplayLog {
    std::uint64_t config_digest = 0;
    std::uint64_t rules_digest = 0;
    std::uint64_t registry_digest = 0;
    std::uint64_t seams_digest = 0;
    std::uint32_t prng_timeline_version = 0;
    std::vector<std::string> prng_timeline_missing;
    std::vector<std::uint8_t> initial_save;
    std::vector<ReplayEntry> entries;
    friend bool operator==(const ReplayLog&, const ReplayLog&) = default;
};

/// The FNV-1a 64 digest of a seam set's names, in the order seam_names() lists them
/// (the names that validate the log against a world, §8.2 item 4).
[[nodiscard]] std::uint64_t seams_digest(const Seams& seams);

/// The names of the original draw sites the sim does not reproduce yet, in DrawSite
/// order (docs/sim/state.md §6). Recorded in the replay-log header.
[[nodiscard]] std::vector<std::string> prng_timeline_missing();

/// The one door a session drives a World through: every command it applies and every
/// advance it makes becomes an entry of its log, so the log is the ground truth of what
/// happened (docs/sim/state.md §8.7). It captures the world's digests, the PRNG
/// timeline and the world's save when it starts (the state before entry 0).
///
/// Precondition, checked by start(): the world can be saved (World::save), i.e. every
/// island's terrain satisfies terrain_matches(). A world filled from the flat state
/// stream alone holds digest-only terrain placeholders; it is for hashing only and
/// cannot be recorded, because the log's initial save would be refused by World::load.
class Recorder {
  public:
    /// A recorder of `world`, or an error naming the first island whose terrain is a
    /// digest-only placeholder (the precondition above). The world must outlive the
    /// recorder.
    [[nodiscard]] static formats::Result<Recorder, std::string> start(World& world);

    /// Logs the command, then applies it; returns World::apply's result. A command
    /// that has no wire form (has_wire_form: encode_record refuses it, so write_log
    /// could not write it) is neither logged nor applied: the result is Reject::Unencodable and the
    /// world and the log are unchanged.
    ApplyResult submit(const Command& command);
    /// Advances the world by `ms`, then logs an Advance entry with the tick it reached;
    /// returns the new tick.
    std::uint64_t advance(std::uint32_t ms);
    /// Logs a checkpoint with the current tick and state hash.
    void checkpoint();

    [[nodiscard]] const ReplayLog& log() const { return log_; }

  private:
    explicit Recorder(World& world);

    World& world_;
    ReplayLog log_;
};

/// A multiplayer turn (docs/sim/state.md §8.7): the commands applied in stream order
/// before the turn's steps.
struct Turn {
    std::uint64_t index = 0;
    std::uint32_t step_ms = 0;
    std::vector<Command> commands;
};

/// MP: apply the turn's commands in stream order, then `steps` advances of
/// turn.step_ms. SP: apply the commands, then one advance of step_ms. When `rec` is
/// non-null the session is driven through it: each command goes through
/// Recorder::submit and becomes one Commands entry of its own (not one block per turn),
/// each advance one Advance entry. Otherwise the World is driven directly, with the
/// recorder's rule for commands: one without a wire form (has_wire_form, e.g. a Build of
/// more placements than one record holds) is skipped, not applied, so a recorded and an
/// unrecorded session of the same commands reach the same state (S10).
void run_turn(World& world, Recorder* rec, const Turn& turn, std::uint32_t steps);
void run_frame(World& world, Recorder* rec, std::span<const Command> commands,
               std::uint32_t step_ms);

/// The log as bytes, or an error (naming the entry) when a Commands entry holds a
/// command that has no wire form (encode_record refuses it). A log made by a Recorder
/// never does (Recorder::submit refuses such a command); a hand-built one may, and is
/// refused rather than written without the command.
[[nodiscard]] formats::Result<std::vector<std::uint8_t>, std::string>
write_log(const ReplayLog& log);
[[nodiscard]] formats::Result<ReplayLog, std::string> read_log(std::span<const std::uint8_t> bytes);

/// The result of replaying a log against a world.
struct ReplayOutcome {
    std::uint64_t entries_applied = 0;
    std::uint64_t checkpoints_total = 0;
    std::uint64_t checkpoints_matched = 0;
    /// The index of the first checkpoint entry whose hash differed, if any (u64, not
    /// size_t: the sim keeps fixed widths in state-shaped structs).
    std::optional<std::uint64_t> diverged_entry;
    std::uint64_t expected_hash = 0;
    std::uint64_t actual_hash = 0;
    std::uint64_t final_tick = 0;
    /// Every checkpoint reproduced (no divergence).
    [[nodiscard]] bool reproduced() const { return !diverged_entry.has_value(); }
};

/// Re-runs `log` against `world`, which must be at the log's initial state and built
/// with a matching registry and seams: an error when the world's digests or PRNG
/// timeline (its version and the list of missing draw sites) differ from the log's (a
/// replay of someone else's build, refused before anything runs, §8.2 item 4). Otherwise it applies
/// the entries in order and stops at the first checkpoint whose hash differs, reporting it in the
/// outcome.
[[nodiscard]] formats::Result<ReplayOutcome, std::string> replay(World& world,
                                                                 const ReplayLog& log);

/// The world state hash after each entry of `log` applied in order to `initial`
/// (8 bytes per entry; what the `hashes` tool prints). `initial` is taken by value.
[[nodiscard]] std::vector<std::uint64_t> hash_per_entry(World initial, const ReplayLog& log);

/// The index of the first entry at which the two hash lists differ (or the length of
/// the shorter one when it is a prefix of the longer), or nullopt when they match.
[[nodiscard]] std::optional<std::size_t> first_divergent_entry(std::span<const std::uint64_t> a,
                                                               std::span<const std::uint64_t> b);

/// The hash tree (scopes down to `depth`) after entries 0..entry (inclusive) of
/// `log` applied to `initial`; `entry` past the end applies the whole log.
[[nodiscard]] HashTree tree_at(World initial, const ReplayLog& log, std::size_t entry,
                               std::uint32_t depth);

/// The path of the first node at which the two trees differ (the root is assumed to
/// differ; the walk descends into the first differing child of each level), e.g.
/// "world/islands/island[0]/cities/city[1]/index/list[4]/building[3]". Empty when
/// the roots are equal.
[[nodiscard]] std::string diff_trees(const HashTree& a, const HashTree& b);

/// A text dump of a scope's fields through the one field list of state_fields.hpp
/// (`fields(a, w)`): one primitive per line, indented by scope depth, so a scope the
/// tree diff named can be read field by field. With a `filter` path only that scope
/// and its descendants are printed (its ancestors appear as headers for context);
/// empty prints the whole stream. The filter takes a diff_trees path verbatim: its
/// leading "world" is the hash tree's name for the root, not a scope of the field list,
/// and is stripped ("world" alone prints the whole stream). A write-side archive: it
/// never changes the world.
class PrintArchive : public detail::Writer<PrintArchive> {
  public:
    PrintArchive() = default;
    explicit PrintArchive(std::string filter) : filter_(strip_root(std::move(filter))) {}

    void emit(Prim kind, std::uint64_t value, std::uint32_t width) {
        if (!primitive_visible()) {
            return;
        }
        add_line(stack_.size(), std::string(prim_name(kind)) + " = " + std::to_string(value) +
                                    " (" + std::to_string(width) + " B)");
    }
    void emit_bytes(Prim kind, std::span<const std::uint8_t> b) {
        if (!primitive_visible()) {
            return;
        }
        add_line(stack_.size(),
                 std::string(prim_name(kind)) + " = " + std::to_string(b.size()) + " bytes");
    }
    template <class F> void scope(ScopeName name, std::uint32_t index, F&& f) {
        enter(name.view, index, true);
        f();
        leave();
    }
    template <class F> void scope(ScopeName name, F&& f) {
        enter(name.view, 0, false);
        f();
        leave();
    }

    [[nodiscard]] const std::string& text() const { return out_; }

  private:
    struct Segment {
        std::string_view name;
        std::uint32_t index = 0;
        bool indexed = false;
    };

    static std::string_view prim_name(Prim kind) {
        switch (kind) {
        case Prim::U8:
            return "u8";
        case Prim::U16:
            return "u16";
        case Prim::U32:
            return "u32";
        case Prim::U64:
            return "u64";
        case Prim::I32:
            return "i32";
        case Prim::I64:
            return "i64";
        case Prim::Bool:
            return "bool";
        case Prim::Count:
            return "count";
        case Prim::Enum:
            return "enum";
        case Prim::StrLen:
            return "strlen";
        case Prim::StrBytes:
            return "str";
        case Prim::Bytes:
            return "bytes";
        case Prim::ScopeOpen:
        case Prim::ScopeClose:
            break;
        }
        return "?";
    }

    [[nodiscard]] std::string path() const {
        std::string p;
        for (const Segment& s : stack_) {
            if (!p.empty()) {
                p += '/';
            }
            p += std::string(s.name);
            if (s.indexed) {
                p += '[' + std::to_string(s.index) + ']';
            }
        }
        return p;
    }

    /// A diff_trees path names the root "world" (HashArchive::finish), which the field
    /// list never opens as a scope: drop it so the path matches the scopes seen here.
    static std::string strip_root(std::string filter) {
        constexpr std::string_view root = "world";
        constexpr std::string_view root_slash = "world/";
        if (filter == root) {
            return {};
        }
        if (filter.rfind(root_slash, 0) == 0) {
            return filter.substr(root_slash.size());
        }
        return filter;
    }

    /// True when `inside` is `of` itself or a descendant scope of it.
    static bool is_within(const std::string& inside, const std::string& of) {
        return of.empty() || inside == of || inside.rfind(of + "/", 0) == 0;
    }
    [[nodiscard]] bool primitive_visible() const { return is_within(path(), filter_); }

    void enter(std::string_view name, std::uint32_t index, bool indexed) {
        stack_.push_back(Segment{name, index, indexed});
        const std::string p = path();
        // Show the scope header when it is the filter, inside it, or an ancestor of
        // it (so the chain down to the filtered scope stays readable).
        if (filter_.empty() || is_within(p, filter_) || is_within(filter_, p)) {
            std::string line = std::string(name);
            if (indexed) {
                line += '[' + std::to_string(index) + ']';
            }
            add_line(stack_.size() - 1, line + " {");
        }
    }
    void leave() { stack_.pop_back(); }

    void add_line(std::size_t depth, const std::string& body) {
        for (std::size_t i = 0; i < depth; ++i) {
            out_ += "  ";
        }
        out_ += body;
        out_ += '\n';
    }

    std::vector<Segment> stack_;
    std::string filter_;
    std::string out_;
};

} // namespace anno::sim
