#include <anno/sim/city_accounts.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

std::optional<bool> inert_financial_active(const BuildingType& type, formats::Bytes record) {
    switch (type.cls) {
    case RuntimeClass::Hunter:
    case RuntimeClass::DualWorkshop:
        // Welt6a700/69140 zero-fill14 and use byte1's two switches.
        // Empty records bypass Load; a longer record overruns original scratch.
        if (record.empty())
            return true;
        if (record.size() > 14)
            return std::nullopt;
        return record.size() > 1 && (record[1] & 3) != 0;
    case RuntimeClass::Castle:
        // Welt6ef70 accepts exactly10; other lengths retain Farm constructor1.
        return record.size() == 10 ? (record[1] & 1) != 0 : true;
    case RuntimeClass::Whaler:
        // Welt741a0: version1 outer byte2; complete verified prefix11 bytes.
        if (record.empty() || record[0] != 1)
            return true;
        if (record.size() < 11)
            return std::nullopt;
        return (record[2] & 1) != 0;
    case RuntimeClass::MarketStall: {
        // Welt70490 accepts one byte;70310 filters enabled lines by output!=0.
        const std::uint8_t flags = record.size() == 1 ? record[0] : 3;
        for (std::size_t i = 0; i < 2 && i < type.output_goods.size(); ++i)
            if ((flags & (1U << i)) != 0 && type.output_goods[i] != 0)
                return true;
        return false;
    }
    default:
        return true; // Audited base constant property8e getter, not body support.
    }
}

std::string city_booking_problem(const SimConfig& cfg, const Rules& rules,
                                 const TypeRegistry& types) {
    if (!rules.city_booking)
        return {};
    if (cfg.economy.climate_names.size() != cfg.monopoly_climate_mask.size())
        return "SimConfig.economy.climate_names: dimensions differ from monopoly_climate_mask";
    if (cfg.billed_goods_last >= cfg.warehouse_goods ||
        cfg.billed_goods_last >= types.goods_table().size())
        return "SimConfig.billed_goods_last: city_booking requires complete stock/bill/price goods "
               "rows";
    return {};
}

} // namespace anno::sim
