// Loading the ship-start opening for the play window (W4-OPENING-1; docs/app/ship-opening.md).
// A typed manifest goes through sim_load::load_ship_start: no cities, no developer start, no
// developer resources, the original loadout, peace and shared information. The result is a
// PlayLoad (the world and the islands the scene is drawn from) plus the controls' setup and the
// identity of what was loaded, which the session record and the save carry.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <anno/app/opening_ui.hpp>
#include <anno/app/play.hpp>
#include <anno/formats/result.hpp>

namespace anno::app {

struct OpeningIdentity {
    /// The manifest file as launched, its content hash (formats::ship_manifest) and name.
    std::string manifest_path, manifest_name, manifest_hash;
    /// The local participant's seat (a player slot of the loaded world).
    std::uint32_t seat = 0;
    /// The rules digest and the registry digest of the loaded world (sim::Rules::digest, the
    /// registry's), as lower-case 16-digit hex: what a save and a recording must match.
    std::string rules_digest, registry_digest;
    /// The hash of the world as the session starts (state_hash), 16 hex digits.
    std::string start_hash;
    /// Set for a resumed session: the save it continued and its state hash.
    std::string resumed_from, resumed_hash;
};

struct OpeningLoad {
    PlayLoad play;
    OpeningSetup setup;
    OpeningIdentity identity;
    /// The tile the camera opens on: the local participant's ship.
    double camera_x = 0, camera_y = 0;
    /// The figure to select at launch is none: the participant selects it.
    std::vector<std::uint8_t> manifest_bytes;
};

/// Reads `manifest_path` (a regular file) and builds the world for `seat`. With `resume`, the world
/// is the save's instead of the loader's (identity checks: manifest hash, seat, rules and registry
/// digests, hash of the saved bytes); the islands of the scene still come from the manifest. An
/// error names the file and the reason; nothing is started on one.
[[nodiscard]] formats::Result<OpeningLoad, std::string>
load_opening(const std::filesystem::path& game_dir, const std::filesystem::path& manifest_path,
             std::uint32_t seat, const std::optional<std::filesystem::path>& resume = std::nullopt);

/// The controls' setup from a loaded world (islands' rectangles, the founding types, the cargo
/// rules); the registry and the local player are given.
[[nodiscard]] OpeningSetup make_opening_setup(const sim::World& world,
                                              std::shared_ptr<const sim::TypeRegistry> registry,
                                              sim::PlayerId local);

/// What Founding::apply would answer to `f` from `issuer` on `world`, without applying it: the same
/// checks in the same order up to the rotation trials (the ship, the type, the centred origin, the
/// island, its lock, the first footprint, the reach, the origin's owner, an own city on the island,
/// then each rotation's bounds, ownership, occupancy and coast rule). Not checked: the player's
/// kind, the ship's state word, the founding-type list and the figure veto after the trials. The
/// founding preview asks this on the sim thread; a test compares it with apply on the real start.
[[nodiscard]] FoundingProbeView probe_founding(const sim::World& world, sim::PlayerId issuer,
                                               const sim::FoundCmd& f);

/// The (figure kind, owner) pairs of the world's sailing ships, ascending: the ship sprites the
/// window prepares.
[[nodiscard]] std::vector<std::pair<std::uint32_t, std::uint32_t>>
opening_ship_kinds(const sim::World& world);

/// A session save (docs/app/ship-opening.md section 5): the world's save bytes in a frame that
/// names what they belong to. Text header lines, a blank line, then the bytes.
struct OpeningSave {
    OpeningIdentity identity;
    std::uint64_t turn = 0, tick = 0;
    std::uint64_t state_hash = 0;
    std::vector<std::uint8_t> world;
};

[[nodiscard]] std::vector<std::uint8_t> format_opening_save(const OpeningSave& save);
[[nodiscard]] formats::Result<OpeningSave, std::string>
parse_opening_save(const std::vector<std::uint8_t>& bytes);

/// The lines the session record carries about the opening (session.txt): manifest, hash, seat,
/// digests, start hash and, for a resumed segment, the save.
[[nodiscard]] std::string format_opening_identity(const OpeningIdentity& id);
/// The inverse for a replay: the manifest path, seat and the optional save, from the lines.
struct OpeningInvocation {
    std::string manifest_path;
    std::uint32_t seat = 0;
    std::string manifest_hash, rules_digest, start_hash, resumed_from;
};
[[nodiscard]] std::optional<OpeningInvocation> parse_opening_identity(const std::string& text);

} // namespace anno::app
