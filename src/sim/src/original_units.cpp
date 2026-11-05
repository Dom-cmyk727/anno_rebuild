// Primary contract: docs/re/original-unit-import.md; acceptance: original-unit-import.md.
#include "original_units.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/tagged.hpp>
#include <anno/sim/ship.hpp>

namespace anno::sim {
namespace {
namespace tagged = formats::tagged;
using formats::Bytes;
using FigureSpec = ScenarioSpec::FigureSpec;
enum class Role { Ship, Scout, CrewScout, HeldDonkey };

struct Coordinate {
    std::int32_t q8 = 0;
    std::int64_t q32 = 0;
};

// Exact f32 decomposition, checked before shifts. The simulation never receives a float.
// Nonnav Q8 truncates; a ship's finer Q32 uses the established navigation ties-even policy.
formats::Result<Coordinate, std::string> coordinate(std::uint32_t bits, std::uint32_t extent) {
    const auto exponent = (bits >> 23) & 255U;
    const auto fraction = bits & 0x7fffffU;
    if (exponent == 255U) {
        return std::string("nonfinite position");
    }
    const std::uint64_t mantissa = exponent == 0 ? fraction : fraction | 0x800000U;
    if (mantissa == 0) {
        return Coordinate{}; // +0 and -0 have the same canonical position.
    }
    if ((bits >> 31) != 0) {
        return std::string("negative position");
    }
    const auto power = exponent == 0 ? -149 : static_cast<std::int32_t>(exponent) - 150;
    // A legal world edge is <= 2^14. Values >= 2^32 tiles are certainly out of world,
    // and rejecting them first keeps both conversion shifts in their integer domains.
    if (power >= 9) {
        return std::string("position outside world");
    }
    auto scaled = [mantissa, power](std::int32_t scale, bool nearest) {
        const auto shift = power + scale;
        if (shift >= 0) {
            return mantissa << static_cast<std::uint32_t>(shift);
        }
        const auto right = static_cast<std::uint32_t>(-shift);
        if (right >= 64) {
            return std::uint64_t{0};
        }
        auto result = mantissa >> right;
        if (nearest) {
            const auto residue = mantissa & ((std::uint64_t{1} << right) - 1);
            const auto half = std::uint64_t{1} << (right - 1);
            result += residue > half || (residue == half && (result & 1U) != 0) ? 1U : 0U;
        }
        return result;
    };
    const auto q8 = scaled(8, false);
    const auto q32 = scaled(32, true);
    const auto bound = std::uint64_t{extent} << 32;
    if (extent == 0 || q32 >= bound || q8 >= std::uint64_t{extent} * 256 ||
        q8 > static_cast<std::uint64_t>(INT32_MAX) || q32 > static_cast<std::uint64_t>(INT64_MAX)) {
        return std::string("position outside world");
    }
    return Coordinate{static_cast<std::int32_t>(q8), static_cast<std::int64_t>(q32)};
}

std::uint64_t payload_offset(const tagged::Record& r) {
    return r.offset + r.encoded_len() - r.payload.size();
}

class Decoder {
  public:
    Decoder(const TypeRegistry& types, const SimConfig& config, const WorldSettings& settings,
            const std::map<std::uint32_t, std::optional<GoodsId>>& goods,
            const UnitImportOptions& limits)
        : types_(types), config_(config), settings_(settings), goods_(goods), limits_(limits) {}

    formats::Result<ImportedUnits, std::string>
    run(const formats::sze::Scene& scene, const std::vector<formats::sze::Player>& players) {
        if (limits_.max_actors == 0 || limits_.max_actors > kListSizeBound ||
            limits_.max_registry_slots == 0 || limits_.max_registry_slots > kListSizeBound ||
            !valid_list_initial_size(config_.list_initial_size) ||
            !valid_list_min_growth(config_.figure_min_growth)) {
            return std::string("unit import: invalid allocation limits");
        }
        auto main = scene.chunks("MAIN");
        if (main.size() > 1 || (!main.empty() && main.front()->payload.size() > 16)) {
            return std::string("unit import: duplicate or oversized MAIN");
        }
        std::array<std::uint8_t, 16> buffer{};
        if (!main.empty()) {
            std::copy(main.front()->payload.begin(), main.front()->payload.end(), buffer.begin());
        }
        // Welt 0x100ae31e..0x100ae347: MAIN+4, not SCENE HEAD, pre-sizes the registry.
        out_.initial_slots = formats::u32_le(buffer, 4);
        slots_ = out_.initial_slots == 0 ? config_.list_initial_size : out_.initial_slots;
        if (slots_ > limits_.max_registry_slots) {
            return std::string("unit import: MAIN registry length exceeds allocation budget");
        }
        for (std::size_t p = 0; p < players.size(); ++p) {
            const auto& player = players[p];
            std::uint32_t ordinal = 0;
            for (const auto& record : player.records) {
                if (record.name != "SCHIFF" && record.name != "SOLDAT") {
                    continue;
                }
                const auto path = "players[" + std::to_string(p) + "]." + std::string(record.name) +
                                  "[" + std::to_string(ordinal++) + "]";
                auto unit = formats::sze::Unit::parse(record.payload, payload_offset(record));
                if (!unit) {
                    return path + ": " + unit.error();
                }
                const auto& u = unit.value();
                if (u.reserved != 0) {
                    return path + ": unsupported unit activation flags";
                }
                auto x = coordinate(u.x_bits, settings_.world_width);
                auto y = coordinate(u.y_bits, settings_.world_height);
                if (!x || !y) {
                    return path + ": " + (!x ? x.error() : y.error());
                }
                const bool ship = record.name == "SCHIFF";
                if (u.attr > (ship ? 15 : 7)) {
                    return path + ": unsupported facing";
                }
                FigureSpec f;
                f.kind = u.figure;
                f.owner = PlayerId{player.index}; // Prefix owner is deliberately never read.
                f.pos = {x.value().q8, y.value().q8};
                f.dir16 = static_cast<std::uint8_t>(ship ? u.attr : u.attr * 2U);
                if (ship) {
                    f.nav_position_q32 = std::array{x.value().q32, y.value().q32};
                }
                if (auto error = actor(u.id, std::move(f), ship ? Role::Ship : Role::Scout,
                                       u.records, path, record.offset);
                    !error.empty()) {
                    return error;
                }
            }
        }
        return std::move(out_);
    }

  private:
    const TypeRegistry& types_;
    const SimConfig& config_;
    const WorldSettings& settings_;
    const std::map<std::uint32_t, std::optional<GoodsId>>& goods_;
    const UnitImportOptions& limits_;
    ImportedUnits out_;
    std::set<std::uint32_t> raw_ids_;
    std::uint64_t slots_ = 0;

    void warning(const std::string& path, const tagged::Record& r, std::string message) {
        out_.warnings.push_back({ScenarioWarningKind::UnknownRecord, path, std::string(r.name),
                                 r.offset, std::move(message)});
    }

    std::string actor(std::uint32_t raw, FigureSpec f, Role role,
                      const std::vector<tagged::Record>& records, const std::string& path,
                      std::uint64_t offset) {
        const auto* type = types_.find_figure_type(f.kind);
        const auto* facts = types_.unit_import_facts(f.kind);
        const auto expected = role == Role::Ship ? 113U : role == Role::HeldDonkey ? 53U : 97U;
        if (type == nullptr || type->bewohntype != expected || facts == nullptr ||
            (role == Role::Ship && !type->nav)) {
            return path + ": unsupported actor kind/class/import facts " + std::to_string(f.kind) +
                   " at offset " + std::to_string(offset);
        }
        if (out_.figures.size() >= limits_.max_actors || !raw_ids_.insert(raw).second) {
            return path + ": actor budget exceeded or duplicate raw unit ID " + std::to_string(raw);
        }
        if (raw >= slots_) {
            const auto required = SlotList<Figure>::grown_size(raw, config_.figure_min_growth);
            if (required == 0 || required > limits_.max_registry_slots) {
                return path + ": required registry growth exceeds allocation budget";
            }
            slots_ = required; // Preflight the real growth, never clip it to the budget.
        }
        f.registry_slot = raw;
        const auto index = static_cast<std::uint32_t>(out_.figures.size());
        out_.figures.push_back(std::move(f));
        bool head = false, waren = false, privat = false;
        for (const auto& record : records) {
            if (record.name == "HEAD") {
                if (head || record.payload.size() != 16) {
                    return path + ".HEAD: duplicate or unsupported record length";
                }
                head = true;
                auto& figure = out_.figures[index];
                const auto p = record.payload;
                figure.state_word = (figure.state_word & ~0x20U) | (p[1] < 128 ? 0x20U : 0U);
                if (role == Role::Ship) {
                    const auto maximum = type->nav->max_hp;
                    if (facts->max_hp != maximum || facts->cargo_slots != type->nav->hold_count_a ||
                        facts->crew_slots != type->nav->hold_count_b) {
                        return path + ".HEAD: ship import facts disagree with navigation facts";
                    }
                    const auto hp = p[9] == 0 ? maximum : std::uint64_t{maximum} * p[9] / 128;
                    if (hp > maximum) {
                        return path + ".HEAD: imported ship health exceeds navigation domain";
                    }
                    figure.health = static_cast<std::uint32_t>(hp);
                    figure.state_word =
                        (figure.state_word & ~0x100U) | (std::uint32_t{p[10] & 1U} << 8);
                    const bool crew =
                        std::min<std::uint32_t>(p[8], facts->other_crew) != 0 || has_scout(index);
                    figure.state_word = (figure.state_word & ~0x8000U) | (crew ? 0U : 0x8000U);
                } else {
                    // Generic HEAD 0x1004991e: signed i16, not a ship's ratio byte.
                    const auto raw_hp = formats::u16_le(p, 8);
                    const auto hp = raw_hp == 0 || raw_hp >= 0x8000U ? facts->max_hp : raw_hp;
                    figure.hp = std::min(hp, facts->max_hp);
                }
                warning(path + ".HEAD", record,
                        "unit HEAD projection: morale/account/cached cell fields are unmodelled; "
                        "ship avoidance stance remains deferred, not presentation-only");
            } else if (record.name == "WAREN") {
                if (waren || std::uint64_t{facts->cargo_slots} * 8 != record.payload.size()) {
                    return path + ".WAREN: duplicate or unsupported hold record length";
                }
                waren = true;
                auto& holds = out_.figures[index].holds;
                holds.reserve(facts->cargo_slots);
                for (std::uint32_t h = 0; h < facts->cargo_slots; ++h) {
                    const auto pos = std::size_t{h} * 8;
                    const auto file_goods = record.payload[pos];
                    const auto tons = formats::u16_le(record.payload, pos + 2);
                    if (file_goods == 0 && tons == 0) {
                        holds.push_back({0, 0});
                        continue;
                    }
                    const auto mapped = goods_.find(file_goods);
                    if (mapped == goods_.end() || !mapped->second || *mapped->second == 0) {
                        if (tons != 0) {
                            return path + ".WAREN: nonempty unknown/unmapped goods " +
                                   std::to_string(file_goods);
                        }
                        holds.push_back({0, 0});
                        warning(path + ".WAREN", record,
                                "empty unknown cargo mapping retains an empty hold");
                        continue;
                    }
                    holds.push_back(
                        {*mapped->second,
                         std::min<std::uint64_t>(std::uint64_t{tons} * 256, type->capacity_88)});
                }
            } else if (record.name == "PRIVAT") {
                if (privat) {
                    return path + ".PRIVAT: duplicate record";
                }
                privat = true;
                if (auto error = private_records(index, role, record, path); !error.empty()) {
                    return error;
                }
            } else if (record.name == "TASK" && record.payload.empty()) {
                // No task is the sole supported task state; nonempty tasks must not vanish.
            } else {
                return path + "." + std::string(record.name) + ": unsupported unit record/state";
            }
        }
        if (!head || (!waren && facts->cargo_slots != 0)) {
            return path + ": missing required HEAD/WAREN";
        }
        return {};
    }

    bool has_scout(std::uint32_t parent) const {
        return std::any_of(out_.figures.begin() + parent + 1, out_.figures.end(),
                           [parent](const FigureSpec& f) {
                               return f.parent == parent && f.crew_slot.has_value();
                           });
    }

    // Only Ship -> CrewScout -> HeldDonkey calls actor again. Role checks precede those calls;
    // arbitrary nested tags never cause unbounded recursion or an actor to be silently skipped.
    std::string private_records(std::uint32_t index, Role role, const tagged::Record& record,
                                const std::string& path) {
        auto records = tagged::parse_records(record.payload, payload_offset(record));
        if (!records) {
            return path + ".PRIVAT: " + records.error();
        }
        bool crew = false, donkey = false, private_head = false;
        for (const auto& child : records.value()) {
            const auto where = path + ".PRIVAT." + std::string(child.name);
            if (role == Role::Ship && child.name == "CREW") {
                if (crew) {
                    return where + ": duplicate crew record";
                }
                crew = true;
                auto entries = tagged::parse_records(child.payload, payload_offset(child));
                if (!entries) {
                    return where + ": " + entries.error();
                }
                const auto* facts = types_.unit_import_facts(out_.figures[index].kind);
                std::set<std::uint32_t> slots;
                std::uint32_t ordinal = 0;
                for (const auto& entry : entries.value()) {
                    const auto at = where + ".ENTRY[" + std::to_string(ordinal++) + "]";
                    if (entry.name != "ENTRY" || entry.payload.size() < 6 ||
                        entry.payload[5] != 0) {
                        return at + ": malformed or unsupported crew prefix";
                    }
                    const auto slot = entry.payload[4];
                    if (slot >= facts->crew_slots || !slots.insert(slot).second) {
                        return at + ": duplicate or unavailable crew slot";
                    }
                    auto body =
                        tagged::parse_records(entry.payload.subspan(6), payload_offset(entry) + 6);
                    if (!body) {
                        return at + ": " + body.error();
                    }
                    FigureSpec f;
                    f.kind = formats::u16_le(entry.payload, 2);
                    f.owner = out_.figures[index].owner;
                    f.parent = index;
                    f.crew_slot = slot;
                    f.pos = out_.figures[index].pos; // Ship reader 0x10042f88.
                    f.state_word = 0x88; // Constructor8; attach (&AE)|80; stop preserves8.
                    if (auto error = actor(formats::u16_le(entry.payload, 0), std::move(f),
                                           Role::CrewScout, body.value(), at, entry.offset);
                        !error.empty()) {
                        return error;
                    }
                    out_.figures[index].state_word &= ~0x8000U; // A qualifying class97 scout.
                }
            } else if ((role == Role::Scout || role == Role::CrewScout) && child.name == "ESEL") {
                if (donkey || child.payload.size() < 8) {
                    return where + ": duplicate or malformed ESEL";
                }
                donkey = true;
                const auto state = out_.figures[index].state_word;
                if (role != Role::CrewScout || (state & 0x80U) == 0) {
                    return where + ": unheld ESEL placement is unsupported";
                }
                auto body =
                    tagged::parse_records(child.payload.subspan(8), payload_offset(child) + 8);
                if (!body) {
                    return where + ": " + body.error();
                }
                FigureSpec f;
                f.kind = formats::u16_le(child.payload, 0);
                f.owner = out_.figures[index].owner;
                f.parent = index;
                // Original held ownxyz is uninitialized; normalize only this nonspatial
                // cached point to its effective parent, as the established carrier model does.
                f.pos = out_.figures[index].pos;
                f.state_word = (state & 0xafU) | 0x80U;
                if (auto error = actor(formats::u16_le(child.payload, 2), std::move(f),
                                       Role::HeldDonkey, body.value(), where, child.offset);
                    !error.empty()) {
                    return error;
                }
            } else if (role == Role::Ship && (child.name == "NAME" || child.name == "VERK")) {
                warning(where, child,
                        "unprojected unit name/trade filter; no automatic trade path");
            } else if (role == Role::Ship && child.name == "HEAD") {
                if (private_head || child.payload.empty() || (child.payload[0] & 1U) != 0) {
                    return where + ": duplicate or unsupported automatic route state";
                }
                private_head = true;
                warning(where, child, "inactive private route state is not projected");
            } else if (role == Role::Ship && child.name == "ROUTE" && child.payload.empty()) {
                // An empty inactive route has no commands. Nonempty routes refuse below.
            } else {
                return where + ": unsupported required unit private record/state";
            }
        }
        return {};
    }
};
} // namespace

formats::Result<ImportedUnits, std::string>
import_original_units(const formats::sze::Scene& scene,
                      const std::vector<formats::sze::Player>& players, const TypeRegistry& types,
                      const SimConfig& config, const WorldSettings& settings,
                      const std::map<std::uint32_t, std::optional<GoodsId>>& goods,
                      const UnitImportOptions& options) {
    return Decoder(types, config, settings, goods, options).run(scene, players);
}
} // namespace anno::sim
