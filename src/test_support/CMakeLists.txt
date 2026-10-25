# Helpers shared by the test suites: the game directory and file lists.
add_library(anno_test_support src/game_dir.cpp src/memory.cpp)
target_include_directories(anno_test_support PUBLIC include)
target_compile_features(anno_test_support PUBLIC cxx_std_20)
anno_target_defaults(anno_test_support)

if(ANNO_BUILD_TESTS)
  add_executable(anno_test_support_tests tests/main.cpp tests/game_dir_test.cpp tests/parallel_test.cpp)
  target_link_libraries(anno_test_support_tests PRIVATE anno_test_support doctest::doctest)
  anno_target_defaults(anno_test_support_tests)
  anno_add_doctest(anno_test_support_tests)
endif()
