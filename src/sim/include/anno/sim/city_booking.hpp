#pragma once
#include <cstdint>
#include <span>

#include <anno/formats/result.hpp>
#include <anno/sim/city_accounts.hpp>
#include <anno/sim/ids.hpp>

namespace anno::sim {
struct SimConfig;
struct BuildingType;
struct Player;
class TypeRegistry;
struct Building;

enum class AccountingFault : std::uint8_t {
    None,
    Product,
    Sum,
    Rounding,
    CostSum,
    Credit,
    Debit,
    StatDecay,
    Sales,
    Configuration,
    City,
    Owner,
    PriceDimensions,
    FinancialState,
    CostUnderflow,
    CostOverflow,
    Type
};
[[nodiscard]] const char* accounting_fault_name(AccountingFault);

struct BookingRow {
    std::uint32_t price = 0, surcharge = 0, billed = 0;
};
struct BookingResult {
    bool stepped = false;
    AccountingFault fault = AccountingFault::None;
    std::int64_t money = 0;
    std::uint64_t income = 0, upkeep = 0, rows = 0;
    CityAccounts accounts;
};
/// Pure, checked preflight. No money/bill/stamp/stat mutation escapes a fault.
[[nodiscard]] BookingResult calculate_city_booking(const SimConfig&, std::uint32_t tick,
                                                   const CityAccounts&, std::span<const BookingRow>,
                                                   std::int64_t money);

struct BuildingCost {
    std::uint32_t kind = 1, amount = 0;
    bool active = true;
    std::uint64_t lines = 0; // deterministic diagnostic work, not state
};
[[nodiscard]] formats::Result<BuildingCost, AccountingFault>
building_cost(const TypeRegistry&, const BuildingType&, const Building&, const Player* owner);
[[nodiscard]] formats::Result<CityAccounts, AccountingFault>
change_city_cost(const CityAccounts&, const BuildingCost&, bool add);

/// Call-time financial barrier in an island's shared Effects. Prefixes are counts
/// in their original vectors, not a new authoritative command or wire format.
struct CityBookingRequest {
    CityRef city;
    PlayerId owner;
    std::uint32_t tick = 0, climate = 0, tax = 0;
    std::uint64_t money_prefix = 0, billed_prefix = 0, fault_prefix = 0;
    CityAccounts accounts;
};

/// Public observational ledger through existing SimEvent output, never state.
/// Faulted attempts apply zero income/upkeep, retain the before balance and carry
/// a typed reason. A later unrelated money effect can still change world money.
struct CityBookingLedger {
    CityRef city;
    PlayerId owner;
    std::uint64_t income = 0, upkeep = 0, rows = 0;
    std::int64_t money_before = 0, money_after = 0;
    AccountingFault fault = AccountingFault::None;
    friend bool operator==(const CityBookingLedger&, const CityBookingLedger&) = default;
};
} // namespace anno::sim
