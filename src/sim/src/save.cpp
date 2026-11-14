// The save frame (docs/sim/state.md §9.5, stage S5): World::save and World::load.
//
//   "A3SV"                      4 bytes
//   u32 save version            kSaveVersion
//   u32 stream version          kStateHashVersion
//   u64 script schema           kScriptSchema
//   u8  PRNG table kind         0 = the original msvc_rand_sequence(2048, 1);
//                               1 = custom: u64 n, n x u16
//   10 sections                  u64 byte length, then the section's bytes: the
//                               top-level scopes of the stream, in order (header,
//                               clock_rng, settings, ids, players, islands, figures,
//                               events, timers_monopoly, script)
//   terrain blobs               per present island, in slot order: bool present,
//                               then height (u64 n + bytes), gfx (u64 n + u16s),
//                               fertility (u64 n + bytes)
//   u64 FNV-1a 64               of everything before it
//
// The sections are the same field lists the hash runs (state_fields.hpp), each
// into its own archive; the length in front lets a reader bound a section before
// it parses it. Unknown section versions/extensions are refused (§9.7). The version check is exact:
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
#include <anno/sim/freight.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

std::vector<std::uint8_t> StateIO::save(const World& w) {
    return save_with_extensions(w, w.extensions_);
}
formats::Result<World, std::string> StateIO::load(std::span<const std::uint8_t> bytes,
                                                  std::shared_ptr<const TypeRegistry> types,
                                                  Seams seams) {
    ExtensionContext extensions;
    auto result = load_with_extensions(bytes, std::move(types), std::move(seams), extensions);
    if (result.ok())
        result.value().extensions_ = std::move(extensions);
    return result;
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
