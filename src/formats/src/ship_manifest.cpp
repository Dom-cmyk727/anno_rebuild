// The ship-start manifest reader. Mirrors tools/map2_manifest.py `parse`, check by check.
#include <algorithm>
#include <array>
#include <set>
#include <string_view>
#include <utility>

#include <anno/formats/json.hpp>
#include <anno/formats/sha256.hpp>
#include <anno/formats/ship_manifest.hpp>

namespace anno::formats::ship_manifest {
namespace {

using json::Kind;
using json::Value;

constexpr std::array<std::string_view, 5> kGoods = {"wood", "food", "tools", "bricks", "spices"};
constexpr std::int64_t kHoldTonsMax = 50;
constexpr std::size_t kHoldCountMax = 4;
constexpr std::array<std::string_view, 6> kForbidden = {"cities",   "reservations", "homes",
                                                        "partners", "starts",       "owner_runs"};
constexpr std::array<std::string_view, 4> kExcluded = {
    "base-scenario players", "base-scenario units", "player cities", "island reservations"};

struct PolicyText {
    std::string_view key, expected;
};
constexpr std::array<PolicyText, 4> kPolicyText = {{{"grid", "tile"},
                                                    {"metric", "manhattan-4"},
                                                    {"clearance_scope", "spawn"},
                                                    {"obstruction", "anlagen-head-conservative"}}};
struct PolicyInt {
    std::string_view key;
    std::int64_t low, high;
};
constexpr std::array<PolicyInt, 5> kPolicyInts = {{{"clearance", 0, 64},
                                                   {"max_route_tiles", 1, 100000},
                                                   {"min_city", 1, 1000000},
                                                   {"share_gap", 0, 10000},
                                                   {"share_min_side", 1, 4096}}};

Error err(std::string message) {
    return Error{std::move(message)};
}

/// A JSON Integer that is not a Bool (the parser has separate kinds).
const std::int64_t* as_int(const Value* v) {
    return v != nullptr && v->kind == Kind::Integer ? &v->integer : nullptr;
}

const std::string* as_str(const Value* v) {
    return v != nullptr && v->kind == Kind::String && !v->text.empty() ? &v->text : nullptr;
}

/// `_keys`: exactly these members. Extras first, then a missing one.
std::optional<Error> keys(const Value& obj, std::initializer_list<std::string_view> allowed,
                          const std::string& label) {
    if (obj.kind != Kind::Object) {
        return err(label + " is not an object");
    }
    for (const auto& member : obj.members) {
        if (std::find(allowed.begin(), allowed.end(), member.first) == allowed.end()) {
            return err("unsupported field " + member.first);
        }
    }
    for (const std::string_view key : allowed) {
        if (obj.find(key) == nullptr) {
            return err(label + " is missing " + std::string(key));
        }
    }
    return std::nullopt;
}

#define TRY_ERR(expr)                                                                              \
    do {                                                                                           \
        if (auto e_ = (expr)) {                                                                    \
            return *e_;                                                                            \
        }                                                                                          \
    } while (false)

/// Reads the integer member `key` of `obj` into `out`, or the reference's message.
std::optional<Error> get_int(const Value& obj, std::string_view key, const std::string& label,
                             std::int64_t& out) {
    const std::int64_t* v = as_int(obj.find(key));
    if (v == nullptr) {
        return err(label + " is not an integer");
    }
    out = *v;
    return std::nullopt;
}

std::optional<Error> get_str(const Value& obj, std::string_view key, const std::string& label,
                             std::string& out) {
    const std::string* v = as_str(obj.find(key));
    if (v == nullptr) {
        return err(label + " is not a string");
    }
    out = *v;
    return std::nullopt;
}

Result<Policy, Error> read_policy(const Value* raw) {
    if (raw == nullptr) {
        return err("policy is not an object");
    }
    TRY_ERR(keys(*raw,
                 {"grid", "metric", "clearance_scope", "obstruction", "clearance",
                  "max_route_tiles", "min_city", "share_gap", "share_min_side"},
                 "policy"));
    Policy out;
    std::string* texts[] = {&out.grid, &out.metric, &out.clearance_scope, &out.obstruction};
    for (std::size_t i = 0; i < kPolicyText.size(); ++i) {
        const Value* v = raw->find(kPolicyText[i].key);
        if (v == nullptr || v->kind != Kind::String || v->text != kPolicyText[i].expected) {
            return err("policy " + std::string(kPolicyText[i].key) + " is unsupported");
        }
        *texts[i] = v->text;
    }
    std::int64_t* ints[] = {&out.clearance, &out.max_route_tiles, &out.min_city, &out.share_gap,
                            &out.share_min_side};
    for (std::size_t i = 0; i < kPolicyInts.size(); ++i) {
        const std::string label = "policy " + std::string(kPolicyInts[i].key);
        TRY_ERR(get_int(*raw, kPolicyInts[i].key, label, *ints[i]));
        if (*ints[i] < kPolicyInts[i].low || *ints[i] > kPolicyInts[i].high) {
            return err(label + " is out of range");
        }
    }
    return out;
}

Result<Loadout, Error> read_loadout(const Value* raw) {
    if (raw == nullptr) {
        return err("loadout is not an object");
    }
    TRY_ERR(keys(
        *raw, {"status", "source", "gold", "ship_figure", "scout_figure", "donkey_figure", "holds"},
        "loadout"));
    Loadout out;
    TRY_ERR(get_str(*raw, "status", "loadout status", out.status));
    TRY_ERR(get_str(*raw, "source", "loadout source", out.source));
    TRY_ERR(get_int(*raw, "gold", "gold", out.gold));
    if (out.gold < 0) {
        return err("gold is negative");
    }
    TRY_ERR(get_int(*raw, "ship_figure", "ship_figure", out.ship_figure));
    TRY_ERR(get_int(*raw, "scout_figure", "scout_figure", out.scout_figure));
    TRY_ERR(get_int(*raw, "donkey_figure", "donkey_figure", out.donkey_figure));
    const Value* holds = raw->find("holds");
    if (holds->kind != Kind::Array || holds->items.empty() || holds->items.size() > kHoldCountMax) {
        return err("holds must be 1..4 entries");
    }
    for (std::size_t i = 0; i < holds->items.size(); ++i) {
        const Value& item = holds->items[i];
        const std::string at = "hold " + std::to_string(i);
        TRY_ERR(keys(item, {"goods", "tons"}, at));
        Hold hold;
        TRY_ERR(get_str(item, "goods", at + " goods", hold.goods));
        if (std::find(kGoods.begin(), kGoods.end(), hold.goods) == kGoods.end()) {
            return err(at + " goods " + hold.goods + " are unknown");
        }
        TRY_ERR(get_int(item, "tons", at + " tons", hold.tons));
        if (hold.tons < 0 || hold.tons > kHoldTonsMax) {
            return err(at + " tons " + std::to_string(hold.tons) + " outside 0.." +
                       std::to_string(kHoldTonsMax));
        }
        out.holds.push_back(std::move(hold));
    }
    return out;
}

bool ascii(Bytes text) {
    return std::all_of(text.begin(), text.end(), [](std::uint8_t b) { return b <= 127; });
}

} // namespace

std::string canonical_bytes(const Manifest& m) {
    const auto str = [](const std::string& s) { return Value::make_string(s); };
    const auto num = [](std::int64_t v) { return Value::make_integer(v); };
    Value doc = Value::make_object();
    const auto put = [&](Value& obj, const char* key, Value v) {
        obj.members.emplace_back(key, std::move(v));
    };
    put(doc, "manifest", str(kManifestName));
    put(doc, "version", num(kManifestVersion));
    put(doc, "profile", str(kProfileId));
    put(doc, "name", str(m.name));
    Value edge = Value::make_array();
    edge.items = {num(m.edge_w), num(m.edge_h)};
    put(doc, "edge", std::move(edge));
    put(doc, "base", str(m.base));
    put(doc, "seed", num(m.seed));
    Value gen = Value::make_object();
    put(gen, "tool", str(m.generator_tool));
    put(gen, "config", str(m.generator_config));
    put(doc, "generator", std::move(gen));
    Value islands = Value::make_array();
    for (const Island& island : m.islands) {
        Value o = Value::make_object();
        put(o, "id", num(island.id));
        put(o, "x", num(island.x));
        put(o, "y", num(island.y));
        put(o, "key", str(island.key));
        islands.items.push_back(std::move(o));
    }
    put(doc, "islands", std::move(islands));
    Value spawns = Value::make_array();
    for (const Spawn& spawn : m.spawns) {
        Value o = Value::make_object();
        put(o, "seat", num(spawn.seat));
        put(o, "x", num(spawn.x));
        put(o, "y", num(spawn.y));
        put(o, "facing", num(spawn.facing));
        spawns.items.push_back(std::move(o));
    }
    put(doc, "spawns", std::move(spawns));
    Value loadout = Value::make_object();
    put(loadout, "status", str(m.loadout.status));
    put(loadout, "source", str(m.loadout.source));
    put(loadout, "gold", num(m.loadout.gold));
    put(loadout, "ship_figure", num(m.loadout.ship_figure));
    put(loadout, "scout_figure", num(m.loadout.scout_figure));
    put(loadout, "donkey_figure", num(m.loadout.donkey_figure));
    Value holds = Value::make_array();
    for (const Hold& hold : m.loadout.holds) {
        Value o = Value::make_object();
        put(o, "goods", str(hold.goods));
        put(o, "tons", num(hold.tons));
        holds.items.push_back(std::move(o));
    }
    put(loadout, "holds", std::move(holds));
    put(doc, "loadout", std::move(loadout));
    Value excluded = Value::make_array();
    for (const std::string_view item : kExcluded) {
        excluded.items.push_back(str(std::string(item)));
    }
    put(doc, "excluded", std::move(excluded));
    Value policy = Value::make_object();
    put(policy, "grid", str(m.policy.grid));
    put(policy, "metric", str(m.policy.metric));
    put(policy, "clearance_scope", str(m.policy.clearance_scope));
    put(policy, "obstruction", str(m.policy.obstruction));
    put(policy, "clearance", num(m.policy.clearance));
    put(policy, "max_route_tiles", num(m.policy.max_route_tiles));
    put(policy, "min_city", num(m.policy.min_city));
    put(policy, "share_gap", num(m.policy.share_gap));
    put(policy, "share_min_side", num(m.policy.share_min_side));
    put(doc, "policy", std::move(policy));
    return json::canonical(doc);
}

Result<Manifest, Error> parse(Bytes text) {
    if (!ascii(text)) {
        return err("manifest is not ASCII");
    }
    auto parsed = json::parse(text);
    if (!parsed) {
        return err("malformed json: " + parsed.error().message);
    }
    const Value& raw = parsed.value();
    if (raw.kind != Kind::Object) {
        return err("manifest is not an object");
    }
    for (const std::string_view key : kForbidden) {
        if (raw.find(key) != nullptr) {
            return err(std::string(key) +
                       " is not allowed; this manifest has no cities or reservations");
        }
    }
    constexpr std::array<std::string_view, 14> kAllowed = {
        "manifest",  "version", "profile", "name",    "edge",     "base",   "seed",
        "generator", "islands", "spawns",  "loadout", "excluded", "policy", "content_hash"};
    for (const auto& member : raw.members) {
        if (std::find(kAllowed.begin(), kAllowed.end(), member.first) == kAllowed.end()) {
            return err("unsupported field " + member.first);
        }
    }
    const Value* manifest_name = raw.find("manifest");
    if (manifest_name == nullptr || manifest_name->kind != Kind::String ||
        manifest_name->text != kManifestName) {
        return err("manifest name is unsupported");
    }
    std::int64_t version = 0;
    {
        const std::int64_t* v = as_int(raw.find("version"));
        if (v == nullptr) {
            return err("version is not an integer");
        }
        version = *v;
    }
    if (version != kManifestVersion) {
        return err("manifest version is unsupported");
    }
    const Value* profile = raw.find("profile");
    if (profile == nullptr || profile->kind != Kind::String || profile->text != kProfileId) {
        return err("profile is unsupported");
    }

    Manifest m;
    TRY_ERR(get_str(raw, "name", "name", m.name));
    const Value* edge = raw.find("edge");
    if (edge == nullptr || edge->kind != Kind::Array || edge->items.size() != 2) {
        return err("edge must be [width, height]");
    }
    const std::int64_t* ew = as_int(&edge->items[0]);
    if (ew == nullptr) {
        return err("edge width is not an integer");
    }
    const std::int64_t* eh = as_int(&edge->items[1]);
    if (eh == nullptr) {
        return err("edge height is not an integer");
    }
    m.edge_w = *ew;
    m.edge_h = *eh;
    if (m.edge_w < 1 || m.edge_h < 1 || m.edge_w > 16384 || m.edge_h > 16384) {
        return err("edge is out of range");
    }
    TRY_ERR(get_str(raw, "base", "base", m.base));
    {
        const std::int64_t* v = as_int(raw.find("seed"));
        if (v == nullptr) {
            return err("seed is not an integer");
        }
        m.seed = *v;
    }
    if (m.seed < 0 || m.seed > 0xFFFFFFFFLL) {
        return err("seed is out of range");
    }
    const Value* generator = raw.find("generator");
    if (generator == nullptr) {
        return err("generator is not an object");
    }
    TRY_ERR(keys(*generator, {"tool", "config"}, "generator"));
    TRY_ERR(get_str(*generator, "tool", "generator tool", m.generator_tool));
    TRY_ERR(get_str(*generator, "config", "generator config", m.generator_config));

    const Value* islands = raw.find("islands");
    const Value* spawns = raw.find("spawns");
    if (islands == nullptr || islands->kind != Kind::Array || islands->items.empty()) {
        return err("islands are missing");
    }
    if (spawns == nullptr || spawns->kind != Kind::Array || spawns->items.size() != kSeatCount) {
        return err("spawns must be five seats");
    }
    std::set<std::int64_t> seen_ids;
    std::int64_t last_id = -1;
    for (std::size_t i = 0; i < islands->items.size(); ++i) {
        const Value& item = islands->items[i];
        const std::string at = "island " + std::to_string(i);
        TRY_ERR(keys(item, {"id", "key", "x", "y"}, at));
        Island island;
        TRY_ERR(get_int(item, "id", at + " id", island.id));
        if (seen_ids.contains(island.id)) {
            return err("duplicate island id " + std::to_string(island.id));
        }
        if (island.id < last_id) {
            return err("islands are not ordered by id");
        }
        // Stricter than the reference, which accepts the id -1 as the first island.
        if (island.id < 0) {
            return err("island id " + std::to_string(island.id) + " is negative");
        }
        last_id = island.id;
        seen_ids.insert(island.id);
        const std::string name = "island " + std::to_string(island.id);
        TRY_ERR(get_int(item, "x", name + " x", island.x));
        TRY_ERR(get_int(item, "y", name + " y", island.y));
        TRY_ERR(get_str(item, "key", name + " key", island.key));
        m.islands.push_back(std::move(island));
    }
    std::set<std::int64_t> seen_seats;
    std::set<std::pair<std::int64_t, std::int64_t>> seen_points;
    std::int64_t last_seat = -1;
    for (std::size_t i = 0; i < spawns->items.size(); ++i) {
        const Value& item = spawns->items[i];
        TRY_ERR(keys(item, {"facing", "seat", "x", "y"}, "spawn " + std::to_string(i)));
        Spawn spawn;
        TRY_ERR(get_int(item, "seat", "spawn " + std::to_string(i) + " seat", spawn.seat));
        if (spawn.seat < 0 || spawn.seat > 4) {
            return err("spawn seat " + std::to_string(spawn.seat) + " is outside 0..4");
        }
        if (seen_seats.contains(spawn.seat)) {
            return err("duplicate spawn seat " + std::to_string(spawn.seat));
        }
        if (spawn.seat < last_seat) {
            return err("spawns are not ordered by seat");
        }
        last_seat = spawn.seat;
        seen_seats.insert(spawn.seat);
        const std::string at = "spawn " + std::to_string(spawn.seat);
        TRY_ERR(get_int(item, "x", at + " x", spawn.x));
        TRY_ERR(get_int(item, "y", at + " y", spawn.y));
        TRY_ERR(get_int(item, "facing", at + " facing", spawn.facing));
        if (spawn.facing < 0 || spawn.facing > 3) {
            return err(at + " facing is outside 0..3");
        }
        // edge <= 16384, so the products cannot overflow int64.
        if (spawn.x < 0 || spawn.y < 0 || spawn.x >= m.edge_w * kMilli ||
            spawn.y >= m.edge_h * kMilli) {
            return err(at + " is outside the world");
        }
        if (!seen_points.emplace(spawn.x, spawn.y).second) {
            return err("duplicate spawn position " + std::to_string(spawn.x) + "," +
                       std::to_string(spawn.y));
        }
        m.spawns.push_back(spawn);
    }
    // Five distinct seats in 0..4 were seen, in ascending order.
    const Value* excluded = raw.find("excluded");
    bool excluded_ok = excluded != nullptr && excluded->kind == Kind::Array &&
                       excluded->items.size() == kExcluded.size();
    for (std::size_t i = 0; excluded_ok && i < kExcluded.size(); ++i) {
        excluded_ok =
            excluded->items[i].kind == Kind::String && excluded->items[i].text == kExcluded[i];
    }
    if (!excluded_ok) {
        return err("excluded does not match the ship-start exclusion list");
    }
    auto policy = read_policy(raw.find("policy"));
    if (!policy) {
        return policy.error();
    }
    m.policy = std::move(policy).value();
    auto loadout = read_loadout(raw.find("loadout"));
    if (!loadout) {
        return loadout.error();
    }
    m.loadout = std::move(loadout).value();

    m.content_hash = [&] {
        const std::string bytes = canonical_bytes(m);
        return sha256_hex(Bytes(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()));
    }();
    const Value* stated = raw.find("content_hash");
    if (stated != nullptr && stated->kind != Kind::Null) {
        if (stated->kind != Kind::String || stated->text != m.content_hash) {
            return err("content_hash does not match the canonical manifest");
        }
        m.stated_hash = stated->text;
    }
    return m;
}

const Loadout& baseline_loadout() {
    static const Loadout kBaseline = {"proposed-baseline",
                                      "03_1024_med_256 human party",
                                      30000,
                                      1502,
                                      717,
                                      718,
                                      {{"wood", 50}, {"food", 10}, {"tools", 50}, {"tools", 30}}};
    return kBaseline;
}

bool baseline_ok(const Manifest& m) {
    return m.loadout == baseline_loadout();
}

} // namespace anno::formats::ship_manifest
