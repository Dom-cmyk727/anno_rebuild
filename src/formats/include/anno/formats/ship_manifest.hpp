// The ship-start manifest, `map2-ship-manifest` version 2, profile
// `map2-ship-start-v2` (our own JSON, not an original format).
// Compatibility reference: tools/map2_manifest.py (`parse`). Contract:
// docs/sim/ship-start-integration.md. A manifest is schema-valid or it is not;
// whether a loader supports its loadout is a separate question (baseline_ok).
//
// Where this reader is deliberately STRICTER than the Python reference (the
// reference accepts these, this reader rejects them; fixtures/synthetic/
// map2_manifest/cases.txt lists each case as `reference=ok engine=reject`):
//   - every integer must fit int64 (Python integers are unbounded);
//   - an island id below 0 is rejected (the reference starts its ordering check at
//     -1 and so accepts the id -1 as the first island);
//   - a lone surrogate \u escape is rejected (Python keeps it in the string);
//   - nesting deeper than json::Limits::max_depth (64) is rejected;
//   - the bytes must be ASCII, as the reference's command line demands
//     (`PathText`); the reference's `parse(text)` itself takes any str.
// Schema validity is not loadability. `base` and the island `key`s are any non-empty string here,
// exactly as in the reference (a NUL, a rooted path or ".." is schema-valid); what may be opened
// is decided where the file system is touched, by sim_load's resolve_content_path, so this
// reader and the Python tool keep one contract.
// Everything else the reference rejects, this reader rejects, and a manifest both
// accept gets the same content hash.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/result.hpp>

namespace anno::formats::ship_manifest {

inline constexpr const char* kManifestName = "map2-ship-manifest";
inline constexpr std::int64_t kManifestVersion = 2;
inline constexpr const char* kProfileId = "map2-ship-start-v2";
/// 1 tile = 1000 milli-tiles.
inline constexpr std::int64_t kMilli = 1000;
/// The version-2 contract: five seats. A property of this version, not of the engine.
inline constexpr std::size_t kSeatCount = 5;

struct Error {
    std::string message;
};

struct Island {
    std::int64_t id = 0;
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::string key;
};

struct Spawn {
    std::int64_t seat = 0;
    /// Milli-tiles.
    std::int64_t x = 0;
    std::int64_t y = 0;
    /// 0..3.
    std::int64_t facing = 0;
};

struct Hold {
    std::string goods;
    std::int64_t tons = 0;
    friend bool operator==(const Hold&, const Hold&) = default;
};

struct Loadout {
    std::string status;
    std::string source;
    std::int64_t gold = 0;
    std::int64_t ship_figure = 0;
    std::int64_t scout_figure = 0;
    std::int64_t donkey_figure = 0;
    std::vector<Hold> holds;
    friend bool operator==(const Loadout&, const Loadout&) = default;
};

struct Policy {
    std::string grid, metric, clearance_scope, obstruction;
    std::int64_t clearance = 0, max_route_tiles = 0, min_city = 0, share_gap = 0,
                 share_min_side = 0;
};

struct Manifest {
    std::string name;
    std::int64_t edge_w = 0;
    std::int64_t edge_h = 0;
    std::string base;
    std::int64_t seed = 0;
    std::string generator_tool;
    std::string generator_config;
    std::vector<Island> islands;
    /// Seat order 0..4.
    std::vector<Spawn> spawns;
    Loadout loadout;
    Policy policy;
    /// SHA-256 of canonical_bytes(), lower-case hex. Always computed.
    std::string content_hash;
    /// The file's `content_hash` when it had one (absent and null are the same).
    std::optional<std::string> stated_hash;
};

/// Parses, validates and hashes. The reference's content_hash rule: absent (or
/// null) is accepted; anything else must equal the computed hash exactly.
[[nodiscard]] Result<Manifest, Error> parse(Bytes text);

/// The bytes that are hashed: the manifest without `content_hash`, rebuilt from the
/// parsed fields, as the reference does (canonical JSON, ASCII).
[[nodiscard]] std::string canonical_bytes(const Manifest& m);

/// The loadout this unit's loader supports: the proposed baseline of
/// docs/research/endless-start-conditions.md (status `proposed-baseline`, gold
/// 30000, figures 1502 / 717 / 718, holds wood 50, food 10, tools 50, tools 30).
[[nodiscard]] const Loadout& baseline_loadout();
[[nodiscard]] bool baseline_ok(const Manifest& m);

} // namespace anno::formats::ship_manifest
