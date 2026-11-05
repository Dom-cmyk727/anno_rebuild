# Two public CLI boundaries: the same non-scenario bytes inside and outside Scenes.
# Own a unique build-local directory; never copy into the original game directory.
if(NOT PROG OR NOT FIXTURE OR NOT TEST_ROOT)
  message(FATAL_ERROR "pass PROG, FIXTURE and TEST_ROOT")
endif()
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef suffix)
set(root "${TEST_ROOT}/scenario-refusal-${suffix}")
file(MAKE_DIRECTORY "${root}/Scenes/synthetic")
file(COPY_FILE "${FIXTURE}" "${root}/Scenes/synthetic/noscene.sze")
file(COPY_FILE "${FIXTURE}" "${root}/noscene.sze")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "ANNO1503_GAME_DIR=${root}"
                "${PROG}" scenario "${root}/Scenes/synthetic/noscene.sze"
                RESULT_VARIABLE inside_rc OUTPUT_VARIABLE inside ERROR_VARIABLE inside_err)
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "ANNO1503_GAME_DIR=${root}"
                "${PROG}" scenario "${root}/noscene.sze"
                RESULT_VARIABLE outside_rc OUTPUT_VARIABLE outside ERROR_VARIABLE outside_err)
file(REMOVE_RECURSE "${root}")
if(NOT inside_rc STREQUAL "1" OR NOT inside MATCHES "refused: not a scenario")
  message(FATAL_ERROR "inside did not reach the content refusal: ${inside_rc} ${inside}${inside_err}")
endif()
if(NOT outside_rc STREQUAL "1" OR NOT outside MATCHES "refused: scenario file lies outside Scenes")
  message(FATAL_ERROR "outside did not reach the namespace refusal: ${outside_rc} ${outside}${outside_err}")
endif()
message(STATUS "inside: content refused; outside: namespace refused")
