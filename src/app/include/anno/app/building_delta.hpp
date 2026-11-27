// Building changes for the play picture (docs/app/game-shell.md, "Building
// changes after tick 0"). The sim thread diffs the buildings it owns after
// each turn and pushes the batch into a mailbox; the window takes the mailbox
// and applies the batches in turn order. No sim type appears here: a change
// is the picture's own record.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <anno/app/loader.hpp>

namespace anno::app {

/// One building the picture must add or remove. A change of rotation, height
/// or tile is a removed delta and an added delta.
struct BuildingDelta {
    enum class Kind : std::uint8_t { Added, Removed };
    Kind kind = Kind::Added;
    SceneBuilding building;
    friend bool operator==(const BuildingDelta&, const BuildingDelta&) = default;
};

/// One non-building or cleared tile object the picture must add or remove. A change of
/// rotation, height, phase, or type (including to/from cleared `type == 0`) is a removed delta and
/// an added delta.
struct TileObjectDelta {
    enum class Kind : std::uint8_t { Added, Removed };
    Kind kind = Kind::Added;
    SceneTileObject object;
    friend bool operator==(const TileObjectDelta&, const TileObjectDelta&) = default;
};

/// The changes of one turn. Immutable once pushed. A normal batch carries the
/// deltas in the order the diff produced them (removed, then added, each in
/// building / tile-object order). A resync batch replaces the picture's lists by
/// `full` and `full_tile_objects`, the sorted lists after `turn`: the mailbox writes
/// one when it cannot hold the batches it was given (BuildingMailbox::push).
struct BuildingBatch {
    std::uint64_t turn = 0;
    bool resync = false;
    std::vector<BuildingDelta> deltas;
    std::vector<SceneBuilding> full;
    std::vector<TileObjectDelta> tile_deltas;
    std::vector<SceneTileObject> full_tile_objects;
};

/// The order of the diff: island, then tile, then type, rotation, height.
/// It is a total order on the record, so equal buildings sit next to each other.
[[nodiscard]] bool building_before(const SceneBuilding& a, const SceneBuilding& b);

/// Sorts `buildings` by building_before (ordered, no hash iteration).
void sort_buildings(std::vector<SceneBuilding>& buildings);

/// The multiset difference of two lists sorted by building_before: removed =
/// prev minus cur, added = cur minus prev. A building present twice on both
/// sides yields nothing. Returns an empty vector when nothing changed.
[[nodiscard]] std::vector<BuildingDelta>
diff_sorted_buildings(const std::vector<SceneBuilding>& prev,
                      const std::vector<SceneBuilding>& cur);

/// The total order of the tile-object diff: island, then tile, then type, rotation, height, phase.
[[nodiscard]] bool tile_object_before(const SceneTileObject& a, const SceneTileObject& b);

/// Sorts `objects` by tile_object_before (ordered, no hash iteration).
void sort_tile_objects(std::vector<SceneTileObject>& objects);

/// The multiset difference of two lists sorted by tile_object_before: removed =
/// prev minus cur, added = cur minus prev. Returns an empty vector when nothing changed.
[[nodiscard]] std::vector<TileObjectDelta>
diff_sorted_tile_objects(const std::vector<SceneTileObject>& prev,
                         const std::vector<SceneTileObject>& cur);

/// The batches waiting for the window. Not thread-safe: the owner holds its
/// own lock around push and take, so that a batch and a snapshot are published
/// together.
///
/// The bound caps the waiting batches. A window that stops taking them (minimised,
/// stalled, or a test with no window) must not grow the queue without limit.
/// When a push would exceed the bound, the waiting batches collapse into one
/// resync batch that carries the full sorted lists after the newest turn, so a
/// window that takes it later gets the same picture as one that took every batch.
class BuildingMailbox {
  public:
    /// `bound` >= 1. The runner uses one second of turns (PlayRunner).
    explicit BuildingMailbox(std::size_t bound);

    /// Whether the next push collapses: the waiting batches are at the bound. The
    /// sim thread is the only pusher and the window only takes, so once this is true
    /// it stays true until the next take.
    [[nodiscard]] bool full() const { return pending_.size() >= bound_; }
    /// Adds `batch`. `current` and `current_tile_objects` are the sorted lists after the
    /// batch's turn, copied only when the mailbox collapses.
    void push(std::shared_ptr<const BuildingBatch> batch, const std::vector<SceneBuilding>& current,
              const std::vector<SceneTileObject>& current_tile_objects = {});
    /// Adds `batch`, taking the lists after its turn by move. A caller that holds a
    /// lock copies the lists before taking it, when full() says the push will collapse,
    /// so the copy is not made under the lock (PlayRunner::publish). The lists are used
    /// only if the mailbox collapses; lists given when it does not are dropped.
    void push_prepared(std::shared_ptr<const BuildingBatch> batch,
                       std::vector<SceneBuilding> current,
                       std::vector<SceneTileObject> current_tile_objects = {});
    /// Every waiting batch, oldest first; the mailbox is empty afterwards.
    [[nodiscard]] std::vector<std::shared_ptr<const BuildingBatch>> take();

    [[nodiscard]] std::size_t pending() const { return pending_.size(); }
    [[nodiscard]] const std::vector<std::shared_ptr<const BuildingBatch>>& waiting() const {
        return pending_;
    }
    [[nodiscard]] std::size_t bound() const { return bound_; }

  private:
    std::size_t bound_;
    std::vector<std::shared_ptr<const BuildingBatch>> pending_;
};

/// Applies `batches` in order to `picture`, the sorted list the window holds.
/// Returns every delta the picture changed, in order, which the caller turns
/// into stale chunks. A resync replaces `picture` by its full list and reports
/// the diff against the previous list. A delta that does not match a building
/// of `picture` is not applied and reported as `false` in the result.
struct ApplyResult {
    std::vector<BuildingDelta> deltas;
    bool consistent = true;
};
[[nodiscard]] ApplyResult
apply_batches(std::vector<SceneBuilding>& picture,
              const std::vector<std::shared_ptr<const BuildingBatch>>& batches);

/// Applies `batches`'s tile-object deltas in order to `picture`, the sorted list of
/// tracked/cleared tile objects the window holds. A resync replaces `picture` by
/// `batch->full_tile_objects` and reports the diff against the previous list. A removed
/// delta without a matching entry reports `consistent = false`.
struct TileObjectApplyResult {
    std::vector<TileObjectDelta> deltas;
    bool consistent = true;
};
[[nodiscard]] TileObjectApplyResult
apply_tile_object_batches(std::vector<SceneTileObject>& picture,
                          const std::vector<std::shared_ptr<const BuildingBatch>>& batches);

} // namespace anno::app
