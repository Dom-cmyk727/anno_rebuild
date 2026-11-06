# Asset-bound consumer: skip absence explicitly, then reuse the existing exit+output validator.
# A nonempty but invalid game directory still runs and fails; it is never an asset-absence skip.
if("$ENV{ANNO1503_GAME_DIR}" STREQUAL "")
  message("skipping: ANNO1503_GAME_DIR not set")
  return()
endif()
include("${CMAKE_CURRENT_LIST_DIR}/RunToolExpectError.cmake")
