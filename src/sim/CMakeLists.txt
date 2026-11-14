# anno_sim: the deterministic simulation. Headless, no I/O, integer
# arithmetic in all state (docs/sim/core.md, docs/sim/state.md). Uses
# anno_formats to read type data from the original files.
add_library(anno_sim src/sim.cpp src/production.cpp src/mine_source.cpp src/mine_facts.cpp src/config.cpp src/types.cpp src/city_accounts.cpp src/city_booking.cpp
            src/script_runtime.cpp src/permissions.cpp src/script.cpp src/build_availability.cpp src/goods_table.cpp src/synthetic.cpp src/player.cpp src/island.cpp src/scenario.cpp src/original_units.cpp
            src/save.cpp src/building.cpp src/create.cpp src/invariants.cpp src/figure.cpp src/ship.cpp src/ship_step.cpp
            src/driver.cpp src/scheduler.cpp src/codec.cpp src/apply.cpp src/replay.cpp
            src/placement.cpp src/legacy_codec.cpp src/events.cpp src/timers.cpp
            src/island_tick.cpp src/scenario_formats.cpp src/island_links.cpp src/transport.cpp src/harvest.cpp src/fisher.cpp src/mason.cpp src/walk_cells.cpp src/walk_search.cpp
            src/game_setup.cpp src/house_groups.cpp src/goods_metadata.cpp src/house_kernel.cpp src/terrain_view.cpp src/site_checks.cpp src/territory.cpp src/founding_payment.cpp src/city_creation.cpp src/figure_veto.cpp src/founding.cpp src/resident.cpp src/resident_job.cpp src/inn.cpp src/house_transition.cpp src/house_tick.cpp
            src/route_cache.cpp src/ship_orders.cpp src/freight.cpp src/ship_cargo.cpp src/ship_mover.cpp src/route_coast.cpp src/route_plan.cpp src/route_world.cpp src/route_stamps.cpp)
target_include_directories(anno_sim PUBLIC include)
target_link_libraries(anno_sim PUBLIC anno_formats)
target_compile_features(anno_sim PUBLIC cxx_std_20)
anno_target_defaults(anno_sim SANITIZE_NO_RECOVER)

if(ANNO_BUILD_TESTS)
  add_executable(anno_sim_tests tests/main.cpp tests/golden_test.cpp tests/sim_test.cpp tests/economy_facts_test.cpp tests/city_booking_test.cpp tests/economy_loop_test.cpp tests/economy_scale_test.cpp tests/freight_state_test.cpp tests/freight_visit_test.cpp tests/freight_lifecycle_test.cpp tests/freight_scale_test.cpp tests/integration_identity_test.cpp tests/integration_stream_test.cpp tests/integration_gameplay_test.cpp tests/integration_cross_feature_test.cpp tests/production_test.cpp
                 tests/archive_test.cpp tests/slot_list_test.cpp tests/scheduler_blocks_test.cpp
                 tests/container_probe_test.cpp tests/config_test.cpp tests/types_test.cpp
                 tests/player_test.cpp tests/tile_hash_test.cpp tests/island_state_test.cpp
                 tests/create_remove_test.cpp tests/building_state_test.cpp
                 tests/world_state_test.cpp tests/scheduler_test.cpp tests/slice_test.cpp tests/alloc_probe.cpp
                 tests/command_codec_test.cpp tests/command_apply_test.cpp tests/replay_test.cpp
                 tests/command_build_test.cpp tests/property_test.cpp
                 tests/driver_classes_test.cpp tests/world_events_test.cpp tests/sweeps_test.cpp
                 tests/figure_removal_test.cpp tests/scenario_formats_test.cpp tests/unit_import_test.cpp
                 tests/scenario_test.cpp tests/island_links_test.cpp
                 tests/fixed_map_load_test.cpp
                 tests/scenario_links_test.cpp tests/transport_test.cpp tests/walk_cells_test.cpp tests/walk_search_test.cpp
                 tests/game_setup_test.cpp tests/house_test.cpp tests/population_goods_test.cpp tests/population_groups_test.cpp tests/house_kernel_test.cpp tests/resident_test.cpp tests/inn_test.cpp tests/population_supply_test.cpp tests/population_progression_test.cpp tests/fisher_test.cpp tests/small_farm_vectors_test.cpp tests/small_farm_worker_test.cpp tests/small_farm_animal_test.cpp tests/small_farm_chain_test.cpp tests/small_farm_resilience_test.cpp tests/resident_state_test.cpp tests/resident_job_test.cpp tests/house_transition_test.cpp tests/house_live_test.cpp tests/population_canary_test.cpp tests/population_live_canary_test.cpp tests/population_scale_test.cpp tests/ship_state_test.cpp tests/ship_start_load_test.cpp
                 tests/terrain_view_test.cpp tests/site_checks_test.cpp tests/territory_test.cpp tests/founding_coords_test.cpp tests/founding_payment_test.cpp tests/island_lock_corpus_test.cpp tests/city_creation_test.cpp tests/figure_veto_test.cpp tests/founding_test.cpp tests/founding_assets_test.cpp tests/figure_facts_test.cpp tests/wide_test.cpp tests/ship_nav_state_test.cpp tests/ship_step_test.cpp tests/ship_orders_test.cpp tests/ship_nav3_test.cpp tests/ship_health_test.cpp tests/route_objects_test.cpp tests/placement_rule_test.cpp tests/build_terrain_test.cpp tests/script2_scale_test.cpp tests/script2_canary_test.cpp tests/script2_session_test.cpp tests/script2_wave_test.cpp tests/script2_money_test.cpp tests/script2_test.cpp tests/script_scale_test.cpp tests/script_canary_test.cpp tests/script_build_test.cpp tests/script_runtime_test.cpp tests/permissions_test.cpp tests/script_test.cpp tests/build_availability_test.cpp tests/early_needs_vectors_test.cpp tests/sheep_farm_test.cpp tests/sheep_cloth_chain_test.cpp
                 tests/cloth_canary_test.cpp tests/salt_registry_test.cpp tests/salt_placement_test.cpp tests/iron_mine_test.cpp tests/iron_chain_test.cpp tests/iron_interrupt_test.cpp tests/quarry_test.cpp tests/resident_walk_home_test.cpp
                 tests/leather_fixture_test.cpp
                 tests/leather_persistence_test.cpp
                 tests/nav_scale_test.cpp
                 tests/unload_test.cpp tests/harvest_state_test.cpp tests/harvest_lifecycle_test.cpp tests/harvest_delivery_test.cpp
                 tests/load_test.cpp
                 tests/evidence_out_test.cpp
                 tests/route_fixture_test.cpp
                 tests/route_unit_test.cpp
                 tests/route_world_test.cpp
                 tests/route_stamps_test.cpp)
  # anno_sim_load: the scenario corpus case reads the island templates of the game
  # directory for the link resolver (S14 stage 2).
  target_link_libraries(anno_sim_tests PRIVATE anno_sim anno_sim_load anno_test_support
                                               doctest::doctest)
  # The goldens (fixtures/synthetic/sim/goldens.txt, docs/sim/state.md §14.3).
  target_compile_definitions(anno_sim_tests PRIVATE
    ANNO_FIXTURES_DIR="${PROJECT_SOURCE_DIR}/fixtures/synthetic"
    ANNO_REPO_DIR="${PROJECT_SOURCE_DIR}")
  anno_target_defaults(anno_sim_tests SANITIZE_NO_RECOVER)
  anno_stage_budget_coord(anno_sim_tests)
  # GCC 13 with -fsanitize=address treats a one-element initializer_list
  # assignment as an out-of-bounds memmove (-Warray-bounds,
  # -Wstringop-overread). The copy length is the list length. UBSan still
  # checks these tests when they run. The warning gate without sanitizers
  # does not report it.
  if(ANNO_SANITIZE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(anno_sim_tests PRIVATE -Wno-array-bounds -Wno-stringop-overread)
  endif()
  anno_add_doctest(anno_sim_tests UBSAN_HALT)
  # ECON-BOOK-1: one real producer run supplies both fresh-process replay arms.
  # Existing exit-status/output helper and its required contract remain the gate.
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/economy-loop")
  set(econ_loop_environment "ANNO_ECON_OUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/economy-loop")
  if(ANNO_SANITIZE AND NOT MSVC)
    list(APPEND econ_loop_environment "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
  endif()
  configure_file(${PROJECT_SOURCE_DIR}/cmake/EconomyLoopProperties.cmake.in
                 ${CMAKE_CURRENT_BINARY_DIR}/economy_loop_properties.cmake @ONLY)
  set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
               "${CMAKE_CURRENT_BINARY_DIR}/economy_loop_properties.cmake")
  add_test(NAME economy_loop_fresh_process
           COMMAND ${CMAKE_COMMAND} -DPROG=$<TARGET_FILE:sim_replay>
                   "-DARGS=run|${CMAKE_CURRENT_BINARY_DIR}/economy-loop/economy.log"
                   -DEXPECT_RC=0 "-DEXPECT=checkpoints 7/7.*reproduced: every checkpoint matched"
                   -P ${PROJECT_SOURCE_DIR}/cmake/RunExpansionReplay.cmake)
  add_test(NAME economy_loop_changed_command_fresh_process
           COMMAND ${CMAKE_COMMAND} -DPROG=$<TARGET_FILE:sim_replay>
                   "-DARGS=run|${CMAKE_CURRENT_BINARY_DIR}/economy-loop/economy-changed-command.log"
                   -DEXPECT_RC=1 "-DEXPECT=checkpoints 3/4.*DIVERGED at entry 3804"
                   -P ${PROJECT_SOURCE_DIR}/cmake/RunExpansionReplay.cmake)
  set_tests_properties(economy_loop_fresh_process economy_loop_changed_command_fresh_process
                       PROPERTIES FIXTURES_REQUIRED econbook_loop_record
                                  SKIP_REGULAR_EXPRESSION "skipping: ANNO1503_GAME_DIR not set")

  # MAP-EXTRACT-1 X5: the salt chain recorded and replayed by anno-sim-replay in a fresh process. Each CTest records
  # its own log first (cmake/RunSaltReplay.cmake runs the recording case with ANNO_SALT_OUT_DIR), so it has no
  # fixture dependency and can be a required contract of the canonical gate by exact name. Both skip explicitly
  # without the original files.
  foreach(salt_arm IN ITEMS fresh_process changed_command_fresh_process)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/salt-chain-${salt_arm}")
  endforeach()
  set(salt_record_case "salt X5: the chain recorded for the fresh-process replay and its changed command")
  add_test(NAME salt_chain_fresh_process
           COMMAND ${CMAKE_COMMAND} -DPROG=$<TARGET_FILE:sim_replay>
                   -DRECORDER=$<TARGET_FILE:anno_sim_tests> "-DCASE=${salt_record_case}"
                   -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/salt-chain-fresh_process
                   -DLOG=salt-chain.log
                   -DEXPECT_RC=0 "-DEXPECT=checkpoints 31/31.*reproduced: every checkpoint matched"
                   -P ${PROJECT_SOURCE_DIR}/cmake/RunSaltReplay.cmake)
  add_test(NAME salt_chain_changed_command_fresh_process
           COMMAND ${CMAKE_COMMAND} -DPROG=$<TARGET_FILE:sim_replay>
                   -DRECORDER=$<TARGET_FILE:anno_sim_tests> "-DCASE=${salt_record_case}"
                   -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/salt-chain-changed_command_fresh_process
                   -DLOG=salt-chain-changed-command.log
                   -DEXPECT_RC=1 "-DEXPECT=checkpoints 0/1.*DIVERGED at entry 2"
                   -P ${PROJECT_SOURCE_DIR}/cmake/RunSaltReplay.cmake)
  set_tests_properties(salt_chain_fresh_process salt_chain_changed_command_fresh_process
                       PROPERTIES SKIP_REGULAR_EXPRESSION "skipping: ANNO1503_GAME_DIR not set")

  # MAP-EXTRACT-2 T5: the iron chain recorded and replayed by anno-sim-replay in a fresh process (the same pattern as the
  # salt chain above; cmake/RunIronReplay.cmake). Both skip explicitly without the original files.
  foreach(iron_arm IN ITEMS fresh_process changed_command_fresh_process)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/iron-chain-${iron_arm}")
  endforeach()
  set(iron_record_case "iron continuation: the chain recorded for the fresh-process replay and its changed command")
  add_test(NAME iron_chain_fresh_process
           COMMAND ${CMAKE_COMMAND} -DPROG=$<TARGET_FILE:sim_replay>
                   -DRECORDER=$<TARGET_FILE:anno_sim_tests> "-DCASE=${iron_record_case}"
                   -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/iron-chain-fresh_process
                   -DLOG=iron-chain.log
                   -DEXPECT_RC=0 "-DEXPECT=checkpoints 31/31.*reproduced: every checkpoint matched"
                   -P ${PROJECT_SOURCE_DIR}/cmake/RunIronReplay.cmake)
  add_test(NAME iron_chain_changed_command_fresh_process
           COMMAND ${CMAKE_COMMAND} -DPROG=$<TARGET_FILE:sim_replay>
                   -DRECORDER=$<TARGET_FILE:anno_sim_tests> "-DCASE=${iron_record_case}"
                   -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/iron-chain-changed_command_fresh_process
                   -DLOG=iron-chain-changed-command.log
                   -DEXPECT_RC=1 "-DEXPECT=checkpoints 0/1.*DIVERGED at entry 2"
                   -P ${PROJECT_SOURCE_DIR}/cmake/RunIronReplay.cmake)
  set_tests_properties(iron_chain_fresh_process iron_chain_changed_command_fresh_process
                       PROPERTIES SKIP_REGULAR_EXPRESSION "skipping: ANNO1503_GAME_DIR not set")

  # MAP-EXPAND E5: one original-data voyage plus five full native suffixes supplies both CLI arms.
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/expansion-voyage")
  set(expansion_environment "ANNO_EXPAND_OUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/expansion-voyage")
  if(ANNO_SANITIZE AND NOT MSVC)
    list(APPEND expansion_environment "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
  endif()
  configure_file(${PROJECT_SOURCE_DIR}/cmake/ExpansionVoyageProperties.cmake.in
                 ${CMAKE_CURRENT_BINARY_DIR}/expansion_voyage_properties.cmake @ONLY)
  set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
               "${CMAKE_CURRENT_BINARY_DIR}/expansion_voyage_properties.cmake")
  add_test(NAME expansion_replay_asset_contract
           COMMAND ${CMAKE_COMMAND}
                   -DWRAPPER=${PROJECT_SOURCE_DIR}/cmake/RunExpansionReplay.cmake
                   -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}
                   -P ${PROJECT_SOURCE_DIR}/cmake/CheckExpansionReplayContract.cmake)
  add_test(NAME expansion_voyage_fresh_process
           COMMAND ${CMAKE_COMMAND} -DPROG=$<TARGET_FILE:sim_replay>
                   "-DARGS=run|${CMAKE_CURRENT_BINARY_DIR}/expansion-voyage/expansion.log"
                   -DEXPECT_RC=0 "-DEXPECT=checkpoints 13/13.*reproduced: every checkpoint matched"
                   -P ${PROJECT_SOURCE_DIR}/cmake/RunExpansionReplay.cmake)
  add_test(NAME expansion_voyage_changed_command_fresh_process
           COMMAND ${CMAKE_COMMAND} -DPROG=$<TARGET_FILE:sim_replay>
                   "-DARGS=run|${CMAKE_CURRENT_BINARY_DIR}/expansion-voyage/expansion-changed-command.log"
                   -DEXPECT_RC=1 "-DEXPECT=checkpoints 10/11.*DIVERGED at entry 458"
                   -P ${PROJECT_SOURCE_DIR}/cmake/RunExpansionReplay.cmake)
  set_tests_properties(expansion_voyage_fresh_process expansion_voyage_changed_command_fresh_process
                       PROPERTIES FIXTURES_REQUIRED expansion_voyage_record
                                  TIMEOUT 900
                                  SKIP_REGULAR_EXPRESSION "skipping: ANNO1503_GAME_DIR not set")

  if(ANNO_SANITIZE AND NOT MSVC)
    set_tests_properties(expansion_voyage_fresh_process expansion_voyage_changed_command_fresh_process
                         PROPERTIES ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
  endif()

  # MAP-ROUTE R6: one genuine producer and five native suffixes supply both CLI arms.
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/automatic-freight")
  set(freight_environment "ANNO_FREIGHT_OUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/automatic-freight")
  if(ANNO_SANITIZE AND NOT MSVC)
    list(APPEND freight_environment "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
  endif()
  configure_file(${PROJECT_SOURCE_DIR}/cmake/FreightVoyageProperties.cmake.in
                 ${CMAKE_CURRENT_BINARY_DIR}/freight_voyage_properties.cmake @ONLY)
  set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
               "${CMAKE_CURRENT_BINARY_DIR}/freight_voyage_properties.cmake")
  add_test(NAME freight_voyage_fresh_process
           COMMAND ${CMAKE_COMMAND} -DPROG=$<TARGET_FILE:sim_replay>
                   "-DARGS=run|${CMAKE_CURRENT_BINARY_DIR}/automatic-freight/freight.log"
                   -DEXPECT_RC=0 "-DEXPECT=checkpoints 11/11.*reproduced: every checkpoint matched"
                   -P ${PROJECT_SOURCE_DIR}/cmake/RunExpansionReplay.cmake)
  add_test(NAME freight_voyage_changed_command_fresh_process
           COMMAND ${CMAKE_COMMAND} -DPROG=$<TARGET_FILE:sim_replay>
                   "-DARGS=run|${CMAKE_CURRENT_BINARY_DIR}/automatic-freight/freight-changed-command.log"
                   -DEXPECT_RC=1 "-DEXPECT=checkpoints 3/4.*DIVERGED at entry 488"
                   -P ${PROJECT_SOURCE_DIR}/cmake/RunExpansionReplay.cmake)
  set_tests_properties(freight_voyage_fresh_process freight_voyage_changed_command_fresh_process
                       PROPERTIES FIXTURES_REQUIRED freight_voyage_record
                                  TIMEOUT 1200
                                  SKIP_REGULAR_EXPRESSION "skipping: ANNO1503_GAME_DIR not set")
  if(ANNO_SANITIZE AND NOT MSVC)
    set_tests_properties(freight_voyage_fresh_process freight_voyage_changed_command_fresh_process
                         PROPERTIES ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
  endif()

  # tools/lint_sim.py: the mechanical part of the determinism rules (no
  # unordered containers, float, clock, rand, mutable globals ...; docs/sim/
  # state.md §14.6), and its own self-test. Named anno_sim_tests_* so that
  # `ctest -R anno_sim_tests` runs them with the sim tests.
  # Fail closed: in CI (the CI environment variable is set) a missing Python 3
  # fails the configuration instead of silently dropping the determinism gate;
  # elsewhere set -DANNO_REQUIRE_LINT=ON to get the same.
  if(DEFINED ENV{CI} AND NOT "$ENV{CI}" STREQUAL "")
    set(anno_require_lint_default ON)
  else()
    set(anno_require_lint_default OFF)
  endif()
  option(ANNO_REQUIRE_LINT "Fail configuration when Python 3 (tools/lint_sim.py) is missing"
         ${anno_require_lint_default})
  if(ANNO_REQUIRE_LINT)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
  else()
    find_package(Python3 COMPONENTS Interpreter)
  endif()
  if(Python3_FOUND)
    add_test(NAME script2_inventory_contract COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/script_inventory2.py --self-test)
    add_test(NAME script2_fresh_process_contract COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/script_canary2.py --self-test)
    add_test(NAME script2_synthetic_fresh_process COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/script_canary2.py --exe $<TARGET_FILE:anno_sim_tests>)
    add_test(NAME script2_original_fresh_process COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/script_canary2.py --original --exe $<TARGET_FILE:anno_sim_tests>)
    # Both modes execute the original endless01 program; synthetic refers to the world.
    set_tests_properties(script2_synthetic_fresh_process script2_original_fresh_process
                         PROPERTIES SKIP_REGULAR_EXPRESSION "skipping: ANNO1503_GAME_DIR not set")
    # Accepted UNIT1 original sanitizer canary measured581.97s. The combined
    # fresh-process contract gets1500s; the unrelated generated opening is excluded.
    set_tests_properties(script2_original_fresh_process PROPERTIES TIMEOUT 1500)
    add_test(NAME script2_stream_reference_contract COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/sim_stream_script2.py --self-test)
    add_test(NAME script2_stream_reference_agrees COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/sim_stream_script2.py --check)
    add_test(NAME script2_reference_contract COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/script_runtime2_ref.py --self-test)
    add_test(NAME script2_reference_agrees COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/script_runtime2_ref.py --check ${PROJECT_SOURCE_DIR}/fixtures/synthetic/sim/script2/handler_vectors.json)
    add_test(NAME integration_stream_reference_contract COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/integration_stream_ref.py --self-test)
    add_test(NAME integration_stream_reference_agrees COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/integration_stream_ref.py)
    add_test(NAME script1_stream_reference_contract
             COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/sim_stream_script1.py --self-test)
    add_test(NAME script1_stream_reference_agrees
             COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/sim_stream_script1.py --check)
    add_test(NAME script1_fresh_process_contract
             COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/script_canary.py --self-test)
    add_test(NAME script1_fresh_process
             COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/script_canary.py --exe $<TARGET_FILE:anno_sim_tests>)
    add_test(NAME bootstrap_budget_contract
             COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/bootstrap_budget.py --self-test)
    add_test(NAME bootstrap_budget_original
             COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/bootstrap_budget.py
                     --check ${PROJECT_SOURCE_DIR}/fixtures/synthetic/sim/bootstrap_budget.json)
    add_test(NAME anno_sim_tests_lint_sim_passes
             COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/lint_sim.py
                     --root ${PROJECT_SOURCE_DIR})
    add_test(NAME anno_sim_tests_lint_sim_self_test
             COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/lint_sim.py --self-test)
  else()
    message(WARNING "python3 not found: the lint_sim tests (the determinism gate) are NOT "
                    "registered; pass -DANNO_REQUIRE_LINT=ON to make this an error")
  endif()

  # The sanitizer gate's probe (asan preset only): a deliberate signed
  # overflow. cmake/RunUbsanProbe.cmake runs it and requires that it printed
  # its start marker, exited non-zero AND printed UBSan's signed-overflow
  # report, so a crash, a missing runtime or a link problem cannot pass for
  # a working gate (-fno-sanitize-recover=all, cmake/AnnoWarnings.cmake).
  if(ANNO_SANITIZE AND NOT MSVC)
    add_executable(anno_sim_ubsan_probe tests/ubsan_probe.cpp)
    anno_target_defaults(anno_sim_ubsan_probe SANITIZE_NO_RECOVER)
    add_test(NAME anno_sim_tests_ubsan_probe_is_caught_by_the_asan_gate
             COMMAND ${CMAKE_COMMAND} -DPROBE=$<TARGET_FILE:anno_sim_ubsan_probe>
                     -P ${PROJECT_SOURCE_DIR}/cmake/RunUbsanProbe.cmake)
  endif()
endif()
