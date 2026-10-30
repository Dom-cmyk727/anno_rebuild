#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <tuple>

#include <anno/app/cli.hpp>
#include <anno/app/loader.hpp>
#include <anno/assets/figure.hpp>
#include <anno/assets/island.hpp>
#include <anno/assets/unit.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/chunk.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/formats/insel.hpp>
#include <anno/formats/sze.hpp>
#include <anno/formats/tex.hpp>

namespace anno::app {

namespace fs = std::filesystem;
namespace insel = formats::insel;
namespace fig = formats::figuren;
using formats::Result;

namespace {

std::optional<std::vector<std::uint8_t>> read_all(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), {});
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

/// A real island template next to `dummy` (same folder, not a dummy, not a
/// pirate island) of `size`, chosen by `number` (see load_scene).
std::optional<std::vector<std::uint8_t>> stand_in(const fs::path& dummy, std::uint16_t w,
                                                  std::uint16_t h, std::uint32_t number) {
    // Template names carry the size as _<w>x<h>_; checked after loading
    // because two templates are misnamed (insel.md).
    char tag[32];
    std::snprintf(tag, sizeof tag, "_%03ux%03u_", unsigned{w}, unsigned{h});
    std::vector<fs::path> names;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dummy.parent_path(), ec)) {
        const std::string n = lower(to_utf8(e.path().filename()));
        if (n.ends_with(".scp") && n.find("dummy") == std::string::npos &&
            n.find("pirat") == std::string::npos && n.find(tag) != std::string::npos) {
            names.push_back(e.path());
        }
    }
    std::sort(names.begin(), names.end());
    if (names.empty()) {
        return std::nullopt;
    }
    const std::size_t start = number % names.size();
    for (std::size_t k = 0; k < names.size(); ++k) {
        auto buf = read_all(names[(start + k) % names.size()]);
        if (!buf) {
            continue;
        }
        auto islands = insel::islands(*buf);
        if (islands && !islands.value().empty() && islands.value().front().width == w &&
            islands.value().front().height == h) {
            return buf;
        }
    }
    return std::nullopt;
}

/// Figure volume parts Figuren<group><nn>.dat of one zoom group.
struct VolumeParts {
    std::vector<fs::path> paths;
    std::vector<std::uint64_t> sizes;
};

VolumeParts volume_parts(const fs::path& dir, std::uint32_t group) {
    VolumeParts out;
    for (std::uint32_t part = 0;; ++part) {
        const fs::path p = dir / fig::volume_file_name(group, part);
        std::error_code ec;
        const auto size = fs::file_size(p, ec);
        if (ec) {
            break;
        }
        out.paths.push_back(p);
        out.sizes.push_back(size);
    }
    return out;
}

/// The ships and soldiers of a scenario; soldiers stand on the ground height
/// of `scene` at their position. Only the frames the units use are read.
Result<std::vector<render::UnitSprite>, std::string> load_units(const fs::path& dir,
                                                                formats::Bytes file_buf,
                                                                const render::Scene& scene,
                                                                std::vector<std::string>& notes) {
    std::vector<render::UnitSprite> out;
    auto scenario = formats::sze::Scene::parse(file_buf);
    if (!scenario) {
        return out;
    }
    auto header_buf = read_all(dir / "FigurenHeader.dat");
    if (!header_buf) {
        return std::string("cannot read FigurenHeader.dat");
    }
    auto header = fig::FigurenHeader::parse(*header_buf);
    if (!header) {
        return "FigurenHeader.dat: " + header.error();
    }
    std::map<std::uint32_t, VolumeParts> parts;
    const assets::unit::VolumeReader read =
        [&](std::uint32_t group, std::uint64_t offset,
            std::uint64_t length) -> std::optional<std::vector<std::uint8_t>> {
        auto it = parts.find(group);
        if (it == parts.end()) {
            it = parts.emplace(group, volume_parts(dir, group)).first;
        }
        const auto pieces = fig::locate(it->second.sizes, offset, length);
        if (!pieces) {
            return std::nullopt;
        }
        std::vector<std::uint8_t> data;
        for (const auto& p : *pieces) {
            std::ifstream in(it->second.paths[p.part], std::ios::binary);
            in.seekg(static_cast<std::streamoff>(p.start));
            std::vector<char> buf(static_cast<std::size_t>(p.end - p.start));
            if (!in.read(buf.data(), static_cast<std::streamsize>(buf.size()))) {
                return std::nullopt;
            }
            data.insert(data.end(), buf.begin(), buf.end());
        }
        return data;
    };
    const std::array kinds = {assets::unit::UnitKind::Ship, assets::unit::UnitKind::Soldier};
    // Colours as at a new single-player game start (unit::start_colours).
    auto units = assets::unit::units(scenario.value(), header.value(), kinds,
                                     assets::figure::kOriginalPlayerColours, read);
    if (!units) {
        return units.error();
    }
    const auto& st = units.value().stats;
    if (st.missing + st.failed_frames > 0) {
        notes.push_back("units: " + std::to_string(st.missing) + " of " + std::to_string(st.units) +
                        " without a figure, " + std::to_string(st.failed_frames) +
                        " frames failed");
    }
    std::size_t off_land = 0;
    for (auto& u : units.value().units) {
        std::optional<float> z;
        if (u.kind == assets::unit::UnitKind::Soldier) {
            z = scene.ground_height(u.x, u.y);
            if (!z) {
                ++off_land;
                continue;
            }
        }
        render::UnitSprite s{u.x, u.y, z, {}, {}};
        auto convert = [&u](const std::vector<std::optional<assets::unit::DecodedFrame>>& v,
                            bool shadow) {
            std::vector<std::optional<render::UnitFrame>> r;
            for (const auto& f : v) {
                r.push_back(f ? std::optional<render::UnitFrame>(
                                    render::to_unit_frame(*f, u.figure, shadow))
                              : std::nullopt);
            }
            return r;
        };
        s.frames = convert(u.frames, false);
        s.shadows = convert(u.shadows, true);
        out.push_back(std::move(s));
    }
    if (off_land > 0) {
        notes.push_back("units: " + std::to_string(off_land) +
                        " soldiers outside every island, not drawn");
    }
    return out;
}

} // namespace

Result<fs::path, std::string> find_game_dir(const std::optional<fs::path>& explicit_dir,
                                            const fs::path& file,
                                            const std::optional<fs::path>& env_dir) {
    if (explicit_dir) {
        return *explicit_dir;
    }
    if (env_dir) {
        return *env_dir;
    }
    std::error_code ec;
    fs::path p = fs::absolute(file, ec).parent_path();
    while (!p.empty()) {
        if (fs::is_regular_file(p / "Anlagen.dat", ec)) {
            return p;
        }
        if (p == p.parent_path()) {
            break;
        }
        p = p.parent_path();
    }
    return std::string("game directory not found; pass --game-dir or set ANNO1503_GAME_DIR");
}

std::optional<fs::path> find_ci(const fs::path& base, const std::string& rel) {
    fs::path cur = base;
    std::size_t start = 0;
    while (start <= rel.size()) {
        const std::size_t end = std::min(rel.find('\\', start), rel.size());
        const std::string seg = lower(rel.substr(start, end - start));
        std::optional<fs::path> next;
        std::error_code ec;
        for (const auto& e : fs::directory_iterator(cur, ec)) {
            if (lower(to_utf8(e.path().filename())) == seg) {
                next = e.path();
                break;
            }
        }
        if (!next) {
            return std::nullopt;
        }
        cur = *next;
        start = end + 1;
    }
    return cur;
}

/// Building items become the sim's set. A file building the sim also placed
/// keeps its bytes (phase and draw order). One the sim did not place is listed
/// and dropped. One the sim placed that the file lacks is appended.
/// One island's buildings rewritten into its records (play mode, round du: the per-island form of
/// apply_world_buildings). `buildings` are the sim's buildings of this island; the payloads of
/// the records it writes are stored in `owned_out` (each buffer keeps its address when moved,
/// so the spans stay valid while the owner lives). Notes and omitted objects go to `notes` and
/// `omitted`. The island's records must be the ones of the scene the buildings belong to;
/// re-applying to records already rewritten gives the same records as applying to the file's,
/// because buildings the sim no longer places are dropped and the new ones are added.
void apply_island_buildings(insel::Island& island, const std::vector<SceneBuilding>& buildings,
                            const SceneObjects& objects,
                            std::vector<std::vector<std::uint8_t>>& owned_out,
                            std::vector<std::string>& notes, std::vector<OmittedObject>& omitted) {
    using Key = std::tuple<std::uint32_t, std::uint32_t, std::uint16_t, std::uint16_t>;
    std::set<Key> placed;
    for (const SceneBuilding& b : buildings) {
        placed.insert({b.island, b.type, b.x, b.y});
    }
    auto owned = [&](std::vector<std::uint8_t> payload) -> formats::Bytes {
        owned_out.push_back(std::move(payload));
        return formats::Bytes(owned_out.back().data(), owned_out.back().size());
    };
    auto append_item = [](std::vector<std::uint8_t>& items, std::uint16_t local_x,
                          std::uint16_t local_y, const SceneBuilding& b) {
        const std::uint32_t packed = b.type | ((static_cast<std::uint32_t>(b.rotation) & 3u) << 14);
        const auto id = static_cast<std::uint16_t>(packed);
        const std::uint16_t state =
            static_cast<std::uint16_t>(static_cast<std::uint16_t>(b.height) << 8);
        items.push_back(static_cast<std::uint8_t>(id & 0xFF));
        items.push_back(static_cast<std::uint8_t>(id >> 8));
        items.push_back(static_cast<std::uint8_t>(state & 0xFF));
        items.push_back(static_cast<std::uint8_t>(state >> 8));
        items.push_back(static_cast<std::uint8_t>(local_x));
        items.push_back(static_cast<std::uint8_t>(local_y));
    };
    // The island head keeps a u32. The PROD2 match uses that number as the
    // slot. The original table index is 16 bits (AnnoScene.dll 0x10008503);
    // a wider number is refused instead of being masked down to a slot.
    if (island.number > 0xFFFF) {
        notes.push_back("play: island number " + std::to_string(island.number) +
                        " does not fit a PROD2 item");
        return;
    }
    const std::uint32_t slot = island.number;
    std::set<Key> drawn;
    std::vector<SceneBuilding> pending = buildings;
    for (formats::chunk::Chunk& rec : island.records) {
        const std::string_view name = rec.name();
        if (name != "GELAENDE" && name != "PROD2" && name != "FLUESSE") {
            continue;
        }
        auto parsed = insel::Block::parse(rec.payload);
        if (!parsed || parsed.value().data.size() % insel::kItemLen != 0) {
            continue;
        }
        const insel::Block& block = parsed.value();
        std::vector<std::uint8_t> items;
        bool changed = false;
        for (std::size_t o = 0; o < block.data.size(); o += insel::kItemLen) {
            const insel::PlacedObject obj =
                insel::PlacedObject::parse(block.data.subspan(o, insel::kItemLen), block);
            if (objects.is_building(obj.anlage)) {
                const Key key{slot, obj.anlage, obj.x, obj.y};
                if (placed.count(key) == 0) {
                    changed = true;
                    notes.push_back("play: island " + std::to_string(slot) + " object " +
                                    std::to_string(obj.anlage) + " at " + std::to_string(obj.x) +
                                    "," + std::to_string(obj.y) +
                                    " was not placed by the sim, so it is not drawn");
                    omitted.push_back(
                        OmittedObject{slot, island.x, island.y, obj.anlage, obj.x, obj.y});
                    continue;
                }
                drawn.insert(key);
            }
            items.insert(items.end(), block.data.begin() + static_cast<std::ptrdiff_t>(o),
                         block.data.begin() + static_cast<std::ptrdiff_t>(o + insel::kItemLen));
        }
        if (name == "PROD2") {
            std::vector<SceneBuilding> left;
            for (const SceneBuilding& b : pending) {
                const Key key{slot, b.type, b.x, b.y};
                if (drawn.count(key) != 0) {
                    continue;
                }
                const bool inside = b.x >= block.x && b.y >= block.y &&
                                    b.x < static_cast<std::uint32_t>(block.x) + block.width &&
                                    b.y < static_cast<std::uint32_t>(block.y) + block.height;
                if (!inside) {
                    left.push_back(b);
                    continue;
                }
                append_item(items, static_cast<std::uint16_t>(b.x - block.x),
                            static_cast<std::uint16_t>(b.y - block.y), b);
                drawn.insert(key);
                changed = true;
            }
            pending = std::move(left);
        }
        if (!changed) {
            continue;
        }
        const auto hdr = block.header_bytes();
        std::vector<std::uint8_t> payload(hdr.begin(), hdr.end());
        payload.insert(payload.end(), items.begin(), items.end());
        rec.payload = owned(std::move(payload));
    }
    std::map<std::pair<std::uint16_t, std::uint16_t>, std::vector<SceneBuilding>> extra;
    for (const SceneBuilding& b : pending) {
        const Key key{slot, b.type, b.x, b.y};
        if (drawn.count(key) != 0) {
            continue;
        }
        const auto bx = static_cast<std::uint16_t>((b.x / insel::kBlock) * insel::kBlock);
        const auto by = static_cast<std::uint16_t>((b.y / insel::kBlock) * insel::kBlock);
        extra[{bx, by}].push_back(b);
    }
    const auto name = formats::chunk::name_field("PROD2");
    if (!name) {
        return;
    }
    for (const auto& [origin, group] : extra) {
        if (origin.first >= island.width || origin.second >= island.height) {
            continue;
        }
        insel::Block block;
        block.x = origin.first;
        block.y = origin.second;
        block.width = static_cast<std::uint8_t>(
            std::min<std::uint32_t>(insel::kBlock, island.width - origin.first));
        block.height = static_cast<std::uint8_t>(
            std::min<std::uint32_t>(insel::kBlock, island.height - origin.second));
        std::vector<std::uint8_t> items;
        for (const SceneBuilding& b : group) {
            if (b.x < block.x || b.y < block.y ||
                b.x >= static_cast<std::uint32_t>(block.x) + block.width ||
                b.y >= static_cast<std::uint32_t>(block.y) + block.height) {
                continue;
            }
            append_item(items, static_cast<std::uint16_t>(b.x - block.x),
                        static_cast<std::uint16_t>(b.y - block.y), b);
        }
        if (items.empty()) {
            continue;
        }
        const auto hdr = block.header_bytes();
        std::vector<std::uint8_t> payload(hdr.begin(), hdr.end());
        payload.insert(payload.end(), items.begin(), items.end());
        formats::chunk::Chunk chunk;
        chunk.name_field = *name;
        chunk.payload = owned(std::move(payload));
        island.records.push_back(std::move(chunk));
    }
}

void apply_world_buildings(std::vector<insel::Island>& islands, LoadedScene& out,
                           const SceneObjects& objects) {
    std::map<std::uint32_t, std::vector<SceneBuilding>> by_slot;
    for (const SceneBuilding& b : objects.buildings) {
        // PROD2 stores the Anlagen id in 12 bits (formats::insel masks with
        // 0x0FFF). A wider id must not be truncated into that field.
        if (b.type > 0x0FFF) {
            out.notes.push_back("play: Anlagen id " + std::to_string(b.type) +
                                " does not fit a PROD2 item");
            continue;
        }
        by_slot[b.island].push_back(b);
    }
    for (insel::Island& island : islands) {
        apply_island_buildings(island, by_slot[island.number], objects, out.buffers, out.notes,
                               out.omitted);
    }
}

Result<LoadedScene, std::string> complete_scene(const fs::path& dir, std::string name,
                                                formats::Bytes unit_source, LoadedScene out,
                                                std::vector<insel::Island> all,
                                                const SceneObjects* objects);

Result<LoadedScene, std::string> load_scene(const fs::path& dir, const fs::path& file,
                                            std::span<const LinkTemplate> chosen,
                                            const SceneObjects* objects) {
    LoadedScene out;
    // Island and AnlagenFile spans point into these buffers. `load` returns a
    // span over a buffer's heap storage, which stays put when the outer
    // vector grows or `out` is returned (moving a vector keeps its storage);
    // pointers to the inner vectors themselves would not.
    auto load = [&](const fs::path& p) -> std::optional<formats::Bytes> {
        auto b = read_all(p);
        if (!b) {
            return std::nullopt;
        }
        out.buffers.push_back(std::move(*b));
        return formats::Bytes(out.buffers.back());
    };
    const auto file_bytes = load(file);
    if (!file_bytes) {
        return "cannot read " + to_utf8(file);
    }
    auto islands = insel::islands(*file_bytes);
    if (!islands) {
        return to_utf8(file) + ": " + islands.error();
    }
    std::vector<insel::Island> all = std::move(islands).value();
    out.file_islands = all.size();
    out.island_sources.assign(all.size(), {});
    // Stand-in islands for INSELLNK records: a link names a placeholder
    // template (*_dummy.scp); the original replaces it at game start by a
    // random real template of the same climate and size, never a PIRAT one,
    // from the world's random sequence (D: AnnoScene.dll 0x10009e0f,
    // 0x10009550; docs/re/annoscene.md §3–5). Without a game seed the viewer
    // picks deterministically from the same candidates: sorted by file name,
    // pick = island number mod count (OQ-028). Falls back to the dummy.
    // --play passes the template the sim already chose and skips that rule.
    std::map<std::uint32_t, const fs::path*> chosen_by_number;
    for (const LinkTemplate& c : chosen) {
        chosen_by_number.emplace(c.number, &c.file);
    }
    std::size_t stand_ins = 0;
    std::size_t from_sim = 0;
    if (auto scenario = formats::sze::Scene::parse(*file_bytes)) {
        auto links = scenario.value().island_links();
        if (!links) {
            return links.error();
        }
        for (const auto& link : links.value()) {
            const std::string rel(link.path.begin(), link.path.end());
            const auto chosen_file = chosen_by_number.find(link.number);
            std::optional<std::vector<std::uint8_t>> buf;
            std::string source;
            if (chosen_file != chosen_by_number.end()) {
                buf = read_all(*chosen_file->second);
                if (!buf) {
                    return "cannot read " + to_utf8(*chosen_file->second);
                }
                source = to_utf8(*chosen_file->second);
                ++from_sim;
            } else {
                const auto dummy = find_ci(dir / "Scenes", rel);
                if (!dummy) {
                    out.notes.push_back("island link " + std::to_string(link.number) + ": " + rel +
                                        " not found, skipped");
                    continue;
                }
                buf = stand_in(*dummy, link.width, link.height, link.number);
                if (!buf) {
                    buf = read_all(*dummy);
                }
                if (!buf) {
                    return "cannot read " + to_utf8(*dummy);
                }
                source = to_utf8(*dummy);
                ++stand_ins;
            }
            out.buffers.push_back(std::move(*buf));
            auto li = insel::islands(out.buffers.back());
            if (!li || li.value().empty()) {
                return std::string("island template without an island");
            }
            insel::Island island = li.value().front();
            island.x = link.x;
            island.y = link.y;
            island.number = link.number;
            all.push_back(std::move(island));
            out.island_sources.push_back(std::move(source));
        }
    }
    if (stand_ins > 0) {
        out.notes.push_back(std::to_string(stand_ins) +
                            " linked islands (INSELLNK) drawn as stand-ins: real templates of the "
                            "same climate and size (OQ-028)");
    }
    if (from_sim > 0) {
        out.notes.push_back(std::to_string(from_sim) +
                            " linked islands drawn from the templates the sim placed");
    }
    // Copy the span before moving `out`: the bytes stay in the file buffer.
    const formats::Bytes unit_source = *file_bytes;
    return complete_scene(dir, to_utf8(file), unit_source, std::move(out), std::move(all), objects);
}

Result<LoadedScene, std::string> complete_scene(const fs::path& dir, std::string name,
                                                formats::Bytes unit_source, LoadedScene out,
                                                std::vector<insel::Island> all,
                                                const SceneObjects* objects) {
    if (objects != nullptr && objects->is_building) {
        apply_world_buildings(all, out, *objects);
    }
    auto load = [&](const fs::path& p) -> std::optional<formats::Bytes> {
        auto b = read_all(p);
        if (!b) {
            return std::nullopt;
        }
        out.buffers.push_back(std::move(*b));
        return formats::Bytes(out.buffers.back());
    };
    const auto felder_bytes = load(dir / "Textures" / "Felder.tex");
    const auto anl_bytes = load(dir / "Anlagen.dat");
    if (!felder_bytes || !anl_bytes) {
        return std::string("cannot read Textures/Felder.tex or Anlagen.dat in the game directory");
    }
    auto anlagen = formats::anlagen::AnlagenFile::parse(*anl_bytes);
    if (!anlagen) {
        return "Anlagen.dat: " + anlagen.error();
    }
    auto felder = formats::tex::TexFile::parse(*felder_bytes);
    if (!felder) {
        return "Felder.tex: " + felder.error();
    }
    auto textures = assets::island::GroundTextures::load(felder.value());
    if (!textures) {
        return "Felder.tex: " + textures.error();
    }
    if (const auto c = render::water_colour(anlagen.value())) {
        textures.value().set_water(*c);
    }
    // The scene owns the bytes its islands and Anlagen view (round dv, R3). Moving the outer
    // vector keeps every buffer at its address, so the spans taken above and the unit source
    // stay valid.
    render::Scene::Backing backing =
        std::make_shared<const std::vector<std::vector<std::uint8_t>>>(std::move(out.buffers));
    out.buffers.clear();
    out.scene = std::make_unique<render::Scene>(
        std::move(name), std::move(all), std::move(textures).value(), std::move(anlagen).value(),
        render::kHeightLift, std::move(backing));
    auto units = load_units(dir, unit_source, *out.scene, out.notes);
    if (!units) {
        return units.error();
    }
    out.scene->units = std::move(units).value();
    if (out.scene->islands.empty()) {
        return std::string("no islands");
    }
    return out;
}

Result<LoadedScene, std::string> load_placed_scene(const fs::path& dir, std::string name,
                                                   std::span<const PlacedIsland> placed,
                                                   const SceneObjects* objects) {
    LoadedScene out;
    std::vector<insel::Island> all;
    all.reserve(placed.size());
    out.island_sources.reserve(placed.size());
    for (const PlacedIsland& item : placed) {
        if (item.x < 0 || item.y < 0 || item.x > 65535 || item.y > 65535) {
            return "placed island " + std::to_string(item.number) +
                   ": position does not fit an island coordinate";
        }
        auto buf = read_all(item.file);
        if (!buf) {
            return "cannot read " + to_utf8(item.file);
        }
        out.buffers.push_back(std::move(*buf));
        auto parsed = insel::islands(out.buffers.back());
        if (!parsed || parsed.value().empty()) {
            return std::string("island template without an island");
        }
        insel::Island island = parsed.value().front();
        island.x = static_cast<std::uint16_t>(item.x);
        island.y = static_cast<std::uint16_t>(item.y);
        island.number = item.number;
        all.push_back(std::move(island));
        out.island_sources.push_back(to_utf8(item.file));
    }
    if (!placed.empty()) {
        out.notes.push_back(std::to_string(placed.size()) +
                            " linked islands drawn from the templates the sim placed");
    }
    return complete_scene(dir, std::move(name), {}, std::move(out), std::move(all), objects);
}

} // namespace anno::app
