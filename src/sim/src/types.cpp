// The type registry, the class table and the name tables (spec:
// docs/sim/state.md §4.4, Appendices A–C).
//
// Evidence: AnnoGame.dll type reader 0x10009240 (HEAD branch 0x10009334–
// 0x100094cd, PROD3 branch 0x100094d2–0x1000972f), name lookups 0x100138e0
// (PRODTYPE) and 0x10013cf0 (research), cost getter 0x1000a5a0, service mask
// 0x1000a600; AnnoWelt.dll factory 0x10024010. The name tables below were read
// from the AnnoGame.dll pointer tables on 2026-10-19.
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include <anno/formats/bytes.hpp>
#include <anno/sim/archive.hpp>
#include <anno/sim/field_count.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

namespace fig = formats::figuren;

namespace {

/// A name of a table: the number the original's lookup returns it for.
struct Named {
    std::uint32_t number;
    std::string_view name;
};

/// PRODTYPE names (D: AnnoGame.dll 0x100138e0, numbers 1..0x1a0, blocks of 32;
/// every other number has no name and reads as UNUSED = 0).
constexpr std::array kProdtypeNames = {
    Named{1, "HANDWERK"},    Named{2, "PLANTAGE"},        Named{3, "MINE"},
    Named{4, "WEIDETIER"},   Named{5, "JAGDHAUS"},        Named{6, "FISCHER"},
    Named{7, "WERFT"},       Named{8, "STEINMETZ"},       Named{9, "UNIVERSAL"},
    Named{10, "SCHAFFARM"},  Named{11, "KANONENBAU"},     Named{12, "HANDWERKDUAL"},
    Named{13, "STEINBRUCH"}, Named{14, "EISLOCH"},        Named{15, "WALFAENGER"},
    Named{16, "PELZJAEGER"}, Named{17, "HANDWERKWAFFEN"}, Named{33, "HAUPT"},
    Named{34, "MARKT"},      Named{35, "KONTOR"},         Named{36, "MARKTKOMBI"},
    Named{65, "BRUNNEN"},    Named{66, "WIRT"},           Named{67, "KAPELLE"},
    Named{68, "KIRCHE"},     Named{69, "BADEHAUS"},       Named{70, "THEATER"},
    Named{71, "ARZT"},       Named{72, "SCHULE"},         Named{73, "HOCHSCHULE"},
    Named{74, "SCHLOSS"},    Named{75, "GALGEN"},         Named{76, "GERICHT"},
    Named{77, "FEUERWEHR"},  Named{78, "BIBLIOTHEK"},     Named{79, "PAVILLION"},
    Named{97, "PIONIER"},    Named{98, "SIEDLER"},        Named{99, "BUERGER"},
    Named{100, "KAUFLEUTE"}, Named{101, "ARISTOKRATEN"},  Named{129, "WACHTURM"},
    Named{130, "MILITAR"},   Named{131, "KANONENTURM"},   Named{161, "MARKTSTAND"},
    Named{193, "STRASSE"},   Named{194, "RAMPE"},         Named{195, "RAMPELOW"},
    Named{196, "RAMPEHIGH"}, Named{197, "PIER"},          Named{198, "BRUECKE"},
    Named{199, "PLATZ"},     Named{200, "GAUKLERPLATZ"},  Named{201, "ZAUN"},
    Named{225, "ROHSTOFF"},  Named{226, "ROHSTWACHS"},    Named{227, "ROHSTDUERR"},
    Named{257, "MAUER"},     Named{258, "MAUERUP"},       Named{259, "TOR"},
    Named{289, "DENKMAL"},   Named{290, "TRIUMPH"},       Named{291, "ZIERDE"},
    Named{321, "FELSEN"},    Named{322, "KLIPPE"},        Named{323, "STEIN"},
    Named{324, "ROHSTSPEZ"}, Named{325, "TIERHOEHLE"},    Named{353, "FLUSS"},
    Named{354, "MEER"},      Named{355, "WASSERFALL"},    Named{385, "RUINE"}};

/// FORSCHTYPE names (D: AnnoGame.dll 0x10013cf0, numbers 1..0x40; Appendix B).
// clang-format off
constexpr std::array kResearchNames = {
    Named{1, "WAFFEN_FERTIGUNG_01"}, Named{2, "KATAPULT"},
    Named{3, "WAFFEN_FERTIGUNG_02"}, Named{4, "BOGNER"},
    Named{5, "BELAGERUNGSMASCHINE"}, Named{6, "KAVALLERIE"},
    Named{7, "GEHAERTETE_SPITZEN"}, Named{8, "GEFIEDERTER_SCHAFT"},
    Named{9, "ARMBRUST"}, Named{10, "ARMBRUST_REICHWEITE"},
    Named{11, "BRANDPFEILE"}, Named{12, "MUSKETE"},
    Named{13, "RADSCHLOSS"}, Named{17, "RUESTUNG_INFAN_01"},
    Named{18, "RUESTUNG_KAVAL_01"}, Named{19, "RUESTUNG_INFAN_02"},
    Named{20, "RUESTUNG_KAVAL_02"}, Named{21, "KANONE"},
    Named{22, "KANONE_GENORMT_KALIBER"}, Named{23, "KANONE_GENORMT_SPRENG"},
    Named{24, "KANONE_MOERSER"}, Named{25, "KANONE_MOERSER_PULVER"},
    Named{26, "KANONE_MOERSER_BRAND"}, Named{33, "MITTLERES_HANDEL"},
    Named{34, "MITTLERES_KRIEG"}, Named{35, "GROSSES_HANDEL"},
    Named{36, "GROSSES_KRIEG"}, Named{37, "VERSTAERKTER_RUMPF"},
    Named{38, "KANONE_SHIP"}, Named{49, "BRUNNEN"},
    Named{50, "GR_WEBER"}, Named{51, "FEUERWEHR"},
    Named{52, "TIEFER_BRUNNEN"}, Named{53, "STEIN_BRUECKE"},
    Named{54, "MEDICUS"}, Named{55, "BIBLIOTHEK"},
    Named{56, "SCHNELLE_HEILUNG"}, Named{57, "TIEFE_ERZMINE"},
    Named{58, "GERICHT"}};
// clang-format on

template <class Table>
std::optional<std::uint32_t> number_of(const Table& table, std::string_view name) {
    if (name.empty() || name == "UNUSED") {
        return 0;
    }
    for (const Named& n : table) {
        if (n.name == name) {
            return n.number;
        }
    }
    return std::nullopt;
}

formats::Result<BuildingType, std::string>
type_from_entry(const fig::Entry& e, const std::vector<GoodsInfo>& goods, const SimConfig& cfg) {
    const std::string who = "entry " + std::to_string(e.id) + ": ";
    const auto head = e.record("HEAD");
    if (!head) {
        return who + "no HEAD record";
    }
    BuildingType t;
    t.id = TypeId{e.id};
    // The reader's HEAD buffer has 56 bytes: 14 dwords are zeroed and the record
    // is read into it (D: 0x10009334–0x1000934f); the records of Anlagen.dat
    // have 36, 44, 48, 52 or 56 bytes.
    t.head_raw.assign(56, 0);
    for (std::size_t i = 0; i < t.head_raw.size() && i < head->size(); ++i) {
        t.head_raw[i] = (*head)[i];
    }
    const formats::Bytes h = t.head_raw;
    const std::string prodtype = e.record_str("PRODTYPE").value_or("");
    const auto number = prodtype_by_name(prodtype);
    if (!number) {
        return who + "unknown PRODTYPE '" + prodtype + "'";
    }
    t.prodtype = *number;
    t.cls = class_for_prodtype(t.prodtype, cfg.layout).value_or(RuntimeClass::Plain);
    // HEAD bytes (D 0x10009376 footprint, 0x1000944d–0x10009452 gold, the rest
    // 0x10009384–0x100093f8; costs by 0x1000a5a0).
    t.width = h[0];
    t.height = h[1];
    t.cost_gold = formats::u16_le(h, 8);
    const std::pair<GoodsId, std::size_t> cost_bytes[] = {
        {0x29, 5}, {0x2a, 4}, {0x2b, 3}, {0x2c, 6}};
    for (const auto& [g, offset] : cost_bytes) {
        if (h[offset] != 0) {
            t.cost_goods.push_back(CostEntry{g, std::uint32_t{h[offset]} << 8});
        }
    }
    t.rubble = h[0x0a];
    t.action_radius = h[0x11];
    t.flags_word = formats::u16_le(h, 0x1c);
    t.max_hp = formats::u16_le(h, 0x2c);
    t.can_burn = h[0x31];
    // ENTRANCE (D AnnoGame 0x10009a48-0x10009aa8): length / 8 points of two f32 (x, y)
    // in tiles; a tail of fewer than 8 bytes is not read (`shr eax, 3`). The points lie
    // on sub-cell centres (P); they are kept in sixths of a tile. A point that is not
    // finite, negative or beyond 2^24 tiles is a load error (the original keeps any
    // float; none in the file).
    if (const auto ent = e.record("ENTRANCE")) {
        for (std::size_t i = 0; i + 8 <= ent->size(); i += 8) {
            EntrancePoint p;
            for (std::size_t k = 0; k < 2; ++k) {
                const auto f = std::bit_cast<float>(formats::u32_le(*ent, i + 4 * k));
                const double six = static_cast<double>(f) * 6.0;
                if (!std::isfinite(six) || six < 0.0 || six > 16777216.0 * 6.0) {
                    return who + "ENTRANCE point " + std::to_string(i / 8) +
                           " is not a position on the type";
                }
                const auto v = static_cast<std::uint32_t>(std::lround(six));
                (k == 0 ? p.x6 : p.y6) = v;
            }
            t.entrances.push_back(p);
        }
    }
    const std::string research = e.record_str("FORSCHTYPE").value_or("");
    const auto research_id = research_by_name(research);
    if (!research_id) {
        return who + "unknown FORSCHTYPE '" + research + "'";
    }
    t.research_id = *research_id;
    auto resolve = [&](const char* record, std::vector<GoodsId>& out) -> std::string {
        const auto bytes = e.record(record);
        if (!bytes) {
            return {};
        }
        for (const std::string& name : ware_names(*bytes)) {
            const auto it = std::find_if(goods.begin(), goods.end(),
                                         [&](const GoodsInfo& g) { return g.name == name; });
            if (it == goods.end()) {
                return who + record + " names unknown goods '" + name + "'";
            }
            out.push_back(static_cast<GoodsId>(it - goods.begin()));
        }
        return {};
    };
    if (std::string problem = resolve("WARE", t.output_goods); !problem.empty()) {
        return problem;
    }
    if (std::string problem = resolve("ROHSTOFF", t.input_goods); !problem.empty()) {
        return problem;
    }
    if (!t.output_goods.empty()) {
        t.service_mask = service_mask_for_goods(t.output_goods[0]);
    }
    t.running_cost = {0, 0};
    const auto prod3 = e.record("PROD3");
    // A PROD3 of another size means "no production data" here, as in the original's
    // reader (D 0x100094d2: the PROD3 branch needs the 68-byte record); finish() refuses
    // such a type when its class ticks.
    if (prod3 && prod3->size() == 68) {
        t.running_cost = {formats::u16_le(*prod3, 0), formats::u16_le(*prod3, 2)};
    }
    auto prod = production_type(e);
    if (!prod) {
        return prod.error(); // already names the entry
    }
    if (prod.value()) {
        t.has_prod = true;
        t.prod = std::move(*prod.value());
    }
    return t;
}

// ---- the digest: one field list over every field of the type data

static_assert(field_count<ProductionType>() == 18,
              "a ProductionType member needs a line in digest_production");
// 22 since T27: the last member, the derived walk_class, is deliberately NOT hashed;
// digest_type skips it. (cls, also derived from the build layout, is hashed.)
static_assert(field_count<BuildingType>() == 22,
              "a BuildingType member needs a line in digest_type (walk_class excepted: derived)");
static_assert(field_count<EntrancePoint>() == 2,
              "an EntrancePoint member needs a line in digest_type");
static_assert(field_count<FigureType>() == 5, "a FigureType member needs a line in the digest");
static_assert(field_count<GoodsInfo>() == 2, "a GoodsInfo member needs a line in the digest");
static_assert(field_count<ClassInfo>() == 4, "a ClassInfo member needs a line in the digest");
static_assert(field_count<CostEntry>() == 2, "a CostEntry member needs a line in the digest");

template <class A, class V> void u32s(A& a, const V& v) {
    ar::seq(a, v, 4, [&](const auto& x) { a.u32(x); });
}

template <class A> void digest_production(A& a, const ProductionType& p) {
    a.u32(p.id);
    a.str(p.prodtype);
    a.u8(static_cast<std::uint8_t>(p.kind));
    ar::seq(a, p.outputs, 8, [&](const std::string& s) { a.str(s); });
    ar::seq(a, p.inputs, 8, [&](const std::string& s) { a.str(s); });
    u32s(a, p.capacity);
    u32s(a, p.output_per_step);
    u32s(a, p.input_per_step);
    a.u32(p.time0_128);
    a.u32(p.time1_128);
    u32s(a, p.max_figures);
    u32s(a, p.figure_type_count);
    u32s(a, p.figure_ids);
    a.u32(p.stages);
    a.u32(p.stage_shift);
    a.u32(p.growth_period);
    a.u32(p.display_rate0);
    a.u32(p.display_rate1);
}

template <class A> void digest_type(A& a, const BuildingType& t) {
    a.u32(t.id.v);
    a.u32(t.prodtype);
    a.enumv(t.cls);
    a.u32(t.width);
    a.u32(t.height);
    a.u32(t.research_id);
    a.u32(t.cost_gold);
    ar::seq(a, t.cost_goods, 8, [&](const CostEntry& c) {
        a.u32(c.goods);
        a.u32(c.amount_88);
    });
    a.u32(t.max_hp);
    a.u32(t.can_burn);
    a.u32(t.action_radius);
    a.u32(t.flags_word);
    a.u32(t.rubble);
    a.u32(t.service_mask);
    u32s(a, t.running_cost);
    u32s(a, t.output_goods);
    u32s(a, t.input_goods);
    ar::seq(a, t.head_raw, 1, [&](const std::uint8_t& b) { a.u8(b); });
    ar::seq(a, t.entrances, 8, [&](const EntrancePoint& p) {
        a.u32(p.x6);
        a.u32(p.y6);
    });
    a.boolean(t.has_prod);
    digest_production(a, t.prod);
}

std::uint64_t digest_registry(std::span<const BuildingType> types, std::span<const GoodsInfo> goods,
                              std::span<const ClassInfo> classes,
                              std::span<const FigureType> figures) {
    HashArchive a(HashOptions{nullptr, 0, true});
    ar::seq(a, types, 1, [&](const BuildingType& t) { digest_type(a, t); });
    ar::seq(a, goods, 12, [&](const GoodsInfo& g) {
        a.str(g.name);
        a.u32(g.forsch);
    });
    ar::seq(a, classes, 7, [&](const ClassInfo& c) {
        a.enumv(c.cls);
        a.enumv(c.tick);
        a.enumv(c.payload);
        a.u32(c.gate_period);
    });
    ar::seq(a, figures, 20, [&](const FigureType& f) {
        a.u32(f.id);
        a.u32(f.speed_q24);
        a.u32(f.capacity_88);
        a.u32(f.layer);
        a.u32(f.load_ticks);
    });
    return a.finish();
}

/// One FIGUREN/ENTRY of FigurenHeader.dat as the transport reads it (D AnnoGame
/// 0x1000fe00; docs/re/transport.md §1, §4): HEAD f32 at byte 4 = the speed (type
/// +0x10), LOGIC (read into a zero-filled 44-byte buffer, 0x100101b9-0x100101cf):
/// [0x10] the walking layer (+0xff), [0x11] << 8 the capacity (+0xf2), f32 at byte 4
/// the load wait (+0x30; 1.0 when not above 0). A HEAD shorter than 8 bytes, or a
/// speed or a wait that is not a finite value of at most 2^16, is a load error.
formats::Result<FigureType, std::string> figure_type_from_entry(const fig::Entry& e) {
    const std::string who = "figure " + std::to_string(e.id) + ": ";
    const auto head = e.record("HEAD");
    if (!head || head->size() < 8) {
        return who + "no HEAD record of 8 bytes";
    }
    std::array<std::uint8_t, 44> logic{};
    if (const auto l = e.record("LOGIC")) {
        std::copy_n(l->begin(), std::min<std::size_t>(l->size(), logic.size()), logic.begin());
    }
    auto f32 = [](formats::Bytes b, std::size_t at) {
        return std::bit_cast<float>(formats::u32_le(b, at));
    };
    FigureType t;
    t.id = e.id;
    const float speed = f32(*head, 4);
    // 0.2f is the figure tick's time scale (AnnoWelt 0x100c3270); the product is
    // formed in double from the two f32 values.
    const double q24 = static_cast<double>(speed) * static_cast<double>(0.2F) * 16777216.0;
    if (!std::isfinite(q24) || q24 < 0.0 || q24 > 16777216.0 * 65536.0) {
        return who + "HEAD speed is not a speed";
    }
    t.speed_q24 = static_cast<std::uint32_t>(std::llround(q24));
    t.layer = logic[0x10];
    t.capacity_88 = std::uint32_t{logic[0x11]} << 8;
    float wait = f32(formats::Bytes(logic), 4);
    if (!(wait > 0.0F)) {
        wait = 1.0F; // 0x100213bc, the default of 0x1001020b
    }
    const double ticks = static_cast<double>(wait) * 500.0;
    if (!std::isfinite(ticks) || ticks > 65536.0 * 500.0) {
        return who + "LOGIC load wait is not a duration";
    }
    t.load_ticks = static_cast<std::uint32_t>(std::llround(ticks));
    return t;
}

/// The Payload alternative a tick kind drives (the class table's tick/payload
/// pairing, Appendix C); nullopt for a TickKind value that is no enumerator.
std::optional<PayloadTag> payload_for_tick(TickKind tick) {
    switch (tick) {
    case TickKind::None:
        return PayloadTag::Inert;
    case TickKind::Workshop:
        return PayloadTag::Workshop;
    case TickKind::WeaponsWorkshop:
        return PayloadTag::WeaponsWorkshop;
    case TickKind::Farm:
    case TickKind::FurTrapper:
        return PayloadTag::Farm;
    case TickKind::Mine:
        return PayloadTag::Mine;
    case TickKind::SmallFarm:
        return PayloadTag::SmallFarm;
    case TickKind::Market:
        return PayloadTag::Market;
    }
    return std::nullopt;
}

/// The production kind a type must have for its class to tick (tick_building
/// reads the production data of that kind); nullopt for TickKind::None and for
/// a value that is no enumerator.
std::optional<BuildingClass> production_kind_for_tick(TickKind tick) {
    switch (tick) {
    case TickKind::Workshop:
        return BuildingClass::Workshop;
    case TickKind::WeaponsWorkshop:
        return BuildingClass::WeaponsWorkshop;
    case TickKind::Farm:
        return BuildingClass::Farm;
    case TickKind::FurTrapper:
        return BuildingClass::FurTrapper;
    case TickKind::Mine:
        return BuildingClass::Mine;
    case TickKind::SmallFarm:
        return BuildingClass::SmallFarm;
    case TickKind::Market: // reads prod.max_figures / figure_ids only when present
    case TickKind::None:
        break;
    }
    return std::nullopt;
}

/// The checks that depend on the configuration (TypeRegistry::validate_against):
/// the public-need masks fit the service counters and the build cost goods are
/// goods that Place charges (SimConfig::build_goods_first .. + build_goods_count,
/// which SimConfig::validate keeps below warehouse_goods).
std::string check_against_config(std::span<const BuildingType> types, const SimConfig& cfg) {
    const std::uint64_t goods_end = std::uint64_t{cfg.build_goods_first} + cfg.build_goods_count;
    for (const BuildingType& t : types) {
        const std::string who = "type " + std::to_string(t.id.v) + ": ";
        // Every bit of a public-need mask is a counter of BuildingIndex
        // (SimConfig::service_bits of them).
        if (cfg.service_bits < 32 && (t.service_mask >> cfg.service_bits) != 0) {
            return who + "service mask " + std::to_string(t.service_mask) +
                   " needs more than SimConfig.service_bits (" + std::to_string(cfg.service_bits) +
                   ") counters";
        }
        // Place indexes its build-goods array by (goods - build_goods_first).
        for (const CostEntry& c : t.cost_goods) {
            if (c.goods < cfg.build_goods_first || std::uint64_t{c.goods} >= goods_end) {
                return who + "build cost goods " + std::to_string(c.goods) +
                       " lies outside SimConfig.build_goods_first (" +
                       std::to_string(cfg.build_goods_first) + ") .. + build_goods_count (" +
                       std::to_string(cfg.build_goods_count) + ")";
            }
        }
    }
    return {};
}

/// The checks that do not depend on the configuration: the class table is
/// indexed by class and pairs each tick with its payload, and a type whose
/// class ticks has the production data that tick reads. Spec §4.4 / §9.6
/// ("payload matches the type", "a Mine type has outputs[0]") relies on them.
std::string check_consistency(std::span<const BuildingType> types,
                              std::span<const ClassInfo> classes) {
    for (std::size_t i = 0; i < classes.size(); ++i) {
        const ClassInfo& c = classes[i];
        const std::string row = "class table row " + std::to_string(i) + ": ";
        if (static_cast<std::size_t>(c.cls) != i) {
            return row + "cls is " + std::to_string(static_cast<unsigned>(c.cls)) +
                   ", the table is indexed by class";
        }
        const auto payload = payload_for_tick(c.tick);
        if (!payload || *payload != c.payload) {
            return row + "tick " + std::to_string(static_cast<unsigned>(c.tick)) + " and payload " +
                   std::to_string(static_cast<unsigned>(c.payload)) + " disagree";
        }
    }
    for (const BuildingType& t : types) {
        const std::string who = "type " + std::to_string(t.id.v) + ": ";
        if (t.width == 0 || t.height == 0) {
            return who + "footprint " + std::to_string(t.width) + " x " + std::to_string(t.height) +
                   " is empty (HEAD[0], HEAD[1])";
        }
        const auto index = static_cast<std::size_t>(t.cls);
        if (index >= classes.size()) {
            return who + "class " + std::to_string(index) + " is outside the class table";
        }
        const auto kind = production_kind_for_tick(classes[index].tick);
        if (!kind) {
            continue; // the class does not tick (or its tick is unported): no data needed
        }
        if (!t.has_prod || t.prod.kind != *kind) {
            return who + "class " + std::to_string(index) +
                   " ticks but the type has no production data of its kind (an Anlagen entry "
                   "needs a PROD3 record of 68 bytes)";
        }
        if (classes[index].tick == TickKind::Mine && t.prod.outputs.empty()) {
            return who + "a Mine type needs an output (WARE[0])";
        }
    }
    return {};
}

/// Lets build() make a shared registry through the protected constructor.
struct Sealed : TypeRegistry {
    Sealed() = default;
};

} // namespace

// ---- name and class tables

std::optional<std::uint32_t> prodtype_by_name(std::string_view name) {
    return number_of(kProdtypeNames, name);
}

std::string_view prodtype_name(std::uint32_t number) {
    for (const Named& n : kProdtypeNames) {
        if (n.number == number) {
            return n.name;
        }
    }
    return {};
}

std::optional<std::uint32_t> research_by_name(std::string_view name) {
    return number_of(kResearchNames, name);
}

std::optional<RuntimeClass> class_for_prodtype(std::uint32_t p) {
    return class_for_prodtype(p, ListLayout::original());
}

std::optional<RuntimeClass> class_for_prodtype(std::uint32_t p, const ListLayout& layout) {
    // Appendix C. 0 is UNUSED; a number the layout gives no list (in the original
    // everything above 192: the blocks of roads, walls, monuments, rocks, water and
    // ruins) is a tile object, not a building (D: Place skips Create when the
    // block start (p - 1) & ~0x1f is above 0xa0, i.e. for p >= 193; 0x10027b32-
    // 0x10027b3c).
    if (p == 0 || !layout.list_for(p).has_value()) {
        return std::nullopt;
    }
    switch (p) {
    case 1:
        return RuntimeClass::Workshop;
    case 2:
    case 4:
    case 6: // FISCHER: vtable overwritten (0x100c2ac4), same tick as the farm
    case 8:
    case 10:
        return RuntimeClass::Farm;
    case 3:
        return RuntimeClass::Mine;
    case 5:
        return RuntimeClass::Hunter;
    case 7:
        return RuntimeClass::Shipyard;
    case 9:
        return RuntimeClass::SmallFarm;
    case 11:
    case 130:
        return RuntimeClass::Cannonworks;
    case 12:
        return RuntimeClass::DualWorkshop;
    case 13:
    case 14:
        return RuntimeClass::Quarry;
    case 15:
        return RuntimeClass::Whaler;
    case 16:
        return RuntimeClass::FurTrapper;
    case 17:
        return RuntimeClass::WeaponsWorkshop;
    case 33:
    case 34:
    case 35:
        return RuntimeClass::Market;
    case 36:
        return RuntimeClass::MarketCombi;
    case 66:
        return RuntimeClass::Inn;
    case 67:
    case 68:
    case 69:
    case 70:
    case 78:
    case 79:
        return RuntimeClass::PublicService;
    case 71:
        return RuntimeClass::Doctor;
    case 72:
    case 73:
        return RuntimeClass::School;
    case 74:
        return RuntimeClass::Castle;
    case 77:
        return RuntimeClass::FireBrigade;
    case 97:
    case 98:
    case 99:
    case 100:
    case 101:
        return RuntimeClass::House;
    case 129:
        return RuntimeClass::Tower;
    case 131:
        return RuntimeClass::CannonTower;
    case 161:
        return RuntimeClass::MarketStall;
    default:
        // The default class (D: 0x100734c0, tick 0x10001030 = ret 4).
        return RuntimeClass::Plain;
    }
}

std::vector<ClassInfo> original_class_table() {
    using C = RuntimeClass;
    using T = TickKind;
    using P = PayloadTag;
    // One row per RuntimeClass, in enum order. The gate period is the compare
    // constant of the class tick (D, Appendix C); 0 where the tick has no stamp
    // gate (the Quarry stub 0x10001030, the Castle's byte counter at +0x38).
    std::vector<ClassInfo> t = {
        {C::Plain, T::None, P::Inert, 0},
        {C::Workshop, T::Workshop, P::Workshop, 500},                      // 0x100680b0
        {C::DualWorkshop, T::None, P::Inert, 500},                         // 0x10068a70
        {C::WeaponsWorkshop, T::WeaponsWorkshop, P::WeaponsWorkshop, 500}, // 0x10069550
        {C::Farm, T::Farm, P::Farm, 500},                                  // 0x1006ea90
        {C::FurTrapper, T::FurTrapper, P::Farm, 500},                      // 0x1006e5d0
        {C::Mine, T::Mine, P::Mine, 500},                                  // 0x1006def0
        {C::SmallFarm, T::SmallFarm, P::SmallFarm, 500},                   // 0x1006f660
        {C::Quarry, T::None, P::Inert, 0},                                 // stub 0x10001030
        {C::Hunter, T::None, P::Inert, 500},                               // 0x10069f5d
        {C::Whaler, T::None, P::Inert, 500},                               // 0x10073737
        {C::Shipyard, T::None, P::Inert, 500},                             // 0x10074ea1
        {C::Cannonworks, T::None, P::Inert, 500},                          // 0x1006cc40
        {C::Market, T::Market, P::Market, 500},                            // 0x1006b6d0
        {C::MarketCombi, T::None, P::Inert, 500},                          // 0x1006c2e0
        {C::MarketStall, T::None, P::Inert, 5000},                         // 0x100702d0
        {C::Inn, T::None, P::Inert, 5000},          // 0x100755f0 (0x10075608)
        {C::PublicService, T::None, P::Inert, 500}, // 0x1006b4e0 (0x1006b4ec)
        {C::Doctor, T::None, P::Inert, 500},        // 0x100676b0
        {C::School, T::None, P::Inert, 500},        // 0x1006b4e0 (shared with PublicService)
        {C::Castle, T::None, P::Inert, 0},          // 0x1006f1d0: no stamp gate
        {C::FireBrigade, T::None, P::Inert, 500},   // 0x10067b50
        {C::House, T::None, P::Inert, 2500},        // 0x10076230
        {C::Tower, T::None, P::Inert, 166},         // 0x10070c50 (0x10070c62)
        {C::CannonTower, T::None, P::Inert, 166},   // 0x1006ab50 (0x1006ab6a)
    };
    return t;
}

std::uint32_t service_mask_for_goods(GoodsId g) {
    // D: AnnoGame.dll 0x1000a600 (the goods byte of WARE[0] at type +0x206).
    if (g < 0x4c || g > 0x5a) {
        return 0;
    }
    switch (g) {
    case 0x51:
        return 0x30;
    case 0x53:
        return 0xc0;
    case 0x54:
        return 0x1c0;
    default:
        return 1U << (g - 0x4c);
    }
}

// ---- TypeRegistry

const BuildingType* TypeRegistry::find(TypeId id) const {
    const auto it =
        std::lower_bound(types_.begin(), types_.end(), id.v,
                         [](const BuildingType& t, std::uint32_t v) { return t.id.v < v; });
    return it != types_.end() && it->id == id ? &*it : nullptr;
}

const ClassInfo& TypeRegistry::class_info(RuntimeClass cls) const {
    const auto i = static_cast<std::size_t>(cls);
    return i < classes_.size() ? classes_[i] : classes_.front();
}

const GoodsInfo* TypeRegistry::goods(GoodsId id) const {
    return id < goods_.size() ? &goods_[id] : nullptr;
}

std::optional<GoodsId> TypeRegistry::goods_by_name(std::string_view name) const {
    for (std::size_t i = 0; i < goods_.size(); ++i) {
        if (goods_[i].name == name) {
            return static_cast<GoodsId>(i);
        }
    }
    return std::nullopt;
}

std::uint32_t TypeRegistry::max_footprint() const {
    return max_footprint_;
}

const BuildingType* TypeRegistry::find_growing(std::uint32_t object) const {
    const BuildingType* t = find(TypeId{object});
    return t != nullptr && t->has_prod && t->prod.kind == BuildingClass::GrowingItem ? t : nullptr;
}

formats::Result<std::shared_ptr<const TypeRegistry>, std::string>
TypeRegistry::finish(std::vector<BuildingType> types, std::vector<GoodsInfo> goods_in,
                     std::vector<ClassInfo> classes, const SimConfig& cfg,
                     std::vector<FigureType> figures) {
    if (classes.size() != static_cast<std::size_t>(RuntimeClass::Count_)) {
        return "class table has " + std::to_string(classes.size()) + " entries, RuntimeClass has " +
               std::to_string(static_cast<std::size_t>(RuntimeClass::Count_));
    }
    std::sort(types.begin(), types.end(),
              [](const BuildingType& a, const BuildingType& b) { return a.id.v < b.id.v; });
    std::uint32_t footprint = 1;
    for (std::size_t i = 0; i < types.size(); ++i) {
        const BuildingType& t = types[i];
        if (i > 0 && types[i - 1].id == t.id) {
            return "duplicate building type id " + std::to_string(t.id.v);
        }
        footprint = std::max({footprint, t.width, t.height});
    }
    if (std::string problem = check_consistency(types, classes); !problem.empty()) {
        return problem;
    }
    if (std::string problem = check_against_config(types, cfg); !problem.empty()) {
        return problem;
    }
    std::sort(figures.begin(), figures.end(),
              [](const FigureType& a, const FigureType& b) { return a.id < b.id; });
    for (std::size_t i = 1; i < figures.size(); ++i) {
        if (figures[i - 1].id == figures[i].id) {
            return "duplicate figure type id " + std::to_string(figures[i].id);
        }
    }
    auto sealed = std::make_shared<Sealed>();
    TypeRegistry& r = *sealed;
    r.digest_ = digest_registry(types, goods_in, classes, figures);
    // walk_class is DERIVED and set after the digest, so it stays out of it (the rule
    // of TileTest in transport.cpp, now a per-type field; task T27): road flag -> 1, a
    // type that occupies a building list -> 2, else 0 (no object or a non-blocking one).
    for (BuildingType& t : types) {
        t.walk_class = (t.flags_word & kRoadFlag) != 0               ? std::uint8_t{1}
                       : cfg.layout.list_for(t.prodtype).has_value() ? std::uint8_t{2}
                                                                     : std::uint8_t{0};
    }
    r.types_ = std::move(types);
    r.figure_types_ = std::move(figures);
    r.goods_ = std::move(goods_in);
    r.classes_ = std::move(classes);
    r.max_footprint_ = footprint;
    return std::shared_ptr<const TypeRegistry>(std::move(sealed));
}

std::uint64_t TypeRegistry::digest_of(std::span<const BuildingType> types,
                                      std::span<const GoodsInfo> goods,
                                      std::span<const ClassInfo> classes,
                                      std::span<const FigureType> figures) {
    return digest_registry(types, goods, classes, figures);
}

const FigureType* TypeRegistry::find_figure_type(std::uint32_t id) const {
    const auto it = std::lower_bound(figure_types_.begin(), figure_types_.end(), id,
                                     [](const FigureType& t, std::uint32_t v) { return t.id < v; });
    return it != figure_types_.end() && it->id == id ? &*it : nullptr;
}

std::string TypeRegistry::validate_against(const SimConfig& cfg) const {
    return check_against_config(types_, cfg);
}

formats::Result<std::shared_ptr<const TypeRegistry>, std::string>
TypeRegistry::build(std::span<const fig::Entry> entries, std::vector<GoodsInfo> goods_in,
                    const SimConfig& cfg) {
    std::vector<BuildingType> types;
    types.reserve(entries.size());
    for (const fig::Entry& e : entries) {
        auto t = type_from_entry(e, goods_in, cfg);
        if (!t) {
            return t.error();
        }
        types.push_back(std::move(t.value()));
    }
    return finish(std::move(types), std::move(goods_in), original_class_table(), cfg);
}

formats::Result<std::shared_ptr<const TypeRegistry>, std::string>
TypeRegistry::build(std::span<const fig::Entry> entries, std::vector<GoodsInfo> goods_in,
                    const SimConfig& cfg, std::span<const fig::Entry> figure_entries) {
    std::vector<BuildingType> types;
    types.reserve(entries.size());
    for (const fig::Entry& e : entries) {
        auto t = type_from_entry(e, goods_in, cfg);
        if (!t) {
            return t.error();
        }
        types.push_back(std::move(t.value()));
    }
    std::vector<FigureType> figures;
    figures.reserve(figure_entries.size());
    for (const fig::Entry& e : figure_entries) {
        auto f = figure_type_from_entry(e);
        if (!f) {
            return f.error();
        }
        figures.push_back(f.value());
    }
    return finish(std::move(types), std::move(goods_in), original_class_table(), cfg,
                  std::move(figures));
}

std::shared_ptr<const TypeRegistry> TypeRegistry::empty() {
    static const std::shared_ptr<const TypeRegistry> registry =
        finish({}, original_goods(), original_class_table(), SimConfig{}).value();
    return registry;
}

// ---- Builder

TypeRegistry::Builder::Builder() : goods_(original_goods()), classes_(original_class_table()) {}

TypeRegistry::Builder& TypeRegistry::Builder::goods(std::vector<GoodsInfo> table) {
    goods_ = std::move(table);
    return *this;
}

TypeRegistry::Builder& TypeRegistry::Builder::classes(std::vector<ClassInfo> table) {
    classes_ = std::move(table);
    return *this;
}

TypeRegistry::Builder& TypeRegistry::Builder::config(const SimConfig& cfg) {
    cfg_ = cfg;
    return *this;
}

TypeRegistry::Builder& TypeRegistry::Builder::add(BuildingType type) {
    types_.push_back(std::move(type));
    return *this;
}

TypeRegistry::Builder& TypeRegistry::Builder::add_plain(TypeId id, std::uint32_t prodtype) {
    BuildingType t;
    t.id = id;
    t.prodtype = prodtype;
    t.cls = class_for_prodtype(prodtype, cfg_.layout).value_or(RuntimeClass::Plain);
    types_.push_back(std::move(t));
    return *this;
}

GoodsId TypeRegistry::Builder::resolve(std::string_view name) {
    if (name.empty()) {
        return 0;
    }
    for (std::size_t i = 0; i < goods_.size(); ++i) {
        if (goods_[i].name == name) {
            return static_cast<GoodsId>(i);
        }
    }
    if (error_.empty()) {
        error_ = "Builder: unknown goods '" + std::string(name) + "'";
    }
    return 0;
}

BuildingType TypeRegistry::Builder::make_production(TypeId id, std::uint32_t prodtype,
                                                    std::string_view prodtype_text,
                                                    BuildingClass kind, std::string_view output0,
                                                    std::string_view output1,
                                                    std::string_view input0,
                                                    std::string_view input1) {
    BuildingType t;
    t.id = id;
    t.prodtype = prodtype;
    t.cls = class_for_prodtype(prodtype, cfg_.layout).value_or(RuntimeClass::Plain);
    t.has_prod = true;
    t.running_cost = {0, 0};
    ProductionType& p = t.prod;
    p.id = id.v;
    p.prodtype = std::string(prodtype_text);
    p.kind = kind;
    for (const std::string_view name : {output0, output1}) {
        if (!name.empty()) {
            p.outputs.emplace_back(name);
            t.output_goods.push_back(resolve(name));
        }
    }
    for (const std::string_view name : {input0, input1}) {
        if (!name.empty()) {
            p.inputs.emplace_back(name);
            t.input_goods.push_back(resolve(name));
        }
    }
    p.capacity = {0, 0};
    p.output_per_step = {0, 0};
    p.input_per_step = {0, 0};
    p.max_figures = {0, 0, 0};
    p.figure_type_count = {0, 0, 0};
    p.figure_ids.assign(9, 0);
    if (!t.output_goods.empty()) {
        t.service_mask = service_mask_for_goods(t.output_goods[0]);
    }
    return t;
}

TypeRegistry::Builder& TypeRegistry::Builder::add_workshop(TypeId id, std::string_view output,
                                                           std::string_view input,
                                                           std::uint32_t rate, std::uint32_t need,
                                                           std::uint32_t cap_tons,
                                                           std::uint32_t max_figures) {
    BuildingType t =
        make_production(id, 1, "HANDWERK", BuildingClass::Workshop, output, {}, input, {});
    t.prod.capacity[0] = cap_tons << 8;
    t.prod.output_per_step[0] = rate;
    t.prod.input_per_step[0] = need;
    t.prod.max_figures[0] = max_figures;
    types_.push_back(std::move(t));
    return *this;
}

TypeRegistry::Builder& TypeRegistry::Builder::add_weapons_workshop(
    TypeId id, std::string_view output0, std::string_view output1, std::string_view input0,
    std::string_view input1, std::uint32_t rate, std::uint32_t cap_tons) {
    BuildingType t = make_production(id, 17, "HANDWERKWAFFEN", BuildingClass::WeaponsWorkshop,
                                     output0, output1, input0, input1);
    t.prod.capacity = {cap_tons << 8, cap_tons << 8};
    t.prod.output_per_step = {rate, rate};
    t.prod.input_per_step = {8, 4};
    t.prod.max_figures[0] = 1;
    types_.push_back(std::move(t));
    return *this;
}

TypeRegistry::Builder& TypeRegistry::Builder::add_farm(TypeId id, std::string_view output,
                                                       std::string_view input, std::uint32_t rate,
                                                       std::uint32_t need, std::uint32_t cap_tons,
                                                       std::uint32_t max_figures) {
    BuildingType t = make_production(id, 2, "PLANTAGE", BuildingClass::Farm, output, {}, input, {});
    t.prod.capacity[0] = cap_tons << 8;
    t.prod.output_per_step[0] = rate;
    t.prod.input_per_step[0] = need;
    t.prod.max_figures[0] = max_figures;
    types_.push_back(std::move(t));
    return *this;
}

TypeRegistry::Builder& TypeRegistry::Builder::add_fur_trapper(TypeId id, std::string_view output,
                                                              std::string_view input,
                                                              std::uint32_t rate,
                                                              std::uint32_t need,
                                                              std::uint32_t cap_tons) {
    BuildingType t =
        make_production(id, 16, "PELZJAEGER", BuildingClass::FurTrapper, output, {}, input, {});
    t.prod.capacity[0] = cap_tons << 8;
    t.prod.output_per_step[0] = rate;
    t.prod.input_per_step[0] = need;
    t.prod.max_figures[0] = 1;
    types_.push_back(std::move(t));
    return *this;
}

TypeRegistry::Builder&
TypeRegistry::Builder::add_small_farm(TypeId id, std::string_view output0, std::string_view output1,
                                      std::string_view input0, std::string_view input1,
                                      std::uint32_t rate0, std::uint32_t rate1, std::uint32_t need0,
                                      std::uint32_t need1, std::uint32_t cap_tons) {
    BuildingType t = make_production(id, 9, "UNIVERSAL", BuildingClass::SmallFarm, output0, output1,
                                     input0, input1);
    t.prod.capacity = {cap_tons << 8, cap_tons << 8};
    t.prod.output_per_step = {rate0, rate1};
    t.prod.input_per_step = {need0, need1};
    t.prod.max_figures[0] = 3; // docs/re/production.md F1: the small farm 809 allows 3
    types_.push_back(std::move(t));
    return *this;
}

TypeRegistry::Builder& TypeRegistry::Builder::add_mine(TypeId id, std::string_view output,
                                                       std::uint32_t rate, std::uint32_t cap_tons) {
    BuildingType t = make_production(id, 3, "MINE", BuildingClass::Mine, output, {}, {}, {});
    t.prod.capacity[0] = cap_tons << 8;
    t.prod.output_per_step[0] = rate;
    types_.push_back(std::move(t));
    return *this;
}

TypeRegistry::Builder& TypeRegistry::Builder::add_market(TypeId id, std::uint32_t cart,
                                                         std::uint32_t max_carts,
                                                         std::uint32_t radius, std::uint32_t w,
                                                         std::uint32_t h) {
    BuildingType t = make_production(id, 34, "MARKT", BuildingClass::Market, {}, {}, {}, {});
    t.width = w;
    t.height = h;
    t.action_radius = radius;
    t.prod.max_figures[0] = max_carts;
    // P: every market and Kontor type of Anlagen.dat has PROD3[4] = 5 (5 t), the source
    // capacity a workshop carrier's score divides by (0x100728f0).
    t.prod.capacity[0] = 5 << 8;
    t.prod.figure_type_count[0] = 1;
    t.prod.figure_ids[0] = cart;
    types_.push_back(std::move(t));
    return *this;
}

BuildingType* TypeRegistry::Builder::last_with(TypeId id) {
    for (auto it = types_.rbegin(); it != types_.rend(); ++it) {
        if (it->id == id) {
            return &*it;
        }
    }
    if (error_.empty()) {
        error_ = "Builder: no type " + std::to_string(id.v) + " to change";
    }
    return nullptr;
}

TypeRegistry::Builder& TypeRegistry::Builder::entrances(TypeId id,
                                                        std::vector<EntrancePoint> points) {
    if (BuildingType* t = last_with(id)) {
        t->entrances = std::move(points);
    }
    return *this;
}

TypeRegistry::Builder& TypeRegistry::Builder::footprint(TypeId id, std::uint32_t w,
                                                        std::uint32_t h) {
    if (BuildingType* t = last_with(id)) {
        t->width = w;
        t->height = h;
    }
    return *this;
}

TypeRegistry::Builder& TypeRegistry::Builder::action_radius(TypeId id, std::uint32_t radius) {
    if (BuildingType* t = last_with(id)) {
        t->action_radius = radius;
    }
    return *this;
}

TypeRegistry::Builder& TypeRegistry::Builder::flags_word(TypeId id, std::uint32_t flags) {
    if (BuildingType* t = last_with(id)) {
        t->flags_word = flags;
    }
    return *this;
}

TypeRegistry::Builder& TypeRegistry::Builder::figure_ids(TypeId id,
                                                         std::vector<std::uint32_t> ids) {
    if (BuildingType* t = last_with(id)) {
        if (!t->has_prod) {
            if (error_.empty()) {
                error_ = "Builder: type " + std::to_string(id.v) + " has no production data";
            }
            return *this;
        }
        t->prod.figure_ids = ids;
        t->prod.figure_ids.resize(std::max<std::size_t>(t->prod.figure_ids.size(), 9), 0);
        if (!t->prod.figure_type_count.empty()) {
            t->prod.figure_type_count[0] = static_cast<std::uint32_t>(ids.size());
        }
    }
    return *this;
}

TypeRegistry::Builder& TypeRegistry::Builder::figure_type(FigureType type) {
    figures_.push_back(type);
    return *this;
}

formats::Result<std::shared_ptr<const TypeRegistry>, std::string>
TypeRegistry::Builder::build() const {
    if (!error_.empty()) {
        return error_;
    }
    return TypeRegistry::finish(types_, goods_, classes_, cfg_, figures_);
}

} // namespace anno::sim
