# coord-ports-1: coordinator regression ports below the ephemeral ranges (2026-11-07)

Spec and reasons: `docs/perf/README.md`, "Coordinator ports — regression ports below the
ephemeral ranges". Identities (base commit, host, compilers, binary hashes, the local doctest
substitution) in `00-identity.txt`. Linux only (cloud container, 4 CPUs); Windows and macOS
were not run (NOT_RUN for those targets).

## Acceptance table

| Requirement | Positive proof | Counterexample / negative control | Evidence |
|---|---|---|---|
| A kernel-assigned client port can block a coordinator bind (the CI mechanism) | — | client connected or half-closed on port P: `BIND-FAILED P`; a TIME_WAIT left by the peer does not block | `01-negative-control.txt` (`ephemeral_collision.py`) |
| Fixed test ports lie outside every default ephemeral range | `budget ports: the fixed band lies below every default ephemeral range` | the same case asserts 47814, 47631, 47600, 47999 are inside Linux's default | gate log |
| A new out-of-band literal cannot be added | `ports::fixed(27810)` compiles | `ports::fixed(47814)` does not compile | `03-compile-time-negative.txt` |
| A host whose range covers the band is reported | host case passes with 32768-60999 | host range 27000-28999: the case fails with the range in its message | `12-host-range-negative.txt` |
| Collisions no longer reach the suite | this tree, range narrowed to 47500-48499, `-j 4 --repeat until-fail:20`: 0 failed | old tree, same range, `--repeat until-fail:3`: 8 of 89 failed, 4 `BIND-FAILED` | `09-*`, `07-*` |
| Suite stable under repeats at the default range | `-j 4 --repeat until-fail:20`: 0 failed (86 executed x 20; 6 skips x 20) | before the cleanup fixes, an immediate rerun of F3 handover / silent failed (`BIND-FAILED 27813`, TIME_WAIT) | `10-*`, `05-*`, `06-leftovers-per-case.txt` |
| Acceptance gate | `run_selected.py`: PASS, selected 92, executed 86, failed 0, skipped 6 (platform 2, opt-in 4) | first gate attempt failed: `bench_picture` not built (kept) | `gate-coord-ports-02.log.gz`, `gate-coord-ports-01.log.gz` |
| ASan/UBSan | asan preset, `-j 4 --repeat until-fail:3`: 0 failed, no sanitizer line | — | `13-*` |
| Warnings | GCC 13 and Clang 18, CI flags `-O3 -Werror`, every changed translation unit | — | `11-werror-gcc-clang.txt` |

Preserved failed runs: `04-FIRST-RUN-own-test-bug.log.gz` (a wrong assertion in the new port
test, fixed), `05-new-once-j4*` (the repeat-stability causes, fixed), the first gate attempt.

## Limits

- doctest was Ubuntu `doctest-dev` 2.4.11 through `FETCHCONTENT_SOURCE_DIR_DOCTEST` (the
  pinned 2.4.12 archive could not be downloaded here); `doctest.cmake` from that package. The
  repository pin is unchanged.
- clang-format 18.1.3 was used, not 23.1.3; one unrelated reflow it made was reverted by hand.
- Windows and macOS were not built or run; the Windows ephemeral range is the documented
  default and is not queried; Windows excluded port ranges are not checked.
- The production ports 47631/47632 are unchanged by owner decision; their residual risk is
  described in the spec section.
