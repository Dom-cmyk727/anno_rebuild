// Test-only projection for accepted pre-BVA1 checkpoints. Never execute a projected world.
#pragma once
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>

#include "world_access.hpp"
namespace construction_test {
namespace sim = anno::sim;
inline anno::formats::Result<std::shared_ptr<const sim::TypeRegistry>, std::string>
legacy_registry(const sim::TypeRegistry& source, const sim::SimConfig& cfg) {
    sim::TypeRegistry::Builder b;
    b.config(cfg);
    b.goods(std::vector<sim::GoodsInfo>(source.goods_table().begin(), source.goods_table().end()));
    b.classes(
        std::vector<sim::ClassInfo>(source.class_table().begin(), source.class_table().end()));
    b.house_groups(
        std::vector<sim::HouseGroup>(source.house_groups().begin(), source.house_groups().end()));
    b.stage_table(
        std::vector<sim::StageRecord>(source.stage_table().begin(), source.stage_table().end()));
    b.walk_tables(source.walk_tables());
    for (const auto& figure : source.figure_types()) {
        b.figure_type(figure);
    }
    for (auto type : source.types()) {
        type.construction = {};
        b.add(type);
    }
    auto legacy = b.build();
    if (!legacy) {
        return legacy.error();
    }
    // Independently read retained accepted K3 log: registry u64 at byte28, SHA25656e389b1...8765.
    if (legacy.value()->digest() != 0x1bd52ba547c3878aULL) {
        return std::string(
            "legacy construction projection changed a non-construction registry fact");
    }
    return legacy.value();
}
inline std::uint64_t legacy_hash(const sim::World& live,
                                 const std::shared_ptr<const sim::TypeRegistry>& legacy) {
    sim::World projection = live;
    sim::WorldTestAccess::set_types(projection, legacy);
    return projection.state_hash();
}
} // namespace construction_test
