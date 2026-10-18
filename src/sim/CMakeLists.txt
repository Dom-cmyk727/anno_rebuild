# anno_sim: the deterministic simulation. Headless, no I/O, integer
# arithmetic in all state (docs/sim/core.md). Uses anno_formats to read type
# data from the original files.
add_library(anno_sim src/sim.cpp src/production.cpp)
target_include_directories(anno_sim PUBLIC include)
target_link_libraries(anno_sim PUBLIC anno_formats)
target_compile_features(anno_sim PUBLIC cxx_std_20)
anno_target_defaults(anno_sim)

if(ANNO_BUILD_TESTS)
  add_executable(anno_sim_tests tests/main.cpp tests/sim_test.cpp tests/production_test.cpp)
  target_link_libraries(anno_sim_tests PRIVATE anno_sim doctest::doctest)
  anno_target_defaults(anno_sim_tests)
  anno_add_doctest(anno_sim_tests)
endif()
