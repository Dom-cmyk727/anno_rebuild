// The probe of the sanitizer gate (docs/sim/state.md §14.6). It performs a
// deliberate signed integer overflow. In the asan preset it is built with
// `-fno-sanitize-recover=all`, so UBSan reports the overflow and stops the
// process with a non-zero exit. cmake/RunUbsanProbe.cmake runs it as the CTest
// case anno_sim_tests_ubsan_probe_is_caught_by_the_asan_gate and requires the
// start marker below, a non-zero exit AND UBSan's "runtime error: signed
// integer overflow" report: if the gate is ever weakened (a recovering UBSan
// prints the report and carries on, exit 0), or the probe fails for another
// reason (crash, no sanitizer runtime), the case fails. Not part of the sim
// tests; never built outside the asan preset.
#include <climits>
#include <cstdio>

int main(int argc, char** /*argv*/) {
    std::puts("ubsan_probe: start");
    std::fflush(stdout);
    // volatile: the compiler may not fold the overflow away. argc is 1.
    volatile int big = INT_MAX;
    volatile int one = argc;
    const int r = big + one; // signed overflow: undefined behaviour, on purpose
    return r == 0 ? 1 : 0;
}
