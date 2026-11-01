// Whether an applied command's change is on screen (see the header).
#include <algorithm>
#include <iterator>
#include <utility>

#include <anno/app/visibility.hpp>

namespace anno::app {

const char* visibility_name(Visibility v) {
    switch (v) {
    case Visibility::NotInPicture:
        return "NotInPicture";
    case Visibility::Committing:
        return "Committing";
    case Visibility::AwaitingFrame:
        return "AwaitingFrame";
    case Visibility::ChunksStale:
        return "ChunksStale";
    case Visibility::NotOnGpu:
        return "NotOnGpu";
    case Visibility::NotPresented:
        return "NotPresented";
    case Visibility::OutOfView:
        return "OutOfView";
    case Visibility::Visible:
        return "Visible";
    case Visibility::Offscreen:
        return "Offscreen";
    case Visibility::NothingToDraw:
        return "NothingToDraw";
    case Visibility::NoVisualChange:
        return "NoVisualChange";
    case Visibility::Unknown:
        return "Unknown";
    }
    return "?";
}

std::string visibility_note(Visibility v, std::uint64_t turn) {
    switch (v) {
    case Visibility::NotInPicture:
        return "not drawn yet: turn " + std::to_string(turn) + " is not in the picture";
    case Visibility::Committing:
        return "not drawn yet: the island's objects are still being rebuilt";
    case Visibility::AwaitingFrame:
        return "not drawn yet: committed, no frame has looked at it";
    case Visibility::ChunksStale:
        return "not drawn yet: chunks of the change are being rebuilt";
    case Visibility::NotOnGpu:
        return "not drawn yet: waiting for the GPU copy of the chunks";
    case Visibility::NotPresented:
        return "not drawn yet: drawn, no frame has been presented since";
    case Visibility::OutOfView:
        return "not drawn yet: the change is outside the current view";
    case Visibility::Visible:
        return {};
    case Visibility::Offscreen:
        return "applied outside the view (not on screen)";
    case Visibility::NothingToDraw:
        return "applied; nothing the picture draws changed";
    case Visibility::NoVisualChange:
        return "applied; the picture did not change";
    case Visibility::Unknown:
        return "applied; whether it is shown is no longer kept";
    }
    return {};
}

bool ChangeVisibility::belongs(std::uint64_t turn, std::uint64_t from, std::uint64_t through,
                               bool coarse, const std::vector<std::uint64_t>& turns) {
    if (turn < from || turn > through) {
        return false;
    }
    return coarse || std::binary_search(turns.begin(), turns.end(), turn);
}

void ChangeVisibility::taken(std::optional<std::uint64_t> followed,
                             const std::vector<std::shared_ptr<const BuildingBatch>>& batches,
                             const std::vector<std::uint64_t>& before,
                             const std::vector<std::uint64_t>& after) {
    // Turn indices start at 0: the first range starts there, later ones after the last followed.
    const std::uint64_t first = followed_ ? *followed_ + 1 : 0;
    if (followed && (!followed_ || *followed > *followed_)) {
        followed_ = followed;
    }
    if (batches.empty()) {
        return;
    }
    // A take can hold a batch newer than the snapshot the follower read first (the mailbox is
    // filled before the snapshot is published): its turn is in the desired state now.
    followed_ = std::max(followed_.value_or(0), batches.back()->turn);
    Take t;
    t.from = first;
    t.through = std::max<std::uint64_t>(followed.value_or(0), batches.back()->turn);
    for (const auto& b : batches) {
        if (b->resync) {
            t.coarse = true;
        } else if (t.turns.empty() || t.turns.back() < b->turn) {
            t.turns.push_back(b->turn);
        }
    }
    if (t.coarse || t.turns.size() > kMaxTurns) {
        t.coarse = true;
        t.turns.clear();
    }
    for (std::size_t i = 0; i < after.size(); ++i) {
        if (i >= before.size() || before[i] != after[i]) {
            t.islands.emplace_back(i, after[i]);
        }
    }
    if (t.islands.empty()) {
        // The picture changed nothing: the turns' batches had no effect on it.
        history_.push_back(
            {t.from, t.through, std::move(t.turns), t.coarse, Visibility::NoVisualChange});
        while (history_.size() > kMaxHistory) {
            floor_ = std::max(floor_.value_or(0), history_.front().through);
            history_.pop_front();
        }
        return;
    }
    if (takes_.size() >= kMaxTakes) {
        // Merge the two oldest: the union of their turns and islands (the newest version of each),
        // so the wait only gets stricter and no change is lost.
        Take b = std::move(takes_[1]);
        Take& a = takes_[0];
        a.through = b.through;
        a.coarse = a.coarse || b.coarse;
        std::vector<std::uint64_t> merged;
        std::set_union(a.turns.begin(), a.turns.end(), b.turns.begin(), b.turns.end(),
                       std::back_inserter(merged));
        a.turns = std::move(merged);
        if (a.turns.size() > kMaxTurns) {
            a.coarse = true;
        }
        if (a.coarse) {
            a.turns.clear();
        }
        for (const auto& [index, version] : b.islands) {
            const auto it =
                std::find_if(a.islands.begin(), a.islands.end(),
                             [index = index](const auto& p) { return p.first == index; });
            if (it == a.islands.end()) {
                a.islands.emplace_back(index, version);
            } else {
                it->second = std::max(it->second, version);
            }
        }
        takes_.erase(takes_.begin() + 1);
    }
    takes_.push_back(std::move(t));
}

void ChangeVisibility::settle_island(std::size_t index, Settle kind) {
    IslandState& s = islands_[index];
    s.settled = std::max(s.settled, s.watch ? s.watch->version : s.committed);
    s.settle_kind = kind;
    s.watch.reset();
}

void ChangeVisibility::committed(const std::vector<ScenePicture::CommittedIsland>& islands) {
    for (const ScenePicture::CommittedIsland& c : islands) {
        IslandState& s = islands_[c.index];
        s.committed = std::max(s.committed, c.version);
        if (s.watch) {
            // A newer commit of an island with a change still waiting: one watch for both, over
            // the union of their chunks and the newer version (a stricter wait).
            s.watch->version = std::max(s.watch->version, c.version);
            for (const render::ChunkKey& k : c.keys) {
                s.watch->keys.push_back(k);
            }
            std::sort(s.watch->keys.begin(), s.watch->keys.end());
            s.watch->keys.erase(
                std::unique(s.watch->keys.begin(), s.watch->keys.end(),
                            [](const auto& a, const auto& b) { return !(a < b) && !(b < a); }),
                s.watch->keys.end());
            s.watch->cpu_ready = s.watch->gpu_ready = false;
        } else if (c.keys.empty()) {
            s.settled = std::max(s.settled, c.version);
            s.settle_kind = Settle::NothingToDraw;
        } else {
            Watch w;
            w.version = c.version;
            w.keys = c.keys;
            std::sort(w.keys.begin(), w.keys.end());
            w.keys.erase(
                std::unique(w.keys.begin(), w.keys.end(),
                            [](const auto& a, const auto& b) { return !(a < b) && !(b < a); }),
                w.keys.end());
            s.watch = std::move(w);
        }
    }
    retire_takes();
}

void ChangeVisibility::begin_frame(render::Rect view, std::size_t group) {
    in_view_.clear();
    bool any = false;
    for (const auto& [index, s] : islands_) {
        any = any || s.watch.has_value();
    }
    if (!any) {
        return;
    }
    const std::vector<render::ChunkKey> keys = view_keys(view, group);
    in_view_.insert(keys.begin(), keys.end());
    std::vector<std::size_t> offscreen;
    for (auto& [index, s] : islands_) {
        if (!s.watch || s.watch->viewed) {
            continue;
        }
        const bool seen = std::any_of(s.watch->keys.begin(), s.watch->keys.end(),
                                      [this](const auto& k) { return in_view_.count(k) != 0; });
        if (seen) {
            s.watch->viewed = true;
        } else {
            offscreen.push_back(index);
        }
    }
    for (const std::size_t index : offscreen) {
        settle_island(index, Settle::Offscreen);
    }
    retire_takes();
}

void ChangeVisibility::frame_done(bool present_ok, const FrameTracer::Ready& fresh,
                                  const FrameTracer::Ready& usable) {
    std::vector<std::size_t> shown;
    for (auto& [index, s] : islands_) {
        if (!s.watch || !s.watch->viewed) {
            continue;
        }
        const KeyObservation o = observe_keys(s.watch->keys, in_view_, fresh, usable);
        s.watch->in_view = o.any;
        if (!o.any) {
            continue; // out of this frame's view: it draws nothing of the change
        }
        s.watch->cpu_ready = o.all_fresh;
        s.watch->gpu_ready = o.all_usable;
        if (o.all_usable && present_ok) {
            shown.push_back(index);
        }
    }
    for (const std::size_t index : shown) {
        settle_island(index, Settle::Presented);
    }
    retire_takes();
}

bool ChangeVisibility::take_done(const Take& t) const {
    return std::all_of(t.islands.begin(), t.islands.end(), [this](const auto& p) {
        const auto it = islands_.find(p.first);
        return it != islands_.end() && it->second.settled >= p.second;
    });
}

Visibility ChangeVisibility::take_stage(const Take& t) const {
    Visibility worst = Visibility::OutOfView;
    bool waiting = false;
    for (const auto& [index, version] : t.islands) {
        const auto it = islands_.find(index);
        if (it != islands_.end() && it->second.settled >= version) {
            continue;
        }
        Visibility stage = Visibility::AwaitingFrame;
        if (it == islands_.end() || it->second.committed < version) {
            stage = Visibility::Committing;
        } else if (it->second.watch) {
            const Watch& w = *it->second.watch;
            stage = !w.viewed      ? Visibility::AwaitingFrame
                    : !w.in_view   ? Visibility::OutOfView
                    : !w.cpu_ready ? Visibility::ChunksStale
                    : !w.gpu_ready ? Visibility::NotOnGpu
                                   : Visibility::NotPresented;
        }
        worst = waiting ? std::min(worst, stage) : stage;
        waiting = true;
    }
    return waiting ? worst : Visibility::Visible;
}

void ChangeVisibility::retire_takes() {
    while (!takes_.empty() && take_done(takes_.front())) {
        const Take& t = takes_.front();
        // How the change ended: shown if any island's presented, else offscreen, else nothing.
        Visibility result = Visibility::NothingToDraw;
        for (const auto& [index, version] : t.islands) {
            const auto it = islands_.find(index);
            const Settle kind = it == islands_.end() ? Settle::None : it->second.settle_kind;
            if (kind == Settle::Presented) {
                result = Visibility::Visible;
                break;
            }
            if (kind == Settle::Offscreen) {
                result = Visibility::Offscreen;
            }
        }
        history_.push_back({t.from, t.through, t.turns, t.coarse, result});
        while (history_.size() > kMaxHistory) {
            floor_ = std::max(floor_.value_or(0), history_.front().through);
            history_.pop_front();
        }
        takes_.pop_front();
    }
}

Visibility ChangeVisibility::classify(std::uint64_t turn) const {
    if (!followed_ || turn > *followed_) {
        return Visibility::NotInPicture;
    }
    for (auto it = takes_.rbegin(); it != takes_.rend(); ++it) {
        if (turn < it->from || turn > it->through) {
            continue;
        }
        if (!belongs(turn, it->from, it->through, it->coarse, it->turns)) {
            return Visibility::NoVisualChange;
        }
        if (!take_done(*it)) {
            return take_stage(*it);
        }
        // Done, behind an older take that is not: its end is the same rule as in retire_takes.
        Visibility result = Visibility::NothingToDraw;
        for (const auto& [index, version] : it->islands) {
            const auto s = islands_.find(index);
            const Settle kind = s == islands_.end() ? Settle::None : s->second.settle_kind;
            if (kind == Settle::Presented) {
                return Visibility::Visible;
            }
            if (kind == Settle::Offscreen) {
                result = Visibility::Offscreen;
            }
        }
        return result;
    }
    for (auto it = history_.rbegin(); it != history_.rend(); ++it) {
        if (turn < it->from || turn > it->through) {
            continue;
        }
        return belongs(turn, it->from, it->through, it->coarse, it->turns)
                   ? it->result
                   : Visibility::NoVisualChange;
    }
    return floor_ && turn <= *floor_ ? Visibility::Unknown : Visibility::NoVisualChange;
}

} // namespace anno::app
