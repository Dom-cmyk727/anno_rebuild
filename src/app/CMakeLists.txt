# anno1503: the application (for now the island viewer). anno_app_core holds
# the window-independent parts (scene loading, PNG output) so tests can use
# them; the window needs SDL3 (ANNO_BUILD_VIEWER).
add_library(anno_app_core
  src/building_delta.cpp
  src/commands_file.cpp
  src/dev_input.cpp
  src/cli.cpp
  src/loader.cpp
  src/picture_check.cpp
  src/play.cpp
  src/png.cpp
  src/scene_picture.cpp
  src/still.cpp
)
target_include_directories(anno_app_core PUBLIC include)
# The play session is window-independent (tests, and --png at tick 0). The
# window only reads the snapshot. Render is not given sim types; the bridge
# is LinkTemplate and SceneBuilding in this library.
target_link_libraries(anno_app_core PUBLIC anno_render anno_sim_load)
target_compile_features(anno_app_core PUBLIC cxx_std_20)
anno_target_defaults(anno_app_core)

add_executable(anno1503 src/main.cpp)
target_link_libraries(anno1503 PRIVATE anno_app_core)
if(MINGW)
  # The entry point is wmain (UTF-16 arguments, main.cpp).
  target_link_options(anno1503 PRIVATE -municode)
endif()
anno_target_defaults(anno1503)
if(ANNO_BUILD_VIEWER)
  target_sources(anno1503 PRIVATE src/viewer.cpp)
  target_compile_definitions(anno1503 PRIVATE ANNO_WITH_VIEWER)
  target_link_libraries(anno1503 PRIVATE SDL3::SDL3)
  if(ANNO_HAVE_WGPU)
    target_sources(anno1503 PRIVATE src/gpu_surface.cpp)
    target_compile_definitions(anno1503 PRIVATE ANNO_VIEWER_GPU)
    target_link_libraries(anno1503 PRIVATE anno_render_gpu)
  endif()
endif()

if(ANNO_BUILD_TESTS)
  add_executable(anno_app_tests tests/main.cpp tests/app_test.cpp tests/play_test.cpp
    tests/building_delta_test.cpp tests/building_mailbox_test.cpp tests/commands_file_test.cpp
    tests/play_load_test.cpp tests/input_test.cpp tests/scene_probe_test.cpp
    tests/scene_picture_test.cpp tests/scene_backing_test.cpp
    tests/scene_picture_recovery_test.cpp
    tests/picture_check_test.cpp)
  target_link_libraries(anno_app_tests PRIVATE anno_app_core anno_test_support doctest::doctest)
  target_compile_definitions(anno_app_tests PRIVATE
    ANNO_FIXTURES_DIR="${PROJECT_SOURCE_DIR}/fixtures/synthetic")
  anno_target_defaults(anno_app_tests)
  anno_stage_budget_coord(anno_app_tests)
  anno_add_doctest(anno_app_tests)
  # Bad arguments print the usage. CTest's output match ignores the exit
  # code; app_test checks that parse_args rejects the input, and main maps a
  # rejection to exit 2 (not tested automatically).
  add_test(NAME anno1503_usage COMMAND anno1503 --view x.sze --group 3)
  set_tests_properties(anno1503_usage PROPERTIES PASS_REGULAR_EXPRESSION "usage:")
endif()
