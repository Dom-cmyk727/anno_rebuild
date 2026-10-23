// Immutable type data: the building types of Anlagen.dat as the simulation
// sees them, the class table that says what a PRODTYPE does at runtime, and
// the goods table (spec: docs/sim/state.md §4.4, Appendices A–C; evidence:
// AnnoGame.dll type reader 0x10009240, name tables 0x100138e0 (PRODTYPE),
// 0x10013be0 (goods), 0x10013cf0 (research), AnnoWelt.dll factory 0x10024010).
//
// A TypeRegistry is built once (from the 921 entries of Anlagen.dat or by hand
// for tests and the synthetic scenarios), never changes, and is shared by
// every copy of a World through shared_ptr<const>. Its digest is hashed once in
// the header scope (state_fields.hpp), so two peers with different type data
// diverge at tick 0. Nothing here is state.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <anno/formats/figuren.hpp>
#include <anno/formats/result.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/production.hpp>

namespace anno::sim {

/// Which production tick a class runs (D15 / OQ-050: a class whose tick body is
/// not ported is inert).
enum class TickKind : std::uint8_t {
    /// Inert: the class tick is not ported (or is a stub); `last` is NOT touched.
    None,
    Workshop,
    WeaponsWorkshop,
    Farm,
    FurTrapper,
    Mine,
    SmallFarm,
    /// The market and the Kontor (D 0x1006b6d0, T24): the cart send.
    Market
};

/// The runtime class of a building by PRODTYPE (D: byte table 0x1002528c, jump
/// table 0x1002521c of the factory 0x10024010; Appendix C). Append-only.
enum class RuntimeClass : std::uint8_t {
    Plain,
    Workshop,
    DualWorkshop,
    WeaponsWorkshop,
    Farm,
    FurTrapper,
    Mine,
    SmallFarm,
    Quarry,
    Hunter,
    Whaler,
    Shipyard,
    Cannonworks,
    Market,
    MarketCombi,
    MarketStall,
    Inn,
    PublicService,
    Doctor,
    School,
    Castle,
    FireBrigade,
    House,
    Tower,
    CannonTower,
    /// Number of classes (not a class); the class table has this many entries.
    Count_
};

/// The Payload alternative a building of a class holds; it is the save tag of
/// Building::payload (docs/sim/state.md §9.3). Append-only.
enum class PayloadTag : std::uint8_t {
    Inert = 0,
    Workshop = 1,
    WeaponsWorkshop = 2,
    Farm = 3,
    SmallFarm = 4,
    Mine = 5,
    Market = 6
};

struct ClassInfo {
    RuntimeClass cls = RuntimeClass::Plain;
    TickKind tick = TickKind::None;
    /// The Payload alternative a building of this class must hold
    /// (check_invariants, load); FurTrapper -> Farm, Quarry/Plain/... -> Inert.
    PayloadTag payload = PayloadTag::Inert;
    /// D: the compare constant of the class tick (`(u16)(t - last) >= period`;
    /// 0 = the tick has no stamp gate: Quarry (stub) and Castle (byte counter)).
    /// The update rule of `last` is unread for the unported classes, so the
    /// scheduler never uses it for them (D15). Addresses: docs/sim/state.md
    /// Appendix C.
    std::uint32_t gate_period = 0;
};

/// One entry of the goods table (D: AnnoGame.dll 0x10013be0, ids 0..0x8e).
struct GoodsInfo {
    /// The name as the files spell it (Latin-1 bytes); "UNUSED" for an id
    /// without a name.
    std::string name;
    /// BGruppen [WAREN] research id (Appendix B), 0 = none (OQ-034).
    std::uint32_t forsch = 0;
};

/// A build cost in goods: the amount in 8.8 tons.
struct CostEntry {
    GoodsId goods = 0;
    std::uint32_t amount_88 = 0;
};

/// An entrance point of a building type in sixths of a tile from the type's
/// north-west corner at rotation 0 (D: AnnoGame.dll 0x10009a48-0x10009aa8, the
/// ANLAGEN record ENTRANCE: length / 8 points of two f32, x then y, at type +0x1a8,
/// count +0x67; P: every point lies on a sub-cell centre k/6 with k odd,
/// docs/re/transport.md §3). The searcher's goals of a building (0x10072be0) and a
/// figure's start (0x1003acc0) are these points rotated by AnnoGame 0x100084c0
/// (rotated_entrance in transport.hpp).
struct EntrancePoint {
    std::uint32_t x6 = 0, y6 = 0;
};

/// A figure type of FigurenHeader.dat as the transport reads it (D: AnnoGame.dll
/// 0x1000fe00 into the type object, vtable 0x10021d10; docs/re/transport.md §1, §4).
/// Integers only: the f32 values of the file are converted once at load, in double
/// precision as the production floats are (docs/re/production.md §8).
struct FigureType {
    std::uint32_t id = 0;
    /// The walking speed per tick in 2^-24 tile: round(f32 HEAD byte 4 · 0.2f · 2^24)
    /// (the type's +0x10, D 0x1000fb10, times the figure tick's time scale 0.2f,
    /// 0x100c3270; the original multiplies in x87 per call, OQ-162). P: the market
    /// cart 220, 0.016, gives 53687.
    std::uint32_t speed_q24 = 0;
    /// LOGIC[0x11] << 8 (type +0xf2, slot +0x84 = 0x1000fb50): the load a figure
    /// reserves and takes (docs/re/transport.md §1.1, §1.2).
    std::uint32_t capacity_88 = 0;
    /// LOGIC[0x10] (type +0xff, slot +0xac = 0x1000faf0): the searcher's walking layer
    /// (what a layer allows is open, OQ-160).
    std::uint32_t layer = 0;
    /// The load step's wait in ticks: the LOGIC f32 at byte 4 (1.0 when not above 0,
    /// 0x100101f4-0x10010211, type +0x30) at the wait rate 0.01 (0x10035260) and the
    /// time scale 0.2: round(500 · v) (OQ-162).
    std::uint32_t load_ticks = 0;
};

struct BuildingType {
    TypeId id;
    /// The PRODTYPE number (D AnnoGame 0x100138e0), 0 = UNUSED.
    std::uint32_t prodtype = 0;
    RuntimeClass cls = RuntimeClass::Plain;
    /// HEAD[0], HEAD[1] (D 0x10009376); swapped on odd rotation.
    std::uint32_t width = 1, height = 1;
    /// FORSCHTYPE name index in table 0x100250f8 (D 0x10013cf0), 0 = none.
    std::uint32_t research_id = 0;
    /// HEAD[8..9] (u16, D 0x1000944d–0x10009452: type +0x1c).
    std::uint32_t cost_gold = 0;
    /// The goods of the build cost with a non-zero amount, in ascending goods
    /// order: HEAD[5] = WERKZEUG 0x29, [4] = HOLZ 0x2a, [3] = ZIEGEL 0x2b,
    /// [6] = MAMOR 0x2c, each << 8 (D 0x1000a5a0).
    std::vector<CostEntry> cost_goods;
    /// HEAD u16 [0x2c], u8 [0x31], u8 [0x11], u16 [0x1c], u8 [0x0a]
    /// (D 0x10009240: type +0x70, +0x79, +0x65, +0x12, +0x7a; docs/re/
    /// fire-and-ships.md §2.1).
    std::uint32_t max_hp = 0, can_burn = 0, action_radius = 0, flags_word = 0, rubble = 0;
    /// Public group: from WARE[0] in 0x4c..0x5a: 1 << (g - 0x4c); 0x51 -> 0x30,
    /// 0x53 -> 0xc0, 0x54 -> 0x1c0 (D AnnoGame 0x1000a600).
    std::uint32_t service_mask = 0;
    /// PROD3 u16 [0x00] and [0x02] (size = running-cost stats, 2); which of them
    /// applies per instance (index P) is O-9. {0, 0} for a type without PROD3.
    std::vector<std::uint32_t> running_cost;
    /// Resolved from the first two WARE / ROHSTOFF names (the original reads
    /// exactly two, D: AnnoGame.dll 0x1000a1f4, 0x1000a2e2; further names are
    /// ignored, as there); an unresolved name is a load error.
    std::vector<GoodsId> output_goods, input_goods;
    /// The HEAD record as the original's reader sees it (a 56-byte buffer,
    /// shorter records zero-filled, D 0x10009334): kept whole, never read (the
    /// bytes without a known consumer are O-21).
    std::vector<std::uint8_t> head_raw;
    /// The ENTRANCE points in file order (empty when the entry has none: P 595 of 921).
    std::vector<EntrancePoint> entrances;
    bool has_prod = false;
    /// Valid when has_prod.
    ProductionType prod;
    /// DERIVED, not file state: the walk class of the tile this type occupies for the
    /// transport search (0 = does not block, 1 = road, 2 = a building). Computed in
    /// TypeRegistry::finish from `flags_word & kRoadFlag` (road) and
    /// `layout.list_for(prodtype)` (a building occupies its footprint) — the rule of
    /// TileTest in transport.cpp, moved to a per-type field (task T27). It is NOT in
    /// `fields()` and NOT in the registry digest (the digest is a function of the parsed
    /// file only; types_test "registry digest changes with every field" pins it), so it
    /// is left out of the per-field mutation list there.
    std::uint8_t walk_class = 0;
};

/// The immutable type data of a world.
class TypeRegistry {
  public:
    class Builder;

    /// nullptr = unknown id: commands fail closed (F4).
    [[nodiscard]] const BuildingType* find(TypeId) const;
    /// The figure type with this FigurenHeader id; nullptr when the registry has none
    /// (a registry built without FigurenHeader.dat has no figure types).
    [[nodiscard]] const FigureType* find_figure_type(std::uint32_t id) const;
    /// All figure types in ascending id order.
    [[nodiscard]] std::span<const FigureType> figure_types() const { return figure_types_; }
    /// The class table entry of `cls`. A value outside the enum (a registry is
    /// built with exactly RuntimeClass::Count_ entries) gives the first entry.
    [[nodiscard]] const ClassInfo& class_info(RuntimeClass) const;
    [[nodiscard]] const GoodsInfo* goods(GoodsId) const;
    /// The first goods with this name ("UNUSED" is id 0).
    [[nodiscard]] std::optional<GoodsId> goods_by_name(std::string_view) const;
    /// The largest of width and height over all types (F3), 1 for an empty registry.
    [[nodiscard]] std::uint32_t max_footprint() const;
    /// The growing-item type (ROHSTWACHS / ROHSTDUERR) whose entry id is the
    /// tile object id `object`; nullptr if there is none (grow() contract:
    /// the successor is `object + 1`, O-13).
    [[nodiscard]] const BuildingType* find_growing(std::uint32_t object) const;
    /// FNV-1a 64 over every field of every type, the goods and the class
    /// tables (state_fields.hpp style: fixed widths, little endian); hashed once
    /// in the header scope.
    [[nodiscard]] std::uint64_t digest() const { return digest_; }
    /// The digest of these tables exactly as given (types in the order given,
    /// which build() sorts by id first), without checking or sealing them: for
    /// tests that perturb one field at a time, including combinations that
    /// build() refuses.
    [[nodiscard]] static std::uint64_t digest_of(std::span<const BuildingType> types,
                                                 std::span<const GoodsInfo> goods,
                                                 std::span<const ClassInfo> classes,
                                                 std::span<const FigureType> figures = {});
    /// Whether the registry fits a configuration (docs/sim/state.md §4.4, §9.6):
    /// the checks of build() that depend on `cfg` (the build cost goods lie in
    /// SimConfig::build_goods_first .. + build_goods_count, service masks fit
    /// service_bits, the extent of cfg.layout), run again on the sealed
    /// registry. build() runs them with the config it was given;
    /// World::create/load (S5) and the header read of the state stream run them
    /// with the world's config, because a registry is shared between worlds and
    /// the config of a loaded header may differ from the one it was built with.
    /// Empty when it fits, else one message naming the type.
    [[nodiscard]] std::string validate_against(const SimConfig& cfg) const;
    /// All types in ascending id order.
    [[nodiscard]] std::span<const BuildingType> types() const { return types_; }
    [[nodiscard]] std::span<const GoodsInfo> goods_table() const { return goods_; }
    [[nodiscard]] std::span<const ClassInfo> class_table() const { return classes_; }

    /// The types of all entries of Anlagen.dat (ANLAGEN/ENTRY). `goods` is the
    /// goods table (original_goods() for the original); an unknown PRODTYPE,
    /// FORSCHTYPE or goods name, a duplicate id or an entry without HEAD is a
    /// load error naming the entry. A set of types the simulation could not run
    /// is refused too (docs/sim/state.md §4.4, F25): a zero footprint, a build
    /// cost good outside the configured build goods, a class that ticks but
    /// whose type has no production data of that kind (a PROD3 record that is
    /// not 68 bytes, a Mine without an output), and a class table whose rows
    /// disagree with their index or with their tick and payload. `cfg` bounds
    /// what the registry may contain (service_mask bits below service_bits, the
    /// build goods, the building extent of cfg.layout).
    static formats::Result<std::shared_ptr<const TypeRegistry>, std::string>
    build(std::span<const formats::figuren::Entry> entries, std::vector<GoodsInfo> goods_in,
          const SimConfig& cfg);
    /// build() plus the figure types of FigurenHeader.dat (FIGUREN/ENTRY: HEAD and
    /// LOGIC, figure_type_from_entry); a duplicate figure id or a HEAD shorter than 8
    /// bytes is a load error naming the entry.
    static formats::Result<std::shared_ptr<const TypeRegistry>, std::string>
    build(std::span<const formats::figuren::Entry> entries, std::vector<GoodsInfo> goods_in,
          const SimConfig& cfg, std::span<const formats::figuren::Entry> figure_entries);

    /// A registry without types (the original goods and class tables): what a
    /// default-constructed World holds. Shared, immutable.
    static std::shared_ptr<const TypeRegistry> empty();

  protected:
    TypeRegistry() = default;

  private:
    /// Sorts, checks and seals a set of types (shared by build() and the Builder).
    static formats::Result<std::shared_ptr<const TypeRegistry>, std::string>
    finish(std::vector<BuildingType> types, std::vector<GoodsInfo> goods_in,
           std::vector<ClassInfo> classes, const SimConfig& cfg,
           std::vector<FigureType> figures = {});

    std::vector<BuildingType> types_;      // ascending id; lookup by binary search
    std::vector<FigureType> figure_types_; // ascending id
    std::vector<GoodsInfo> goods_;
    std::vector<ClassInfo> classes_;
    std::uint32_t max_footprint_ = 1;
    std::uint64_t digest_ = 0;
};

/// A non-null handle to the immutable registry of a world. A moved-from handle
/// keeps pointing at the registry (a move copies the pointer), like a
/// moved-from WorldRng keeps its table (rng.hpp): a World that has been moved
/// from still has a registry to read, so types(), state_hash() and the header
/// scope never dereference null. A null argument is replaced by the empty
/// registry.
class RegistryRef {
  public:
    explicit RegistryRef(std::shared_ptr<const TypeRegistry> registry)
        : registry_(registry ? std::move(registry) : TypeRegistry::empty()) {}
    RegistryRef(const RegistryRef&) = default;
    RegistryRef& operator=(const RegistryRef&) = default;
    RegistryRef(RegistryRef&& other) noexcept : registry_(other.registry_) {}
    RegistryRef& operator=(RegistryRef&& other) noexcept {
        registry_ = other.registry_;
        return *this;
    }
    ~RegistryRef() = default;

    [[nodiscard]] const TypeRegistry& operator*() const { return *registry_; }
    [[nodiscard]] const TypeRegistry* operator->() const { return registry_.get(); }
    [[nodiscard]] const std::shared_ptr<const TypeRegistry>& shared() const { return registry_; }

  private:
    std::shared_ptr<const TypeRegistry> registry_; // never null
};

/// Hand-built registries for tests and the synthetic scenarios, so that no
/// test touches formats::figuren::Entry (CI has no game files). Defaults: the
/// original goods and class tables, SimConfig{} limits. The helper
/// add_* functions resolve their goods names against the goods table at the
/// time of the call, so set goods() first; an unknown name is reported by
/// build(). add_plain and the add_* helpers derive the runtime class with the
/// layout of the config() set at the time of the call, so call config() first
/// when the layout is not the original's.
class TypeRegistry::Builder {
  public:
    Builder();
    Builder& goods(std::vector<GoodsInfo> table);
    Builder& classes(std::vector<ClassInfo> table);
    Builder& config(const SimConfig& cfg);
    /// Adds a fully specified type (nothing is derived).
    Builder& add(BuildingType type);
    /// A type without production data: `prodtype` decides the class (Plain for a
    /// number that is not a building), 1 x 1 tiles. build() refuses it when its
    /// class ticks (a Workshop, Farm, ... needs production data): use it for
    /// classes whose tick is None (houses, markets, roads, ...).
    Builder& add_plain(TypeId id, std::uint32_t prodtype);
    /// HANDWERK workshop: one output from one input (`input` empty = none).
    /// rate / need are the per-step integers of Anlagen.dat (docs/re/
    /// production.md F1: weaver 6 / 11, capacity 5 t).
    Builder& add_workshop(TypeId id, std::string_view output, std::string_view input,
                          std::uint32_t rate, std::uint32_t need, std::uint32_t cap_tons,
                          std::uint32_t max_figures = 1);
    /// HANDWERKWAFFEN: two output lines from two inputs.
    Builder& add_weapons_workshop(TypeId id, std::string_view output0, std::string_view output1,
                                  std::string_view input0, std::string_view input1,
                                  std::uint32_t rate, std::uint32_t cap_tons);
    /// PLANTAGE farm: one output from one raw input (F1: grain farm 37 / 80).
    Builder& add_farm(TypeId id, std::string_view output, std::string_view input,
                      std::uint32_t rate, std::uint32_t need, std::uint32_t cap_tons,
                      std::uint32_t max_figures = 1);
    /// PELZJAEGER fur trapper (same fields as the farm, class FurTrapper).
    Builder& add_fur_trapper(TypeId id, std::string_view output, std::string_view input,
                             std::uint32_t rate, std::uint32_t need, std::uint32_t cap_tons);
    /// UNIVERSAL small farm: two lines.
    Builder& add_small_farm(TypeId id, std::string_view output0, std::string_view output1,
                            std::string_view input0, std::string_view input1, std::uint32_t rate0,
                            std::uint32_t rate1, std::uint32_t need0, std::uint32_t need1,
                            std::uint32_t cap_tons);
    /// MINE producing `output` at `rate` per step.
    Builder& add_mine(TypeId id, std::string_view output, std::uint32_t rate,
                      std::uint32_t cap_tons);
    /// A market (PRODTYPE 34 MARKT, class Market) with figure group 0 = one figure
    /// type `cart` and at most `max_carts` out, action radius `radius`, footprint w x h,
    /// and the output capacity 5 t of every shipped market type (P).
    Builder& add_market(TypeId id, std::uint32_t cart, std::uint32_t max_carts,
                        std::uint32_t radius, std::uint32_t w = 3, std::uint32_t h = 3);
    /// Sets the entrance points of the type added last with this id.
    Builder& entrances(TypeId id, std::vector<EntrancePoint> points);
    /// Sets the footprint of the type added last with this id.
    Builder& footprint(TypeId id, std::uint32_t w, std::uint32_t h);
    /// Sets the action radius (HEAD[0x11], type +0x65) of the type added last with this
    /// id: the area of its figures' search (docs/re/transport.md §1.1).
    Builder& action_radius(TypeId id, std::uint32_t radius);
    /// Sets the HEAD flag word (+0x12, HEAD u16 [0x1c]) of the type added last with
    /// this id: 0x4000 marks a road or bridge (docs/re/production.md §2.3).
    Builder& flags_word(TypeId id, std::uint32_t flags);
    /// Sets the figure ids of figure group 0 (PROD3 0x32, the first `figure_type_count[0]`
    /// entries) of the production type added last with this id.
    Builder& figure_ids(TypeId id, std::vector<std::uint32_t> ids);
    /// Adds a figure type.
    Builder& figure_type(FigureType type);

    /// Seals the registry. Errors: a duplicate id, a goods name that did not
    /// resolve, a class table that does not have RuntimeClass::Count_ entries,
    /// and every inconsistency TypeRegistry::build refuses (see there).
    [[nodiscard]] formats::Result<std::shared_ptr<const TypeRegistry>, std::string> build() const;

  private:
    GoodsId resolve(std::string_view name);
    BuildingType make_production(TypeId id, std::uint32_t prodtype, std::string_view prodtype_text,
                                 BuildingClass kind, std::string_view output0,
                                 std::string_view output1, std::string_view input0,
                                 std::string_view input1);

    BuildingType* last_with(TypeId id);

    std::vector<BuildingType> types_;
    std::vector<FigureType> figures_;
    std::vector<GoodsInfo> goods_;
    std::vector<ClassInfo> classes_;
    SimConfig cfg_;
    std::string error_;
};

// ---- the tables (types.cpp, goods_table.cpp)

/// The runtime class of a PRODTYPE number (Appendix C); nullopt for 0 and for
/// numbers that are not a building in `layout` (ListLayout::list_for gives no
/// list: roads, walls and scenery are tile objects, not buildings). The
/// numbers inside the layout's blocks that no class names are Plain. The extent
/// of "is a building" is the layout's, so a modded layout with more blocks and
/// this function cannot disagree.
std::optional<RuntimeClass> class_for_prodtype(std::uint32_t prodtype, const ListLayout& layout);
/// The same for the original layout (ListLayout::original(): numbers 1..192).
std::optional<RuntimeClass> class_for_prodtype(std::uint32_t prodtype);

/// The class table: one entry per RuntimeClass, in enum order (Appendix C).
std::vector<ClassInfo> original_class_table();

/// PRODTYPE name -> number (D AnnoGame 0x100138e0, 1..0x1a0); "UNUSED" and
/// the empty string are 0; an unknown name is nullopt.
std::optional<std::uint32_t> prodtype_by_name(std::string_view name);

/// The PRODTYPE name of a number; empty for a number without a name.
std::string_view prodtype_name(std::uint32_t number);

/// FORSCHTYPE name -> research id (D AnnoGame 0x10013cf0, 1..0x40, Appendix B);
/// "UNUSED" and the empty string are 0; an unknown name is nullopt.
std::optional<std::uint32_t> research_by_name(std::string_view name);

/// The goods table of the original (D AnnoGame 0x10013be0): ids 0..0x8e, every
/// id without a name is "UNUSED" (Appendix A), with the research requirements
/// of BGruppen [WAREN].
std::vector<GoodsInfo> original_goods();

/// The public-need bits a goods number stands for (D AnnoGame 0x1000a600):
/// 1 << (g - 0x4c) for g in 0x4c..0x5a, except 0x51 -> 0x30, 0x53 -> 0xc0,
/// 0x54 -> 0x1c0; 0 for any other goods.
std::uint32_t service_mask_for_goods(GoodsId goods);

} // namespace anno::sim
