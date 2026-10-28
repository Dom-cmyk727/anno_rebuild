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
  add_subdirectory(src/test_support)
endif()

add_subdirectory(src/formats)
add_subdirectory(src/assets)
add_subdirectory(src/render)
add_subdirectory(src/app)
add_subdirectory(src/sim)
add_subdirectory(src/sim_load)
add_subdirectory(src/tools)

# The coordinator is a separate executable. Multi-config and per-library
# output directories do not put it next to every test, and a client looks
# beside its own executable. Copy it there after each of those tests links.
if(ANNO_BUILD_TESTS)
  set(anno_coord_hosts anno_formats_tests anno_assets_tests anno_render_tests anno_sim_tests)
  foreach(host anno_app_tests anno_render_gpu_tests)
    if(TARGET ${host})
      list(APPEND anno_coord_hosts ${host})
    endif()
  endforeach()
  foreach(host ${anno_coord_hosts})
    add_dependencies(${host} anno_budget_coord)
    add_custom_command(TARGET ${host} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              $<TARGET_FILE:anno_budget_coord> $<TARGET_FILE_DIR:${host}>
      VERBATIM)
  endforeach()
endif()
