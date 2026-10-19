// The executor seam of the simulation (spec: docs/sim/state.md §11, stage S5).
//
// Work that is a pure function of disjoint pieces of state (the chunk digests
// of a tile layer, the content of the islands a scenario builds) is written as
// `parallel_for(executor, n, body)`: `body(i)` runs exactly once for every `i`
// in [0, n), on any thread and in any order, and the call returns when all
// have finished. A body may only write state that belongs to its own `i`
// (one island, one chunk) and must not draw from the PRNG; results are
// combined by the caller in index order, so the outcome never depends on how
// the bodies were scheduled (docs/sim/state.md §11, "Hash and save").
//
// The sim itself never starts a thread (tools/lint_sim.py forbids std::thread
// here): the default executor runs the bodies in order on the calling thread,
// and a host that wants cores implements `Executor` with its own pool (the
// tests do, to prove that the digests do not depend on it). A body must not
// throw (the sim has no exceptions).
#pragma once

#include <cstdint>

namespace anno::sim {

/// Runs `n` independent jobs. Implementations call `body(ctx, i)` once for
/// every `i` in [0, n) and return after the last one finished.
class Executor {
  public:
    using Body = void (*)(void* ctx, std::uint64_t i);
    virtual void run(std::uint64_t n, Body body, void* ctx) const = 0;

  protected:
    ~Executor() = default;
};

/// The default executor: ascending `i` on the calling thread.
class SerialExecutor final : public Executor {
  public:
    void run(std::uint64_t n, Body body, void* ctx) const override {
        for (std::uint64_t i = 0; i < n; ++i) {
            body(ctx, i);
        }
    }
};

/// The shared stateless serial executor (immutable: it holds no state, so the
/// reference is safe to share between threads and worlds).
const Executor& serial_executor();

/// `f(i)` for every `i` in [0, n) on `ex` (see the file comment for the rules a
/// body obeys). `f` is called through a const reference, so it may be called
/// from several threads at once.
template <class F> void parallel_for(const Executor& ex, std::uint64_t n, const F& f) {
    ex.run(
        n, [](void* ctx, std::uint64_t i) { (*static_cast<const F*>(ctx))(i); },
        const_cast<void*>(static_cast<const void*>(&f)));
}

} // namespace anno::sim
