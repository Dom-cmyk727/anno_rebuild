#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>

#include <anno/app/cli.hpp>
#include <anno/app/opening.hpp>
#include <anno/formats/ship_manifest.hpp>
#include <anno/sim/executor.hpp>
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

std::optional<std::vector<std::uint8_t>> read_file(const fs::path& p) {
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
        if (s.identity.rules_digest != hex16(rules.digest())) {
            return rwhere + "the save was made with other rules (digest " + s.identity.rules_digest +
                   ", these are " + hex16(rules.digest()) + ")";
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
        world.emplace(std::move(loaded).value());
        out.identity.resumed_from = to_utf8(*resume);
        out.identity.resumed_hash = hex16(s.state_hash);
    } else {
        auto loaded =
            sim_load::load_ship_start(manifest, game_dir, registry.types, {}, rules);
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

std::vector<std::uint8_t> format_opening_save(const OpeningSave& save) {
    std::ostringstream h;
    h << "anno1503-opening-save 1\n"
      << "manifest_hash " << save.identity.manifest_hash << "\n"
      << "manifest_name " << save.identity.manifest_name << "\n"
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
    bool have_bytes = false, have_hash = false;
    std::map<std::string, bool> seen;
    while (std::getline(header, line)) {
        const std::size_t sp = line.find(' ');
        if (sp == std::string::npos) {
            return "malformed header line `" + line + "`";
        }
        const std::string key = line.substr(0, sp), value = line.substr(sp + 1);
        if (seen[key]) {
            return "duplicate header line `" + key + "`";
        }
        seen[key] = true;
        char* end = nullptr;
        if (key == "manifest_hash") {
            save.identity.manifest_hash = value;
        } else if (key == "manifest_name") {
            save.identity.manifest_name = value;
        } else if (key == "seat") {
            save.identity.seat = static_cast<std::uint32_t>(std::strtoul(value.c_str(), &end, 10));
        } else if (key == "rules_digest") {
            save.identity.rules_digest = value;
        } else if (key == "registry_digest") {
            save.identity.registry_digest = value;
        } else if (key == "turn") {
            save.turn = std::strtoull(value.c_str(), &end, 10);
        } else if (key == "tick") {
            save.tick = std::strtoull(value.c_str(), &end, 10);
        } else if (key == "state_hash") {
            save.state_hash = std::strtoull(value.c_str(), &end, 16);
            have_hash = true;
        } else if (key == "bytes") {
            declared = std::strtoull(value.c_str(), &end, 10);
            have_bytes = true;
        } else {
            return "unknown header line `" + key + "`";
        }
        if (end != nullptr && (*end != '\0' || value.empty())) {
            return "header line `" + key + "` is not a number";
        }
    }
    if (!have_bytes || !have_hash || save.identity.manifest_hash.empty() ||
        save.identity.rules_digest.empty() || save.identity.registry_digest.empty()) {
        return std::string("the header lacks a required line");
    }
    const std::size_t body = split + 2;
    if (bytes.size() - body != declared) {
        return "the save holds " + std::to_string(bytes.size() - body) + " bytes of world, the header says " +
               std::to_string(declared) + " (truncated or extended)";
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

} // namespace anno::app
