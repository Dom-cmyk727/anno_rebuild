#pragma once
#include <cstdint>
#include <optional>
#include <string>

#include <anno/formats/bytes.hpp>

namespace anno::sim {
struct BuildingType;
struct SimConfig;
struct Rules;
class TypeRegistry;

/// ECON-BOOK-1: authoritative financial history, not reconstructed on native load.
/// Original city +0xa stamp, +0x60c/+0x5fc/+0x600 statistics and property1/2 costs.
/// House type/research changes do not retroactively repair the cost history.
struct CityAccounts {
    std::uint16_t stamp = 0;
    std::uint64_t sales = 0;
    std::uint64_t trade_sales = 0;
    std::uint64_t trade_purchases = 0;
    std::uint64_t civil_cost = 0;
    std::uint64_t military_cost = 0;
};

/// Financial state only; no production-body decoding. Empty input preserves
/// constructor state. Unknown unsafe/truncated financial input returns nullopt.
[[nodiscard]] std::optional<bool> inert_financial_active(const BuildingType&,
                                                         formats::Bytes record = {});
[[nodiscard]] std::string city_booking_problem(const SimConfig&, const Rules&, const TypeRegistry&);

} // namespace anno::sim
