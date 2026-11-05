// Destructions on the process reclaimer told by scene identity (round ed, package 3; shared by the
// app and render tests since round ee, E1), for the cases that check a held generation is not
// destroyed early, or is destroyed on the reclaimer. A generation number alone does not name a
// scene: every lineage starts at 0, and a scene of an earlier case or iteration can reach the
// shared reclaimer after the next one installed its observer. A case names its lineage
// (Scene::name) uniquely; a pointer is not an identity either (an address is reused).
#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
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
                seen->threads.push_back(std::this_thread::get_id());
            });
    }
    /// Uninstalls the observer (round ee review of E1: no reliance on a separate reset; a call
    /// already running still writes only into the shared record).
    ~Destructions() { anno::render::SceneReclaimer::shared().set_test_observer({}); }
    Destructions(const Destructions&) = delete;
    Destructions& operator=(const Destructions&) = delete;
    /// That scene's destruction (lineage and generation).
    [[nodiscard]] bool of(const Held& h) const {
        const std::lock_guard lock(seen_->mutex);
        return std::find(seen_->scenes.begin(), seen_->scenes.end(),
                         std::pair{h.lineage, h.generation}) != seen_->scenes.end();
    }
    /// The thread that destroyed that scene (nullopt: not destroyed while installed).
    [[nodiscard]] std::optional<std::thread::id> thread_of(const Held& h) const {
        const std::lock_guard lock(seen_->mutex);
        const auto it = std::find(seen_->scenes.begin(), seen_->scenes.end(),
                                  std::pair{h.lineage, h.generation});
        if (it == seen_->scenes.end()) {
            return std::nullopt;
        }
        return seen_->threads[static_cast<std::size_t>(it - seen_->scenes.begin())];
    }
    /// How many scenes of `lineage` at `min_generation` or later were destroyed.
    [[nodiscard]] std::size_t count(const std::string& lineage,
                                    std::uint64_t min_generation = 0) const {
        const std::lock_guard lock(seen_->mutex);
        return static_cast<std::size_t>(
            std::count_if(seen_->scenes.begin(), seen_->scenes.end(), [&](const auto& p) {
                return p.first == lineage && p.second >= min_generation;
            }));
    }
    /// How many scenes of `lineage` were destroyed on thread `t`.
    [[nodiscard]] std::size_t on_thread(const std::string& lineage, std::thread::id t) const {
        const std::lock_guard lock(seen_->mutex);
        std::size_t n = 0;
        for (std::size_t i = 0; i < seen_->scenes.size(); ++i) {
            n += seen_->scenes[i].first == lineage && seen_->threads[i] == t ? 1U : 0U;
        }
        return n;
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
        std::vector<std::thread::id> threads;
    };
    std::shared_ptr<Seen> seen_ = std::make_shared<Seen>();
};

} // namespace anno::test
