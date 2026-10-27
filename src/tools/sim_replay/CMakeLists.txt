# anno-sim-replay: drive and diagnose a simulation replay log (docs/sim/state.md
# §8.7, §15 S9). A thin front end over anno_sim's replay API.
add_executable(sim_replay main.cpp)
set_target_properties(sim_replay PROPERTIES OUTPUT_NAME anno-sim-replay)
target_link_libraries(sim_replay PRIVATE anno_sim anno_sim_load)
anno_target_defaults(sim_replay)

if(ANNO_BUILD_TESTS)
  # Numeric flags the tool must refuse with a usage error and exit status 2, never
  # with an uncaught exception (T8 review item 6). The flags are parsed before the
  # log is opened, so the log path does not need to exist; the last case is a valid
  # number that gets past the parse and fails on the missing log instead.
  function(anno_sim_replay_arg_test name args expect)
    add_test(NAME anno_sim_replay_args_${name}
             COMMAND ${CMAKE_COMMAND} -DPROG=$<TARGET_FILE:sim_replay> "-DARGS=${args}"
                     -DEXPECT_RC=2 "-DEXPECT=${expect}"
                     -P ${PROJECT_SOURCE_DIR}/cmake/RunToolExpectError.cmake)
  endfunction()
  anno_sim_replay_arg_test(entry_not_a_number "tree|no.log|--entry|abc" "bad --entry value 'abc'")
  anno_sim_replay_arg_test(entry_negative "tree|no.log|--entry|-1" "bad --entry value '-1'")
  anno_sim_replay_arg_test(entry_trailing "tree|no.log|--entry|12abc" "bad --entry value '12abc'")
  anno_sim_replay_arg_test(entry_overflow "tree|no.log|--entry|99999999999999999999999"
                           "bad --entry value")
  anno_sim_replay_arg_test(depth_not_a_number "tree|no.log|--depth|x" "bad --depth value 'x'")
  anno_sim_replay_arg_test(depth_beyond_u32 "tree|no.log|--depth|4294967296"
                           "bad --depth value '4294967296'")
  anno_sim_replay_arg_test(depth_negative "tree|no.log|--depth|-3" "bad --depth value '-3'")
  anno_sim_replay_arg_test(entry_valid_reaches_the_log "tree|no-such.log|--entry|3|--depth|2"
                           "cannot read no-such.log")
  # The golden G2 (docs/sim/state.md §14.3, stage S10): the tool rebuilds the world from
  # the committed log on the registry its digest names and reproduces every checkpoint.
  add_test(NAME anno_sim_replay_g2_run
           COMMAND sim_replay run ${PROJECT_SOURCE_DIR}/fixtures/synthetic/sim/g2.log)
  set_tests_properties(anno_sim_replay_g2_run PROPERTIES
                       PASS_REGULAR_EXPRESSION "checkpoints 6/6.*reproduced: every checkpoint matched")
  # The golden G3 (stage S11): every production class through advance on the synthetic
  # types, 21 checkpoints.
  add_test(NAME anno_sim_replay_g3_run
           COMMAND sim_replay run ${PROJECT_SOURCE_DIR}/fixtures/synthetic/sim/g3.log)
  set_tests_properties(anno_sim_replay_g3_run PROPERTIES
                       PASS_REGULAR_EXPRESSION "checkpoints 21/21.*reproduced: every checkpoint matched")
  # The golden G4 (stage S12): players with upkeep and bankruptcy, timers, timed events,
  # the figure registry's expiry list, the monopoly table and the 7.5 minute
  # event over 600 000 ticks, 21 checkpoints.
  add_test(NAME anno_sim_replay_g4_run
           COMMAND sim_replay run ${PROJECT_SOURCE_DIR}/fixtures/synthetic/sim/g4.log)
  set_tests_properties(anno_sim_replay_g4_run PROPERTIES
                       PASS_REGULAR_EXPRESSION "checkpoints 21/21.*reproduced: every checkpoint matched")
  # The golden G5 (stage S13): the growth sweep and two droughts of the 300 s event over
  # 1 000 000 ticks on the growth types, 21 checkpoints.
  add_test(NAME anno_sim_replay_g5_run
           COMMAND sim_replay run ${PROJECT_SOURCE_DIR}/fixtures/synthetic/sim/g5.log)
  set_tests_properties(anno_sim_replay_g5_run PROPERTIES
                       PASS_REGULAR_EXPRESSION "checkpoints 21/21.*reproduced: every checkpoint matched")
  # The golden G6 (task T24): a market cart and a workshop carrier over 300 000 ticks on
  # the transport types, 21 checkpoints.
  add_test(NAME anno_sim_replay_g6_run
           COMMAND sim_replay run ${PROJECT_SOURCE_DIR}/fixtures/synthetic/sim/g6.log)
  set_tests_properties(anno_sim_replay_g6_run PROPERTIES
                       PASS_REGULAR_EXPRESSION "checkpoints 21/21.*reproduced: every checkpoint matched")
  # The scenario adapter (stage S14): the usage and the refusal of a file that is not a
  # scenario need no game files; one shipped scenario runs when ANNO1503_GAME_DIR is set.
  anno_sim_replay_arg_test(scenario_ticks_not_a_number "scenario|x.sze|--ticks|abc"
                           "bad --ticks value 'abc'")
  anno_sim_replay_arg_test(scenario_step_zero "scenario|x.sze|--step-ms|0" "bad --step-ms value '0'")
  anno_sim_replay_arg_test(scenario_no_file "scenario" "usage: anno-sim-replay")
  add_test(NAME anno_sim_replay_scenario_refuses_a_non_scenario
           COMMAND ${CMAKE_COMMAND} -DPROG=$<TARGET_FILE:sim_replay>
                   "-DARGS=scenario|${PROJECT_SOURCE_DIR}/fixtures/synthetic/sze_synthetic/noscene.sze"
                   -DEXPECT_RC=1 "-DEXPECT=refused: not a scenario"
                   -P ${PROJECT_SOURCE_DIR}/cmake/RunToolExpectError.cmake)
  add_test(NAME anno_sim_replay_scenario_single_01
           COMMAND ${CMAKE_COMMAND} -DPROG=$<TARGET_FILE:sim_replay> -DSCENE=0200Single/Single_01.sze
                   -DTICKS=20000 -P ${PROJECT_SOURCE_DIR}/cmake/RunScenarioTool.cmake)
  set_tests_properties(anno_sim_replay_scenario_single_01 PROPERTIES
                       SKIP_REGULAR_EXPRESSION "skipping: ANNO1503_GAME_DIR not set")
  # S14 stage 2: a scenario whose islands all come from links (27 INSELLNK) resolves
  # every link over the game's templates and runs five minutes clean.
  add_test(NAME anno_sim_replay_scenario_endless_03_links
           COMMAND ${CMAKE_COMMAND} -DPROG=$<TARGET_FILE:sim_replay>
                   -DSCENE=0010SingleEndlos/03_1024_med_256.sze -DTICKS=150000
                   "-DEXPECT=links 27 links_resolved 27 links_unresolved 0"
                   -P ${PROJECT_SOURCE_DIR}/cmake/RunScenarioTool.cmake)
  set_tests_properties(anno_sim_replay_scenario_endless_03_links PROPERTIES
                       SKIP_REGULAR_EXPRESSION "skipping: ANNO1503_GAME_DIR not set")
  # T41: the measure line names every timed field. A synthetic scenario, R = 2,
  # no game files. --ticks 0 still creates the world and hashes it.
  add_test(NAME anno_sim_replay_measure_synthetic
           COMMAND sim_replay measure synthetic --ticks 0 --reps 2)
  set_tests_properties(anno_sim_replay_measure_synthetic PROPERTIES
                       PASS_REGULAR_EXPRESSION
                       "hash_clean_us min=.*median=.*p95=.*max=.*hash_incremental_us min=.*median=.*p95=.*max=.*save_bytes=.*tiles=.*chunks=")
  anno_sim_replay_arg_test(measure_reps_not_a_number "measure|x.sze|--reps|abc"
                           "bad --reps value 'abc'")
  anno_sim_replay_arg_test(measure_reps_zero "measure|x.sze|--reps|0" "bad --reps value '0'")
  anno_sim_replay_arg_test(measure_ticks_negative "measure|x.sze|--ticks|-1"
                           "bad --ticks value '-1'")
  # T45: load and run_turn on the code-built world. The test checks the
  # validation line and the turn mapping, not a wall-time threshold.
  add_test(NAME anno_sim_replay_join_synthetic
           COMMAND sim_replay join synthetic --ticks 0 --reps 2 --warmup 1 --catchup 3
                   --replay-reps 2)
  set_tests_properties(anno_sim_replay_join_synthetic PROPERTIES
                       PASS_REGULAR_EXPRESSION
                       "join_ok validation=ok replay_method=run_turn age_method=advance ticks_per_turn=50 commands_rejected=0 speed_bad=0 burst_over_cap=1 empty_payload_bytes=0 scripted_commands=1")
  anno_sim_replay_arg_test(join_reps_not_a_number "join|synthetic|--reps|abc"
                           "bad --reps value 'abc'")
  anno_sim_replay_arg_test(join_reps_zero "join|synthetic|--reps|0" "bad --reps value '0'")
  anno_sim_replay_arg_test(join_warmup_negative "join|synthetic|--warmup|-1"
                           "bad --warmup value '-1'")
  anno_sim_replay_arg_test(join_catchup_zero "join|synthetic|--catchup|0"
                           "bad --catchup value '0'")
  anno_sim_replay_arg_test(join_replay_reps_zero "join|synthetic|--replay-reps|0"
                           "bad --replay-reps value '0'")
  anno_sim_replay_arg_test(join_ticks_negative "join|synthetic|--ticks|-1"
                           "bad --ticks value '-1'")
  anno_sim_replay_arg_test(join_no_path "join" "usage: anno-sim-replay")
  add_test(NAME anno_sim_replay_join_missing
           COMMAND ${CMAKE_COMMAND} -DPROG=$<TARGET_FILE:sim_replay>
                   "-DARGS=join|no-such.sze|--ticks|0|--reps|1|--warmup|0|--catchup|1|--replay-reps|1"
                   -DEXPECT_RC=1 "-DEXPECT=cannot read no-such.sze"
                   -P ${PROJECT_SOURCE_DIR}/cmake/RunToolExpectError.cmake)
  add_test(NAME anno_sim_replay_join_refuses_a_non_scenario
           COMMAND ${CMAKE_COMMAND} -DPROG=$<TARGET_FILE:sim_replay>
                   "-DARGS=join|${PROJECT_SOURCE_DIR}/fixtures/synthetic/sze_synthetic/noscene.sze|--ticks|0|--reps|1|--warmup|0|--catchup|1|--replay-reps|1"
                   -DEXPECT_RC=1 "-DEXPECT=validation=fail"
                   -P ${PROJECT_SOURCE_DIR}/cmake/RunToolExpectError.cmake)
endif()
