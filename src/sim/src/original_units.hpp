// Original actor projection only; never used by native load or simulation.
#pragma once
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <anno/formats/sze.hpp>
#include <anno/sim/scenario_formats.hpp>

namespace anno::sim {
struct ImportedUnits {
    std::uint32_t initial_slots = 0;
    std::vector<ScenarioSpec::FigureSpec> figures;
    std::vector<ScenarioWarning> warnings;
};

formats::Result<ImportedUnits, std::string>
import_original_units(const formats::sze::Scene&, const std::vector<formats::sze::Player>&,
                      const TypeRegistry&, const SimConfig&, const WorldSettings&,
                      const std::map<std::uint32_t, std::optional<GoodsId>>&,
                      const UnitImportOptions&);
} // namespace anno::sim
