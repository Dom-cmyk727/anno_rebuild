# Runs a command-line tool and checks its exit status AND its output together (task
# MAP-FOUND-3 package B). Used for refusals (EXPECT_RC non-zero) and for expected
# successes (EXPECT_RC=0) and divergences alike, because CTest's PASS_REGULAR_EXPRESSION
# ignores the exit status. The contract of this helper is itself tested:
# cmake/CheckToolCheckContract.cmake (ctest anno_sim_replay_check_contract).
#   cmake -DPROG=<exe> -DARGS=<a|b|c> -DEXPECT_RC=<n> -DEXPECT=<regex> -P RunToolExpectError.cmake
# ARGS separates the arguments with '|' (a ';' would split the -D value). Passes only
# when the tool exited with EXPECT_RC and its output matches EXPECT, so an uncaught
# exception (std::terminate: an abort with another exit status and without the
# tool's own message) fails the test.
if(NOT PROG OR NOT DEFINED EXPECT_RC OR NOT EXPECT)
  message(FATAL_ERROR "RunToolExpectError.cmake: pass -DPROG, -DEXPECT_RC and -DEXPECT")
endif()
string(REPLACE "|" ";" args "${ARGS}")
execute_process(COMMAND "${PROG}" ${args}
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
set(all "${out}${err}")
if(NOT rc STREQUAL "${EXPECT_RC}")
  message(FATAL_ERROR "exit status ${rc}, expected ${EXPECT_RC}:\n${all}")
endif()
if(NOT all MATCHES "${EXPECT}")
  message(FATAL_ERROR "the output does not match '${EXPECT}':\n${all}")
endif()
message(STATUS "exit status ${rc} and output as expected")
