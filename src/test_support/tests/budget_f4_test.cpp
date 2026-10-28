// T10c F4 regressions (round cu).
//
//   E1: a successor never reuses a hold id that is in the journal it replayed,
//       so a journaled promise survives two restarts in both orders.
//
// Helpers are the F3 process harness (budget_proc_harness.hpp), unchanged.
// Ports 47820–47839; journals under temp_directory_path()/anno1503-f4.
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/test/budget.hpp>

#include "budget_proc_harness.hpp"
#include "child_process.hpp"

namespace {

using anno::test::child::Child;
using namespace std::chrono_literals;

constexpr std::uint16_t kPortOrphanReuse = 47820; // E1, orphan kept
constexpr std::uint16_t kPortLiveHold = 47821;    // E1, live hold kept
constexpr std::uint16_t kPortCancelAll = 47822;   // E2

std::filesystem::path f4_dir() {
    const auto dir = std::filesystem::temp_directory_path() / "anno1503-f4";
    std::filesystem::create_directories(dir);
    return dir;
}

std::string journal_text(const std::filesystem::path& journal) {
    std::ifstream in(journal);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

} // namespace

TEST_CASE("F4 (E1): an orphan survives a successor's reuse of its hold id and a second restart" *
          doctest::test_suite("budget")) {
    const auto journal = f4_dir() / "a.journal";
    const auto budget_path = (f4_dir() / "a.budget").string();
    remove_file(journal);
    std::string listening;
    Child c1 = start_coordinator(kPortOrphanReuse, journal, std::nullopt, std::nullopt, listening);
    const Vars vars = client_vars(kPortOrphanReuse, "f4-a", "30");

    std::string seen;
    Child holder_p;
    {
        Env env(vars);
        holder_p = probe({"holder", budget_path, "50"});
    }
    const std::string ready_p = want(holder_p, "READY", 15000ms, &seen);
    INFO("P: " << seen);
    REQUIRE(has(ready_p, "READY 1"));

    c1.stop(); // P stays alive; its session is lost; P owns an orphan.

    Child c2 = start_coordinator(kPortOrphanReuse, journal, std::nullopt, std::nullopt, listening);
    const std::string c2_listen = listening;
    INFO("C2 LISTENING: " << c2_listen);
    CHECK(has(c2_listen, "orphans=1"));

    Child holder_q;
    {
        Env env(vars);
        holder_q = probe({"holder", budget_path, "20"});
    }
    seen.clear();
    const std::string ready_q = want(holder_q, "READY", 15000ms, &seen);
    INFO("Q: " << seen);
    REQUIRE(has(ready_q, "READY 1"));
    holder_q.send("q\n");
    REQUIRE(eventually([&] { return !holder_q.running(); }, 10000ms));

    // Compaction rewrites only after 20000 appends (Journal::wants_compaction).
    // C2 has appended a handful of A/R lines, so the file still has the A lines
    // for 50 and 20. C3's open() therefore replays those lines, which is the
    // collision this case is for. Checked by reading the file, not by guessing.
    const std::string journal_before_c3 = journal_text(journal);
    const auto holds = open_hold_bytes(journal);
    INFO("journal before C3:\n" << journal_before_c3);
    REQUIRE(contains_value(holds, 50));
    REQUIRE(contains_value(holds, 20));

    c2.stop();
    Child c3 = start_coordinator(kPortOrphanReuse, journal, std::nullopt, std::nullopt, listening);
    const std::string c3_listen = listening;
    INFO("C3 LISTENING: " << c3_listen);
    CHECK(has(c3_listen, "orphans=1"));

    Child check;
    {
        Env env(vars);
        check = probe({"try", budget_path, "90"});
    }
    seen.clear();
    const std::string claim = want(check, "claim=", 15000ms, &seen);
    INFO("try 90: " << claim);
    CHECK(has(claim, "claim=0"));
    CHECK(has(claim, "pending=50"));

    holder_p.send("q\n");
    c3.stop();
}

TEST_CASE("F4 (E1): a successor's live hold survives the orphan owner's exit and a second restart" *
          doctest::test_suite("budget")) {
    const auto journal = f4_dir() / "b.journal";
    const auto budget_path = (f4_dir() / "b.budget").string();
    remove_file(journal);
    std::string listening;
    Child c1 = start_coordinator(kPortLiveHold, journal, std::nullopt, std::nullopt, listening);
    const Vars vars = client_vars(kPortLiveHold, "f4-b", "30");

    std::string seen;
    Child holder_p;
    {
        Env env(vars);
        holder_p = probe({"holder", budget_path, "50"});
    }
    REQUIRE(has(want(holder_p, "READY", 15000ms, &seen), "READY 1"));

    c1.stop();
    Child c2 = start_coordinator(kPortLiveHold, journal, std::nullopt, std::nullopt, listening);
    CHECK(has(listening, "orphans=1"));

    Child holder_q;
    {
        Env env(vars);
        holder_q = probe({"holder", budget_path, "20"});
    }
    seen.clear();
    REQUIRE(has(want(holder_q, "READY", 15000ms, &seen), "READY 1"));

    holder_p.send("q\n");
    // C2 checks owners every 250 ms. A try of 90 must see only Q's 20.
    bool dropped = false;
    std::string claim;
    const bool waited = eventually(
        [&] {
            Child check;
            {
                Env env(vars);
                check = probe({"try", budget_path, "90"});
            }
            claim = want(check, "claim=", 5000ms);
            dropped = has(claim, "pending=20");
            return dropped;
        },
        10000ms);
    INFO("try 90 against C2 after P exit: " << claim);
    REQUIRE(waited);
    REQUIRE(dropped);

    const std::string journal_before_c3 = journal_text(journal);
    const auto holds = open_hold_bytes(journal);
    INFO("journal before C3:\n" << journal_before_c3);
    REQUIRE(contains_value(holds, 50));
    REQUIRE(contains_value(holds, 20));

    c2.stop();
    Child c3 = start_coordinator(kPortLiveHold, journal, std::nullopt, std::nullopt, listening);
    const std::string c3_listen = listening;
    INFO("C3 LISTENING: " << c3_listen);
    CHECK(has(c3_listen, "orphans=1"));

    Child check;
    {
        Env env(vars);
        check = probe({"try", budget_path, "90"});
    }
    seen.clear();
    const std::string after = want(check, "claim=", 15000ms, &seen);
    INFO("try 90 against C3: " << after);
    CHECK(has(after, "claim=0"));
    CHECK(has(after, "pending=20"));

    holder_q.send("q\n");
    c3.stop();
}

TEST_CASE("F4 (E2): one cancel ends every waiter of a budget" * doctest::test_suite("budget")) {
    // Bound: one connection attempt (an immediate reject here) plus one pause
    // of at most ANNO_TEST_BUDGET_RECHECK_MS (100). 3 s is the margin. At
    // 94cf3cb the second thread does not end within the bound; the case fails
    // with the lines seen and the probe is killed with this Child.
    const auto journal = f4_dir() / "c.journal";
    const auto budget_path = (f4_dir() / "c.budget").string();
    remove_file(journal);
    std::string listening;
    Child coord =
        start_coordinator(kPortCancelAll, journal, 0xc1c1c1c1c1c1c1c1ULL, std::nullopt, listening);
    const Vars vars = client_vars(kPortCancelAll, "f4-c", "60");

    std::string seen;
    Child holder;
    {
        Env env(vars);
        holder = probe({"holder", budget_path, "50"});
    }
    REQUIRE(has(want(holder, "READY", 15000ms, &seen), "READY 1"));

    Child requester;
    {
        Env env(vars);
        Env exact({{"ANNO_TEST_BUDGET_EXACT_BUILD", "1"}});
        requester = probe({"reqwait2", budget_path, "30"});
    }
    seen.clear();
    const std::string waiting = want(requester, "WAITING", 15000ms, &seen);
    INFO("requester: " << seen);
    REQUIRE(has(waiting, "WAITING"));
    std::this_thread::sleep_for(300ms);
    requester.send("c\n");

    const auto deadline = std::chrono::steady_clock::now() + 3000ms;
    std::string first;
    std::string second;
    auto remain = [&] {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            return 0ms;
        }
        return std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
    };
    first = want(requester, "RESULT", remain(), &seen);
    second = want(requester, "RESULT", remain(), &seen);
    INFO("after cancel: " << seen);
    CHECK(has(first, "RESULT"));
    CHECK(has(second, "RESULT"));
    CHECK(has(seen, "RESULT 1 0"));
    CHECK(has(seen, "RESULT 2 0"));
    CHECK(has(seen, "cancelled"));
    CHECK(holder.running());
    CHECK(coord.running());
}
