#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>

#include <anno/app/cli.hpp>
#include <anno/app/loader.hpp>
#include <anno/assets/figure.hpp>
#include <anno/assets/island.hpp>
#include <anno/assets/unit.hpp>
#include <anno/formats/anlagen.hpp>
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
        auto convert = [](const std::vector<std::optional<assets::unit::DecodedFrame>>& v) {
            std::vector<std::optional<render::UnitFrame>> r;
            for (const auto& f : v) {
                r.push_back(f ? std::optional<render::UnitFrame>({f->frame, f->image})
                              : std::nullopt);
            }
            return r;
        };
        s.frames = convert(u.frames);
        s.shadows = convert(u.shadows);
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

Result<LoadedScene, std::string> load_scene(const fs::path& dir, const fs::path& file) {
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
    const auto felder_bytes = load(dir / "Textures" / "Felder.tex");
    const auto anl_bytes = load(dir / "Anlagen.dat");
    if (!felder_bytes || !anl_bytes) {
        return std::string("cannot read Textures/Felder.tex or Anlagen.dat in the game directory");
    }
    auto islands = insel::islands(*file_bytes);
    if (!islands) {
        return to_utf8(file) + ": " + islands.error();
    }
    std::vector<insel::Island> all = std::move(islands).value();
    out.file_islands = all.size();
    // Stand-in islands for INSELLNK records: a link names a placeholder
    // template (*_dummy.scp); the original replaces it at game start by a
    // random real template of the same climate and size, never a PIRAT one,
    // from the world's random sequence (D: AnnoScene.dll 0x10009e0f,
    // 0x10009550; docs/re/annoscene.md §3–5). Without a game seed the viewer
    // picks deterministically from the same candidates: sorted by file name,
    // pick = island number mod count (OQ-028). Falls back to the dummy.
    std::size_t linked = 0;
    if (auto scenario = formats::sze::Scene::parse(*file_bytes)) {
        auto links = scenario.value().island_links();
        if (!links) {
            return links.error();
        }
        for (const auto& link : links.value()) {
            const std::string rel(link.path.begin(), link.path.end());
            const auto dummy = find_ci(dir / "Scenes", rel);
            if (!dummy) {
                out.notes.push_back("island link " + std::to_string(link.number) + ": " + rel +
                                    " not found, skipped");
                continue;
            }
            auto buf = stand_in(*dummy, link.width, link.height, link.number);
            if (!buf) {
                buf = read_all(*dummy);
            }
            if (!buf) {
                return "cannot read " + to_utf8(*dummy);
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
            ++linked;
        }
    }
    if (linked > 0) {
        out.notes.push_back(std::to_string(linked) +
                            " linked islands (INSELLNK) drawn as stand-ins: real templates of the "
                            "same climate and size (OQ-028)");
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
    out.scene =
        std::make_unique<render::Scene>(to_utf8(file), std::move(all), std::move(textures).value(),
                                        std::move(anlagen).value(), render::kHeightLift);
    auto units = load_units(dir, *file_bytes, *out.scene, out.notes);
    if (!units) {
        return units.error();
    }
    out.scene->units = std::move(units).value();
    if (out.scene->islands.empty()) {
        return std::string("no islands");
    }
    return out;
}

} // namespace anno::app
