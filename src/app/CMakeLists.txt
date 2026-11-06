# anno1503: the application (for now the island viewer). anno_app_core holds
# the window-independent parts (scene loading, PNG output) so tests can use
# them; the window needs SDL3 (ANNO_BUILD_VIEWER).
add_library(anno_app_core
  src/building_delta.cpp
  src/command_outcome.cpp
  src/commands_file.cpp
  src/dev_feedback.cpp
  src/dev_session.cpp
  src/dev_input.cpp
  src/dev_tool.cpp
  src/frame_projection.cpp
  src/legal_workload.cpp
  src/cli.cpp
  src/loader.cpp
  src/opening.cpp
  src/opening_session.cpp
  src/opening_ui.cpp
  src/opening_view.cpp
  src/picture_check.cpp
  src/picture_trace.cpp
  src/play.cpp
  src/play_follow.cpp
  src/png.cpp
  src/publication_hold.cpp
  src/scene_picture.cpp
  src/ship_layer.cpp
  src/ship_motion.cpp
  src/still.cpp
  src/visibility.cpp
  src/window_trace.cpp
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
  # The play window's SDL mapping for the developer tool (round ee, B3): the window and its tests
  # link the same code.
  add_library(anno_app_sdl src/sdl_input.cpp src/dev_script.cpp src/window_capture.cpp)
  target_link_libraries(anno_app_sdl PUBLIC anno_app_core SDL3::SDL3)
  anno_target_defaults(anno_app_sdl)
  target_sources(anno1503 PRIVATE src/viewer.cpp)
  target_compile_definitions(anno1503 PRIVATE ANNO_WITH_VIEWER)
  target_link_libraries(anno1503 PRIVATE anno_app_sdl)
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
    tests/scene_picture_recovery_test.cpp tests/scene_picture_ownership_test.cpp
    tests/scene_picture_publish_test.cpp
    tests/play_follow_test.cpp
    tests/credit_test.cpp tests/picture_check_test.cpp tests/legal_workload_test.cpp
    tests/picture_trace_test.cpp tests/window_trace_test.cpp tests/command_outcome_test.cpp
    tests/visibility_test.cpp tests/dev_occupancy_test.cpp tests/dev_visibility_test.cpp
    tests/opening_ui_test.cpp tests/opening_test.cpp tests/ship_motion_test.cpp)
  target_link_libraries(anno_app_tests PRIVATE anno_app_core anno_test_support doctest::doctest)
  target_compile_definitions(anno_app_tests PRIVATE
    ANNO_FIXTURES_DIR="${PROJECT_SOURCE_DIR}/fixtures/synthetic"
    ANNO_REPO_DIR="${PROJECT_SOURCE_DIR}")
  anno_target_defaults(anno_app_tests)
  anno_stage_budget_coord(anno_app_tests)
  anno_add_doctest(anno_app_tests)
  # The two full-corpus cases (every scenario file, every assertion) are "heavy" (the suite name in
  # play_test.cpp); in a sanitizer build they also get an 8000 s per-test timeout, set after the cases
  # are discovered (AnnoAppSanitizerTimeouts.cmake; docs/testing/README.md).
  if(ANNO_SANITIZE)
    set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
                 "${CMAKE_CURRENT_SOURCE_DIR}/AnnoAppSanitizerTimeouts.cmake")
  endif()
  # Bad arguments print the usage. CTest's output match ignores the exit
  # code; app_test checks that parse_args rejects the input, and main maps a
  # rejection to exit 2 (not tested automatically).
  # The last-owner release paths allocate nothing (round dy, A6): its own executable, because it
  # replaces the global allocation functions (they count and refuse only on the thread that asks).
  add_executable(anno_app_alloc_tests tests/alloc_test.cpp)
  target_link_libraries(anno_app_alloc_tests PRIVATE anno_app_core doctest::doctest)
  anno_target_defaults(anno_app_alloc_tests)
  anno_add_doctest(anno_app_alloc_tests)
  if(ANNO_BUILD_VIEWER)
    # The developer tool through the window's own SDL mapping (synthetic SDL events; no window).
    add_executable(anno_app_input_tests tests/main.cpp tests/dev_tool_test.cpp
      tests/dev_feedback_test.cpp tests/dev_tool_mod_test.cpp
      tests/dev_barrier_test.cpp tests/dev_script_test.cpp)
    target_link_libraries(anno_app_input_tests PRIVATE anno_app_sdl anno_test_support
      doctest::doctest)
    target_compile_definitions(anno_app_input_tests PRIVATE
      ANNO_FIXTURES_DIR="${PROJECT_SOURCE_DIR}/fixtures/synthetic")
    anno_target_defaults(anno_app_input_tests)
    anno_stage_budget_coord(anno_app_input_tests)
    anno_add_doctest(anno_app_input_tests)
    # The production play window against a scripted session, headless (SDL's dummy video driver
    # and the CPU renderer; W4-WIN-CLOSE C1): R1's title/script consistency inside a held
    # publication and R4's occupancy wiring. They need the original game files and skip (77)
    # without them. Reverting the window's call sites fails them (docs/app/game-shell.md).
    if(Python3_FOUND)
      foreach(case r1_held_publication r4_occupancy)
        add_test(NAME viewer_wiring_${case}
          COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                  $<TARGET_FILE:anno1503> ${case})
        set_tests_properties(viewer_wiring_${case} PROPERTIES
          SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      endforeach()
      # W4-SETTLEMENT-1 W5 Proof B: the developer inspection UI (PlayRunner::request_inspect,
      # PlaySnapshot::inspection) reports real, live building/household state through the real
      # window, built through the legal dev-tool placement path on the lan5 developer island.
      add_test(NAME viewer_wiring_settlement_activity
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> settlement_activity)
      set_tests_properties(viewer_wiring_settlement_activity PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # F2 (W4-WIN-FINAL): the CPU window settles a change only after the frame was delivered through
      # SDL_UpdateTexture, SDL_RenderTexture and SDL_RenderPresent; each stage is made to fail in turn.
      add_test(NAME viewer_wiring_cpu_delivery_failures
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> cpu_delivery_failures
                --expect-stderr "CPU frame delivery failed (SDL_UpdateTexture"
                --expect-stderr "CPU frame delivery failed (SDL_RenderTexture"
                --expect-stderr "CPU frame delivery failed (SDL_RenderPresent")
      set_tests_properties(viewer_wiring_cpu_delivery_failures PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # W4-OPENING-1: the ship-start opening through the production window (CPU renderer, SDL's dummy
      # driver). Controls: selection, the order gestures and their cancels, one short voyage. Session: the
      # whole opening (launch, select, sail 550 tiles, save; resume, refused foundings, found, unload,
      # build, ledger) and the replay of both recordings in fresh processes. They need the original files.
      foreach(case opening_controls)
        add_test(NAME viewer_wiring_${case}
          COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                  $<TARGET_FILE:anno1503> ${case}
                  --app-arg --ship-start
                  --app-arg ${PROJECT_SOURCE_DIR}/data/maps/candidates/ship-start-v2/lan5-ship-a.json
                  --app-arg --seat --app-arg 0 --app-arg --speed --app-arg 200)
        set_tests_properties(viewer_wiring_${case} PROPERTIES
          SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      endforeach()
      add_test(NAME opening_session_replay
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/opening_session.py
                $<TARGET_FILE:anno1503>)
      set_tests_properties(opening_session_replay PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 1800 PROCESSORS 4)
      if(ANNO_SANITIZE)
        # Every process of the session (three windows, thirteen replays) loads the game files, about 170 s
        # each under ASan/UBSan; the voyage of stage 1 adds its own real time on top of that (turn p95
        # measured at 509.5 ms under ASan here, against 0.2-0.4 ms native). viewer_wiring.py's own
        # ANNO_VIEW_QUIT_S force-quits a window that never reaches its script's `quit`, at 240 s by
        # default: too short for this voyage under the sanitizer slowdown (measured: stage 1 alone needs
        # 434.4 s wall time to complete naturally here). Scale it by the same 5x as the watchdog below, to
        # 1200 s: comfortable headroom over the measurement, and still under the watchdog so a genuine
        # hang is still caught by name. Not loosened for the normal build (viewer_wiring.py's default stays
        # 240 s there).
        set_tests_properties(opening_session_replay PROPERTIES
          TIMEOUT 5400 ENVIRONMENT "ANNO_VIEWER_WIRING_WATCHDOG_S=1500;ANNO_VIEW_QUIT_S=1200")
      endif()
    endif()
  endif()
  add_test(NAME anno1503_usage COMMAND anno1503 --view x.sze --group 3)
  set_tests_properties(anno1503_usage PROPERTIES PASS_REGULAR_EXPRESSION "usage:")
endif()
