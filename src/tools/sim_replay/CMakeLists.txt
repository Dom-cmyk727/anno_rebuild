# anno-sim-replay: drive and diagnose a simulation replay log (docs/sim/state.md
# §8.7, §15 S9). A thin front end over anno_sim's replay API.
add_executable(sim_replay main.cpp)
set_target_properties(sim_replay PROPERTIES OUTPUT_NAME anno-sim-replay)
target_link_libraries(sim_replay PRIVATE anno_sim)
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
  # the figure registry's expiry and deferred lists, the monopoly table and the 7.5 minute
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
endif()
