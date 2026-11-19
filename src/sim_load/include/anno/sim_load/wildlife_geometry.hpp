// Conditional ordinary source geometry; docs/sim/wildlife-ordinary-project.md.
#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <anno/sim_load/wildlife_import_plan.hpp>

namespace anno::sim_load {
enum class WildlifeRounding { Nearest, Down, Up, TowardZero };
struct WildlifeFpProfile {
    std::uint32_t precision;
    WildlifeRounding rounding;
    bool operator==(const WildlifeFpProfile&) const = default;
};
enum class WildlifeGeometryStage { Context, Numeric, Position, Height, Source, Selection };
struct WildlifeGeometryError {
    WildlifeGeometryStage stage;
    std::string reason;
};
struct WildlifeNumericInputs {
    std::optional<WildlifeFpProfile> table, callback;
    std::string provenance;
};
class WildlifeNumericContext {
  public:
    const auto& table() const { return table_; }
    auto table_profile() const { return table_profile_; }
    auto callback_profile() const { return callback_profile_; }
    const auto& provenance() const { return provenance_; }

  private:
    std::array<std::uint32_t, 256> table_; // Native unsigned corner byte domain, not engine limit.
    WildlifeFpProfile table_profile_, callback_profile_;
    std::string provenance_;
    WildlifeNumericContext(std::array<std::uint32_t, 256> table, WildlifeFpProfile t,
                           WildlifeFpProfile c, std::string provenance)
        : table_(table), table_profile_(t), callback_profile_(c),
          provenance_(std::move(provenance)) {}
    friend formats::Result<WildlifeNumericContext, WildlifeGeometryError>
    prepare_wildlife_numeric_context(const WildlifeNumericInputs&);
};
[[nodiscard]] formats::Result<WildlifeNumericContext, WildlifeGeometryError>
prepare_wildlife_numeric_context(const WildlifeNumericInputs&);
struct WildlifePositionInput {
    std::uint32_t origin_x, origin_z, chunk_x, chunk_z, packed, outcome;
};
[[nodiscard]] formats::Result<std::array<std::uint32_t, 2>, WildlifeGeometryError>
wildlife_ordinary_position(const WildlifePositionInput&, WildlifeFpProfile);
struct WildlifeHeightResult {
    std::uint32_t word;
    std::uint8_t triangle;
};
// Explicit numeric leaf, NOT a source binding for caller-supplied table words.
[[nodiscard]] formats::Result<WildlifeHeightResult, WildlifeGeometryError>
wildlife_ordinary_height(std::uint32_t x, std::uint32_t z,
                         const std::array<std::uint32_t, 4>& table_words, WildlifeFpProfile);
struct WildlifeOrdinaryGeometry {
    std::array<std::uint32_t, 3> words;
    std::array<std::uint32_t, 2> home;
    std::array<std::uint8_t, 4> corners;
    std::uint8_t triangle = 0;
    std::uint32_t actual_object = 0;
    std::string scene_sha256, numeric_provenance, corner_provenance;
    std::uint64_t terrain_digest = 0;
    WildlifeFpProfile table_profile, callback_profile;
    WildlifeImportDrawCounts unchanged_draws;
    std::uint32_t corner_reads = 0;
};
class WildlifeOrdinarySource {
  public:
    [[nodiscard]] formats::Result<WildlifeOrdinaryGeometry, WildlifeGeometryError>
    project(std::uint64_t token) const;

  private:
    struct Request {
        std::uint32_t position, outcome, object, kind;
    };
    WildlifeNumericContext numeric_;
    std::shared_ptr<const sim::Terrain> terrain_;
    std::map<std::uint64_t, Request> requests_;
    WildlifeImportDrawCounts draws_;
    std::uint32_t x_, z_, width_, height_, cx_, cz_;
    std::string source_, corner_provenance_;
    WildlifeOrdinarySource(WildlifeNumericContext numeric) : numeric_(std::move(numeric)) {}
    friend formats::Result<WildlifeOrdinarySource, WildlifeGeometryError>
    bind_wildlife_ordinary_source(const WildlifeFirstCallResult&, const WildlifeImportPlan&,
                                  const WildlifeNumericContext&, std::string);
};
// Fresh completed prepared HEIGHT only. Digest validated once/batch; projects read four bytes.
[[nodiscard]] formats::Result<WildlifeOrdinarySource, WildlifeGeometryError>
bind_wildlife_ordinary_source(const WildlifeFirstCallResult&, const WildlifeImportPlan&,
                              const WildlifeNumericContext&, std::string corner_provenance);
} // namespace anno::sim_load
