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
endif()
