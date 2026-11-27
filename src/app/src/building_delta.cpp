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

bool tile_object_before(const SceneTileObject& a, const SceneTileObject& b) {
    return std::tie(a.island, a.x, a.y, a.type, a.rotation, a.height, a.phase) <
           std::tie(b.island, b.x, b.y, b.type, b.rotation, b.height, b.phase);
}

void sort_tile_objects(std::vector<SceneTileObject>& objects) {
    std::sort(objects.begin(), objects.end(), tile_object_before);
}

std::vector<TileObjectDelta> diff_sorted_tile_objects(const std::vector<SceneTileObject>& prev,
                                                      const std::vector<SceneTileObject>& cur) {
    std::vector<TileObjectDelta> out;
    std::vector<SceneTileObject> removed;
    std::set_difference(prev.begin(), prev.end(), cur.begin(), cur.end(),
                        std::back_inserter(removed), tile_object_before);
    std::vector<SceneTileObject> added;
    std::set_difference(cur.begin(), cur.end(), prev.begin(), prev.end(), std::back_inserter(added),
                        tile_object_before);
    out.reserve(removed.size() + added.size());
    for (const SceneTileObject& obj : removed) {
        out.push_back({TileObjectDelta::Kind::Removed, obj});
    }
    for (const SceneTileObject& obj : added) {
        out.push_back({TileObjectDelta::Kind::Added, obj});
    }
    return out;
}

BuildingMailbox::BuildingMailbox(std::size_t bound) : bound_(bound == 0 ? 1 : bound) {}

void BuildingMailbox::push(std::shared_ptr<const BuildingBatch> batch,
                           const std::vector<SceneBuilding>& current,
                           const std::vector<SceneTileObject>& current_tile_objects) {
    // The copy is made here only when the push collapses.
    if (batch && full()) {
        push_prepared(std::move(batch), current, current_tile_objects);
        return;
    }
    push_prepared(std::move(batch), {}, {});
}

void BuildingMailbox::push_prepared(std::shared_ptr<const BuildingBatch> batch,
                                    std::vector<SceneBuilding> current,
                                    std::vector<SceneTileObject> current_tile_objects) {
    if (!batch) {
        return;
    }
    if (pending_.size() < bound_) {
        pending_.push_back(std::move(batch));
        return;
    }
    // Full: this batch would be one more than the bound. The waiting batches and
    // this one collapse into a resync that carries the lists after this turn.
    auto resync = std::make_shared<BuildingBatch>();
    resync->turn = batch->turn;
    resync->resync = true;
    resync->full = std::move(current);
    resync->full_tile_objects = std::move(current_tile_objects);
    pending_.clear();
    pending_.push_back(std::move(resync));
}

std::vector<std::shared_ptr<const BuildingBatch>> BuildingMailbox::take() {
    std::vector<std::shared_ptr<const BuildingBatch>> out;
    out.swap(pending_);
    return out;
}

namespace {

template <class Record, class Before>
bool erase_one(std::vector<Record>& picture, const Record& item, Before before) {
    const auto it = std::lower_bound(picture.begin(), picture.end(), item, before);
    if (it == picture.end() || !(*it == item)) {
        return false;
    }
    picture.erase(it);
    return true;
}

template <class Record, class Before>
void insert_sorted(std::vector<Record>& picture, const Record& item, Before before) {
    picture.insert(std::lower_bound(picture.begin(), picture.end(), item, before), item);
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
                if (!erase_one(picture, delta.building, building_before)) {
                    result.consistent = false;
                    continue;
                }
            } else {
                insert_sorted(picture, delta.building, building_before);
            }
            result.deltas.push_back(delta);
        }
    }
    return result;
}

TileObjectApplyResult
apply_tile_object_batches(std::vector<SceneTileObject>& picture,
                          const std::vector<std::shared_ptr<const BuildingBatch>>& batches) {
    TileObjectApplyResult result;
    for (const auto& batch : batches) {
        if (!batch) {
            continue;
        }
        if (batch->resync) {
            std::vector<TileObjectDelta> diff =
                diff_sorted_tile_objects(picture, batch->full_tile_objects);
            picture = batch->full_tile_objects;
            result.deltas.insert(result.deltas.end(), diff.begin(), diff.end());
            continue;
        }
        for (const TileObjectDelta& delta : batch->tile_deltas) {
            if (delta.kind == TileObjectDelta::Kind::Removed) {
                if (!erase_one(picture, delta.object, tile_object_before)) {
                    result.consistent = false;
                    continue;
                }
            } else {
                insert_sorted(picture, delta.object, tile_object_before);
            }
            result.deltas.push_back(delta);
        }
    }
    return result;
}

} // namespace anno::app
