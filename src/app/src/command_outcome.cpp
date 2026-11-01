// Command outcomes (see the header).
#include <algorithm>
#include <utility>

#include <anno/app/command_outcome.hpp>

namespace anno::app {

namespace {

std::size_t origin_index(CommandOrigin origin) {
    return origin == CommandOrigin::Script ? 0U : 1U;
}

} // namespace

std::size_t CommandOutcome::records_done() const {
    return static_cast<std::size_t>(std::count(records.begin(), records.end(), sim::Reject::None));
}

OutcomeLog::OutcomeLog(std::size_t max_outcomes, std::size_t max_records)
    : max_outcomes_(std::max<std::size_t>(max_outcomes, 1)), max_records_(max_records) {}

void OutcomeLog::record(CommandOutcome outcome) {
    const std::lock_guard lock(mu_);
    outcome.seq = next_seq_++;
    std::uint64_t& ran = ran_[origin_index(outcome.id.origin)];
    ran = std::max(ran, outcome.id.n);
    std::uint64_t& issued = issued_[origin_index(outcome.id.origin)];
    issued = std::max(issued, outcome.id.n);
    kept_records_ += outcome.records.size();
    kept_.push_back(std::move(outcome));
    evict_locked();
}

void OutcomeLog::evict_locked() {
    // The newest outcome is always kept, so a reader that keeps up never loses one; a single
    // outcome larger than max_records is kept alone.
    while (kept_.size() > 1 && (kept_.size() > max_outcomes_ || kept_records_ > max_records_)) {
        kept_records_ -= kept_.front().records.size();
        kept_.pop_front();
    }
}

std::uint64_t OutcomeLog::next() const {
    const std::lock_guard lock(mu_);
    return next_seq_;
}

void OutcomeLog::set_bounds(std::size_t max_outcomes, std::size_t max_records) {
    const std::lock_guard lock(mu_);
    max_outcomes_ = std::max<std::size_t>(max_outcomes, 1);
    max_records_ = max_records;
    evict_locked();
}

std::size_t OutcomeLog::max_outcomes() const {
    const std::lock_guard lock(mu_);
    return max_outcomes_;
}

std::size_t OutcomeLog::max_records() const {
    const std::lock_guard lock(mu_);
    return max_records_;
}

void OutcomeLog::issued(CommandOrigin origin, std::uint64_t n) {
    const std::lock_guard lock(mu_);
    std::uint64_t& issued = issued_[origin_index(origin)];
    issued = std::max(issued, n);
}

OutcomeRead OutcomeLog::since(std::uint64_t from) const {
    const std::lock_guard lock(mu_);
    OutcomeRead read;
    from = std::max<std::uint64_t>(from, 1);
    read.next = std::max(from, next_seq_);
    const std::uint64_t first = kept_.empty() ? next_seq_ : kept_.front().seq;
    if (from < first) {
        read.lost_from = from;
        read.lost = first - from;
    }
    for (const CommandOutcome& o : kept_) {
        if (o.seq >= from) {
            read.outcomes.push_back(o);
        }
    }
    return read;
}

CommandOutcome OutcomeLog::find(CommandId id) const {
    const std::lock_guard lock(mu_);
    CommandOutcome out;
    out.id = id;
    const std::size_t o = origin_index(id.origin);
    if (id.n == 0 || id.n > issued_[o]) {
        out.state = CommandOutcome::State::Unknown;
        return out;
    }
    if (id.n > ran_[o]) {
        out.state = CommandOutcome::State::Queued;
        return out;
    }
    for (const CommandOutcome& k : kept_) {
        if (k.id == id) {
            return k;
        }
    }
    out.state = CommandOutcome::State::Lost;
    return out;
}

const char* outcome_state_name(CommandOutcome::State state) {
    switch (state) {
    case CommandOutcome::State::Queued:
        return "Queued";
    case CommandOutcome::State::Applied:
        return "Applied";
    case CommandOutcome::State::Refused:
        return "Refused";
    case CommandOutcome::State::Lost:
        return "Lost";
    case CommandOutcome::State::Unknown:
        return "Unknown";
    }
    return "?";
}

} // namespace anno::app
