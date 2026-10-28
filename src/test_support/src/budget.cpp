// Temporary admission stub. The scheduler tests call this and fail until the
// reservation rules replace it. Every claim is rejected and nothing is tracked,
// so a refusal, an admission and a shared budget are all wrong here.
#include "anno/test/budget.hpp"

#include <utility>

namespace anno::test {

struct Budget::State {};

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

void Budget::release_id(std::uint64_t) noexcept {}

void Budget::release_cpu() noexcept {}

std::shared_ptr<Budget> Budget::memory(BudgetHooks hooks, std::uint64_t headroom,
                                       const std::shared_ptr<Budget>&) {
    auto budget = std::shared_ptr<Budget>(new Budget());
    budget->state_ = std::make_shared<State>();
    budget->headroom_ = headroom;
    budget->hooks_ = hooks;
    budget->slot_ = 0;
    return budget;
}

std::shared_ptr<Budget> Budget::file(const std::filesystem::path&, BudgetHooks hooks,
                                     std::uint64_t headroom) {
    return memory(hooks, headroom);
}

Budget& Budget::process() {
    static auto budget = memory(BudgetHooks{}, 0);
    return *budget;
}

Reservation Budget::try_reserve(std::uint64_t bytes, Admit* info) {
    if (info != nullptr) {
        *info = Admit{};
        info->estimate = bytes;
        info->headroom = headroom_;
        info->reason = "stub";
    }
    return Reservation{};
}

Reservation Budget::reserve(std::uint64_t bytes, Admit* info) {
    if (hooks_.on_wait != nullptr) {
        hooks_.on_wait();
    }
    if (info != nullptr) {
        *info = Admit{};
        info->estimate = bytes;
        info->waited = hooks_.on_wait != nullptr;
        info->reason = "stub";
    }
    return Reservation{};
}

CpuToken Budget::acquire_cpu() {
    return CpuToken{};
}

std::uint64_t Budget::reserved() const {
    return 0;
}

void Budget::set_stop_below(std::uint64_t) {}

void Budget::wake() {}

Reservation reserve_cpu_render(std::uint64_t) {
    return Reservation{};
}

} // namespace anno::test
