# cmake -DSOURCE_DIR=<repo> -P CheckTestNames.cmake: fails when a doctest
# TEST_CASE name contains a character that breaks its registration as a CTest
# test (anno_add_doctest): ';' splits the name as a CMake list, and '*', '?',
# '[', ']', '\' act as doctest filter patterns. A broken name runs no test
# case and still passes (found 2026-10-18), so it is checked here.
file(GLOB_RECURSE sources "${SOURCE_DIR}/src/*.cpp")
set(bad "")
foreach(f ${sources})
  file(STRINGS "${f}" lines REGEX "TEST_CASE\\(\"")
  foreach(line IN LISTS lines)
    string(REGEX MATCH "TEST_CASE\\(\"[^\"]*\"" name "${line}")
    if(name MATCHES "[*?\\\\]" OR name MATCHES "\\[" OR name MATCHES "\\]")
      list(APPEND bad "${f}: ${name}")
    endif()
  endforeach()
  # file(STRINGS) splits at ';', so look for it in the raw text.
  file(READ "${f}" text)
  string(REGEX MATCHALL "TEST_CASE\\(\"[^\"\n]*;[^\"\n]*\"" semis "${text}")
  if(semis)
    list(APPEND bad "${f}: a TEST_CASE name with ';'")
  endif()
endforeach()
if(bad)
  string(REPLACE ";" "\n" msg "${bad}")
  message(FATAL_ERROR "test names that CTest/doctest cannot select:\n${msg}")
endif()
message(STATUS "test names ok")
