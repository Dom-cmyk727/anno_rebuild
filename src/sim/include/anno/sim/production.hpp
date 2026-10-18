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
    Workshop,          ///< HANDWERK (tick 0x100680b0)
    TwoOutputWorkshop, ///< HANDWERKDUAL, PRODTYPE 12 (tick 0x10068a70; no entry uses it)
    WeaponsWorkshop,   ///< HANDWERKWAFFEN, PRODTYPE 17 (tick 0x10069550)
    Farm,              ///< PLANTAGE, WEIDETIER, STEINMETZ, SCHAFFARM, FISCHER (tick 0x1006ea90)
    FurTrapper,        ///< PELZJAEGER (own tick 0x1006e5d0)
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
    /// S (HEAD[0x10]), shift k (HEAD[0x18]), period P in ticks, a u16 as in
    /// the original (type +0x1fe; 0 = none).
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

/// A building's status code, status icons and status time (D: building
/// +0x0d bits 5–7 code, bits 0–4 icons; u16 time via slots +0x120/+0x124;
/// helper 0x10072ed0, clear 0x10072fb0). A message is due only after a
/// condition has lasted `repeat` ticks, and then on every later report (the
/// time is set to t − repeat); a lower code than the current one is ignored;
/// a higher one restarts the time.
struct StatusTracker {
    Status code = Status::None;
    /// Icon bits: bit c is set when a message for code c was raised.
    std::uint8_t icons = 0;
    /// Whether icons change at all: 0x10073010 first asks the city owner
    /// (city +0x10 → slot +0x1c → slot +0xc4(3)) and returns unless that is
    /// 1 — meaning not traced, possibly "human player" (OQ-033). Without
    /// icons, clear() never drops the code.
    bool shows_icons = true;
    std::uint16_t time = 0;
    /// Reports status `s` at `t`; whether a message is raised now. Not
    /// modelled: before raising, code 1 (for inputs that are not crops,
    /// goods 0x38–0x4b, or animals, 0x89–0x8d) and code 2 call checks
    /// (0x10071f40, 0x10072390) whose failure turns the message into code 3;
    /// their meaning is not traced (docs/sim/production.md).
    bool report(Status s, std::uint32_t t, std::uint32_t repeat);
    /// Clears the codes in `mask` (bit per code) whose icon is shown: if any
    /// icon of the mask is set, each code of the mask is dropped if current,
    /// and its icon removed (0x10072fb0; production calls it with 0x0e).
    void clear(std::uint8_t mask);
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
    /// The "blocked" flag changed (the city is notified: city slot +0x18 →
    /// slot +0x168 with {type id, x, y, rotation, blocked}).
    bool blocked_changed = false;
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
/// output may exceed its capacity by up to one step.
StepResult workshop_step(Workshop& w, const ProductionType& type, std::uint32_t t);

/// A HANDWERKWAFFEN workshop (two production lines, e.g. 1006 bows and
/// crossbows) at runtime (D: AnnoWelt.dll class 0x100693c0, vtable
/// 0x100c2d1c; fields +0x18/+0x1a outputs, +0x20/+0x22 inputs, +0x24/+0x26
/// activity, +0x2a/+0x2b line switches, +0x30/+0x34 carrier slots).
struct WeaponsWorkshop {
    std::uint16_t last = 0;
    /// Per line: switched on (bytes +0x2a/+0x2b, both 1 after construction).
    std::vector<bool> line_on;
    /// Per line: the owner meets the output goods' requirement. In the
    /// original a per-goods byte b of the owner's goods table (26 bytes per
    /// goods, byte +0x18) is 0 or the owner's object +0xbc reports bit 1 for
    /// b (0x10069580–0x100695cf); likely research (OQ-034). Set by the caller
    /// from the owner's state.
    std::vector<bool> unlocked;
    bool suspended = false;
    bool blocked = false;
    std::vector<std::uint32_t> out; ///< output stock per line, 8.8
    std::vector<std::uint32_t> in;  ///< input stock per input, 8.8
    /// Activity average per line: a u16 in the original (+0x24/+0x26,
    /// addw), kept 16-bit to match.
    std::vector<std::uint16_t> activity;
    std::uint32_t carriers_out = 0;
    StatusTracker status;
};

/// A weapons workshop sized for `type` (one line per output; stocks and
/// activity 0, every line switched on and unlocked).
WeaponsWorkshop make_weapons_workshop(const ProductionType& type);

/// One production tick at world tick `t` of a HANDWERKWAFFEN workshop (D:
/// AnnoWelt.dll 0x10069550). A line works when it is switched on and
/// unlocked. Inputs limit the step as in the workshop (ratio of 128). The
/// step is shared by the working lines: with both, each makes rate · (64 ·
/// ratio >> 7) >> 7, with one, that line makes the full step. Every line
/// checks the output-0 capacity (type +0xc0) and reports "storage full" on
/// its own; the inputs are used up in full as soon as any line made
/// something. The split byte +0x2c of the class is not read here (only
/// HANDWERKDUAL, 0x10068a70, uses it). docs/sim/production.md.
StepResult weapons_workshop_step(WeaponsWorkshop& w, const ProductionType& type, std::uint32_t t);

/// A farm (PLANTAGE family) at runtime (D: class at 0x1006f160).
struct Farm {
    std::uint16_t last = 0;
    bool switched_on = true;
    bool suspended = false;
    bool blocked = false;
    std::uint32_t out = 0; ///< output stock, 8.8
    std::uint32_t raw = 0; ///< harvested raw stock, 8.8
    /// Activity average: a u16 in the original (+0x1e, addw), which wraps for
    /// farms averaging more than 16 per step; kept 16-bit to match.
    std::uint16_t activity = 0;
    std::uint32_t workers_out = 0;
    StatusTracker status;
};

/// One farm tick at world tick `t` (D: AnnoWelt.dll 0x1006ea90).
StepResult farm_step(Farm& f, const ProductionType& type, std::uint32_t t);

/// One fur trapper tick (PELZJAEGER, 824; D: AnnoWelt.dll 0x1006e5d0) on
/// the farm's fields. Unlike the farm: nothing is made while any trapper is
/// out; the input is checked before the capacity, as in the workshop, and
/// "missing input" repeats after 45,000 ticks. A trapper is due only when
/// none is out and the step did not run at the full ratio (128), and when
/// 384 / rate ≥ raw / need; max_figures is not read. `send_figure` is that
/// condition: the original then also needs a target from its radius search
/// (0x10071da0(8), nearest animal of the input goods; float distances), done
/// by the caller, before it sends job 0x0d.
StepResult fur_trapper_step(Farm& f, const ProductionType& type, std::uint32_t t);

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

    /// Adds `amount` of goods `g`; if the result would exceed the capacity,
    /// the stock is set to the capacity instead (u32 arithmetic as the
    /// original: a stock above the capacity is lowered to it). Returns the
    /// amount the stock changed by, wrapped to u32 like the original's.
    /// Goods numbers ≥ stock.size() are refused (as 0x1002da50 refuses
    /// g ≥ 0x38).
    std::uint32_t add(std::uint32_t g, std::uint32_t amount);
    /// Removes up to `amount`, clipped at the stock; returns what was removed.
    /// The original remover has no goods-number check; refusing goods ≥
    /// stock.size() here is a bounds deviation.
    std::uint32_t remove(std::uint32_t g, std::uint32_t amount);
};

/// Warehouse goods in the original (numbers 0–0x37; D: 0x1002da50 refuses
/// g > 0x37). Default size of CityStock::stock.
inline constexpr std::uint32_t kOriginalWarehouseGoods = 0x38;

} // namespace anno::sim
