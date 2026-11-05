#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>

#include <anno/app/cli.hpp>
#include <anno/app/opening.hpp>
#include <anno/formats/ship_manifest.hpp>
#include <anno/sim/executor.hpp>
#include <anno/sim/site_checks.hpp>
#include <anno/sim/terrain_view.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/scenario_load.hpp>
#include <anno/sim_load/ship_start.hpp>

namespace anno::app {

namespace {

namespace fs = std::filesystem;
namespace sm = formats::ship_manifest;

std::string hex16(std::uint64_t v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
    return buf;
}

/// A decimal (base 10) or hexadecimal (base 16) unsigned number of digits only: no sign, no space,
/// no trailing text, no overflow. nullopt otherwise.
std::optional<std::uint64_t> strict_number(const std::string& s, int base) {
    if (s.empty() || s.size() > 20) {
        return std::nullopt;
    }
    std::uint64_t v = 0;
    for (const char c : s) {
        int d = -1;
        if (c >= '0' && c <= '9') {
            d = c - '0';
        } else if (base == 16 && c >= 'a' && c <= 'f') {
            d = c - 'a' + 10;
        } else if (base == 16 && c >= 'A' && c <= 'F') {
            d = c - 'A' + 10;
        }
        if (d < 0 ||
            v > (UINT64_MAX - static_cast<std::uint64_t>(d)) / static_cast<std::uint64_t>(base)) {
            return std::nullopt;
        }
        v = v * static_cast<std::uint64_t>(base) + static_cast<std::uint64_t>(d);
    }
    return v;
}

std::optional<std::vector<std::uint8_t>> read_file(const fs::path& p) {
    std::error_code ec;
    if (!fs::is_regular_file(p, ec)) {
        return std::nullopt;
    }
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), {});
}

} // namespace

OpeningSetup make_opening_setup(const sim::World& world,
                                std::shared_ptr<const sim::TypeRegistry> registry,
                                sim::PlayerId local) {
    OpeningSetup s;
    s.registry = std::move(registry);
    s.local = local;
    for (const std::uint32_t t : world.rules().founding_types) {
        s.founding_types.push_back(sim::TypeId{t});
    }
    s.cargo = world.rules().cargo;
    s.world_w = world.settings().world_width;
    s.world_h = world.settings().world_height;
    for (const auto& island : world.islands()) {
        if (!island) {
            continue;
        }
        s.islands.push_back(IslandRect{island->index.v, island->tiles.x0(), island->tiles.y0(),
                                       island->tiles.width(), island->tiles.height()});
    }
    return s;
}

formats::Result<OpeningLoad, std::string> load_opening(const fs::path& game_dir,
                                                       const fs::path& manifest_path,
                                                       std::uint32_t seat,
                                                       const std::optional<fs::path>& resume) {
    const std::string where = to_utf8(manifest_path) + ": ";
    auto bytes = read_file(manifest_path);
    if (!bytes) {
        return where + "cannot read the ship-start manifest";
    }
    auto parsed = sm::parse(*bytes);
    if (!parsed.ok()) {
        return where + "invalid manifest: " + parsed.error().message;
    }
    const sm::Manifest manifest = std::move(parsed).value();
    if (seat >= manifest.spawns.size()) {
        return where + "--seat " + std::to_string(seat) + " is not a seat of this start (" +
               std::to_string(manifest.spawns.size()) + " seats, 0.." +
               std::to_string(manifest.spawns.empty() ? 0 : manifest.spawns.size() - 1) + ")";
    }
    const auto registry =
        sim_load::load_original_registry(game_dir, sim_load::FigureEntries::Required);
    if (!registry.types) {
        return where + "registry: " + registry.error;
    }
    sim::Rules rules;
    auto names = sim_load::load_city_names(game_dir);
    if (!names.ok()) {
        return where + "city names: " + names.error();
    }
    rules.city_names = names.value();

    OpeningLoad out;
    out.manifest_bytes = *bytes;
    out.identity.manifest_path = to_utf8(manifest_path);
    out.identity.manifest_name = manifest.name;
    out.identity.manifest_hash = manifest.content_hash;
    out.identity.seat = seat;

    std::optional<sim::World> world;
    if (resume) {
        const std::string rwhere = to_utf8(*resume) + ": ";
        auto rbytes = read_file(*resume);
        if (!rbytes) {
            return rwhere + "cannot read the save";
        }
        auto save = parse_opening_save(*rbytes);
        if (!save.ok()) {
            return rwhere + save.error();
        }
        const OpeningSave& s = save.value();
        if (s.identity.manifest_hash != manifest.content_hash) {
            return rwhere + "the save belongs to another manifest (" + s.identity.manifest_hash +
                   ", not " + manifest.content_hash + ")";
        }
        if (s.identity.seat != seat) {
            return rwhere + "the save belongs to seat " + std::to_string(s.identity.seat) +
                   ", not seat " + std::to_string(seat);
        }
        // The rules a launch of this manifest gives are those of a freshly loaded start (the loader
        // adds the shared-information policy to the defaults), so that is the reference.
        auto reference = sim_load::load_ship_start(manifest, game_dir, registry.types, {}, rules);
        if (!reference.ok()) {
            return rwhere + "load_ship_start (the reference of the rules): " + reference.error();
        }
        const std::string expected_rules = hex16(reference.value().rules().digest());
        if (s.identity.rules_digest != expected_rules) {
            return rwhere + "the save was made with other rules (digest " +
                   s.identity.rules_digest + ", these are " + expected_rules + ")";
        }
        if (s.identity.registry_digest != hex16(registry.types->digest())) {
            return rwhere + "the save was made with other game data (registry digest " +
                   s.identity.registry_digest + ", this game dir gives " +
                   hex16(registry.types->digest()) + ")";
        }
        auto loaded = sim::World::load(s.world, registry.types);
        if (!loaded.ok()) {
            return rwhere + "the saved world does not load: " + loaded.error();
        }
        if (loaded.value().state_hash() != s.state_hash) {
            return rwhere + "the saved world's hash is not the one the save states";
        }
        if (hex16(loaded.value().rules().digest()) != expected_rules) {
            return rwhere + "the saved world carries other rules than its header names";
        }
        if (loaded.value().clock().tick() != s.tick) {
            return rwhere + "the saved world is at tick " +
                   std::to_string(loaded.value().clock().tick()) + ", the header says " +
                   std::to_string(s.tick);
        }
        // The scene is drawn from the manifest's islands: the saved world must have the same ones.
        const auto rects = [](const sim::World& w) {
            std::vector<std::array<std::int64_t, 5>> r;
            for (const auto& island : w.islands()) {
                if (island) {
                    r.push_back({island->index.v, island->tiles.x0(), island->tiles.y0(),
                                 island->tiles.width(), island->tiles.height()});
                }
            }
            return r;
        };
        if (rects(loaded.value()) != rects(reference.value())) {
            return rwhere + "the saved world's islands are not the manifest's";
        }
        world.emplace(std::move(loaded).value());
        out.identity.resumed_from = to_utf8(*resume);
        out.identity.resumed_hash = hex16(s.state_hash);
    } else {
        auto loaded = sim_load::load_ship_start(manifest, game_dir, registry.types, {}, rules);
        if (!loaded.ok()) {
            return where + "load_ship_start: " + loaded.error();
        }
        world.emplace(std::move(loaded).value());
        if (const std::string problem = sim_load::expedition_baseline_problem(*world);
            !problem.empty()) {
            return where + "the loaded start is not the baseline: " + problem;
        }
    }

    // The scene's islands, from the manifest: the template file each island was placed from.
    const sim_load::IslandTemplates templates = sim_load::scan_island_templates(game_dir);
    std::map<std::string, std::size_t> by_name;
    for (std::size_t i = 0; i < templates.summaries.size(); ++i) {
        by_name.emplace(templates.summaries[i].name, i);
    }
    PlayLoad& play = out.play;
    for (const sm::Island& island : manifest.islands) {
        std::string upper = island.key;
        for (char& c : upper) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        const auto found = by_name.find(upper);
        if (found == by_name.end()) {
            return where + "island template " + island.key + " is not in Scenes/Islands";
        }
        PlacedIsland placed;
        placed.number = static_cast<std::uint32_t>(island.id);
        placed.x = static_cast<std::int32_t>(island.x);
        placed.y = static_cast<std::int32_t>(island.y);
        placed.file = templates.files[found->second];
        play.placed.push_back(std::move(placed));
    }
    for (const auto& island : world->islands()) {
        if (!island) {
            continue;
        }
        PlayIsland row;
        row.slot = island->index.v;
        row.x = island->rect().x0;
        row.y = island->rect().y0;
        row.w = island->rect().w;
        row.h = island->rect().h;
        row.template_key = "manifest";
        play.islands.push_back(std::move(row));
    }
    play.fixed_map = true; // the scene is built from `placed` (load_placed_scene)
    play.name = manifest.name;
    play.registry = registry.types;

    const sim::PlayerId local{seat};
    if (world->players().find(local) == nullptr) {
        return where + "seat " + std::to_string(seat) + " is not a player of the loaded world";
    }
    out.setup = make_opening_setup(*world, registry.types, local);
    // The camera opens on the participant's ship (figure ids 3 * seat + 1: the baseline).
    const sim::Figure* ship = nullptr;
    for (const auto& entry : world->figures().by_id) {
        const sim::Figure* f = sim::find_figure(world->figures(), entry.first);
        if (f != nullptr && f->nav.has_value() && !f->parent.valid() && f->owner == local) {
            ship = f;
            break;
        }
    }
    if (ship != nullptr) {
        out.camera_x = ship->pos.x / 256.0;
        out.camera_y = ship->pos.y / 256.0;
    } else if (!resume) {
        return where + "the participant has no ship";
    }
    out.identity.rules_digest = hex16(world->rules().digest());
    out.identity.registry_digest = hex16(registry.types->digest());
    out.identity.start_hash = hex16(world->state_hash());
    play.world = std::move(*world);
    return out;
}

std::vector<std::pair<std::uint32_t, std::uint32_t>> opening_ship_kinds(const sim::World& world) {
    std::set<std::pair<std::uint32_t, std::uint32_t>> kinds;
    for (const auto& entry : world.figures().by_id) {
        const sim::Figure* f = sim::find_figure(world.figures(), entry.first);
        if (f != nullptr && f->nav.has_value() && !f->parent.valid()) {
            kinds.emplace(f->kind, f->owner.v);
        }
    }
    return std::vector<std::pair<std::uint32_t, std::uint32_t>>(kinds.begin(), kinds.end());
}

namespace {

/// Informational header text on one line: control characters (a newline would end the header early)
/// become spaces.
std::string header_text(std::string s) {
    for (char& c : s) {
        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) {
            c = ' ';
        }
    }
    return s;
}

} // namespace

std::vector<std::uint8_t> format_opening_save(const OpeningSave& save) {
    std::ostringstream h;
    h << "anno1503-opening-save 1\n"
      << "manifest_hash " << save.identity.manifest_hash << "\n"
      << "manifest_name " << header_text(save.identity.manifest_name) << "\n"
      << "seat " << save.identity.seat << "\n"
      << "rules_digest " << save.identity.rules_digest << "\n"
      << "registry_digest " << save.identity.registry_digest << "\n"
      << "turn " << save.turn << "\n"
      << "tick " << save.tick << "\n"
      << "state_hash " << hex16(save.state_hash) << "\n"
      << "bytes " << save.world.size() << "\n\n";
    const std::string head = h.str();
    std::vector<std::uint8_t> out(head.begin(), head.end());
    out.insert(out.end(), save.world.begin(), save.world.end());
    return out;
}

formats::Result<OpeningSave, std::string>
parse_opening_save(const std::vector<std::uint8_t>& bytes) {
    const std::string text(bytes.begin(), bytes.end());
    const std::size_t split = text.find("\n\n");
    if (split == std::string::npos) {
        return std::string("not an opening save (no header)");
    }
    std::istringstream header(text.substr(0, split));
    std::string line;
    if (!std::getline(header, line) || line != "anno1503-opening-save 1") {
        return std::string("not an opening save, or a version this build does not read");
    }
    OpeningSave save;
    std::uint64_t declared = 0;
    std::set<std::string> seen;
    while (std::getline(header, line)) {
        const std::size_t sp = line.find(' ');
        if (sp == std::string::npos) {
            return "malformed header line `" + line + "`";
        }
        const std::string key = line.substr(0, sp), value = line.substr(sp + 1);
        if (!seen.insert(key).second) {
            return "duplicate header line `" + key + "`";
        }
        const auto number = [&](int base) { return strict_number(value, base); };
        if (key == "manifest_hash") {
            save.identity.manifest_hash = value;
        } else if (key == "manifest_name") {
            save.identity.manifest_name = value;
        } else if (key == "seat") {
            const auto n = number(10);
            if (!n || *n > UINT32_MAX) {
                return std::string("header line `seat` is not a number");
            }
            save.identity.seat = static_cast<std::uint32_t>(*n);
        } else if (key == "rules_digest") {
            save.identity.rules_digest = value;
        } else if (key == "registry_digest") {
            save.identity.registry_digest = value;
        } else if (key == "turn" || key == "tick" || key == "bytes" || key == "state_hash") {
            const auto n = number(key == "state_hash" ? 16 : 10);
            if (!n) {
                return "header line `" + key + "` is not a number";
            }
            (key == "turn"    ? save.turn
             : key == "tick"  ? save.tick
             : key == "bytes" ? declared
                              : save.state_hash) = *n;
        } else {
            return "unknown header line `" + key + "`";
        }
    }
    // Every line is required: a save with a line cut out (a seat, a tick) is not read as a default.
    for (const char* required : {"manifest_hash", "manifest_name", "seat", "rules_digest",
                                 "registry_digest", "turn", "tick", "state_hash", "bytes"}) {
        if (!seen.contains(required)) {
            return std::string("the header lacks the required line `") + required + "`";
        }
    }
    if (save.identity.manifest_hash.empty() || save.identity.rules_digest.empty() ||
        save.identity.registry_digest.empty()) {
        return std::string("the header lacks a required value");
    }
    const std::size_t body = split + 2;
    if (bytes.size() - body != declared) {
        return "the save holds " + std::to_string(bytes.size() - body) +
               " bytes of world, the header says " + std::to_string(declared) +
               " (truncated or extended)";
    }
    save.world.assign(bytes.begin() + static_cast<std::ptrdiff_t>(body), bytes.end());
    return save;
}

std::string format_opening_identity(const OpeningIdentity& id) {
    std::string s;
    s += "ship-start " + id.manifest_path + "\n";
    s += "manifest-hash " + id.manifest_hash + "\n";
    s += "seat " + std::to_string(id.seat) + "\n";
    s += "rules-digest " + id.rules_digest + "\n";
    s += "registry-digest " + id.registry_digest + "\n";
    s += "start-hash " + id.start_hash + "\n";
    if (!id.resumed_from.empty()) {
        s += "resumed-from " + id.resumed_from + "\n";
        s += "resumed-hash " + id.resumed_hash + "\n";
    }
    return s;
}

std::optional<OpeningInvocation> parse_opening_identity(const std::string& text) {
    OpeningInvocation inv;
    bool have_manifest = false, have_seat = false;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back(); // a session.txt that went through a CRLF editor or transfer
        }
        const std::size_t sp = line.find(' ');
        if (sp == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, sp), value = line.substr(sp + 1);
        if (key == "ship-start") {
            inv.manifest_path = value;
            have_manifest = true;
        } else if (key == "manifest-hash") {
            inv.manifest_hash = value;
        } else if (key == "seat") {
            inv.seat = static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
            have_seat = true;
        } else if (key == "rules-digest") {
            inv.rules_digest = value;
        } else if (key == "start-hash") {
            inv.start_hash = value;
        } else if (key == "resumed-from") {
            inv.resumed_from = value;
        }
    }
    if (!have_manifest || !have_seat) {
        return std::nullopt;
    }
    return inv;
}

FoundingProbeView probe_founding(const sim::World& world, sim::PlayerId issuer,
                                 const sim::FoundCmd& f) {
    FoundingProbeView out;
    out.cmd = f;
    const auto refuse = [&out](sim::Reject r) {
        out.reason = r;
        return out;
    };
    const sim::TypeRegistry& types = world.types();
    const sim::Figure* ship = sim::find_figure(world.figures(), f.ship);
    if (ship == nullptr) {
        return refuse(sim::Reject::UnknownFigure);
    }
    if (ship->owner != issuer) {
        return refuse(sim::Reject::NotShipOwner);
    }
    const sim::PlacementWire& pw = f.placement;
    const sim::BuildingType* type = pw.type.v == 0 ? nullptr : types.find(pw.type);
    if (type == nullptr) {
        return refuse(sim::Reject::UnknownType);
    }
    if (pw.rotation > 3) {
        return refuse(sim::Reject::BadRotation);
    }
    const auto [first_w, first_h] = sim::footprint(*type, pw.rotation);
    const sim::site::Origin origin =
        sim::site::centred_origin(f.click_x, f.click_y, first_w, first_h);
    if (origin.x != pw.x || origin.y != pw.y) {
        return refuse(sim::Reject::BadValue);
    }
    if (origin.x > INT32_MAX || origin.y > INT32_MAX) {
        return refuse(sim::Reject::UnknownIsland);
    }
    const sim::Island* island =
        world.island_at(static_cast<std::int32_t>(origin.x), static_cast<std::int32_t>(origin.y));
    if (island == nullptr) {
        return refuse(sim::Reject::UnknownIsland);
    }
    if (island->locked()) {
        return refuse(sim::Reject::IslandLocked);
    }
    const sim::TileLayer& tiles = island->tiles;
    if (!sim::site::footprint_inside(tiles, origin, first_w, first_h)) {
        return refuse(sim::Reject::OutOfIsland);
    }
    if (!sim::site::within_range(f.click_x, f.click_y, ship->pos)) {
        return refuse(sim::Reject::OutOfRange);
    }
    const std::uint32_t neutral = world.config().neutral_owner;
    if (sim::site::footprint_ownership(tiles, neutral, origin, 1, 1) !=
        sim::site::Ownership::AllNeutral) {
        return refuse(sim::Reject::SiteOwned);
    }
    for (const std::optional<sim::City>& c : island->cities) {
        if (c.has_value() && c->owner == issuer) {
            return refuse(sim::Reject::OwnCityOnIsland);
        }
    }
    const sim::VertexGrid grid = sim::VertexGrid::of(*island);
    for (const std::uint8_t rot : sim::site::rotation_trials(pw.rotation)) {
        const auto [fw, fh] = sim::footprint(*type, rot);
        if (!sim::site::footprint_inside(tiles, origin, fw, fh) ||
            sim::site::footprint_ownership(tiles, neutral, origin, fw, fh) !=
                sim::site::Ownership::AllNeutral) {
            continue;
        }
        const sim::site::Compat compat =
            sim::site::footprint_occupancy(tiles, types, *type, origin, fw, fh);
        if (compat == sim::site::Compat::Unsupported) {
            return refuse(sim::Reject::Unsupported);
        }
        if (compat == sim::site::Compat::Incompatible) {
            continue;
        }
        const sim::site::TerrainVerdict tv =
            sim::site::coast_height_check(grid, *type, rot, origin);
        if (tv.result == sim::site::TerrainResult::Unsupported ||
            tv.result == sim::site::TerrainResult::Unreadable) {
            return refuse(sim::Reject::Unsupported);
        }
        if (tv.result == sim::site::TerrainResult::Rejected) {
            continue;
        }
        out.rotation = rot;
        return out;
    }
    return refuse(sim::Reject::NoRotationFits);
}

} // namespace anno::app
