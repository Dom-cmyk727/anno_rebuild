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
option(ANNO_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer (GCC/Clang)" OFF)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(AnnoWarnings)

if(ANNO_BUILD_TESTS)
  enable_testing()
  include(FetchContent)
  # doctest 2.4.11 (2023), pinned.
  FetchContent_Declare(doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.11.tar.gz
    URL_HASH SHA256=632ed2c05a7f53fa961381497bf8069093f0d6628c5f26286161fbd32a560186
    # Header only: do not run doctest's own CMakeLists (it predates CMake 4).
    SOURCE_SUBDIR no-cmake)
  FetchContent_MakeAvailable(doctest)
  add_library(anno_doctest INTERFACE)
  add_library(doctest::doctest ALIAS anno_doctest)
  target_include_directories(anno_doctest SYSTEM INTERFACE ${doctest_SOURCE_DIR})
  add_subdirectory(src/test_support)
endif()

add_subdirectory(src/formats)
add_subdirectory(src/assets)
add_subdirectory(src/render)
add_subdirectory(src/tools)
