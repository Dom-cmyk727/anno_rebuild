# Runs `anno-sim-replay scenario` on one scenario of the game directory:
#   cmake -DPROG=<exe> -DSCENE=<path relative to Scenes> -DTICKS=<n> -P RunScenarioTool.cmake
# Skips (the "skipping:" line, SKIP_REGULAR_EXPRESSION) when ANNO1503_GAME_DIR is not
# set; otherwise passes only when the tool exits 0 and reports clean invariants.
if(NOT PROG OR NOT SCENE OR NOT DEFINED TICKS)
  message(FATAL_ERROR "RunScenarioTool.cmake: pass -DPROG, -DSCENE and -DTICKS")
endif()
if("$ENV{ANNO1503_GAME_DIR}" STREQUAL "")
  message("skipping: ANNO1503_GAME_DIR not set")
  return()
endif()
execute_process(COMMAND "${PROG}" scenario "$ENV{ANNO1503_GAME_DIR}/Scenes/${SCENE}" --ticks ${TICKS}
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc STREQUAL "0")
  message(FATAL_ERROR "exit status ${rc}:\n${out}${err}")
endif()
if(NOT out MATCHES "check_invariants clean")
  message(FATAL_ERROR "no clean check_invariants:\n${out}${err}")
endif()
message(STATUS "${out}")
