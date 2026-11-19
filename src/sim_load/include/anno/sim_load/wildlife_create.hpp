// Owned original-profile facts and pure effects only; docs/sim/wildlife-create-effects.md.
#pragma once
#include <array>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <anno/formats/figuren.hpp>
#include <anno/sim/island.hpp>

namespace anno::sim_load {
enum class WildlifeDescriptorProfile { Gog };
class WildlifeCreationDescriptor {
  public:
    sim::TypeId id() const { return id_; }
    std::uint32_t kind() const { return kind_; }
    std::uint16_t hp() const { return hp_; }
    sim::GoodsId yield() const { return yield_; }
    const std::string& image_sha256() const { return image_sha256_; }
    const std::string& source_sha256() const { return source_sha256_; }
    bool logic_present() const { return logic_present_; }
    bool ware_present() const { return ware_present_; }

  private:
    friend class WildlifeSpeciesCatalogue;
    WildlifeCreationDescriptor(sim::TypeId id, std::uint32_t kind, std::uint16_t hp,
                               sim::GoodsId yield, std::string image, std::string source,
                               bool logic, bool ware)
        : id_(id), kind_(kind), hp_(hp), yield_(yield), image_sha256_(std::move(image)),
          source_sha256_(std::move(source)), logic_present_(logic), ware_present_(ware) {}
    sim::TypeId id_;
    std::uint32_t kind_;
    std::uint16_t hp_;
    sim::GoodsId yield_;
    std::string image_sha256_, source_sha256_;
    bool logic_present_, ware_present_;
    friend formats::Result<std::vector<WildlifeCreationDescriptor>, std::string>
        bind_wildlife_creation_descriptors(WildlifeDescriptorProfile, std::span<const sim::TypeId>,
                                           std::span<const formats::figuren::Entry>,
                                           formats::Bytes);
};
[[nodiscard]] formats::Result<std::vector<WildlifeCreationDescriptor>, std::string>
bind_wildlife_creation_descriptors(WildlifeDescriptorProfile, std::span<const sim::TypeId>,
                                   std::span<const formats::figuren::Entry>, formats::Bytes game);

// Fresh unique definition facts, not reused catalogues or overlays. Generic
// selector facts are distinct from the sealed New-only capability.
struct WildlifeSpeciesFact {
    sim::TypeId id;
    std::uint32_t kind = 0;
    sim::GoodsId yield = 0;
    std::uint16_t hp = 0;
    std::string image_sha256, source_sha256;
    bool logic_present = false, ware_present = false, ware_matched = false;
    bool operator==(const WildlifeSpeciesFact&) const = default;
};
[[nodiscard]] formats::Result<std::vector<WildlifeSpeciesFact>, std::string>
bind_wildlife_species_facts(WildlifeDescriptorProfile, std::span<const sim::TypeId>,
                            std::span<const formats::figuren::Entry>, formats::Bytes game);

enum class WildlifeManagerOutcome { Success, Null };
enum class WildlifeCreationPlace { Ordinary, ResolvedCaveEntrance };
struct WildlifeCreationGeometry {
    sim::TileRect area;
    std::uint32_t home_x = 0, home_z = 0;
    WildlifeCreationPlace place = WildlifeCreationPlace::Ordinary;
    std::uint32_t entrance_count = 0; // Supplied original provider outcome, not proved by this API.
};
struct WildlifeSpawnProjection {
    std::uint16_t id = 0;
    std::uint8_t timestamp = 0, packed = 0;
    std::uint16_t goods = 0;
    bool operator==(const WildlifeSpawnProjection&) const = default;
};
struct WildlifeCreationInputs {
    WildlifeManagerOutcome manager = WildlifeManagerOutcome::Success;
    std::uint32_t assigned_id = 0, creation_time = 0;
    std::array<std::uint32_t, 3> position_words{};
    WildlifeCreationGeometry geometry;
    WildlifeSpawnProjection prior;
    std::uint32_t sweep_time = 0;
    std::uint64_t helper_consumed = 0; // Already consumed elsewhere, never advanced here.
};
// No prior packed-byte placeholder. Null ignores absent success-only ID/time.
struct WildlifeCreationOutcomeInputs {
    WildlifeManagerOutcome manager;
    std::optional<std::uint32_t> assigned_id, creation_time;
    std::array<std::uint32_t, 3> position_words;
    WildlifeCreationGeometry geometry;
    std::uint32_t sweep_time;
    std::uint64_t helper_consumed;
};
struct WildlifeSpawnWrites {
    std::uint16_t id;
    std::uint8_t timestamp;
    std::uint16_t goods;
};
enum class WildlifeCreationStage {
    ManagerNull,
    ConstructorQuantity,
    AttachType,
    AttachWorld,
    SuppliedRegistration,
    SetTime,
    CopyPosition,
    SuppliedSpatialRegistration,
    SetHome,
    SetCargoGoods,
    AttachIsland,
    QueryYield,
    SaveId,
    SaveTimestamp,
    MarkOccupied,
    SaveYield
};
struct WildlifeCreatedProjection {
    sim::TypeId type;
    std::uint32_t engine_id = 0, full_creation_time = 0;
    std::uint16_t original_id = 0, hp = 0;
    std::array<std::uint16_t, 3> original_times{};
    std::array<std::uint32_t, 3> position_words{}, second_position_words{};
    std::array<std::uint32_t, 2> home{};
    std::array<std::uint16_t, 2> original_home{}, original_current_tile{};
    std::uint32_t quantity = 0;
    sim::GoodsId carried_goods = 0, yield = 0;
    std::uint16_t flags = 8; // Original word, separate from the byte action/state fields.
    std::uint8_t action = 0x13, requested = 27, current = 0, behavior = 4;
    std::uint32_t configuration = 4;
    bool first_action_started = false;
    std::string descriptor_source_sha256;
    bool operator==(const WildlifeCreatedProjection&) const = default;
};
struct WildlifeCreationEffects {
    std::optional<WildlifeCreatedProjection> animal;
    WildlifeSpawnProjection spawn;
    std::vector<WildlifeCreationStage> stages;
    std::uint64_t helper_consumed = 0;
    bool operator==(const WildlifeCreationEffects&) const = default;
};
struct WildlifeCreationOutcomeEffects {
    std::optional<WildlifeCreatedProjection> animal;
    std::optional<WildlifeSpawnWrites>
        writes; // Engaged only for successful creation; occupied=true.
    std::vector<WildlifeCreationStage> stages;
    std::uint64_t helper_consumed = 0;
};
[[nodiscard]] formats::Result<WildlifeCreationOutcomeEffects, std::string>
project_wildlife_creation_outcome(const WildlifeCreationDescriptor&,
                                  const WildlifeCreationOutcomeInputs&);
// Pure projection under explicit supplied manager/geometry outcomes. No live figure,
// manager rollback, actual spatial links, PRNG, I/O or new cross-host FP policy.
[[nodiscard]] formats::Result<WildlifeCreationEffects, std::string>
project_wildlife_creation(const WildlifeCreationDescriptor&, const WildlifeCreationInputs&);
} // namespace anno::sim_load
