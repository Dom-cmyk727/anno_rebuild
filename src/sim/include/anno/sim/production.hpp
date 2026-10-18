// Goods production: building type data and the per-step rules (spec:
// docs/sim/production.md; evidence: docs/re/production.md). Integer only;
// stocks are 8.8 fixed point (256 = 1 ton), time is the world tick t (2 ms).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <anno/formats/figuren.hpp>
#include <anno/formats/result.hpp>

namespace anno::sim {

/// Ticks between two production steps of a building (D: AnnoWelt.dll gate
/// in every production tick, e.g. 0x100680b0: (u16)(t − last) < 500).
inline constexpr std::uint32_t kStepTicks = 500;
/// One ton in 8.8 fixed point.
inline constexpr std::uint32_t kTon = 256;

/// Runtime class of a building type, by PRODTYPE (D: AnnoWelt.dll factory
/// 0x10024010; docs/re/production.md §2.1, §3).
enum class BuildingClass {
    Workshop,          ///< HANDWERK
    TwoOutputWorkshop, ///< HANDWERKWAFFEN (and PRODTYPE 12)
    Farm,              ///< PLANTAGE, WEIDETIER, STEINMETZ, SCHAFFARM, FISCHER, PELZJAEGER
    Mine,              ///< MINE
    SmallFarm,         ///< UNIVERSAL (two farm lines)
    Market,            ///< HAUPT, MARKT, KONTOR, MARKTKOMBI
    GrowingItem,       ///< ROHSTWACHS, ROHSTDUERR (tile objects, not buildings)
    Other,             ///< every other PRODTYPE (no production tick here)
};

/// Production data of one Anlagen.dat entry, as the original's type object
/// holds it after loading (D: AnnoGame.dll 0x10009240, PROD3 branch
/// 0x100094d2–0x1000972f, growth 0x100088b0).
struct ProductionType {
    std::uint32_t id = 0;
    std::string prodtype;
    BuildingClass kind = BuildingClass::Other;
    /// WARE / ROHSTOFF goods names (UNUSED dropped; up to two in the files).
    std::vector<std::string> outputs, inputs;
    /// Per production line i: storage capacity of output i (PROD3[4 + 2i] <<
    /// 8), output per step (trunc(256/T + 0.5)), input i per step.
    std::vector<std::uint32_t> capacity, output_per_step, input_per_step;
    /// trunc(T · 128) of line 0 and 1 (type +0x214, +0x218).
    std::uint32_t time0_128 = 0, time1_128 = 0;
    /// Figure groups: max figures out at once and figure types (PROD3
    /// 0x28 + 3g: byte 0 = type count, byte 1 = max), figure ids (PROD3
    /// 0x32, u16 each).
    std::vector<std::uint32_t> max_figures, figure_type_count;
    std::vector<std::uint32_t> figure_ids;
    /// Growth (ROHSTWACHS/ROHSTDUERR with more than one stage): stage count
    /// S (HEAD[0x10]), shift k (HEAD[0x18]), period P in ticks (0 = none).
    std::uint32_t stages = 0, stage_shift = 0, growth_period = 0;
    /// Display rate of line 0 and 1 in 8.8 tons per minute (type +0x20c).
    std::uint32_t display_rate0 = 0, display_rate1 = 0;
};

/// The production type of an Anlagen.dat ANLAGEN entry; nullopt if it has
/// no PROD3 record of 68 bytes. The float → integer conversion follows the
/// original's x87 sequence in double precision (docs/sim/production.md,
/// "Load-time conversion").
formats::Result<std::optional<ProductionType>, std::string>
production_type(const formats::figuren::Entry& entry);

/// Status codes a production step reports (D: status helper 0x10072ed0).
enum class Status : std::uint8_t {
    None = 0,
    MissingInput = 1, ///< "raw material missing"
    StorageFull = 2,  ///< "storage full"
};

/// Raises a player message only when the code changes or the repeat
/// interval has passed since the last message (D: 0x10072ed0).
struct StatusTracker {
    Status code = Status::None;
    std::uint32_t last_t = 0;
    /// Records `s` at `t`; whether a message is raised.
    bool report(Status s, std::uint32_t t, std::uint32_t repeat);
};

/// Status message repeat intervals in ticks (D: docs/re/production.md §6).
inline constexpr std::uint32_t kRepeatWorkshop = 45000;
inline constexpr std::uint32_t kRepeatFarmMissing = 60000;
inline constexpr std::uint32_t kRepeatMineMissing = 30000;

/// What one call of a production tick did.
struct StepResult {
    /// The step gate opened (a production step took place).
    bool stepped = false;
    Status status = Status::None;
    /// Whether the status raises a player message (StatusTracker).
    bool message = false;
    /// 8.8 output made (per output for two-output workshops).
    std::vector<std::uint32_t> made;
    /// The building sends a carrier / worker now.
    bool send_figure = false;
};

/// A HANDWERK workshop at runtime (D: fields of the class
/// at AnnoWelt.dll 0x100688f0; widths widened, docs/sim/production.md).
struct Workshop {
    std::uint16_t last = 0;
    bool switched_on = true;
    /// Building flags bits 4 and 5 suspend production (meaning open).
    bool suspended = false;
    /// The player-visible "blocked" flag (building +0x0c bit 3).
    bool blocked = false;
    std::vector<std::uint32_t> out;      ///< output stock per output, 8.8
    std::vector<std::uint32_t> in;       ///< input stock per input, 8.8
    std::vector<std::uint32_t> activity; ///< activity average per output
    std::uint32_t carriers_out = 0;
    StatusTracker status;
};

/// A workshop sized for `type` (stocks and activity 0, switched on).
Workshop make_workshop(const ProductionType& type);

/// One production tick at world tick `t` of a HANDWERK workshop (D:
/// AnnoWelt.dll 0x100680b0). Production is proportional to the share of one
/// step's inputs in stock (ratio of 128), nothing below half a step; the
/// output may exceed its capacity by up to one step. HANDWERKWAFFEN (two
/// outputs, 0x10068a70) is not implemented: its split rule is only "likely".
StepResult workshop_step(Workshop& w, const ProductionType& type, std::uint32_t t);

/// A farm (PLANTAGE family) at runtime (D: class at 0x1006f160).
struct Farm {
    std::uint16_t last = 0;
    bool switched_on = true;
    bool suspended = false;
    std::uint32_t out = 0; ///< output stock, 8.8
    std::uint32_t raw = 0; ///< harvested raw stock, 8.8
    std::uint32_t activity = 0;
    std::uint32_t workers_out = 0;
    StatusTracker status;
};

/// One farm tick at world tick `t` (D: AnnoWelt.dll 0x1006ea90).
StepResult farm_step(Farm& f, const ProductionType& type, std::uint32_t t);

/// A growing tile object (field, forest): object id and phase byte (bits
/// 0–5 counter, bit 6 parity, bit 7 kept; D: island tile grid +0x0c).
struct GrowthTile {
    std::uint32_t object = 0;
    std::uint8_t phase = 0;
};

/// One chunk visit of a growing tile at tick `t` (D: AnnoWelt.dll
/// 0x1008b5f0): on every parity flip of t / (P/2) the counter advances;
/// after S · 2^k flips the object becomes the next id. Returns whether the
/// object changed. Types with P = 0 do not grow.
bool grow(GrowthTile& tile, const ProductionType& type, std::uint32_t t);

/// A city's warehouse (D: city +0x334 records, capacity +0x624; adder
/// 0x1002da50, remover 0x1002da90).
struct CityStock {
    /// Stock per goods number, 8.8. Goods numbers ≥ size() are refused (the
    /// original has 0x38 warehouse goods; the size is data).
    std::vector<std::uint32_t> stock;
    /// The same capacity for every goods, 8.8 (savegame HEAD2 u16 0x24 in
    /// tons, 0 → 50 t).
    std::uint32_t capacity = 50 * kTon;

    /// Adds up to `amount` of goods `g`, clipped so stock ≤ capacity; returns
    /// what was added.
    std::uint32_t add(std::uint32_t g, std::uint32_t amount);
    /// Removes up to `amount`, clipped at the stock; returns what was removed.
    std::uint32_t remove(std::uint32_t g, std::uint32_t amount);
};

/// Warehouse goods in the original (numbers 0–0x37; D: 0x1002da50 refuses
/// g > 0x37). Default size of CityStock::stock.
inline constexpr std::uint32_t kOriginalWarehouseGoods = 0x38;

} // namespace anno::sim
