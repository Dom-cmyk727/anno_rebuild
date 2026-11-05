#pragma once
#include <anno/formats/bytes.hpp>
#include <anno/sim/build_availability.hpp>
#include <anno/sim/script.hpp>
namespace anno::sim {
// Engine rows use widened type ids. Values preserve the original byte.
using PermissionRows = std::vector<BuildPermission>;
[[nodiscard]] std::string validate_permissions(const PermissionRows&, const TypeRegistry&);
[[nodiscard]] formats::Result<PermissionRows, std::string> import_permissions(formats::Bytes,
                                                                              const TypeRegistry&);
// Known, validated player/registry; raw value is deliberately NOT normalized.
bool set_construction_permission(Player&, PermissionRows&, const TypeRegistry&, TypeId,
                                 std::uint32_t raw);
bool set_all_construction_permissions(Player&, PermissionRows&, const TypeRegistry&,
                                      std::uint32_t raw, ScriptWork&);
// Trusted stored state path: binary lookup, no validation scan per query.
[[nodiscard]] BuildEligibility stored_build_eligibility(const TypeRegistry&, const PlayerTable&,
                                                        PlayerId, TypeId, const PermissionRows&);
} // namespace anno::sim
