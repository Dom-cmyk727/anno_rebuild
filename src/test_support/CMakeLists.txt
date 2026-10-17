# Helpers shared by the test suites: the game directory and file lists.
add_library(anno_test_support src/game_dir.cpp)
target_include_directories(anno_test_support PUBLIC include)
target_compile_features(anno_test_support PUBLIC cxx_std_20)
anno_target_defaults(anno_test_support)
