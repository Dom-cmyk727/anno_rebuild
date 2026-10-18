# anno1503: the application (for now the island viewer). anno_app_core holds
# the window-independent parts (scene loading, PNG output) so tests can use
# them; the window needs SDL3 (ANNO_BUILD_VIEWER).
add_library(anno_app_core
  src/cli.cpp
  src/loader.cpp
  src/png.cpp
)
target_include_directories(anno_app_core PUBLIC include)
target_link_libraries(anno_app_core PUBLIC anno_render)
target_compile_features(anno_app_core PUBLIC cxx_std_20)
anno_target_defaults(anno_app_core)

add_executable(anno1503 src/main.cpp)
target_link_libraries(anno1503 PRIVATE anno_app_core)
anno_target_defaults(anno1503)
if(ANNO_BUILD_VIEWER)
  target_sources(anno1503 PRIVATE src/viewer.cpp)
  target_compile_definitions(anno1503 PRIVATE ANNO_WITH_VIEWER)
  target_link_libraries(anno1503 PRIVATE SDL3::SDL3)
endif()

if(ANNO_BUILD_TESTS)
  add_executable(anno_app_tests tests/main.cpp tests/app_test.cpp)
  target_link_libraries(anno_app_tests PRIVATE anno_app_core anno_test_support doctest::doctest)
  anno_target_defaults(anno_app_tests)
  add_test(NAME anno_app_tests COMMAND anno_app_tests)
  # Bad arguments print the usage. CTest's output match ignores the exit
  # code; app_test checks that parse_args rejects the input, and main maps a
  # rejection to exit 2 (not tested automatically).
  add_test(NAME anno1503_usage COMMAND anno1503 --view x.sze --group 3)
  set_tests_properties(anno1503_usage PROPERTIES PASS_REGULAR_EXPRESSION "usage:")
endif()
