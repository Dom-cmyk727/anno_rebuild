// Source-built first-call projection only; docs/sim/wildlife-first-call.md.
#pragma once

#include <anno/sim_load/wildlife_postload_fill.hpp>

namespace anno::sim_load {
struct WildlifeFirstCallSelection {
    std::uint64_t ordinal = 0;
    std::uint32_t slot = 0;
};
enum class WildlifeFirstCallStage { Source, Prefix, FillValidation, FillApply };
struct WildlifeFirstCallFailure {
    WildlifeFirstCallStage stage;
    std::string reason;
    // Existing prefix diagnostic only: never successful output or committed draws.
    std::optional<PreparedPrefix> diagnostic;
};
struct WildlifeFirstCallResult {
    WildlifeFillResult fill;
    std::string source_sha256;
    std::uint64_t ordinal = 0;
    PrefixInputs supplied;
};
// Source/catalogue buffers need live only during this synchronous transaction.
// Single fresh embedded island is a bounded adapter profile, not an engine cap.
// Supplied local draws/stamps/mode are not reconstructed whole-world history.
[[nodiscard]] formats::Result<WildlifeFirstCallResult, WildlifeFirstCallFailure>
prepare_wildlife_first_call(formats::Bytes scene, WildlifeFirstCallSelection selection,
                            std::span<const formats::figuren::Entry> entries,
                            formats::Bytes annogame, const sim::TypeRegistry&, PrefixInputs,
                            std::span<const std::uint32_t> draws, WildlifeFillRoles = {});
} // namespace anno::sim_load
