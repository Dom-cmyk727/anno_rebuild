# Runs the fresh-process replay of a recorded salt chain (MAP-EXTRACT-1 X5), or skips explicitly when the original
# game files are not available (the chain needs the original registry, and the recording case writes nothing
# without them). Delegates to RunToolExpectError.cmake, so the exit status and the output are both checked.
#   cmake -DPROG=<exe> -DARGS=<a|b> -DEXPECT_RC=<n> -DEXPECT=<regex> -P RunSaltReplay.cmake
if("$ENV{ANNO1503_GAME_DIR}" STREQUAL "")
  message(STATUS "skipping: ANNO1503_GAME_DIR not set")
  return()
endif()
include("${CMAKE_CURRENT_LIST_DIR}/RunToolExpectError.cmake")
