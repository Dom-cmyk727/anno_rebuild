#include <limits>
#include <utility>

#include <anno/sim_load/wildlife_first_call.hpp>

namespace anno::sim_load {
formats::Result<WildlifeFirstCallResult, WildlifeFirstCallFailure>
prepare_wildlife_first_call(formats::Bytes scene, WildlifeFirstCallSelection selection,
                            std::span<const formats::figuren::Entry> entries,
                            formats::Bytes annogame, const sim::TypeRegistry& types,
                            PrefixInputs inputs, std::span<const std::uint32_t> draws,
                            WildlifeFillRoles roles) {
    const auto source_failure = [](std::string reason) {
        return WildlifeFirstCallFailure{
            WildlifeFirstCallStage::Source, "wildlife first call: " + reason, {}};
    };
    const auto islands = formats::insel::islands(scene);
    if (!islands)
        return source_failure(islands.error());
    // Prefix-all/fill-all would change original inter-island ordering. §C1.
    if (islands.value().size() != 1)
        return source_failure("single fresh embedded island required");
    if (selection.ordinal != 0 || islands.value()[0].number != selection.slot)
        return source_failure("selected island identity mismatch");
    const auto& raw = islands.value()[0];
    if (raw.version != formats::insel::Version::Insel5)
        return source_failure("fresh embedded INSEL5 required");
    if (raw.width == 0 || raw.height == 0 ||
        std::int64_t{raw.x} + raw.width > std::numeric_limits<std::int32_t>::max() ||
        std::int64_t{raw.y} + raw.height > std::numeric_limits<std::int32_t>::max())
        return source_failure("positive representable full geometry required");
    // Welt 0x100a2545–0x100a2584 shifts BOTH edges by origin % 8.
    // Accept only a source already matching that factory rectangle; never patch.
    if (raw.x % 8 != 0 || raw.y % 8 != 0)
        return source_failure("source geometry requires original factory adjustment");
    auto prefix =
        prepare_wildlife_prefix(scene, entries, annogame, types, std::move(inputs), draws);
    if (!prefix)
        return WildlifeFirstCallFailure{WildlifeFirstCallStage::Prefix, prefix.error().reason,
                                        std::move(prefix.error().diagnostic)};
    // The accepted validator independently checks full completion, exact source,
    // HEIGHT/GFX and genuine final FRUCHTBAR. Missing ground is never repaired.
    auto prepared = prepare_wildlife_postload_fill(prefix.value(), selection.ordinal, scene,
                                                   entries, types, roles);
    if (!prepared)
        return WildlifeFirstCallFailure{
            WildlifeFirstCallStage::FillValidation, prepared.error(), {}};
    auto filled = apply_wildlife_postload_fill(std::move(prepared).value(),
                                               sim::TileRect{raw.x, raw.y, raw.width, raw.height});
    if (!filled)
        return WildlifeFirstCallFailure{WildlifeFirstCallStage::FillApply, filled.error(), {}};
    return WildlifeFirstCallResult{std::move(filled).value(),
                                   std::move(prefix.value().source_sha256), selection.ordinal,
                                   std::move(prefix.value().supplied)};
}
} // namespace anno::sim_load
