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
| Collisions no longer reach the suite | this tree, range narrowed to 47500-48499, `-j 4 --repeat until-fail:20`: 0 failed (`09-*`; again after review fixes, `15-*`) | ports-only control (this tree with the old port numbers), same range and command: 18 of 89 failed, 10 `BIND-FAILED` (`14-*`). The earlier old-tree run (`07-*`) mixes both causes: 4 of its 8 failures came on a second repetition | `14-*`, `15-*`, `07-*` |
| Suite stable under repeats at the default range | `-j 4 --repeat until-fail:20`: 0 failed (86 executed x 20; 6 skips x 20) | before the cleanup fixes, an immediate rerun of F3 handover / silent failed (`BIND-FAILED 27813`, TIME_WAIT) | `10-*`, `05-*`, `06-leftovers-per-case.txt` |
| Acceptance gate | `run_selected.py`: PASS, selected 92, executed 86, failed 0, skipped 6 (platform 2, opt-in 4), before review (`02`) and after the review fixes (`04`) | `01` failed: `bench_picture` not built; `03` failed the skip-phrasing preflight on a new "other" skip message, removed (all kept) | `gate-coord-ports-0{1,2,3,4}.log.gz` |
| ASan/UBSan | asan preset, `-j 4 --repeat until-fail:3`: 0 failed, no sanitizer line | — | `13-*` |
| Warnings | GCC 13 and Clang 18, CI flags `-O3 -Werror`, every changed translation unit | — | `11-*`, `16-*` (after review) |
| Windows host check is not counted as executed | on Windows the host-range case prints a `skipping: ... on Windows` (platform) message | — (not run on Windows) | `budget_ports_test.cpp` |

Preserved failed runs: `04-FIRST-RUN-own-test-bug.log.gz` (a wrong assertion in the new port
test, fixed), `05-new-once-j4*` (the repeat-stability causes, fixed), the first gate attempt.

## Limits

- doctest was Ubuntu `doctest-dev` 2.4.11 through `FETCHCONTENT_SOURCE_DIR_DOCTEST` (the
  pinned 2.4.12 archive could not be downloaded here); `doctest.cmake` from that package. The
  repository pin is unchanged.
- clang-format 18.1.3 was used locally; the reviewer ran 23.1.3 `--dry-run --Werror` on the
  pre-review files (clean). The review-fix edits were checked with 18.1.3 only.
- Windows and macOS were not built or run (MSVC's handling of `consteval` calling a
  declared-only function is unverified). The Windows range and excluded port ranges are not
  read; check `netsh int ipv4 show dynamicport tcp` and `show excludedportrange protocol=tcp`
  by hand before Windows acceptance.
- The client re-reads `ANNO_TEST_BUDGET_COORD`/`_PORT` on every connect: a `Budget::process()`
  first used inside a `FileClientPort` scope would reach the test coordinator on 27632. No case
  does this today (F2's `Scope` has the same pattern).
- The production ports 47631/47632 are unchanged by owner decision; their residual risk is
  described in the spec section.

## Independent review

Read-only review subagent of this session, revision 85b8a76 (code e5d0b87). No blocking finding.
Fixed: (1) the negative control mixed the port cause with the repeat causes, so a ports-only
control was added (`14-*`); (2) the docs said every earlier port number was 20000 lower, so the
production ports and the manual A/B ports are now named as unchanged; (3) on Windows the host
case compared constants, so it now reports a platform skip; (4) the Windows-only F1 "killed
owner" case now uses the file-client port, and the WSL case is documented as exempt. Accepted
as residual and documented: Windows and WSL use the same F3-F5 ports, and the environment is
read on every connect.
