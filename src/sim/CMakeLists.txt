# anno_sim: the deterministic simulation. Headless, no I/O, integer
# arithmetic in all state (docs/sim/core.md, docs/sim/state.md). Uses
# anno_formats to read type data from the original files.
add_library(anno_sim src/sim.cpp src/production.cpp src/config.cpp src/types.cpp
            src/goods_table.cpp src/synthetic.cpp src/player.cpp src/island.cpp src/scenario.cpp
            src/save.cpp src/building.cpp src/create.cpp src/invariants.cpp src/figure.cpp
            src/driver.cpp src/scheduler.cpp src/codec.cpp src/apply.cpp src/replay.cpp
            src/placement.cpp src/legacy_codec.cpp src/events.cpp src/timers.cpp)
target_include_directories(anno_sim PUBLIC include)
target_link_libraries(anno_sim PUBLIC anno_formats)
target_compile_features(anno_sim PUBLIC cxx_std_20)
anno_target_defaults(anno_sim SANITIZE_NO_RECOVER)

if(ANNO_BUILD_TESTS)
  add_executable(anno_sim_tests tests/main.cpp tests/sim_test.cpp tests/production_test.cpp
                 tests/archive_test.cpp tests/slot_list_test.cpp tests/scheduler_blocks_test.cpp
                 tests/container_probe_test.cpp tests/config_test.cpp tests/types_test.cpp
                 tests/player_test.cpp tests/tile_hash_test.cpp tests/island_state_test.cpp
                 tests/create_remove_test.cpp tests/building_state_test.cpp
                 tests/world_state_test.cpp tests/scheduler_test.cpp tests/slice_test.cpp tests/alloc_probe.cpp
                 tests/command_codec_test.cpp tests/command_apply_test.cpp tests/replay_test.cpp
                 tests/command_build_test.cpp tests/property_test.cpp
                 tests/driver_classes_test.cpp tests/world_events_test.cpp)
  target_link_libraries(anno_sim_tests PRIVATE anno_sim anno_test_support doctest::doctest)
  # The goldens (fixtures/synthetic/sim/goldens.txt, docs/sim/state.md §14.3).
  target_compile_definitions(anno_sim_tests PRIVATE
    ANNO_FIXTURES_DIR="${PROJECT_SOURCE_DIR}/fixtures/synthetic")
  anno_target_defaults(anno_sim_tests SANITIZE_NO_RECOVER)
  anno_add_doctest(anno_sim_tests UBSAN_HALT)

  # tools/lint_sim.py: the mechanical part of the determinism rules (no
  # unordered containers, float, clock, rand, mutable globals ...; docs/sim/
  # state.md §14.6), and its own self-test. Named anno_sim_tests_* so that
  # `ctest -R anno_sim_tests` runs them with the sim tests.
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

  # The sanitizer gate's probe (asan preset only): a deliberate signed
  # overflow. cmake/RunUbsanProbe.cmake runs it and requires that it printed
  # its start marker, exited non-zero AND printed UBSan's signed-overflow
  # report, so a crash, a missing runtime or a link problem cannot pass for
  # a working gate (-fno-sanitize-recover=all, cmake/AnnoWarnings.cmake).
  if(ANNO_SANITIZE AND NOT MSVC)
    add_executable(anno_sim_ubsan_probe tests/ubsan_probe.cpp)
    anno_target_defaults(anno_sim_ubsan_probe SANITIZE_NO_RECOVER)
    add_test(NAME anno_sim_tests_ubsan_probe_is_caught_by_the_asan_gate
             COMMAND ${CMAKE_COMMAND} -DPROBE=$<TARGET_FILE:anno_sim_ubsan_probe>
                     -P ${PROJECT_SOURCE_DIR}/cmake/RunUbsanProbe.cmake)
  endif()
endif()
