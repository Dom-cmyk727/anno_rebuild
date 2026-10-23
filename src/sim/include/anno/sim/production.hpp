// Goods production: building type data and the per-step rules (spec:
// docs/sim/production.md; evidence: docs/re/production.md). Integer only;
// stocks are 8.8 fixed point (256 = 1 ton), time is the world tick t (2 ms).
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
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
    SmallFarm,         ///< UNIVERSAL (two farm lines, tick 0x1006f660)
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

/// The number of names the original reads from a WARE or a ROHSTOFF record
/// (the loops `cmp eax, 2` at AnnoGame.dll 0x1000a1f4 and 0x1000a2e2). A fixed
/// width of the type reader, mirrored here: a mod with more goods lines needs
/// the reader, ProductionType and the production model widened together; raise
/// this constant with them.
inline constexpr std::size_t kWareRecordNames = 2;

/// The goods names of a WARE or ROHSTOFF record (NUL-terminated strings): the
/// first kWareRecordNames (two), as the original reads exactly two (AnnoGame.dll 0x1000a1f4,
/// 0x1000a2e2; further names are ignored), up to the last one that is not
/// UNUSED or empty. For ROHSTOFF this is the original's count (the index + 1
/// of the last non-UNUSED name, 0x1000a2c9-0x1000a2cf); the WARE branch has no
/// count, the cut is the rebuild's convention there. An UNUSED name before the
/// last used one stays (it resolves to goods 0).
std::vector<std::string> ware_names(formats::Bytes record);

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
/// "Missing input" of a small-farm line (D: 0x1006f572, 0x124f8).
inline constexpr std::uint32_t kRepeatSmallFarmMissing = 75000;

/// What one call of a production tick did.
struct StepResult {
    /// The step gate opened (a production step took place).
    bool stepped = false;
    Status status = Status::None;
    /// Whether the status raises a player message (StatusTracker).
    bool message = false;
    /// A player message id raised besides the status (mines: 0x19 / 0x15b
    /// when an iron-ore deposit runs out); 0 if none.
    std::uint16_t notice = 0;
    /// The "blocked" flag changed (the city is notified: city slot +0x18 →
    /// slot +0x168 with {type id, x, y, rotation, blocked}).
    bool blocked_changed = false;
    /// 8.8 output made (per output for two-output workshops).
    std::vector<std::uint32_t> made;
    /// The building sends a carrier / worker now.
    bool send_figure = false;
    /// Small farm line 1: an animal of world figure kind `animal_kind` is
    /// sent now (job 0x2e).
    bool send_animal = false;
    std::uint16_t animal_kind = 0;
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
    /// Per output: the part of `out` that carts have reserved (D +0x1a, u16 8.8, raised
    /// by slot +0xb4 = 0x10068460 and lowered by +0xb8 = 0x10068490 and the load;
    /// docs/re/transport.md §1.4).
    std::vector<std::uint32_t> reserved;
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
    /// Per line: reserved by carts (D +0x1c + 2i, 0x1006a540 / 0x10068f50).
    std::vector<std::uint32_t> reserved;
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
    /// Reserved by carts (D +0x1a, 0x10068460 / 0x10068490).
    std::uint32_t reserved = 0;
    std::uint32_t workers_out = 0;
    StatusTracker status;
};

/// One farm tick at world tick `t` (D: AnnoWelt.dll 0x1006ea90).
StepResult farm_step(Farm& f, const ProductionType& type, std::uint32_t t);

/// A source of world PRNG draws without a dependency on the generator (D:
/// docs/sim/state.md §4.2, §7.4): a context pointer and a function. The
/// driver forwards to its DrawSource, tests to a WorldRng (DrawRef::of). The
/// call returns the next u16 draw. A default-constructed DrawRef is a
/// precondition violation by the driver (there is no draw to give), but it is
/// fail-safe, not undefined: it returns 0 and never touches the generator.
struct DrawRef {
    static std::uint16_t none(void* /*ctx*/) { return 0; }
    void* ctx = nullptr;
    std::uint16_t (*fn)(void*) = &DrawRef::none;
    std::uint16_t operator()() const { return fn != nullptr ? fn(ctx) : std::uint16_t{0}; }
    /// A DrawRef over any object with `std::uint16_t next()` (WorldRng); `s`
    /// must outlive the reference.
    template <class Source> static DrawRef of(Source& s) {
        return DrawRef{&s, [](void* c) {
                           return static_cast<std::uint16_t>(static_cast<Source*>(c)->next());
                       }};
    }
};

/// The small farm's animal choice for line 1 (D: AnnoWelt.dll 0x1006f810–
/// 0x1006f838, table at 0x100c8d48): the world figure kind picked by a world
/// PRNG draw (NOT the island climate; 8 entries, the original's `draw & 7` is
/// `draw % size`), the kind of counter 0 (`base`; counter i counts kind base +
/// i), and how many animals of each counter may be out at once. Data with the
/// original values as defaults.
struct SmallFarmAnimals {
    std::vector<std::uint16_t> kind_by_draw = {107, 110, 107, 109, 107, 109, 107, 109};
    std::uint16_t base = 106;
    std::vector<std::uint8_t> limit = {1, 2, 0, 1, 1, 0, 0, 32};
};

/// A UNIVERSAL small farm (809) at runtime: two farm lines (D: class
/// 0x1006ff80, vtable 0x100c8d60; fields +0x18/+0x1a outputs, +0x1c/+0x1e
/// activity, +0x24/+0x26 raw stocks, +0x2a–+0x2e figure counters,
/// +0x2f/+0x30 line switches).
///
/// Invariant (kept by make_small_farm; the config validation and the load
/// path of docs/sim/state.md §3.1 must reject a violation): `line_on`, `out`,
/// `raw` and `activity` have the same size (one entry per line, two for a
/// UNIVERSAL type), and `figures_out` has at least one counter plus one per
/// animal kind the animal table can pick (`kind - base` < `figures_out.size()`).
/// small_farm_step does nothing at all (no step, no draw) on a SmallFarm that
/// violates the size part, instead of reading out of bounds.
struct SmallFarm {
    std::uint16_t last = 0;
    std::vector<bool> line_on;
    bool suspended = false;
    bool blocked = false;
    std::vector<std::uint32_t> out, raw; ///< per line, 8.8
    /// Activity per line: u16 in the original, kept 16-bit.
    std::vector<std::uint16_t> activity;
    /// Per line: reserved by carts (D +0x20 + 2i, 0x1006fa70 / 0x1006fad0).
    std::vector<std::uint32_t> reserved;
    /// Figures out per counter: 0 = line 0's workers, 1–4 = animals of kind
    /// SmallFarmAnimals::base + i (bytes +0x2a–+0x2e).
    std::vector<std::uint32_t> figures_out;
    StatusTracker status;
};

/// A small farm sized for `type` (one line per output) and `animals`: the
/// figure counters are the original five (+0x2a–+0x2e, the default), or more
/// when the table can pick a kind with `kind - base` of 5 or more (a modded
/// table), so every kind it can pick has a counter.
SmallFarm make_small_farm(const ProductionType& type,
                          const SmallFarmAnimals& animals = SmallFarmAnimals{});

/// One small farm tick at world tick `t` (D: AnnoWelt.dll 0x1006f660, line
/// step 0x1006f440). Each switched-on line works like a farm line against
/// the output-0 capacity: capacity first, then the input ("missing input"
/// after 75,000 ticks); a step that would use nothing makes nothing; the
/// status is cleared unless the other line is switched on and full. Line 0
/// sends a worker when it made nothing and none is out; line 1 sends an
/// animal and needs exactly one world PRNG draw per step in which it is
/// switched on (after the step gate and the suspension test, before the
/// limit tests; D 0x1006f810–0x1006f838): the kind is
/// `animals.kind_by_draw[draw % size]`, limited by that kind's limit and by
/// max_figures(0) over all animals. Both also need 384 / rate ≥ raw / need.
/// `draw` is called exactly once in such a step and never otherwise (not
/// gated, not suspended, line 1 off or absent). An empty table or a kind below
/// `base` (or beyond the counters or limits) still consumes the draw and sends
/// nothing: fail-closed where the original reads outside its tables; the
/// config validation (docs/sim/state.md §3.1) rejects such tables, so these
/// returns are defensive only.
StepResult small_farm_step(SmallFarm& f, const ProductionType& type, std::uint32_t t, DrawRef draw,
                           const SmallFarmAnimals& animals = SmallFarmAnimals{});

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

/// An ore deposit (D: AnnoWelt.dll class 0x100637a0, vtable 0x100c7508):
/// the remaining amount (+0x28, u32, 8.8; 2,000 t when created) and the
/// reserve flag (+0x30, set when created): with it, a shallow mine keeps
/// kDepositReserve untouched and a deep mine takes without using anything
/// up. Goods: the deposit object's type goods (slot +0x84). Position: the
/// island tile of the object.
struct Deposit {
    std::string goods;
    std::int32_t x = 0, y = 0;
    std::uint32_t remaining = 2000 * kTon;
    bool reserve = true;
};

/// The part of a deposit a shallow mine leaves while the reserve flag is
/// set: 0x64000 = 1,600 t (D: 0x10063650, 0x10063670).
inline constexpr std::uint32_t kDepositReserve = 0x64000;

/// What a mine can still take (slot +0x88, 0x10063650): remaining −
/// kDepositReserve for a shallow mine with the reserve flag (u32, as the
/// original), else remaining.
std::uint32_t deposit_remaining(const Deposit& d, bool deep);

/// Takes up to `rate` (slot +0x8c, 0x10063670); returns the amount taken.
/// With `depletes` false (world setting 14, bit 1, OQ-035) nothing is used
/// up and the full rate is returned. With the reserve flag, a deep mine
/// always gets the full rate, a shallow one stops at the reserve. Without
/// the flag: min(rate, remaining). u32 arithmetic as the original.
std::uint32_t deposit_extract(Deposit& d, std::uint32_t rate, bool deep, bool depletes);

/// The footprint a mine searches from: tile x, y and size w × h (by
/// rotation; D: 0x1006ddf0 builds it from the building's position and the
/// type's size).
struct MineArea {
    std::int32_t x = 0, y = 0;
    std::uint32_t w = 0, h = 0;
};

/// Whether a deposit at tile (x, y) is in reach of `area`: within a circle
/// of radius 7 around the footprint, half-widths 7, 7, 7, 6, 6, 5, 4, 2 for
/// rows 0–7 tiles beyond it (D: table built at 0x100787cb, test at
/// 0x10078899–0x100788f1). The original converts the object's float
/// position to tiles first; positions here are tiles.
bool deposit_in_reach(const MineArea& area, std::int32_t x, std::int32_t y);

/// The deposit a mine takes (D: 0x100787c0): of the deposits of `goods` in
/// reach, in island object order, the one with the largest
/// deposit_remaining(deep); the first one on a tie. nullopt if none.
std::optional<std::size_t> find_deposit(std::span<const Deposit> deposits, const std::string& goods,
                                        bool deep, const MineArea& area);

/// A mine (MINE) at runtime (D: class 0x1006e460, vtable 0x100c8990;
/// fields +0x18 output, +0x1c activity, +0x20 switched on, +0x21 deep,
/// +0x24 deposit).
struct Mine {
    std::uint16_t last = 0;
    bool switched_on = true;
    /// Deep mining: set when the type is attached (0x1006e270): for an iron
    /// ore mine (output goods 1, EISENERZ) when the type's research id is
    /// 0x39 (TIEFE_ERZMINE), for every other mine always.
    bool deep = true;
    bool blocked = false;
    std::uint32_t out = 0; ///< output stock, 8.8
    /// Activity average: a u16 in the original (+0x1c), kept 16-bit.
    std::uint16_t activity = 0;
    /// Reserved by carts (D +0x1a, 0x10068460 / 0x10068490).
    std::uint32_t reserved = 0;
    /// The deposit worked (index into the island's deposits); found when the
    /// mine joins its city (0x1006ddf0).
    std::optional<std::size_t> deposit;
    StatusTracker status;
};

/// A market or Kontor (HAUPT, MARKT, KONTOR: PRODTYPE 33-35; D class 0x1006c080,
/// vtable 0x100c86c8, tick 0x1006b6d0; T24). It has no stock of its own: its add,
/// remove and available slots forward to its city (docs/re/transport.md §1.4). The
/// members before carts_out are the Inert ones (the same creation values).
struct Market {
    std::uint16_t last = 0;
    bool blocked = false;
    bool suspended = false;
    StatusTracker status;
    /// Market carts out (D: the non-null pointers of +0x20 / +0x24, figure group 0).
    std::uint32_t carts_out = 0;
};

/// One market tick at world tick `t` (D 0x1006b6d0, docs/re/transport.md §1.5): the
/// gate (u16)(t - last) >= 500 adds 500 to `last`; then while fewer than
/// max_figures[0] carts are out (a type without production data has no carts) the
/// step asks for one (send_figure; the caller raises carts_out and sends job 0x34
/// with figure_ids[0]). Group 2 (+0x28 / +0x2c, a figure picked by a value from the
/// world) is not modelled: no shipped market type has one (P).
StepResult market_step(Market& m, const ProductionType& type, bool has_prod, std::uint32_t t);

/// The deep flag for a mine of `type` (D: 0x1006e270): false only for an
/// iron ore mine without the research id 0x39. `research_id` is the type's
/// research id (type slot +0xec).
bool mine_is_deep(const ProductionType& type, std::uint32_t research_id);

/// One mine tick at world tick `t` (D: AnnoWelt.dll 0x1006def0). No
/// suspension check (unlike the other buildings); the activity ages by
/// 63/64; storage full is checked first (45,000 ticks), then the deposit
/// (none or nothing taken: "missing input", 30,000 ticks). It takes up to
/// rate0 from its deposit; when that was not a full step or the deposit is
/// used up with it, the mine searches `deposits` again (find_deposit within
/// `area`) and, if nothing is left and it mines EISENERZ, raises notice
/// 0x15b (deep ore left) or 0x19. `depletes` is world setting 14, bit 1.
/// The fill-level graphic (slot +0x11c) is not modelled.
StepResult mine_step(Mine& m, const ProductionType& type, std::uint32_t t,
                     std::vector<Deposit>& deposits, const MineArea& area, bool depletes);

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
    /// the stock is set to the capacity instead (a stock above the capacity is
    /// lowered to it, as the original's wrapped u32 difference does; the sum
    /// itself is formed in u64, F35). Returns the
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
