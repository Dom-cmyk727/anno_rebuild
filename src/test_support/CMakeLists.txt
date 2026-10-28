# Helpers shared by the test suites: the game directory and file lists.
add_library(anno_test_support src/game_dir.cpp src/memory.cpp src/budget.cpp src/coord.cpp)
target_include_directories(anno_test_support PUBLIC include)
target_include_directories(anno_test_support PRIVATE src)
target_compile_features(anno_test_support PUBLIC cxx_std_20)
if(WIN32)
  target_link_libraries(anno_test_support PRIVATE psapi ws2_32)
endif()
anno_target_defaults(anno_test_support)

if(ANNO_BUILD_TESTS)
  add_executable(anno_budget_coord src/budget_coord.cpp)
  target_link_libraries(anno_budget_coord PRIVATE anno_test_support)
  anno_target_defaults(anno_budget_coord)

  add_executable(anno_budget_probe tests/budget_probe.cpp)
  target_link_libraries(anno_budget_probe PRIVATE anno_test_support)
  anno_target_defaults(anno_budget_probe)

  add_executable(anno_test_support_tests tests/main.cpp tests/game_dir_test.cpp
    tests/parallel_test.cpp tests/budget_test.cpp tests/budget_f1_test.cpp)
  target_link_libraries(anno_test_support_tests PRIVATE anno_test_support doctest::doctest)
  anno_target_defaults(anno_test_support_tests)
  anno_add_doctest(anno_test_support_tests)
endif()
