// Host memory reservations for the test harness. In-memory budgets are the
// unit-test double. Cross-process budgets talk to one coordinator so Windows
// and WSL clients subtract the same pending bytes from free memory.
#include "anno/test/budget.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "admit_detail.hpp"
#include "anno/test/parallel.hpp"
#include "coord_session.hpp"

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace anno::test {
namespace {

using namespace detail;

void emit_log(bool enabled, const std::filesystem::path& log_path, const std::string& line) {
    if (!enabled) {
        return;
    }
    std::fprintf(stderr, "%s\n", line.c_str());
    std::fflush(stderr);
    if (log_path.empty()) {
        return;
    }
    std::FILE* file = std::fopen(log_path.string().c_str(), "ab");
    if (file == nullptr) {
        return;
    }
    std::fwrite(line.data(), 1, line.size(), file);
    std::fputc('\n', file);
    std::fflush(file);
#if defined(_WIN32)
    _commit(_fileno(file));
#else
    fsync(fileno(file));
#endif
    std::fclose(file);
}

std::string num(std::uint64_t value) {
    return std::to_string(value);
}

std::string admit_line(const char* kind, const Admit& info) {
    return std::string("[anno-test memory] ") + kind + " estimate=" + num(info.estimate) +
           " available=" + num(info.available) + " headroom=" + num(info.headroom) +
           " pending=" + num(info.pending) + " resident=" + num(info.resident) +
           " reason=" + info.reason;
}

struct Waiter {
    std::uint64_t seq = 0;
    std::uint64_t bytes = 0;
    int slot = -1;
};

struct MemTable {
    std::mutex mu;
    std::condition_variable cv;
    struct Slot {
        BudgetHooks hooks;
        std::vector<Hold> holds;
        ReadingBase base;
        std::uint64_t cancel_gen = 0;
    };
    std::vector<Slot> slots;
    std::vector<Waiter> waiters;
    std::uint64_t next_seq = 1;
    std::uint64_t next_id = 1;
};

struct MemEval {
    Decision decision = Decision::Refuse;
    Admit info;
};

MemEval evaluate_mem(MemTable& table, int slot, std::uint64_t bytes, std::uint64_t headroom,
                     std::uint64_t stop_below, std::uint64_t self_seq) {
    // In one process every reading is taken here, under the table lock, after
    // every release and residency change it could have missed.
    Constraint c;
    for (MemTable::Slot& one : table.slots) {
        observe(one.holds, one.base, one.hooks);
        for (const Hold& hold : one.holds) {
            c.resident = sat_add(c.resident, hold.resident_bytes);
            c.pending = sat_add(c.pending, hold.bytes - hold.resident_bytes);
        }
    }
    std::uint64_t oldest_seq = 0;
    std::uint64_t oldest_bytes = 0;
    for (const Waiter& waiter : table.waiters) {
        if (oldest_seq == 0 || waiter.seq < oldest_seq) {
            oldest_seq = waiter.seq;
            oldest_bytes = waiter.bytes;
        }
    }
    View view;
    view.headroom = headroom;
    view.stop_below = stop_below;
    view.oldest_bytes = oldest_bytes;
    view.oldest_seq = oldest_seq;
    view.self_seq = self_seq;
    const BudgetHooks& hooks = table.slots.at(static_cast<std::size_t>(slot)).hooks;
    c.meter = hooks.free_bytes != nullptr;
    if (c.meter) {
        c.free = hooks.free_bytes();
    }
    if (hooks.total_bytes != nullptr) {
        c.total = hooks.total_bytes();
    }
    view.constraints.push_back(c);
    MemEval out;
    out.decision = decide(view, bytes, out.info);
    return out;
}

std::filesystem::path default_budget_path() {
    if (const char* env = std::getenv("ANNO_TEST_BUDGET_FILE")) {
        if (env[0] != '\0') {
            return env;
        }
    }
#if defined(_WIN32)
    if (const char* local = std::getenv("LOCALAPPDATA")) {
        if (local[0] != '\0') {
            return std::filesystem::path(local) / "anno1503" / "test-budget.bin";
        }
    }
#else
    if (const char* user = std::getenv("USER")) {
        if (user[0] != '\0') {
            const std::filesystem::path local =
                std::filesystem::path("/mnt/c/Users") / user / "AppData" / "Local";
            std::error_code error;
            if (std::filesystem::is_directory(local, error)) {
                return local / "anno1503" / "test-budget.bin";
            }
        }
    }
    if (const char* home = std::getenv("HOME")) {
        if (home[0] != '\0') {
            return std::filesystem::path(home) / ".cache" / "anno1503" / "test-budget.bin";
        }
    }
#endif
    return std::filesystem::temp_directory_path() / "anno1503" / "test-budget.bin";
}

std::filesystem::path log_path_for(const std::filesystem::path& budget_path) {
    if (const char* env = std::getenv("ANNO_TEST_MEMORY_LOG")) {
        if (env[0] != '\0') {
            return env;
        }
    }
    if (budget_path.empty()) {
        return {};
    }
    return budget_path.parent_path() / "memory.log";
}

} // namespace

struct Budget::State {
    enum class Kind { Memory, File } kind = Kind::Memory;
    std::shared_ptr<MemTable> mem;
    std::shared_ptr<detail::CoordSession> coord;
    std::filesystem::path log_path;
};

std::shared_ptr<Budget> Budget::open_coord(const std::filesystem::path& path, BudgetHooks hooks,
                                           std::uint64_t headroom, bool host_wide) {
    auto budget = std::shared_ptr<Budget>(new Budget());
    budget->state_ = std::make_shared<State>();
    budget->state_->kind = State::Kind::File;
    std::string name;
    if (host_wide) {
        name = "host";
    } else if (const char* id = std::getenv("ANNO_TEST_BUDGET_ID")) {
        if (id[0] != '\0') {
            name = id;
        }
    }
    budget->state_->coord = detail::coord_connect(path, name, host_wide, hooks);
    budget->state_->log_path = log_path_for(path);
    budget->headroom_ = headroom;
    budget->hooks_ = hooks;
    return budget;
}

Reservation::Reservation(Reservation&& other) noexcept
    : budget_(std::move(other.budget_)), id_(other.id_), bytes_(other.bytes_) {
    other.id_ = 0;
    other.bytes_ = 0;
}

Reservation& Reservation::operator=(Reservation&& other) noexcept {
    if (this != &other) {
        release();
        budget_ = std::move(other.budget_);
        id_ = other.id_;
        bytes_ = other.bytes_;
        other.id_ = 0;
        other.bytes_ = 0;
    }
    return *this;
}

Reservation::~Reservation() {
    release();
}

void Reservation::release() noexcept {
    if (budget_) {
        budget_->release_id(id_);
        budget_.reset();
    }
    id_ = 0;
    bytes_ = 0;
}

CpuToken::CpuToken(CpuToken&& other) noexcept
    : budget_(std::move(other.budget_)), held_(other.held_) {
    other.held_ = false;
}

CpuToken& CpuToken::operator=(CpuToken&& other) noexcept {
    if (this != &other) {
        release();
        budget_ = std::move(other.budget_);
        held_ = other.held_;
        other.held_ = false;
    }
    return *this;
}

CpuToken::~CpuToken() {
    release();
}

void CpuToken::release() noexcept {
    if (budget_ && held_) {
        budget_->release_cpu();
    }
    held_ = false;
    budget_.reset();
}

std::shared_ptr<Budget> Budget::memory(BudgetHooks hooks, std::uint64_t headroom,
                                       const std::shared_ptr<Budget>& share) {
    auto budget = std::shared_ptr<Budget>(new Budget());
    if (share && share->state_ && share->state_->kind == State::Kind::Memory) {
        budget->state_ = share->state_;
    } else {
        budget->state_ = std::make_shared<State>();
        budget->state_->kind = State::Kind::Memory;
        budget->state_->mem = std::make_shared<MemTable>();
    }
    MemTable::Slot slot;
    slot.hooks = hooks;
    budget->state_->mem->slots.push_back(std::move(slot));
    budget->slot_ = static_cast<int>(budget->state_->mem->slots.size()) - 1;
    budget->headroom_ = headroom;
    budget->hooks_ = hooks;
    return budget;
}

std::shared_ptr<Budget> Budget::file(const std::filesystem::path& path, BudgetHooks hooks,
                                     std::uint64_t headroom) {
    return open_coord(path, hooks, headroom, false);
}

Budget& Budget::process() {
    static std::shared_ptr<Budget> budget = [] {
        BudgetHooks hooks;
        hooks.free_bytes = &free_memory_bytes;
        hooks.process_private = &process_private_bytes;
        hooks.total_bytes = &total_memory_bytes;
        auto created = open_coord(default_budget_path(), hooks, memory_headroom_bytes(), true);
        created->logging_ = true;
        created->state_->coord->log = [path = created->state_->log_path](const std::string& line) {
            emit_log(true, path, line);
        };
        // Which coordinator build this test process measures against.
        emit_log(true, created->state_->log_path,
                 "[anno-test memory] coordinator " + created->state_->coord->identity);
        created->set_stop_below(memory_stop_bytes());
        return created;
    }();
    return *budget;
}

Reservation Budget::make_hold(std::uint64_t id, std::uint64_t bytes) {
    Reservation reservation;
    reservation.budget_ = shared_from_this();
    reservation.id_ = id;
    reservation.bytes_ = bytes;
    return reservation;
}

Reservation Budget::try_reserve(std::uint64_t bytes, Admit* info) {
    if (state_->kind == State::Kind::Memory) {
        std::lock_guard<std::mutex> lock(state_->mem->mu);
        const MemEval eval = evaluate_mem(*state_->mem, slot_, bytes, headroom_, stop_below_, 0);
        if (info != nullptr) {
            *info = eval.info;
        }
        if (eval.decision != Decision::Admit) {
            return Reservation{};
        }
        Hold hold;
        hold.id = state_->mem->next_id++;
        hold.bytes = bytes;
        state_->mem->slots.at(static_cast<std::size_t>(slot_)).holds.push_back(hold);
        state_->mem->cv.notify_all();
        return make_hold(hold.id, bytes);
    }
    const detail::CoordResult result =
        detail::coord_try(*state_->coord, bytes, headroom_, stop_below_);
    if (result.info.admitted) {
        emit_log(logging_, state_->log_path, admit_line("admit", result.info));
    } else if (result.info.refused) {
        emit_log(logging_, state_->log_path, admit_line("refuse", result.info));
    }
    if (info != nullptr) {
        *info = result.info;
    }
    if (!result.info.admitted) {
        return Reservation{};
    }
    return make_hold(result.hold_id, bytes);
}

Reservation Budget::reserve(std::uint64_t bytes, Admit* info) {
    if (state_->kind == State::Kind::Memory) {
        std::unique_lock<std::mutex> lock(state_->mem->mu);
        auto& slot = state_->mem->slots.at(static_cast<std::size_t>(slot_));
        const std::uint64_t seen = slot.cancel_gen;
        std::uint64_t my_seq = 0;
        bool hooked = false;
        const auto deadline = std::chrono::steady_clock::now() + admit_deadline();
        for (;;) {
            if (std::chrono::steady_clock::now() >= deadline) {
                std::erase_if(state_->mem->waiters,
                              [&](const Waiter& waiter) { return waiter.seq == my_seq; });
                state_->mem->cv.notify_all();
                Admit out;
                out.refused = true;
                out.waited = hooked;
                out.reason = "wait deadline";
                out.estimate = bytes;
                if (info != nullptr) {
                    *info = out;
                }
                return Reservation{};
            }
            if (slot.cancel_gen != seen) {
                std::erase_if(state_->mem->waiters,
                              [&](const Waiter& waiter) { return waiter.seq == my_seq; });
                state_->mem->cv.notify_all();
                Admit out;
                out.refused = true;
                out.waited = hooked;
                out.reason = "cancelled";
                out.estimate = bytes;
                if (info != nullptr) {
                    *info = out;
                }
                return Reservation{};
            }
            const MemEval eval =
                evaluate_mem(*state_->mem, slot_, bytes, headroom_, stop_below_, my_seq);
            if (eval.decision == Decision::Admit) {
                std::erase_if(state_->mem->waiters,
                              [&](const Waiter& waiter) { return waiter.seq == my_seq; });
                Hold hold;
                hold.id = state_->mem->next_id++;
                hold.bytes = bytes;
                state_->mem->slots.at(static_cast<std::size_t>(slot_)).holds.push_back(hold);
                state_->mem->cv.notify_all();
                Admit out = eval.info;
                out.waited = hooked;
                if (info != nullptr) {
                    *info = out;
                }
                return make_hold(hold.id, bytes);
            }
            if (eval.decision == Decision::Refuse) {
                std::erase_if(state_->mem->waiters,
                              [&](const Waiter& waiter) { return waiter.seq == my_seq; });
                state_->mem->cv.notify_all();
                Admit out = eval.info;
                out.waited = hooked;
                if (info != nullptr) {
                    *info = out;
                }
                return Reservation{};
            }
            if (my_seq == 0) {
                my_seq = state_->mem->next_seq++;
                state_->mem->waiters.push_back(Waiter{my_seq, bytes, slot_});
            }
            if (!hooked) {
                lock.unlock();
                if (hooks_.on_wait != nullptr) {
                    hooks_.on_wait();
                }
                hooked = true;
                lock.lock();
                continue;
            }
            // A timed recheck: free memory can change outside the budget
            // (external pressure or recovery). It only re-decides.
            state_->mem->cv.wait_for(lock, recheck_period());
        }
    }

    const detail::CoordResult result =
        detail::coord_reserve(*state_->coord, bytes, headroom_, stop_below_);
    if (result.info.admitted) {
        emit_log(logging_, state_->log_path, admit_line("admit", result.info));
    } else if (result.info.refused) {
        emit_log(logging_, state_->log_path, admit_line("refuse", result.info));
    } else {
        emit_log(logging_, state_->log_path, admit_line("wait", result.info));
    }
    if (info != nullptr) {
        *info = result.info;
    }
    if (!result.info.admitted) {
        return Reservation{};
    }
    return make_hold(result.hold_id, bytes);
}

CpuToken Budget::acquire_cpu() {
    if (state_->kind != State::Kind::File) {
        return CpuToken{};
    }
    const detail::CoordResult result = detail::coord_cpu_acquire(*state_->coord);
    if (!result.info.admitted) {
        return CpuToken{};
    }
    CpuToken token;
    token.budget_ = shared_from_this();
    token.held_ = true;
    return token;
}

void Budget::release_id(std::uint64_t id) noexcept {
    if (id == 0 || !state_) {
        return;
    }
    try {
        if (state_->kind == State::Kind::Memory) {
            std::lock_guard<std::mutex> lock(state_->mem->mu);
            auto& slot = state_->mem->slots.at(static_cast<std::size_t>(slot_));
            observe(slot.holds, slot.base, slot.hooks);
            for (const Hold& hold : slot.holds) {
                if (hold.id == id) {
                    fold_released(slot.base, hold);
                }
            }
            std::erase_if(slot.holds, [&](const Hold& hold) { return hold.id == id; });
            state_->mem->cv.notify_all();
            return;
        }
        const std::uint64_t released = detail::coord_release(*state_->coord, id);
        if (logging_) {
            const std::optional<std::uint64_t> private_bytes =
                hooks_.process_private != nullptr ? hooks_.process_private() : std::nullopt;
            emit_log(true, state_->log_path,
                     "[anno-test memory] end estimate=" + num(released) + " private=" +
                         (private_bytes ? num(*private_bytes) : std::string("unknown")));
        }
    } catch (...) {
    }
}

void Budget::release_cpu() noexcept {
    if (!state_ || state_->kind != State::Kind::File) {
        return;
    }
    try {
        detail::coord_cpu_release(*state_->coord);
    } catch (...) {
    }
}

std::uint64_t Budget::reserved() const {
    if (!state_) {
        return 0;
    }
    if (state_->kind == State::Kind::Memory) {
        std::lock_guard<std::mutex> lock(state_->mem->mu);
        return sum_reserved(state_->mem->slots.at(static_cast<std::size_t>(slot_)).holds);
    }
    return detail::coord_local_reserved(*state_->coord);
}

void Budget::set_stop_below(std::uint64_t bytes) {
    stop_below_ = bytes;
}

void Budget::wake() {
    if (!state_) {
        return;
    }
    if (state_->kind == State::Kind::Memory) {
        state_->mem->cv.notify_all();
        return;
    }
    detail::coord_wake(*state_->coord);
}

void Budget::cancel() {
    if (!state_) {
        return;
    }
    if (state_->kind == State::Kind::Memory) {
        std::lock_guard<std::mutex> lock(state_->mem->mu);
        auto& slot = state_->mem->slots.at(static_cast<std::size_t>(slot_));
        slot.cancel_gen += 1;
        state_->mem->cv.notify_all();
        return;
    }
    detail::coord_cancel(*state_->coord);
}

static Reservation reserve_bytes(std::uint64_t bytes) {
    if (bytes == 0) {
        return Reservation{};
    }
    Admit info;
    Reservation hold = Budget::process().reserve(bytes, &info);
    if (!hold) {
        throw std::runtime_error("[anno-test memory] refused estimate=" + std::to_string(bytes) +
                                 " reason=" + info.reason +
                                 " available=" + std::to_string(info.available));
    }
    return hold;
}

Reservation reserve_cpu_render(std::uint64_t pixels) {
    if (pixels == 0) {
        return Reservation{};
    }
    return reserve_bytes(cpu_render_additional_bytes(pixels));
}

Reservation reserve_cpu_setup() {
    return reserve_bytes(cpu_render_setup_bytes());
}

} // namespace anno::test
