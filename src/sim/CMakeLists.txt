# anno_sim: the deterministic simulation. Headless, no I/O, standard library
# only, integer arithmetic in all state (docs/sim/core.md).
add_library(anno_sim src/sim.cpp)
target_include_directories(anno_sim PUBLIC include)
target_compile_features(anno_sim PUBLIC cxx_std_20)
anno_target_defaults(anno_sim)

if(ANNO_BUILD_TESTS)
  add_executable(anno_sim_tests tests/main.cpp tests/sim_test.cpp)
  target_link_libraries(anno_sim_tests PRIVATE anno_sim doctest::doctest)
  anno_target_defaults(anno_sim_tests)
  add_test(NAME anno_sim_tests COMMAND anno_sim_tests)
endif()
