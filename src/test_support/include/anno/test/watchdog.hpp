// A bounded scope for tests that deliberately block worker threads (round eb). The gates such a
// test uses release only when told to (or at scope exit); what bounds the test is this watchdog:
// if the scope has not ended within `limit`, it reports and ends the process (exit 124), so a hang
// fails that test instead of the whole run waiting for CTest's timeout. _Exit: hung threads cannot
// be joined. The limit is not a pass criterion: observations use their own bounded waits.
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>

namespace anno::test {

class Watchdog {
  public:
    Watchdog(std::chrono::seconds limit, const char* what)
        : thread_([this, limit, what] {
              std::unique_lock lock(m_);
              if (!cv_.wait_for(lock, limit, [this] { return done_; })) {
                  std::fprintf(stderr, "watchdog: %s did not finish within %lld s\n", what,
                               static_cast<long long>(limit.count()));
                  std::fflush(stderr);
                  std::_Exit(124);
              }
          }) {}
    Watchdog(const Watchdog&) = delete;
    Watchdog& operator=(const Watchdog&) = delete;
    ~Watchdog() {
        {
            const std::lock_guard lock(m_);
            done_ = true;
        }
        cv_.notify_all();
        thread_.join();
    }

  private:
    std::mutex m_;
    std::condition_variable cv_;
    bool done_ = false;
    std::thread thread_;
};

} // namespace anno::test
