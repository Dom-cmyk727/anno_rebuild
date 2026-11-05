#include <algorithm>

#include <anno/sim/permissions.hpp>
namespace anno::sim {
namespace {
auto position(PermissionRows& rows, TypeId id) {
    return std::lower_bound(rows.begin(), rows.end(), id,
                            [](const auto& row, TypeId key) { return row.type < key; });
}
void store(PermissionRows& rows, TypeId id, std::uint32_t raw) {
    auto it = position(rows, id);
    if (it == rows.end() || it->type != id)
        it = rows.insert(it, {id, 0});
    it->value = raw & 255u;
}
} // namespace
std::string validate_permissions(const PermissionRows& rows, const TypeRegistry& types) {
    for (std::size_t i = 0; i < rows.size(); ++i)
        if (rows[i].value > 255 || !types.find(rows[i].type) ||
            (i != 0 && !(rows[i - 1].type < rows[i].type)))
            return "invalid construction permission row";
    return {};
}
formats::Result<PermissionRows, std::string>
import_permissions(formats::Bytes bytes, const TypeRegistry& types, PermissionRows rows) {
    if (bytes.size() % 4 != 0)
        return std::string("ANLTRG length is not a multiple of four");
    if (auto problem = validate_permissions(rows, types); !problem.empty())
        return problem;
    for (std::size_t i = 0; i < bytes.size(); i += 4) {
        TypeId id{formats::u16_le(bytes, i)};
        if (!types.find(id))
            return std::string("ANLTRG unknown construction type");
        store(rows, id, bytes[i + 2]);
    }
    return rows;
}
bool set_construction_permission(Player& player, PermissionRows& rows, const TypeRegistry& types,
                                 TypeId id, std::uint32_t raw) {
    const auto* type = types.find(id);
    if (!type || type->construction.stage >= 32)
        return false;
    for (auto research : type->permissions.grant_research)
        if (research >= player.research.state.size())
            return false;
    store(rows, id, raw);
    if (raw != 1)
        return true;
    if (type->construction.stage != 31)
        player.flags |= std::uint32_t{1} << type->construction.stage;
    else
        for (auto research : type->permissions.grant_research)
            player.research.state[research] |= 2u;
    return true;
}
bool set_all_construction_permissions(Player& player, PermissionRows& rows,
                                      const TypeRegistry& types, std::uint32_t raw,
                                      ScriptWork& work) {
    for (auto id : types.permission_types(player.volk)) {
        if (!work.consume() || !set_construction_permission(player, rows, types, id, raw))
            return false;
    }
    return true;
}
BuildEligibility stored_build_eligibility(const TypeRegistry& types, const PlayerTable& players,
                                          PlayerId owner, TypeId type, const PermissionRows& rows) {
    auto it = std::lower_bound(rows.begin(), rows.end(), type,
                               [](const auto& row, TypeId key) { return row.type < key; });
    // Reuse the reviewed predicate with only the looked-up row; no unrelated row scan.
    return build_eligibility(types, players, owner, type,
                             {owner, it != rows.end() && it->type == type
                                         ? std::span<const BuildPermission>(&*it, 1)
                                         : std::span<const BuildPermission>{}});
}
} // namespace anno::sim
