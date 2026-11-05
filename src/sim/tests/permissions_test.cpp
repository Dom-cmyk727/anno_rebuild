#include <algorithm>
#include <array>

#include <doctest/doctest.h>

#include <anno/sim/permissions.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>
using namespace anno::sim;
namespace {
auto custom_types() {
    TypeRegistry::Builder b;
    for (std::uint32_t n = 0; n < 3; ++n) {
        BuildingType t;
        t.id = TypeId{90001 + n};
        t.width = t.height = 1;
        t.construction.stage = n == 2 ? 31 : 3;
        t.permissions.folks = {70};
        t.permissions.bulk_excluded = n == 1;
        if (n == 2)
            t.permissions.grant_research = {0, 4};
        b.add(t);
    }
    return b.build();
}
} // namespace
TEST_CASE("SCRIPT1 permissions raw values imports and grants are distinct") {
    auto types = custom_types();
    REQUIRE(types);
    auto p = make_player(PlayerId{70}, SimConfig{});
    p.volk = 70;
    PermissionRows rows;
    REQUIRE(set_construction_permission(p, rows, *types.value(), TypeId{90001}, 257));
    CHECK(rows[0].value == 1);
    CHECK(p.flags == 1);
    REQUIRE(set_construction_permission(p, rows, *types.value(), TypeId{90001}, 1));
    CHECK(p.flags == 9);
    CHECK((p.flags & (1u << 26)) == 0);
    REQUIRE(set_construction_permission(p, rows, *types.value(), TypeId{90001}, 0));
    CHECK(rows[0].value == 0);
    CHECK(p.flags == 9);
    REQUIRE(set_construction_permission(p, rows, *types.value(), TypeId{90003}, 1));
    CHECK(p.research.researched(0));
    CHECK(p.research.researched(4));
    CHECK_FALSE(p.research.researched(9));
    CHECK(types.value()->permission_types(70).size() == 2);
    ScriptWork work{2};
    REQUIRE(set_all_construction_permissions(p, rows, *types.value(), 0, work));
    CHECK(work.used == 2);
    CHECK(rows.size() == 2);
    REQUIRE(set_construction_permission(p, rows, *types.value(), TypeId{90002}, 1));
    CHECK(rows.size() == 3);
    CHECK(p.research.researched(4));
    CHECK_FALSE(set_construction_permission(p, rows, *types.value(), TypeId{99999}, 1));
}
TEST_CASE("SCRIPT1 ANLTRG is raw last write wins with strict validation") {
    TypeRegistry::Builder b;
    b.add_plain(TypeId{100}, 0);
    auto types = b.build();
    REQUIRE(types);
    const std::array<std::uint8_t, 12> bytes{100, 0, 1, 255, 100, 0, 0, 99, 100, 0, 255, 1};
    auto rows = import_permissions(bytes, *types.value());
    REQUIRE(rows);
    REQUIRE(rows.value().size() == 1);
    CHECK(rows.value()[0].value == 255);
    auto p = make_player(PlayerId{0}, SimConfig{});
    CHECK(p.flags == 1);
    CHECK_FALSE(import_permissions(std::span(bytes).first(3), *types.value()));
    auto bad = bytes;
    bad[0] = 101;
    CHECK_FALSE(import_permissions(bad, *types.value()));
    auto table = make_player_table(SimConfig{});
    table.slots[0] = p;
    CHECK(stored_build_eligibility(*types.value(), table, PlayerId{0}, TypeId{100}, rows.value())
              .allowed());
    rows.value()[0].value = 0;
    CHECK(stored_build_eligibility(*types.value(), table, PlayerId{0}, TypeId{100}, rows.value())
              .status == BuildEligibilityStatus::ScenarioDenied);
}
TEST_CASE("SCRIPT1 original ALL filter and research grants (game files)") {
    auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("SKIP: ANNO1503_GAME_DIR is unset");
        return;
    }
    const auto loaded =
        anno::sim_load::load_original_registry(*game, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(loaded.error.empty(), loaded.error);
    auto ids = loaded.types->permission_types(1);
    CHECK(ids.size() == 461);
    const auto contains = [&](std::uint32_t id) {
        return std::find(ids.begin(), ids.end(), TypeId{id}) != ids.end();
    };
    CHECK(contains(801));
    CHECK(contains(501));
    CHECK(contains(1001));
    CHECK_FALSE(contains(2801));
    CHECK_FALSE(contains(2229));
    auto p = make_player(PlayerId{0}, SimConfig{});
    PermissionRows rows;
    REQUIRE(set_construction_permission(p, rows, *loaded.types, TypeId{1006}, 1));
    CHECK(p.research.researched(4));
    CHECK_FALSE(p.research.researched(9));
    REQUIRE(set_construction_permission(p, rows, *loaded.types, TypeId{1012}, 1));
    CHECK(p.research.researched(38));
    CHECK(p.research.researched(12));
    REQUIRE(set_construction_permission(p, rows, *loaded.types, TypeId{1341}, 1));
    CHECK(p.research.researched(49));
    REQUIRE(set_construction_permission(p, rows, *loaded.types, TypeId{2229}, 1));
    CHECK((p.flags & (1u << 14)) != 0);
}
TEST_CASE("SCRIPT1 permission fact digest has independent PRM1 bytes") {
    TypeRegistry::Builder b;
    BuildingType t;
    t.id = TypeId{90001};
    t.width = t.height = 1;
    b.add(t);
    auto base = b.build();
    REQUIRE(base);
    TypeRegistry::Builder c;
    t.permissions = {{70}, true, {4}};
    c.add(t);
    auto changed = c.build();
    REQUIRE(changed);
    // PRM1,count1,id90001,folk-count1,folk70,excluded1,grant-count1,research4.
    const std::vector<std::uint8_t> bytes{
        0x50, 0x52, 0x4d, 0x31, 1, 0, 0, 0, 0, 0, 0, 0, 0x91, 0x5f, 1, 0, 1, 0, 0, 0, 0,
        0,    0,    0,    70,   0, 0, 0, 1, 1, 0, 0, 0, 0,    0,    0, 0, 4, 0, 0, 0};
    auto expected = base.value()->digest();
    for (auto byte : bytes) {
        expected ^= byte;
        expected *= 0x100000001b3ULL;
    }
    CHECK(changed.value()->digest() == expected);
}
