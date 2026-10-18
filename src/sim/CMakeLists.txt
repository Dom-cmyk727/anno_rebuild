# anno_sim: the deterministic simulation. Headless, no I/O, integer
# arithmetic in all state (docs/sim/core.md, docs/sim/state.md). Uses
# anno_formats to read type data from the original files.
add_library(anno_sim src/sim.cpp src/production.cpp)
target_include_directories(anno_sim PUBLIC include)
target_link_libraries(anno_sim PUBLIC anno_formats)
target_compile_features(anno_sim PUBLIC cxx_std_20)
anno_target_defaults(anno_sim)

if(ANNO_BUILD_TESTS)
  add_executable(anno_sim_tests tests/main.cpp tests/sim_test.cpp tests/production_test.cpp
                 tests/archive_test.cpp)
  target_link_libraries(anno_sim_tests PRIVATE anno_sim anno_test_support doctest::doctest)
  # The goldens (fixtures/synthetic/sim/goldens.txt, docs/sim/state.md §14.3).
  target_compile_definitions(anno_sim_tests PRIVATE
    ANNO_FIXTURES_DIR="${PROJECT_SOURCE_DIR}/fixtures/synthetic")
  anno_target_defaults(anno_sim_tests)
  anno_add_doctest(anno_sim_tests)

  # tools/lint_sim.py: the mechanical part of the determinism rules (no
  # unordered containers, float, clock, rand, mutable globals ...; docs/sim/
  # state.md §14.6), and its own self-test. Named anno_sim_tests_* so that
  # `ctest -R anno_sim_tests` runs them with the sim tests.
  #
  # Fail closed: in CI (the CI environment variable is set) a missing Python 3
  # fails the configuration instead of silently dropping the determinism gate;
  # elsewhere set -DANNO_REQUIRE_LINT=ON to get the same.
  if(DEFINED ENV{CI} AND NOT "$ENV{CI}" STREQUAL "")
    set(anno_require_lint_default ON)
  else()
    set(anno_require_lint_default OFF)
  endif()
  option(ANNO_REQUIRE_LINT "Fail configuration when Python 3 (tools/lint_sim.py) is missing"
         ${anno_require_lint_default})
  if(ANNO_REQUIRE_LINT)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
  else()
    find_package(Python3 COMPONENTS Interpreter)
  endif()
  if(Python3_FOUND)
    add_test(NAME anno_sim_tests_lint_sim_passes
             COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/lint_sim.py
                     --root ${PROJECT_SOURCE_DIR})
    add_test(NAME anno_sim_tests_lint_sim_self_test
             COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/lint_sim.py --self-test)
  else()
    message(WARNING "python3 not found: the lint_sim tests (the determinism gate) are NOT "
                    "registered; pass -DANNO_REQUIRE_LINT=ON to make this an error")
  endif()
endif()
