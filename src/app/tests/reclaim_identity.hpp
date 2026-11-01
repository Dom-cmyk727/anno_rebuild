// Destructions on the process reclaimer told by scene identity (round ed, package 3), for the cases
// that check a held generation is not destroyed early. A generation number alone does not name a
// scene: every lineage starts at 0, and a scene of an earlier case or iteration can reach the
// shared reclaimer after the next one installed its observer.
#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <anno/render/scene.hpp>

namespace anno::test {

/// A scene by identity, without owning it: its lineage (Scene::name), its generation, and a weak
/// handle that expires when its last owner lets go (round ed, package 3).
struct Held {
    std::string lineage;
    std::uint64_t generation = 0;
    std::weak_ptr<const anno::render::Scene> scene;
};
inline Held hold(const std::shared_ptr<const anno::render::Scene>& s) {
    return {s->name, s->generation(), s};
}

/// The scenes the process reclaimer destroys while this is installed, by lineage and generation.
/// The record is shared with the observer by value: the reclaimer copies its observer before it
/// calls it, without its lock, so a call can still run after the observer is cleared; it must
/// not touch the case's own locals.
class Destructions {
  public:
    Destructions() {
        anno::render::SceneReclaimer::shared().set_test_observer(
            [seen = seen_](const anno::render::Scene& s) {
                const std::lock_guard lock(seen->mutex);
                seen->scenes.push_back({s.name, s.generation()});
            });
    }
    /// That scene's destruction (lineage and generation).
    [[nodiscard]] bool of(const Held& h) const {
        const std::lock_guard lock(seen_->mutex);
        return std::find(seen_->scenes.begin(), seen_->scenes.end(),
                         std::pair{h.lineage, h.generation}) != seen_->scenes.end();
    }
    /// A destruction of any scene with that generation number (round ec's check, which a
    /// generation 0 of another lineage satisfies).
    [[nodiscard]] bool of_generation(std::uint64_t generation) const {
        const std::lock_guard lock(seen_->mutex);
        return std::any_of(seen_->scenes.begin(), seen_->scenes.end(),
                           [generation](const auto& p) { return p.second == generation; });
    }

  private:
    struct Seen {
        std::mutex mutex;
        std::vector<std::pair<std::string, std::uint64_t>> scenes;
    };
    std::shared_ptr<Seen> seen_ = std::make_shared<Seen>();
};

} // namespace anno::test
