#include <algorithm>
#include <iterator>
#include <tuple>

#include <anno/app/building_delta.hpp>

namespace anno::app {

bool building_before(const SceneBuilding& a, const SceneBuilding& b) {
    return std::tie(a.island, a.x, a.y, a.type, a.rotation, a.height) <
           std::tie(b.island, b.x, b.y, b.type, b.rotation, b.height);
}

void sort_buildings(std::vector<SceneBuilding>& buildings) {
    std::sort(buildings.begin(), buildings.end(), building_before);
}

std::vector<BuildingDelta> diff_sorted_buildings(const std::vector<SceneBuilding>& prev,
                                                 const std::vector<SceneBuilding>& cur) {
    std::vector<BuildingDelta> out;
    // set_difference keeps multiplicities: a record listed twice in `prev` and
    // once in `cur` leaves one removed delta.
    std::vector<SceneBuilding> removed;
    std::set_difference(prev.begin(), prev.end(), cur.begin(), cur.end(),
                        std::back_inserter(removed), building_before);
    std::vector<SceneBuilding> added;
    std::set_difference(cur.begin(), cur.end(), prev.begin(), prev.end(), std::back_inserter(added),
                        building_before);
    out.reserve(removed.size() + added.size());
    for (const SceneBuilding& b : removed) {
        out.push_back({BuildingDelta::Kind::Removed, b});
    }
    for (const SceneBuilding& b : added) {
        out.push_back({BuildingDelta::Kind::Added, b});
    }
    return out;
}

} // namespace anno::app
