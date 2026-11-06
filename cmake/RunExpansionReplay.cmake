# Asset-bound consumer: skip absence explicitly, then reuse the existing exit+output validator.
# A nonempty but invalid game directory still runs and fails; it is never an asset-absence skip.
if("$ENV{ANNO1503_GAME_DIR}" STREQUAL "")
  message("skipping: ANNO1503_GAME_DIR not set")
  return()
endif()
include("${CMAKE_CURRENT_LIST_DIR}/RunToolExpectError.cmake")
# Keep actual child stdout AND stderr in the canonical raw log, including checkpoint counts
# and sanitizer diagnostics even when the expected divergence exit code is1.
message(STATUS "${all}")
