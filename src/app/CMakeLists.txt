# anno1503: the application (for now the island viewer). anno_app_core holds
# the window-independent parts (scene loading, PNG output) so tests can use
# them; the window needs SDL3 (ANNO_BUILD_VIEWER).
add_library(anno_app_core
  src/balance_rows.cpp
  src/building_delta.cpp
  src/command_outcome.cpp
  src/commands_file.cpp
  src/construction_menu.cpp
  src/selection.cpp
  src/status_bar.cpp
  src/range_view.cpp
  src/dev_feedback.cpp
  src/dev_session.cpp
  src/dev_input.cpp
  src/dev_tool.cpp
  src/fertility_indicator.cpp
  src/frame_projection.cpp
  src/hud_tabs.cpp
  src/info_preview.cpp
  src/legal_workload.cpp
  src/minimap.cpp
  src/cli.cpp
  src/loader.cpp
  src/opening.cpp
  src/opening_session.cpp
  src/opening_ui.cpp
  src/opening_view.cpp
  src/picture_check.cpp
  src/picture_trace.cpp
  src/placement_ghost.cpp
  src/road_drag_ui.cpp
  src/play.cpp
  src/play_follow.cpp
  src/png.cpp
  src/publication_hold.cpp
  src/scene_picture.cpp
  src/ship_layer.cpp
  src/ship_motion.cpp
  src/still.cpp
  src/ui_layout.cpp
  src/ui_mock.cpp
  src/ui_transform.cpp
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

# anno_ui_mock: the CPU reference composition of a HUD/construction screen, as a PNG
# (tools/test/ui_mock_compare.py diffs it against the UI.1 Python oracle's own mock). No SDL/GPU
# dependency - window-independent, like anno_app_core itself.
add_executable(anno_ui_mock src/ui_mock_main.cpp)
target_link_libraries(anno_ui_mock PRIVATE anno_app_core)
anno_target_defaults(anno_ui_mock)

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
    tests/opening_ui_test.cpp tests/opening_test.cpp tests/ship_motion_test.cpp
    tests/ui_layout_test.cpp tests/ui_transform_test.cpp tests/ui_construction_eligibility_test.cpp
    tests/construction_menu_test.cpp tests/inspect_building_test.cpp tests/selection_test.cpp
    tests/minimap_test.cpp tests/hud_tabs_test.cpp tests/status_bar_test.cpp
    tests/balance_rows_test.cpp tests/atlas_pages_lan5_test.cpp tests/info_preview_test.cpp
    tests/range_view_test.cpp tests/fertility_indicator_test.cpp tests/tile_object_picture_test.cpp
    tests/placement_ghost_test.cpp tests/road_drag_ui_test.cpp)
  target_link_libraries(anno_app_tests PRIVATE anno_app_core anno_test_support doctest::doctest)
  target_compile_definitions(anno_app_tests PRIVATE
    ANNO_FIXTURES_DIR="${PROJECT_SOURCE_DIR}/fixtures/synthetic"
    ANNO_REPO_DIR="${PROJECT_SOURCE_DIR}")
  anno_target_defaults(anno_app_tests)
  anno_stage_budget_coord(anno_app_tests)
  anno_add_doctest(anno_app_tests)
  # W4-UI-2 H4/C4: the shipped CPU reference composition (anno_ui_mock) checked pixel by pixel
  # against the UI.1 Python oracle's own mock, for all nine named screens. Needs the original game
  # files and Pillow; skips (77) without either (tools/test/ui_mock_compare.py).
  add_test(NAME ui_mock_matches_oracle
    COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/ui_mock_compare.py
            $<TARGET_FILE:anno_ui_mock>)
  set_tests_properties(ui_mock_matches_oracle PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 300)
  # UI-WINDOW-1: the UI layer at 2542 x 1351, UI scale 1.0 / 1.5 / 2.0, against the History
  # Edition's own captures (ANNO_UI_HE_CAPTURES, never in git; skips without it).
  add_test(NAME ui_window_matches_he
    COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/ui_window_compare.py
            $<TARGET_FILE:anno_ui_mock>)
  set_tests_properties(ui_window_matches_he PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)
  # UI-WINDOW-1 fix round R2: the four tab panels' static chrome against the GOG shots
  # (ANNO_UI_ORACLE_SHOTS, never in git; skips without it).
  add_test(NAME ui_panels_match_gog
    COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/ui_panels_compare.py
            $<TARGET_FILE:anno_ui_mock>)
  set_tests_properties(ui_panels_match_gog PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)
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
      tests/dev_barrier_test.cpp tests/dev_script_test.cpp tests/live_resize_test.cpp)
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
      foreach(case r1_held_publication r4_occupancy roads_picture_objects)
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
      # W4-UI-2 C4: a construction-menu grid click (not the developer tool's own cycle) arms a
      # type through DevTool::arm and places it through the ordinary command path, on the same
      # lan5 tile/type settlement_activity.script already proves free and buildable.
      add_test(NAME viewer_wiring_construction_menu_arm
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> construction_menu_arm
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_construction_menu_arm PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # UI-WINDOW-1: the UI at 2542 x 1351, default scale (s10) and --ui-scale 2.0 (s20): chrome
      # clicks never reach the map, the construction tab opens the menu, a cell arms its type.
      foreach(case s10 s20)
        set(ui_scale_args)
        if(case STREQUAL "s20")
          set(ui_scale_args --app-arg --ui-scale --app-arg 2.0)
        endif()
        add_test(NAME viewer_wiring_ui_window_click_${case}
          COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                  $<TARGET_FILE:anno1503> ui_window_click_${case}
                  --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                  --app-arg --construction-menu --app-arg on
                  --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui ${ui_scale_args})
        set_tests_properties(viewer_wiring_ui_window_click_${case} PROPERTIES
          SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      endforeach()
      # UI-INFO-2: the info panel's rotate arrows turn the armed type's rotation (left -1, right
      # +1), the one the keys turn, at 1024 x 768 and at 2542 x 1351 (right-anchored chrome).
      foreach(case ui_rotate_arrows ui_rotate_arrows_s10)
        add_test(NAME viewer_wiring_${case}
          COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                  $<TARGET_FILE:anno1503> ${case}
                  --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                  --app-arg --construction-menu --app-arg on
                  --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
        set_tests_properties(viewer_wiring_${case} PROPERTIES
          SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      endforeach()
      # UI-WINDOW-1 fix round R1 (F2): closing the menu (tab, Esc) turns an armed tool off.
      add_test(NAME viewer_wiring_ui_menu_close_disarms
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> ui_menu_close_disarms
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_ui_menu_close_disarms PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # FISHER-VIEW-1: --fisher-rules on|off in --play on a shipped scenario (Tutorial/02Trade01,
      # player 2), with the hovered hut's readout; `{game}` is ANNO1503_GAME_DIR (viewer_wiring.py).
      # `closed`: the closed site (242,217) rotation 1 with the switch on (FISHER-1f).
      foreach(case on off closed)
        set(fisher_args)
        if(NOT case STREQUAL "off")
          set(fisher_args --app-arg --fisher-rules --app-arg on)
        endif()
        add_test(NAME viewer_wiring_fisher_view_${case}
          COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                  $<TARGET_FILE:anno1503> fisher_view_${case}
                  --app-arg --play --app-arg {game}/Scenes/Tutorial/02Trade01.sze
                  --app-arg --player --app-arg 2 --app-arg --speed --app-arg 200 ${fisher_args})
        set_tests_properties(viewer_wiring_fisher_view_${case} PROPERTIES
          SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      endforeach()
      # UI-PLACE-1: the pickaxe with nothing armed, a cell chosen while demolishing, the cancel-X.
      # UI-PLACE-2: ESC and the right click one step back, a right drag still pans.
      # UI-PAN-1: a click (under the 4 px slop) moves the map not at all, either button.
      # UI-MINIMAP-1 C: the minimap's controls (a diamond click centres the camera, the zoom
      # buttons, the magnifier, dead chevrons, the parchment strip left of the chrome).
      foreach(case ui_place_demolish ui_place_escape ui_click_no_pan ui_minimap_controls)
        add_test(NAME viewer_wiring_${case}
          COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                  $<TARGET_FILE:anno1503> ${case}
                  --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                  --app-arg --construction-menu --app-arg on
                  --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
        set_tests_properties(viewer_wiring_${case} PROPERTIES
          SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      endforeach()
      # UI-MINIMAP-1 C: the minimap's viewer is the runner's local player (--player).
      add_test(NAME viewer_wiring_ui_minimap_player
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> ui_minimap_player
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --player --app-arg 3
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_ui_minimap_player PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # UI-MINIMAP-2: the magnifier's - reaches the wheel's furthest zoom-out in the continuous
      # zoom (ui_magnifier_range) and keeps the original's clamp at level 2 in the stepped one
      # (ui_magnifier_range_stepped). Both in the GPU window (--gpu): the continuous zoom needs the
      # GPU renderer, and on it --zoom-mode stepped itself must pick the stepped zoom.
      if(ANNO_HAVE_WGPU)
        foreach(case ui_magnifier_range ui_magnifier_range_stepped)
          set(zoom_mode_args)
          if(case STREQUAL "ui_magnifier_range_stepped")
            set(zoom_mode_args --app-arg --zoom-mode --app-arg stepped)
          endif()
          add_test(NAME viewer_wiring_${case}
            COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                    $<TARGET_FILE:anno1503> ${case} --gpu
                    --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                    --app-arg --construction-menu --app-arg on
                    --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui ${zoom_mode_args})
          set_tests_properties(viewer_wiring_${case} PROPERTIES
            SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4 LABELS gpu RESOURCE_LOCK gpu)
        endforeach()
      endif()
      # UI-WINDOW-1 fix round R1 (F6, F7): a chrome drag starts no pan; a lost UI press is dropped.
      add_test(NAME viewer_wiring_ui_chrome_press
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> ui_chrome_press
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_ui_chrome_press PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # UI-WINDOW-1 fix round R2: the four tabs as switches, D/O keys, disarm on leaving.
      add_test(NAME viewer_wiring_ui_tabs
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> ui_tabs
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_ui_tabs PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # UI-BUILDCAT-1: the construction menu's eight category tabs, each selected by a press on
      # its own gadget (also left of the column), and the column/grid boundary (OQ-714).
      add_test(NAME viewer_wiring_ui_buildcat
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> ui_buildcat
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_ui_buildcat PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # UI-CATNAV-1: a category tab click with a building armed shows that category's grid and
      # keeps the building armed (ui.panel, tool.armed).
      add_test(NAME viewer_wiring_ui_catnav
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> ui_catnav
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_ui_catnav PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # UI-TABS-1: every sub-tab of the three views, the keys S, C, D, O, each view keeping its own.
      add_test(NAME viewer_wiring_ui_subtabs
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> ui_subtabs
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_ui_subtabs PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # UI-TABS-1 stage B: the balance pages' figures as facts (docs/re/ui.md 5.5): with the
      # accounts kept (--rules living), under the original rules, and before the first founding.
      add_test(NAME viewer_wiring_ui_balance_rows
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> ui_balance_rows
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --rules --app-arg living
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_ui_balance_rows PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      add_test(NAME viewer_wiring_ui_balance_rows_original
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> ui_balance_rows_original
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_ui_balance_rows_original PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      add_test(NAME viewer_wiring_ui_balance_rows_no_city
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> ui_balance_rows_no_city
                --app-arg --ship-start
                --app-arg ${PROJECT_SOURCE_DIR}/data/maps/candidates/ship-start-v2/lan5-ship-a.json
                --app-arg --seat --app-arg 0
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_ui_balance_rows_no_city PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # UI-STATUS-1 B: the status bar shows the island under the cursor (docs/re/ui.md 5.6;
      # replaces UI-WINDOW-1 R2/R4's ui_counters_centred, the view-centre rule).
      add_test(NAME viewer_wiring_ui_status_island
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> ui_status_island
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_ui_status_island PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # UI-WINDOW-1 fix round R4, UI-STATUS-1 B: no city yet (the ship-start opening): gold only.
      add_test(NAME viewer_wiring_ui_counters_before_founding
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> ui_counters_before_founding
                --app-arg --ship-start
                --app-arg ${PROJECT_SOURCE_DIR}/data/maps/candidates/ship-start-v2/lan5-ship-a.json
                --app-arg --seat --app-arg 0
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_ui_counters_before_founding PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # W4-UI-2 V1: the full interaction proof - every category tab, a locked cell (no command),
      # an armed placement that actually builds, and a demolish of what was just built.
      add_test(NAME viewer_wiring_construction_full_proof
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> construction_full_proof
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_construction_full_proof PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # W4-UI-2 V1: an unaffordable placement - ANNO_DEV_GOODS_TONS=0 (main.cpp's own override,
      # added for exactly this) leaves type 501's wood cost unpayable; the refusal is the ordinary
      # BuildCmd path's CannotAfford, not a construction-menu-specific check (arming never looks
      # at cost - ConstructionMenu::click's own doc comment).
      add_test(NAME viewer_wiring_construction_unaffordable
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> construction_unaffordable
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_construction_unaffordable PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4
        ENVIRONMENT "ANNO_DEV_GOODS_TONS=0")
      # UI-GHOST-1: the placement ghost's states from the sim's verdict through the real window
      # (tool.ghost: valid over free land, refused over the inlet, hidden over the panel's chrome,
      # unaffordable with the stock spent, its blink phase flipping both ways). ANNO_DEV_GOODS_TONS=3
      # pays for exactly one pioneer house (ui_ghost.script's own comment).
      add_test(NAME viewer_wiring_ui_ghost
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> ui_ghost
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_ui_ghost PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4
        ENVIRONMENT "ANNO_DEV_GOODS_TONS=3")
      # UI-GHOST-1 R1: the ghost and the click anywhere on the map, from the default start view (no
      # `look`): refused on another island and over open sea, the click refused by the sim with the
      # same reason (OutOfIsland), valid on the city's island.
      add_test(NAME viewer_wiring_ui_ghost_world
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> ui_ghost_world
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_ui_ghost_world PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4
        ENVIRONMENT "ANNO_DEV_GOODS_TONS=3")
      # UI-GHOST-1 R2: the play window opens on the developer city (the owner's decision of
      # 2026-11-17): the start camera's tile lies on the city's island, near its Kontor.
      add_test(NAME viewer_wiring_ui_start_view
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> ui_start_view
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_ui_start_view PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # UI-RANGE-1 B1: the lit city and the dark-tile count for a selected Kontor and an armed road,
      # from the default start view (range_view.hpp, docs/re/ui.md 12).
      add_test(NAME viewer_wiring_ui_range
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> ui_range
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_ui_range PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # UI-GHOST-1: the same session's screenshots, the outline's exact 0xffff00 pixels counted
      # (ghost_pixel_check.py; the hidden shot is the negative control).
      add_test(NAME viewer_wiring_ui_ghost_pixel_check
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/ghost_pixel_check.py
                $<TARGET_FILE:anno1503>)
      set_tests_properties(viewer_wiring_ui_ghost_pixel_check PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 300 PROCESSORS 4)
      # ROADS-3 / UI-ROADS-1: press-hold L-drag road building from the construction menu,
      # live ghost path, unaffordable tail blink, yellow 1x1 refusal rectangle, 40-tile cap,
      # stay armed on release, right-click disarm (ANNO_DEV_GOLD=20 -> 4 affordable tiles of 7).
      add_test(NAME viewer_wiring_ui_roads
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> ui_roads
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_ui_roads PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4
        ENVIRONMENT "ANNO_DEV_GOLD=20")
      # ROADS-3: pixel check on ui_roads screenshots (refusal diamond, live L-drag ghost path,
      # unaffordable tail blink diff between phase 0 and phase 1, chrome stays on top).
      add_test(NAME viewer_wiring_ui_roads_pixel_check
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/roads_ghost_pixel_check.py
                $<TARGET_FILE:anno1503>)
      set_tests_properties(viewer_wiring_ui_roads_pixel_check PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 300 PROCESSORS 4)
      # UI-GHOST-2: ghost hides overlappable flag-0x20 objects under footprint without mutating Scene,
      # restores on move/cancel/phase-1, draws only yellow refusal box when default-off, and draws
      # blocking footprint outline only under --occupied-outline on (ADAPTATION, default off).
      add_test(NAME viewer_wiring_ui_ghost2
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> ui_ghost2
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_ui_ghost2 PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4
        ENVIRONMENT "ANNO_DEV_GOODS_TONS=6")
      add_test(NAME viewer_wiring_ui_ghost2_occupied
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> ui_ghost2_occupied
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui
                --app-arg --occupied-outline --app-arg on)
      set_tests_properties(viewer_wiring_ui_ghost2_occupied PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4
        ENVIRONMENT "ANNO_DEV_GOODS_TONS=3")
      add_test(NAME viewer_wiring_ui_ghost2_pixel_check
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/ghost2_pixel_check.py
                $<TARGET_FILE:anno1503>)
      set_tests_properties(viewer_wiring_ui_ghost2_pixel_check PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 300 PROCESSORS 4)
      # W4-UI-2 V2: construction_full_proof's own recording (commands.txt/hashes.txt), replayed
      # headlessly in a fresh process (main.cpp's --commands/--replay-hashes, no window, no SDL -
      # opening_session.py's own mechanism, applied to the fixed-map/construction-menu session).
      # A negative control (the recorded build's type changed) must disagree at a checkpoint.
      add_test(NAME construction_session_replay
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/construction_session_replay.py
                $<TARGET_FILE:anno1503>)
      set_tests_properties(construction_session_replay PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # W4-NEXT-2 K: row 18, selecting a built house through the real window - build, select,
      # deselect (Escape), select another, demolish it while selected (selection.hpp's "a building
      # demolished meanwhile drops the selection", exercised end to end, not just the unit test).
      add_test(NAME viewer_wiring_selected_house_draw_check
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> selected_house_draw_check
                --app-arg --play --app-arg ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt
                --app-arg --construction-menu --app-arg on
                --app-arg --layout --app-arg ${PROJECT_SOURCE_DIR}/data/ui)
      set_tests_properties(viewer_wiring_selected_house_draw_check PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # The same recording, replayed headlessly in a fresh process (construction_session_replay's
      # own mechanism; selection itself writes no sim state, so this checks only what the
      # recording carries: the two builds and the demolish, by their state hashes).
      add_test(NAME selected_house_session_replay
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/selected_house_session_replay.py
                $<TARGET_FILE:anno1503>)
      set_tests_properties(selected_house_session_replay PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # W4-UI-2 H1/C1 second slice: the live window's own drawing (draw_cpu ->
      # draw_live_screen_xrgb), checked pixel by pixel against the UI.1 oracle (tools/test/
      # ui_mock_compare.py already checks the standalone anno_ui_mock composer; this checks the
      # real window instead), through a real captured screenshot (reads ANNO1503_GAME_DIR itself
      # at run time, like ui_mock_compare.py; Windows only - the screenshot step's own platform
      # limit).
      add_test(NAME live_screen_matches_oracle
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/live_screen_compare.py
                $<TARGET_FILE:anno1503>)
      set_tests_properties(live_screen_matches_oracle PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 300 PROCESSORS 4)
      # W4-NEXT-1 part U rows 5/10 (orchestrator review finding 6, 2026-11-10): the pickaxe's
      # active frame and the pause overlay, neither of which compose_screen models - see
      # ui_dynamic_draw_check.py's own doc comment for how the oracle is built for them.
      add_test(NAME ui_dynamic_draw_check
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/ui_dynamic_draw_check.py
                $<TARGET_FILE:anno1503>)
      set_tests_properties(ui_dynamic_draw_check PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 300 PROCESSORS 4)
      # W4-NEXT-2 K, independent-review finding 7 (2026-11-11): row 18's panel had no pixel-level
      # check (only title text and command outcomes) - the same evidence gap ui_dynamic_draw_check
      # closed for rows 5/10, now closed here the same way. See selected_house_pixel_check.py's
      # own doc comment for how the oracle is built.
      add_test(NAME selected_house_pixel_check
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/selected_house_pixel_check.py
                $<TARGET_FILE:anno1503>)
      set_tests_properties(selected_house_pixel_check PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 300 PROCESSORS 4)
      # UI-INFO-1: the info panel's name, preview, costs and upkeep through the real window,
      # pixel-exact against tools/ui_layout.py compose_info_panel (info_panel_pixel_check.py).
      add_test(NAME info_panel_pixel_check
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/info_panel_pixel_check.py
                $<TARGET_FILE:anno1503>)
      set_tests_properties(info_panel_pixel_check PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 300 PROCESSORS 4)
      # UI-INFO-1 fix round F (F8): the same reference against the original's own GOG shots
      # (ANNO_UI_ORACLE_SHOTS, never in git; skips without it).
      add_test(NAME info_panel_gog_compare
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/info_panel_gog_compare.py)
      set_tests_properties(info_panel_gog_compare PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)
      # UI-TABS-1: the sub-tab columns' selected and idle frames, the GPU layer's composer against
      # the GOG shots (ANNO_UI_ORACLE_SHOTS, never in git; skips without it) and the window's CPU
      # path against that composer.
      add_test(NAME ui_subtabs_compare
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/ui_subtabs_compare.py
                $<TARGET_FILE:anno_ui_mock> $<TARGET_FILE:anno1503>)
      set_tests_properties(ui_subtabs_compare PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # UI-TABS-1 stage B: the balance rows, the GPU layer's composer against the GOG shots
      # (ANNO_UI_ORACLE_SHOTS, never in git; skips without it) with four negative controls, and
      # the window's CPU path against that composer.
      add_test(NAME ui_balance_rows_compare
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/ui_balance_rows_compare.py
                $<TARGET_FILE:anno_ui_mock> $<TARGET_FILE:anno1503>)
      set_tests_properties(ui_balance_rows_compare PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 600 PROCESSORS 4)
      # UI-INFO-2: the preview at rotations 0..3 after clicks on the rotate arrows, through the
      # real window, pixel-exact against compose_info_panel(rotation) (info_panel_rotation_check.py),
      # and that reference against the original's own shots after its arrows and keys
      # (ANNO_UI_ROTATION_SHOTS, never in git; skips without it).
      add_test(NAME info_panel_rotation_check
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/info_panel_rotation_check.py
                $<TARGET_FILE:anno1503>)
      set_tests_properties(info_panel_rotation_check PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 300 PROCESSORS 4)
      add_test(NAME info_panel_rotation_gog_compare
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/info_panel_rotation_gog_compare.py)
      set_tests_properties(info_panel_rotation_gog_compare PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)
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
      # W4-EXPAND-LIVE-1 D4: the developer Load adapter (key L) driven in the real window, after the
      # same full voyage and real founding as opening_sail.script/opening_resume.script use (not a
      # short one: the proven founding coast is 550 tiles out). TIMEOUT follows opening_session_replay's
      # budget for this same voyage, not viewer_wiring's 600 s default.
      add_test(NAME viewer_wiring_expand_live_load
        COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring.py
                $<TARGET_FILE:anno1503> expand_live_load
                --app-arg --ship-start
                --app-arg ${PROJECT_SOURCE_DIR}/data/maps/candidates/ship-start-v2/lan5-ship-a.json
                --app-arg --seat --app-arg 0 --app-arg --speed --app-arg 200)
      set_tests_properties(viewer_wiring_expand_live_load PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 1200 PROCESSORS 4)
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
        # viewer_wiring_expand_live_load's own comment above says its TIMEOUT already follows
        # opening_session_replay's budget for the same voyage (550 tiles out, one real founding);
        # under ASan it hit exactly the failure mode opening_session_replay's own fix above
        # documents, in two steps found by reading the actual failure (never guessed): first,
        # viewer_wiring.py's own ANNO_VIEWER_WIRING_WATCHDOG_S subprocess timeout (default 300 s)
        # fired before the outer CTest TIMEOUT could ("the window did not end within 300 s
        # (watchdog)"); once that was scaled, the run (390 s) still failed, this time with "window
        # exit 1" although script.log showed every reached line passing -- because the app's own
        # ANNO_VIEW_QUIT_S force-quit (default 240 s, main.cpp: status=1 when
        # `!script->done()`) fired before the script reached its last lines (the negative-control
        # refusal checks and the final `quit`), so the window closed mid-script, not after it.
        # This case was never run under ASan until the D5 WSL-ASan rerun (2026-11-07) added it to
        # the lane's selection regex, so neither gap was previously observed. Scale both the same
        # 5x as opening_session_replay.
        set_tests_properties(viewer_wiring_expand_live_load PROPERTIES
          TIMEOUT 5400 ENVIRONMENT "ANNO_VIEWER_WIRING_WATCHDOG_S=1500;ANNO_VIEW_QUIT_S=1200")
      endif()
    endif()
  endif()
  add_test(NAME anno1503_usage COMMAND anno1503 --view x.sze --group 3)
  set_tests_properties(anno1503_usage PROPERTIES PASS_REGULAR_EXPRESSION "usage:")
endif()
