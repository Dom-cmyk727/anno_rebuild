# ANNO 1503 rebuild — C++20 (ADR 0001). One library per responsibility, as
# the former Rust crates: formats ← assets ← render ← app; sim, net, ui.
cmake_minimum_required(VERSION 3.25)
project(anno1503 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ANNO_BUILD_TESTS "Build the test suites" ON)
option(ANNO_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(ANNO_BUILD_VIEWER "Build the windowed viewer (SDL3)" ON)
option(ANNO_BUILD_GPU "Build the GPU renderer (wgpu-native, prebuilt)" ON)
option(ANNO_SYSTEM_SDL "Use an installed SDL3 instead of the pinned source" OFF)
option(ANNO_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer (GCC/Clang)" OFF)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(AnnoWarnings)

include(FetchContent)
include(EmbedFile)

if(ANNO_BUILD_GPU)
  include(WgpuNative)
endif()

if(ANNO_BUILD_VIEWER)
  if(ANNO_SYSTEM_SDL)
    find_package(SDL3 REQUIRED CONFIG)
  else()
    # SDL 3.4.18 (2026-10-02), pinned; hash of the GitHub release asset.
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
      URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
      URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
      SYSTEM)
    FetchContent_MakeAvailable(SDL3)
  endif()
endif()

if(ANNO_BUILD_TESTS)
  enable_testing()
  # doctest 2.4.12, pinned. With libc++ 22 its implementation needs <cfloat>
  # included first: see src/*/tests/main.cpp.
  FetchContent_Declare(doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.tar.gz
    URL_HASH SHA256=73381c7aa4dee704bd935609668cf41880ea7f19fa0504a200e13b74999c2d70
    # Header only: do not run doctest's own CMakeLists (it predates CMake 4).
    SOURCE_SUBDIR no-cmake)
  FetchContent_MakeAvailable(doctest)
  add_library(anno_doctest INTERFACE)
  add_library(doctest::doctest ALIAS anno_doctest)
  target_include_directories(anno_doctest SYSTEM INTERFACE ${doctest_SOURCE_DIR})
  # Include the real <ostream> instead of forward declarations (libc++ does
  # this anyway; MSVC needs it to print std::string_view in assertions).
  target_compile_definitions(anno_doctest INTERFACE DOCTEST_CONFIG_USE_STD_HEADERS)
  # Clang 22 reports doctest's __COUNTER__ under -Wpedantic at every TEST_CASE.
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag(-Wno-c2y-extensions ANNO_HAVE_WNO_C2Y)
  if(ANNO_HAVE_WNO_C2Y)
    target_compile_options(anno_doctest INTERFACE -Wno-c2y-extensions)
  endif()
  # Every TEST_CASE becomes its own CTest test (doctest's discovery runs the
  # test binary after each build), so `ctest -j` spreads the long corpus
  # cases over the cores instead of running each suite as one process.
  include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
  # Heavy cases reserve host memory (docs/perf/README.md, T10c) instead of
  # taking every CTest slot. Four is how many of these processes can sit
  # loaded on the measured PC while one full-map render runs: about 0.95 GiB
  # before the render, 6.17 GiB additional, 15.38 GiB free at the start of the
  # one-worker run, and 4 GiB of headroom. The reservation still refuses a
  # second full map that does not fit. Override with
  # -DANNO_HEAVY_TEST_PROCESSORS=N. CTest caps the value at -j. GPU tests keep
  # RESOURCE_LOCK gpu.
  set(ANNO_HEAVY_TEST_PROCESSORS 4 CACHE STRING
      "CTest slots one heavy test occupies")
  # anno_add_doctest(<target> [UBSAN_HALT] [LABELS ...] [PROPERTIES ...]):
  # registers the cases of a doctest executable as "<target>.<case name>".
  # UBSAN_HALT (asan preset only; for targets built with SANITIZE_NO_RECOVER)
  # puts UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 into the environment
  # of every case. A caller that sets its own ENVIRONMENT property in
  # PROPERTIES wins over this one. LABELS are CTest labels for every case; the
  # heavy cases get "heavy" beside them (so pass labels here, not in
  # PROPERTIES, which the heavy label would replace).
  #
  # The cases are discovered twice: the "heavy" suite gets PROCESSORS
  # ANNO_HEAVY_TEST_PROCESSORS and the label "heavy" (`ctest -L heavy`,
  # `ctest -LE heavy`), the rest the caller's properties only. doctest's
  # discovery script (2.4.12, doctestAddTests.cmake) passes TEST_SPEC as
  # "--test-case=<first element>" followed by the other elements as raw
  # arguments, which is how the suite filters reach --list-test-cases; the
  # two spec hashes differ, so the two discoveries write separate files.
  function(anno_add_doctest target)
    cmake_parse_arguments(ARG "UBSAN_HALT" "" "PROPERTIES;LABELS" ${ARGN})
    set(props ${ARG_PROPERTIES})
    if(ARG_UBSAN_HALT AND ANNO_SANITIZE AND NOT MSVC)
      list(PREPEND props ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
    endif()
    set(heavy_labels heavy ${ARG_LABELS})
    if(ARG_LABELS)
      list(APPEND props LABELS "${ARG_LABELS}")
    endif()
    doctest_discover_tests(${target} TEST_PREFIX "${target}."
      TEST_SPEC "*" "--test-suite-exclude=heavy" TEST_LIST ${target}_TESTS
      PROPERTIES ${props})
    doctest_discover_tests(${target} TEST_PREFIX "${target}."
      TEST_SPEC "*" "--test-suite=heavy" TEST_LIST ${target}_HEAVY_TESTS
      PROPERTIES ${props} PROCESSORS ${ANNO_HEAVY_TEST_PROCESSORS})
    # Runs after both discovery files (TEST_INCLUDE_FILES keeps the order).
    set(label_file "${CMAKE_CURRENT_BINARY_DIR}/${target}_heavy_label.cmake")
    # The caller's LABELS stay beside "heavy" (set_tests_properties replaces
    # the property; ctest's reader has no APPEND for tests of an included file).
    file(WRITE "${label_file}"
      "if(${target}_HEAVY_TESTS)\n"
      "  set_tests_properties(\${${target}_HEAVY_TESTS} PROPERTIES LABELS \"${heavy_labels}\")\n"
      "endif()\n")
    set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${label_file}")
  endfunction()
  add_test(NAME test_names_are_selectable
           COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
                   -P ${CMAKE_SOURCE_DIR}/cmake/CheckTestNames.cmake)
  # These contracts are also run before expensive gates; CTest registration keeps
  # the workflow itself covered by the ordinary CI selection.
  find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
  add_test(NAME test_verdict_contract
           COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/test/ctest_verdict_test.py)
  add_test(NAME test_workflow_contract
           COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/test/workflow_test.py)
  set_tests_properties(test_verdict_contract test_workflow_contract PROPERTIES LABELS "workflow")
  add_test(NAME journal_report_contract
           COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/test/journal_report_test.py)
  set_tests_properties(journal_report_contract PROPERTIES LABELS "workflow")
  # The gate's verdict reader (tools/test/ctest_verdict.py, ported from MAP-2) and the launcher's
  # preflight order (round ec): tools/test/run_selected.sh runs test_verdict_contract, with
  # test_names_are_selectable and picture_matrix_contract, as mandatory preflights. Python3 is
  # required above, so this is always true; kept so the reference tests stay visually grouped
  # under their own guard rather than restructured during the merge.
  if(Python3_FOUND)
    add_test(NAME freight_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/freight_ref.py check
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/freight/contract_vectors.jsonl)
    add_test(NAME freight_reference_contract
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/freight_ref_test.py)
    add_test(NAME figure_digest_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/figure_digest_ref.py --check
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/figure_digest_vectors.txt)
    add_test(NAME ship_nav_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/ship_nav_ref.py check
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/nav/movement_vectors.txt)
    add_test(NAME ship_nav_reference_selftest
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/ship_nav_ref.py selftest
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/nav/movement_vectors.txt)
    add_test(NAME ship_passage_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/ship_nav_ref.py passage-check
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/nav/passage_cases.txt)
    add_test(NAME ship_passage_reference_selftest
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/ship_nav_ref.py passage-selftest
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/nav/passage_cases.txt)
    add_test(NAME ship_route_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/ship_nav_ref.py route-check
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/nav/route_runs.txt)
    add_test(NAME ship_route_reference_selftest
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/ship_nav_ref.py route-selftest
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/nav/route_runs.txt
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/nav/route_counterexamples.txt)
    add_test(NAME ship_object_shore_selftest
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/ship_nav_ref.py object-selftest
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/nav/object_shore_cases.txt)
    add_test(NAME ship_offer_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/ship_nav_ref.py offer-check
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/nav/offer_cases.txt)
    add_test(NAME ship_offer_reference_selftest
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/ship_nav_ref.py offer-selftest
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/nav/offer_cases.txt)
    add_test(NAME ship_object_shore_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/ship_nav_ref.py object-check
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/nav/object_shore_cases.txt)
    add_test(NAME ship_route_counterexamples_rejected
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/ship_nav_ref.py route-reject
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/nav/route_counterexamples.txt)
    add_test(NAME ship_admission_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/ship_nav_ref.py admission-check
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/nav/admission_cases.txt)
    add_test(NAME ship_step_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/ship_step_ref.py check
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/nav/stepper_vectors.txt)
    add_test(NAME ship_speed_factor_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/ship_step_ref.py check-factor
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/nav/speed_factor_vectors.txt)
    add_test(NAME nav_state_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/nav_state_ref.py check
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/nav/nav_state_vectors.txt)
    add_test(NAME nav_state_reference_selftest
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/nav_state_ref.py selftest)
    add_test(NAME placement_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/placement_ref.py check
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/placement_vectors.txt)
    add_test(NAME placement_reference_selftest
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/placement_ref.py selftest)
    add_test(NAME cargo_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/cargo_ref.py check
                     # no path: both fixtures, the unload cargo_vectors.txt and (MAP-CARGO-2) the
                     # load cargo_load_vectors.txt; a missing file fails the test
                     )
    add_test(NAME cargo_reference_selftest
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/cargo_ref.py selftest)
    add_test(NAME booking_checked_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/booking_checked_ref.py
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/econbook1/checked_vectors.json)
    add_test(NAME booking_checked_reference_contract
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/booking_checked_ref_contract_test.py
                     --ref ${CMAKE_SOURCE_DIR}/tools/booking_checked_ref.py
                     --vectors ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/econbook1/checked_vectors.json)
    add_test(NAME forester_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/forester_ref.py
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/forester_vectors.txt)
    add_test(NAME forester_reference_contract
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/forester_ref_contract_test.py
                     --ref ${CMAKE_SOURCE_DIR}/tools/forester_ref.py
                     --vectors ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/forester_vectors.txt)
    add_test(NAME house_kernel_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/house_kernel_ref.py
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/house_kernel_vectors.txt)
    add_test(NAME house_kernel_reference_contract
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/house_kernel_ref_contract_test.py
                     --ref ${CMAKE_SOURCE_DIR}/tools/house_kernel_ref.py
                     --vectors ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/house_kernel_vectors.txt)
    add_test(NAME resident_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/resident_ref.py
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/resident_vectors.txt)
    add_test(NAME resident_reference_contract
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/resident_ref_contract_test.py
                     --ref ${CMAKE_SOURCE_DIR}/tools/resident_ref.py
                     --vectors ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/resident_vectors.txt)
    add_test(NAME inn_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/inn_ref.py
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/inn_vectors.txt)
    add_test(NAME inn_reference_contract
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/inn_ref_contract_test.py
                     --ref ${CMAKE_SOURCE_DIR}/tools/inn_ref.py
                     --vectors ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/inn_vectors.txt)
    add_test(NAME walk_goal_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/walk_goal_ref.py
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/walk_goal_vectors.txt)
    add_test(NAME walk_goal_reference_contract
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/walk_goal_ref_contract_test.py
                     --ref ${CMAKE_SOURCE_DIR}/tools/walk_goal_ref.py
                     --vectors ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/walk_goal_vectors.txt)
    add_test(NAME house_transition_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/house_transition_ref.py
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/house_transition_vectors.txt)
    add_test(NAME house_transition_reference_contract
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/house_transition_ref_contract_test.py
                     --ref ${CMAKE_SOURCE_DIR}/tools/house_transition_ref.py
                     --vectors ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/house_transition_vectors.txt)
    add_test(NAME house_progression_vectors_agree
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/house_transition_ref.py
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/house_progression_vectors.txt)
    add_test(NAME small_farm_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/small_farm_ref.py
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/small_farm_vectors.txt)
    add_test(NAME small_farm_reference_contract
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/small_farm_ref_contract_test.py
                     --ref ${CMAKE_SOURCE_DIR}/tools/small_farm_ref.py
                     --vectors ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/small_farm_vectors.txt)
    add_test(NAME early_needs_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/early_needs_ref.py
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/early_needs_vectors.txt)
    add_test(NAME early_needs_reference_contract
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/early_needs_ref_contract_test.py
                     --ref ${CMAKE_SOURCE_DIR}/tools/early_needs_ref.py
                     --vectors ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/early_needs_vectors.txt)
    add_test(NAME hunter_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/hunter_ref.py
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/hunter_vectors.txt)
    add_test(NAME hunter_reference_contract
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/hunter_ref_contract_test.py
                     --ref ${CMAKE_SOURCE_DIR}/tools/hunter_ref.py
                     --vectors ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/hunter_vectors.txt)
    add_test(NAME house_live_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/house_live_ref.py
                     ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/house_live_vectors.txt)
    add_test(NAME house_live_reference_contract
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/house_live_ref_contract_test.py
                     --ref ${CMAKE_SOURCE_DIR}/tools/house_live_ref.py
                     --vectors ${CMAKE_SOURCE_DIR}/fixtures/synthetic/sim/house_live_vectors.txt)
    add_test(NAME population_oracles_agree
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/sim_stream.py --check)
    add_test(NAME ship_step_reference_selftest
             COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/ship_step_ref.py selftest)
  else()
    message(WARNING "Python3 not found: the figure-digest and ship-navigation reference tests are NOT registered (NOT_RUN)")
  endif()
  add_subdirectory(src/test_support)
endif()

# The coordinator is its own executable. A client looks beside its own
# executable, and each test library has its own output directory. A macro
# (not a function) so add_custom_command runs in the caller's directory.
macro(anno_stage_budget_coord target)
  add_dependencies(${target} anno_budget_coord)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:anno_budget_coord> $<TARGET_FILE_DIR:${target}>
    VERBATIM)
endmacro()

add_subdirectory(src/formats)
add_subdirectory(src/assets)
add_subdirectory(src/render)
add_subdirectory(src/app)
add_subdirectory(src/sim)
add_subdirectory(src/sim_load)
add_subdirectory(src/tools)

if(ANNO_BUILD_TESTS)
  # Explicit canonical gate build target: current sim/formats/replay binaries
  # and the mandatory ancillary picture contract, without viewer/GPU matrices.
  add_custom_target(economy_checks DEPENDS anno_sim_tests anno_formats_tests sim_replay anno_dump bench_picture)
  if(TARGET anno_sim_ubsan_probe)
    add_dependencies(economy_checks anno_sim_ubsan_probe)
  endif()
endif()
