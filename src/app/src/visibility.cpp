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

void ChangeVisibility::record_settle(IslandState& s, std::uint64_t version, Settle kind) {
    s.settled = std::max(s.settled, version);
    s.settles.emplace_back(version, kind);
    while (s.settles.size() > 256) {
        s.settles.pop_front();
    }
}

void ChangeVisibility::advance(IslandState& s) {
    while (!s.parts.empty()) {
        const Part& front = s.parts.front();
        Settle kind = Settle::None;
        switch (front.state) {
        case Part::State::Shown:
            kind = Settle::Presented;
            break;
        case Part::State::Offscreen:
            kind = Settle::Offscreen;
            break;
        case Part::State::Empty:
            kind = Settle::NothingToDraw;
            break;
        default:
            return; // not finished: the later parts wait for it
        }
        record_settle(s, front.version, kind);
        s.parts.pop_front();
    }
}

Visibility ChangeVisibility::take_result(const Take& t) const {
    Visibility result = Visibility::NothingToDraw;
    for (const auto& [index, version] : t.islands) {
        const auto it = islands_.find(index);
        if (it == islands_.end()) {
            continue;
        }
        // The settle that completed this version: the first one at or after it.
        const auto settle = std::find_if(it->second.settles.begin(), it->second.settles.end(),
                                         [v = version](const auto& p) { return p.first >= v; });
        if (settle == it->second.settles.end()) {
            return Visibility::Unknown; // aged out of the island's bounded record
        }
        if (settle->second == Settle::Presented) {
            return Visibility::Visible;
        }
        if (settle->second == Settle::Offscreen) {
            result = Visibility::Offscreen;
        }
    }
    return result;
}

void ChangeVisibility::committed(const std::vector<ScenePicture::CommittedIsland>& islands) {
    for (const ScenePicture::CommittedIsland& c : islands) {
        IslandState& s = islands_[c.index];
        s.committed = std::max(s.committed, c.version);
        Part part;
        part.version = c.version;
        part.keys = c.keys;
        std::sort(part.keys.begin(), part.keys.end());
        part.keys.erase(
            std::unique(part.keys.begin(), part.keys.end(),
                        [](const auto& a, const auto& b) { return !(a < b) && !(b < a); }),
            part.keys.end());
        part.state = part.keys.empty() ? Part::State::Empty : Part::State::Unjudged;
        s.parts.push_back(std::move(part));
        if (s.parts.size() > kMaxParts) {
            // Merge the two oldest: the union of their chunks under the newer version, judged
            // again. Only stricter than keeping both.
            Part second = std::move(s.parts[1]);
            Part& first = s.parts[0];
            first.version = second.version;
            first.keys.insert(first.keys.end(), second.keys.begin(), second.keys.end());
            std::sort(first.keys.begin(), first.keys.end());
            first.keys.erase(
                std::unique(first.keys.begin(), first.keys.end(),
                            [](const auto& a, const auto& b) { return !(a < b) && !(b < a); }),
                first.keys.end());
            first.state = first.keys.empty() ? Part::State::Empty : Part::State::Unjudged;
            first.cpu_ready = first.gpu_ready = false;
            first.in_view = true;
            s.parts.erase(s.parts.begin() + 1);
        }
        advance(s);
    }
    retire_takes();
}

void ChangeVisibility::begin_frame(render::Rect view, std::size_t group) {
    in_view_.clear();
    bool any = false;
    for (const auto& [index, s] : islands_) {
        any = any || !s.parts.empty();
    }
    if (!any) {
        return;
    }
    const std::vector<render::ChunkKey> keys = view_keys(view, group);
    in_view_.insert(keys.begin(), keys.end());
    for (auto& [index, s] : islands_) {
        for (Part& part : s.parts) {
            if (part.state != Part::State::Unjudged) {
                continue;
            }
            // The first attempt after the commit decides (FrameTracer::begin): in view, it is
            // watched; none of its chunks in view, it is offscreen and never waited for.
            const bool seen = std::any_of(part.keys.begin(), part.keys.end(),
                                          [this](const auto& k) { return in_view_.count(k) != 0; });
            part.state = seen ? Part::State::Viewed : Part::State::Offscreen;
        }
        advance(s);
    }
    retire_takes();
}

void ChangeVisibility::frame_done(bool present_ok, const FrameTracer::Ready& fresh,
                                  const FrameTracer::Ready& usable) {
    for (auto& [index, s] : islands_) {
        for (Part& part : s.parts) {
            if (part.state != Part::State::Viewed) {
                continue;
            }
            const KeyObservation o = observe_keys(part.keys, in_view_, fresh, usable);
            part.in_view = o.any;
            if (!o.any) {
                continue; // out of this frame's view: it draws nothing of the change
            }
            part.cpu_ready = o.all_fresh;
            part.gpu_ready = o.all_usable;
            if (o.all_usable && present_ok) {
                part.state = Part::State::Shown;
            }
        }
        advance(s);
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
        } else if (!it->second.parts.empty()) {
            // The parts settle in order: the front one is what holds this version back.
            const Part& p = it->second.parts.front();
            stage = p.state == Part::State::Unjudged ? Visibility::AwaitingFrame
                    : !p.in_view                     ? Visibility::OutOfView
                    : !p.cpu_ready                   ? Visibility::ChunksStale
                    : !p.gpu_ready                   ? Visibility::NotOnGpu
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
        history_.push_back({t.from, t.through, t.turns, t.coarse, take_result(t)});
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
        return take_result(*it);
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
