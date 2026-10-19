#include <utility>

#include <anno/sim/synthetic.hpp>

namespace anno::sim::synthetic {

formats::Result<SyntheticTypes, std::string> make_synthetic_types() {
    SyntheticTypes s;
    s.weaver = TypeId{9001};
    s.farm = TypeId{9002};
    s.fur_trapper = TypeId{9003};
    s.small_farm = TypeId{9004};
    s.bow_maker = TypeId{9005};
    s.iron_mine = TypeId{9006};
    s.pioneer_house = TypeId{9007};
    s.kontor = TypeId{9008};
    s.market_stall = TypeId{9009};
    s.road = TypeId{9010};
    TypeRegistry::Builder b;
    b.add_workshop(s.weaver, "STOFFE", "WOLLE", 6, 11, 5)
        .add_farm(s.farm, "KORN", "GETREIDE", 37, 80, 5)
        .add_fur_trapper(s.fur_trapper, "PELZE", "PELZTIER", 26, 25, 5)
        .add_small_farm(s.small_farm, "KORN", "WOLLE", "GETREIDE", "GRAS", 13, 4, 128, 50, 4)
        .add_weapons_workshop(s.bow_maker, "BOGEN", "ARMBRUST", "HOLZ", "SEILE", 9, 5)
        .add_mine(s.iron_mine, "EISENERZ", 21, 5)
        .add_plain(s.pioneer_house, 97)
        .add_plain(s.kontor, 35)
        .add_plain(s.market_stall, 161)
        .add_plain(s.road, 193);
    auto built = b.build();
    if (!built) {
        return built.error();
    }
    s.registry = std::move(built.value());
    return s;
}

} // namespace anno::sim::synthetic
