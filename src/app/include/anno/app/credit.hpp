// Admission credits of the scene picture (docs/app/game-shell.md, "Credits and byte accounting").
// A credit is one build's slot and its reserved bytes in a Ledger. It is held by the object that
// carries the build's payload and is released only after that payload has been destroyed, or when
// the payload has been handed to a scene generation (an explicit transfer, accounted there by
// render::payload_counts). Round dy, A1/A2.
#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace anno::app {

/// The admission counts: the credits that are held and their bytes. Only Credit changes them.
/// The peak is the largest byte total seen at a reservation or a resize.
struct Ledger {
    std::atomic<std::size_t> count{0};
    std::atomic<std::size_t> bytes{0};
    std::atomic<std::size_t> peak_bytes{0};
};

/// One build's credit. The release takes no lock, allocates nothing and does not throw (workers
/// and the reaper run it).
class Credit {
  public:
    Credit() = default;
    Credit(std::shared_ptr<Ledger> ledger, std::size_t bytes)
        : ledger_(std::move(ledger)), bytes_(bytes) {
        ledger_->count.fetch_add(1, std::memory_order_relaxed);
        note_peak(ledger_->bytes.fetch_add(bytes_, std::memory_order_relaxed) + bytes_);
    }
    Credit(Credit&& other) noexcept
        : ledger_(std::move(other.ledger_)), bytes_(std::exchange(other.bytes_, 0)) {}
    Credit& operator=(Credit&& other) noexcept {
        if (this != &other) {
            release();
            ledger_ = std::move(other.ledger_);
            bytes_ = std::exchange(other.bytes_, 0);
        }
        return *this;
    }
    Credit(const Credit&) = delete;
    Credit& operator=(const Credit&) = delete;
    ~Credit() { release(); }

    /// Replaces the reserved bytes with `bytes` (a measured size for the estimate the build was
    /// admitted by). No effect on an empty credit.
    void resize(std::size_t bytes) noexcept {
        if (!ledger_) {
            return;
        }
        if (bytes >= bytes_) {
            note_peak(ledger_->bytes.fetch_add(bytes - bytes_, std::memory_order_relaxed) +
                      (bytes - bytes_));
        } else {
            ledger_->bytes.fetch_sub(bytes_ - bytes, std::memory_order_relaxed);
        }
        bytes_ = bytes;
    }
    [[nodiscard]] std::size_t bytes() const noexcept { return ledger_ ? bytes_ : 0; }
    [[nodiscard]] bool held() const noexcept { return ledger_ != nullptr; }

  private:
    void note_peak(std::size_t now) noexcept {
        std::size_t peak = ledger_->peak_bytes.load(std::memory_order_relaxed);
        while (now > peak &&
               !ledger_->peak_bytes.compare_exchange_weak(peak, now, std::memory_order_relaxed)) {
        }
    }
    void release() noexcept {
        if (ledger_) {
            ledger_->bytes.fetch_sub(bytes_, std::memory_order_relaxed);
            ledger_->count.fetch_sub(1, std::memory_order_relaxed);
            ledger_.reset();
        }
    }

    std::shared_ptr<Ledger> ledger_;
    std::size_t bytes_ = 0;
};

/// The credit of an Accounted object, as a base class of its own.
struct CreditHolder {
    Credit credit;
};

/// A payload and the credit that accounts for it. CreditHolder is the first base, so it is
/// constructed first and destroyed last: every payload member has been destroyed before the credit
/// is released. Move assignment keeps that boundary for the payload it replaces: the old payload
/// and credit move into a temporary, the new ones take their place, and the temporary is destroyed
/// payload first, credit last. Payload must leave a moved-from object holding no resources (true
/// for the standard containers and smart pointers it is made of), so assigning into it frees
/// nothing.
template <class Payload> struct Accounted : CreditHolder, Payload {
    Accounted() = default;
    Accounted(Accounted&&) noexcept(std::is_nothrow_move_constructible_v<Payload>) = default;
    Accounted&
    operator=(Accounted&& other) noexcept(std::is_nothrow_move_constructible_v<Payload> &&
                                          std::is_nothrow_move_assignable_v<Payload>) {
        if (this != &other) {
            Accounted old(std::move(*this));
            static_cast<Payload&>(*this) = std::move(static_cast<Payload&>(other));
            credit = std::move(other.credit);
        }
        return *this;
    }
    Accounted(const Accounted&) = delete;
    Accounted& operator=(const Accounted&) = delete;
    ~Accounted() = default;
};

} // namespace anno::app
