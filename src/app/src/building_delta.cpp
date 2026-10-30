#include <algorithm>
#include <iterator>
#include <tuple>
#include <utility>

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

BuildingMailbox::BuildingMailbox(std::size_t bound) : bound_(bound == 0 ? 1 : bound) {}

void BuildingMailbox::push(std::shared_ptr<const BuildingBatch> batch,
                           const std::vector<SceneBuilding>& current) {
    if (!batch) {
        return;
    }
    if (pending_.size() < bound_) {
        pending_.push_back(std::move(batch));
        return;
    }
    // Full: this batch would be one more than the bound. The waiting batches and
    // this one collapse into a resync that carries the list after this turn.
    auto resync = std::make_shared<BuildingBatch>();
    resync->turn = batch->turn;
    resync->resync = true;
    resync->full = current;
    pending_.clear();
    pending_.push_back(std::move(resync));
}

std::vector<std::shared_ptr<const BuildingBatch>> BuildingMailbox::take() {
    std::vector<std::shared_ptr<const BuildingBatch>> out;
    out.swap(pending_);
    return out;
}

namespace {

// Removes one building equal to `b` from the sorted `picture`. False when none is there.
bool erase_one(std::vector<SceneBuilding>& picture, const SceneBuilding& b) {
    const auto it = std::lower_bound(picture.begin(), picture.end(), b, building_before);
    if (it == picture.end() || !(*it == b)) {
        return false;
    }
    picture.erase(it);
    return true;
}

void insert_sorted(std::vector<SceneBuilding>& picture, const SceneBuilding& b) {
    picture.insert(std::lower_bound(picture.begin(), picture.end(), b, building_before), b);
}

} // namespace

ApplyResult apply_batches(std::vector<SceneBuilding>& picture,
                          const std::vector<std::shared_ptr<const BuildingBatch>>& batches) {
    ApplyResult result;
    for (const auto& batch : batches) {
        if (!batch) {
            continue;
        }
        if (batch->resync) {
            std::vector<BuildingDelta> diff = diff_sorted_buildings(picture, batch->full);
            picture = batch->full;
            result.deltas.insert(result.deltas.end(), diff.begin(), diff.end());
            continue;
        }
        for (const BuildingDelta& delta : batch->deltas) {
            if (delta.kind == BuildingDelta::Kind::Removed) {
                if (!erase_one(picture, delta.building)) {
                    result.consistent = false;
                    continue;
                }
            } else {
                insert_sorted(picture, delta.building);
            }
            result.deltas.push_back(delta);
        }
    }
    return result;
}

} // namespace anno::app
