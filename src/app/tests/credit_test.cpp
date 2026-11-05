// Admission credits and their payload (round dy, A1/A2; docs/app/game-shell.md, "Credits and byte
// accounting"). A probe in the payload records the ledger's bytes at the moment the payload is
// destroyed: the credit must still be held then, on every path that destroys or replaces a
// payload (destruction, move construction, move assignment, swap, exception unwinding), and an
// explicit transfer of the payload releases the credit while the payload lives on elsewhere.
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/credit.hpp>

namespace app = anno::app;

namespace {

/// Records the ledger's bytes when it is destroyed (the last part of the payload to go).
struct Probe {
    std::shared_ptr<app::Ledger> ledger;
    std::vector<std::size_t>* seen;
    Probe(std::shared_ptr<app::Ledger> l, std::vector<std::size_t>* s)
        : ledger(std::move(l)), seen(s) {}
    Probe(const Probe&) = delete;
    Probe& operator=(const Probe&) = delete;
    ~Probe() { seen->push_back(ledger->bytes.load()); }
};

/// A payload as the picture's are made: a probe first, then the data it accounts for.
struct Payload {
    std::shared_ptr<Probe> probe;
    std::vector<int> data;
};
using Held = app::Accounted<Payload>;

Held make(const std::shared_ptr<app::Ledger>& ledger, std::vector<std::size_t>& seen,
          std::size_t bytes) {
    Held h;
    h.probe = std::make_shared<Probe>(ledger, &seen);
    h.data.assign(16, 1);
    h.credit = app::Credit(ledger, bytes);
    return h;
}

} // namespace

TEST_CASE("credit: a payload is destroyed while its credit is still held" *
          doctest::test_suite("credit")) {
    auto ledger = std::make_shared<app::Ledger>();
    std::vector<std::size_t> seen;
    {
        Held h = make(ledger, seen, 64);
        CHECK(ledger->bytes.load() == 64);
        CHECK(ledger->count.load() == 1);
    }
    REQUIRE(seen.size() == 1);
    CHECK(seen[0] == 64); // the probe went last of the payload, before the credit
    CHECK(ledger->bytes.load() == 0);
    CHECK(ledger->count.load() == 0);
}

TEST_CASE("credit: move construction carries the credit with the payload" *
          doctest::test_suite("credit")) {
    auto ledger = std::make_shared<app::Ledger>();
    std::vector<std::size_t> seen;
    {
        Held a = make(ledger, seen, 64);
        Held b(std::move(a));
        CHECK(ledger->bytes.load() == 64);
        CHECK(ledger->count.load() == 1);
        CHECK(!a.credit.held());
        CHECK(b.credit.bytes() == 64);
        CHECK(seen.empty()); // nothing was destroyed by the move
    }
    REQUIRE(seen.size() == 1);
    CHECK(seen[0] == 64);
    CHECK(ledger->bytes.load() == 0);
}

TEST_CASE("credit: move assignment destroys the replaced payload before its credit" *
          doctest::test_suite("credit")) {
    auto ledger = std::make_shared<app::Ledger>();
    std::vector<std::size_t> seen;
    {
        Held target = make(ledger, seen, 64);
        Held incoming = make(ledger, seen, 32);
        CHECK(ledger->bytes.load() == 96);
        target = std::move(incoming);
        // The replaced payload (64) was destroyed with both credits held, then its credit went.
        REQUIRE(seen.size() == 1);
        CHECK(seen[0] == 96);
        CHECK(ledger->bytes.load() == 32);
        CHECK(target.credit.bytes() == 32);
        CHECK(target.data.size() == 16);
    }
    REQUIRE(seen.size() == 2);
    CHECK(seen[1] == 32);
    CHECK(ledger->bytes.load() == 0);
}

TEST_CASE("credit: a swap keeps each credit with its payload" * doctest::test_suite("credit")) {
    auto ledger = std::make_shared<app::Ledger>();
    std::vector<std::size_t> seen;
    Held a = make(ledger, seen, 64);
    Held b = make(ledger, seen, 32);
    std::swap(a, b);
    CHECK(seen.empty());
    CHECK(a.credit.bytes() == 32);
    CHECK(b.credit.bytes() == 64);
    CHECK(ledger->bytes.load() == 96);
    b = Held{};
    REQUIRE(seen.size() == 1);
    CHECK(seen[0] == 96); // b's payload (credit 64) went first
    CHECK(ledger->bytes.load() == 32);
}

TEST_CASE("credit: exception unwinding destroys the payload before the credit" *
          doctest::test_suite("credit")) {
    auto ledger = std::make_shared<app::Ledger>();
    std::vector<std::size_t> seen;
    auto fail_while_filling = [&] {
        Held h = make(ledger, seen, 64);
        h.data.push_back(2);
        throw std::runtime_error("injected");
    };
    CHECK_THROWS_AS(fail_while_filling(), std::runtime_error);
    REQUIRE(seen.size() == 1);
    CHECK(seen[0] == 64);
    CHECK(ledger->bytes.load() == 0);
    CHECK(ledger->count.load() == 0);
}

TEST_CASE("credit: a transfer releases the credit while the payload lives on elsewhere" *
          doctest::test_suite("credit")) {
    // A commit hands the payload to the next generation (accounted there), then the emptied
    // result goes: its credit is released with the payload still alive.
    auto ledger = std::make_shared<app::Ledger>();
    std::vector<std::size_t> seen;
    std::shared_ptr<Probe> moved;
    {
        Held h = make(ledger, seen, 64);
        moved = std::move(h.probe);
    }
    CHECK(seen.empty());
    CHECK(ledger->bytes.load() == 0);
    moved.reset();
    REQUIRE(seen.size() == 1);
    CHECK(seen[0] == 0);
}

TEST_CASE("credit: resize replaces the estimate and the peak keeps the largest total" *
          doctest::test_suite("credit")) {
    auto ledger = std::make_shared<app::Ledger>();
    app::Credit c(ledger, 100);
    CHECK(ledger->peak_bytes.load() == 100);
    c.resize(40);
    CHECK(ledger->bytes.load() == 40);
    c.resize(150);
    CHECK(ledger->bytes.load() == 150);
    CHECK(ledger->peak_bytes.load() == 150);
    app::Credit empty;
    empty.resize(10);
    CHECK(ledger->bytes.load() == 150);
    c = app::Credit{};
    CHECK(ledger->bytes.load() == 0);
    CHECK(ledger->count.load() == 0);
    CHECK(ledger->peak_bytes.load() == 150);
}
