# A fake child for CheckToolCheckContract.cmake: prints TEXT on stdout (when non-empty) and
# exits 0, or exits 1 when RC is 1 (message(FATAL_ERROR) is cmake -P's only non-zero status).
if(TEXT)
  message(STATUS "${TEXT}")
endif()
if(RC STREQUAL "1")
  message(FATAL_ERROR "fake child failing as asked")
endif()
