# Records the salt chain (the recording doctest case writes salt-chain.log and salt-chain-changed-command.log
# into ANNO_SALT_OUT_DIR) and replays one of the logs with anno-sim-replay in a fresh process (MAP-EXTRACT-1 X5).
# Skips explicitly when the original game files are not available (the chain needs the original registry).
# Delegates the exit status and output check to RunToolExpectError.cmake.
#   cmake -DPROG=<sim_replay> -DRECORDER=<anno_sim_tests> -DCASE=<test case> -DOUT_DIR=<dir> -DLOG=<file name>
#         -DEXPECT_RC=<n> -DEXPECT=<regex> -P RunSaltReplay.cmake
if("$ENV{ANNO1503_GAME_DIR}" STREQUAL "")
  message(STATUS "skipping: ANNO1503_GAME_DIR not set")
  return()
endif()
if(NOT PROG OR NOT RECORDER OR NOT CASE OR NOT OUT_DIR OR NOT LOG)
  message(FATAL_ERROR "RunSaltReplay.cmake: pass -DPROG, -DRECORDER, -DCASE, -DOUT_DIR and -DLOG")
endif()
file(REMOVE "${OUT_DIR}/salt-chain.log" "${OUT_DIR}/salt-chain-changed-command.log")
set(ENV{ANNO_SALT_OUT_DIR} "${OUT_DIR}")
execute_process(COMMAND "${RECORDER}" "--test-case=${CASE}"
                RESULT_VARIABLE record_rc OUTPUT_VARIABLE record_out ERROR_VARIABLE record_err)
if(NOT record_rc EQUAL 0 OR NOT EXISTS "${OUT_DIR}/${LOG}")
  message(FATAL_ERROR "recording failed (exit ${record_rc}):\n${record_out}${record_err}")
endif()
set(ARGS "run|${OUT_DIR}/${LOG}")
include("${CMAKE_CURRENT_LIST_DIR}/RunToolExpectError.cmake")
