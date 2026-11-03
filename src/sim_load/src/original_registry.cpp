// The original-data registry build. See original_registry.hpp.
#include <fstream>
#include <iterator>
#include <utility>
#include <vector>

#include <anno/formats/anlagen.hpp>
#include <anno/formats/annowelt.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/formats/text.hpp>
#include <anno/sim/goods_metadata.hpp>
#include <anno/sim/house_groups.hpp>
#include <anno/sim_load/original_registry.hpp>

namespace anno::sim_load {

namespace fs = std::filesystem;

namespace {

std::vector<std::uint8_t> read_all(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

} // namespace

OriginalRegistryLoad load_original_registry(const fs::path& game_dir, FigureEntries figures) {
    OriginalRegistryLoad out;
    const fs::path anlagen_path = game_dir / "Anlagen.dat";
    const std::vector<std::uint8_t> anlagen_bytes = read_all(anlagen_path);
    auto anlagen = formats::anlagen::AnlagenFile::parse(anlagen_bytes);
    if (!anlagen) {
        out.error = "cannot read " + anlagen_path.string();
        return out;
    }

    std::vector<formats::figuren::Entry> figure_entries;
    const fs::path figures_path = game_dir / "FigurenHeader.dat";
    const std::vector<std::uint8_t> figure_bytes = read_all(figures_path);
    auto header = formats::figuren::FigurenHeader::parse(figure_bytes);
    if (header) {
        figure_entries = std::move(header.value().entries);
    } else if (figures == FigureEntries::Required) {
        out.error = header.error();
        return out;
    } else {
        // sim_replay printed this for every parse failure, not the parser's text.
        out.figure_note = "no figure types (cannot read " + figures_path.string() + ")";
    }

    const std::vector<std::uint8_t> walk_bytes = read_all(game_dir / "AnnoWelt.dll");
    auto walk = formats::annowelt::read_walk_tables(walk_bytes);
    if (!walk) {
        out.error = std::string("AnnoWelt.dll: ") + formats::annowelt::error_message(walk.error());
        return out;
    }

    const std::vector<std::uint8_t> group_bytes = read_all(game_dir / "Data" / "BGruppen.dat");
    std::vector<sim::GoodsInfo> goods = sim::original_goods();
    if (const std::string problem = sim::apply_goods_metadata(group_bytes, goods);
        !problem.empty()) {
        out.error = problem;
        return out;
    }
    auto groups = sim::parse_house_groups(group_bytes, goods);
    if (!groups) {
        out.error = "BGruppen.dat: " + groups.error();
        return out;
    }

    auto built = sim::TypeRegistry::build(
        anlagen.value().entries, goods, sim::SimConfig{}, figure_entries,
        std::make_shared<const formats::annowelt::WalkTables>(std::move(walk).value()),
        std::move(groups).value());
    if (!built) {
        out.error = built.error();
        return out;
    }
    out.types = std::move(built).value();
    return out;
}

} // namespace anno::sim_load

namespace anno::sim_load {

formats::Result<std::vector<std::string>, std::string>
load_city_names(const std::filesystem::path& game_dir) {
    std::ifstream in(game_dir / "Texte.dat", std::ios::binary);
    if (!in) {
        return std::string("Texte.dat: cannot be opened in the game directory");
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    const formats::text::Texte texte = formats::text::Texte::parse(bytes);
    const formats::text::TextSection* section = texte.section("STAEDTE");
    if (section == nullptr) {
        return std::string("Texte.dat: no [STAEDTE] section");
    }
    std::vector<std::string> names;
    names.reserve(section->lines.size());
    for (const formats::Bytes& line : section->lines) {
        std::string name(line.begin(), line.end());
        if (name.find('\0') != std::string::npos) {
            return std::string("Texte.dat [STAEDTE]: a name contains a NUL byte");
        }
        names.push_back(std::move(name));
    }
    return names;
}

} // namespace anno::sim_load
