# The contract of RunToolExpectError.cmake (task MAP-FOUND-3 package B): it must pass only
# when the child's exit status equals EXPECT_RC AND its output matches EXPECT, and it must fail
# - with the specific message - on every other combination. This script runs the helper as a
# child (a fake child is a tiny cmake script that prints a line and exits 0 or 1) and asserts the
# helper's own exit status (1 on failure, 0 on success) together with its message, so a helper
# that is lenient in either direction fails THIS test. It uses no CTest property that ignores the
# exit status.
#   cmake -DHELPER=<RunToolExpectError.cmake> -DFAKE=<FakeToolChild.cmake> -P CheckToolCheckContract.cmake
if(NOT HELPER OR NOT FAKE)
  message(FATAL_ERROR "pass -DHELPER and -DFAKE")
endif()

set(failures "")
set(cases 0)

# run_case(<name> <prog> <args '|'-separated> <expect_rc> <expect regex> <helper_rc> <message regex>)
function(run_case name prog args expect_rc expect helper_rc message)
  execute_process(COMMAND ${CMAKE_COMMAND} "-DPROG=${prog}" "-DARGS=${args}"
                          "-DEXPECT_RC=${expect_rc}" "-DEXPECT=${expect}" -P "${HELPER}"
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  set(all "${out}${err}")
  set(bad "")
  if(NOT rc STREQUAL "${helper_rc}")
    string(APPEND bad " helper exit ${rc}, wanted ${helper_rc};")
  endif()
  if(NOT all MATCHES "${message}")
    string(APPEND bad " helper message lacks '${message}';")
  endif()
  math(EXPR n "${cases} + 1")
  set(cases ${n} PARENT_SCOPE)
  if(bad)
    set(failures "${failures}\n[${name}]${bad}\n${all}" PARENT_SCOPE)
  endif()
endfunction()

set(P "${CMAKE_COMMAND}")
function(fake text rc out)
  set(${out} "-DTEXT=${text}|-DRC=${rc}|-P|${FAKE}" PARENT_SCOPE)
endfunction()

# Right status and right text: accepted, in both polarities (success 0, divergence 1).
fake("checkpoints 4/4 reproduced" 0 f)
run_case("ok_success" "${P}" "${f}" 0 "checkpoints 4/4" 0 "as expected")
fake("checkpoints 1/2 DIVERGED at entry 3" 1 f)
run_case("ok_divergence" "${P}" "${f}" 1 "DIVERGED at entry 3" 0 "as expected")

# Right text, wrong status, both directions.
fake("checkpoints 1/2 DIVERGED at entry 3" 0 f)
run_case("text_ok_status_0_expected_1" "${P}" "${f}" 1 "DIVERGED at entry 3" 1
         "exit status 0, expected 1")
fake("checkpoints 4/4 reproduced" 1 f)
run_case("text_ok_status_1_expected_0" "${P}" "${f}" 0 "checkpoints 4/4" 1
         "exit status 1, expected 0")

# Right status, wrong text, both polarities.
fake("something else entirely" 0 f)
run_case("status_ok_text_wrong_success" "${P}" "${f}" 0 "checkpoints 4/4" 1
         "does not match")
fake("something else entirely" 1 f)
run_case("status_ok_text_wrong_divergence" "${P}" "${f}" 1 "DIVERGED at entry 3" 1
         "does not match")

# Missing output (a child that prints nothing), both polarities.
fake("" 0 f)
run_case("no_output_success" "${P}" "${f}" 0 "checkpoints 4/4" 1 "does not match")
fake("" 1 f)
run_case("no_output_divergence" "${P}" "${f}" 1 "DIVERGED" 1 "does not match")

# A child that cannot be launched is never a pass, whatever the expected status.
run_case("launch_failure_expect_0" "${CMAKE_CURRENT_LIST_DIR}/no-such-program-xyz" "a" 0 "x" 1
         "expected 0")
run_case("launch_failure_expect_1" "${CMAKE_CURRENT_LIST_DIR}/no-such-program-xyz" "a" 1 "x" 1
         "expected 1")

# An empty expectation is a configuration error, not a pass.
run_case("empty_expectation" "${P}" "-E|true" 0 "" 1 "pass -DPROG")

if(failures)
  message(FATAL_ERROR "tool-check contract violated (${cases} cases):${failures}")
endif()
message(STATUS "tool-check contract: ${cases} cases passed")
