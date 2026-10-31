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
  # file(STRINGS) splits at ';', so look for it in the raw text, over the whole name: a name may
  # start on the line after TEST_CASE( and continue over several adjacent literals (round dz: such
  # a name with ';' was not found by the line check above and ran no test case). The ';' is
  # replaced first, so the match list is not split at it.
  file(READ "${f}" text)
  string(REPLACE ";" "<semicolon>" text "${text}")
  string(REGEX MATCHALL "TEST_CASE\\([ \t\r\n]*(\"[^\"\n]*\"[ \t\r\n]*)+" names "${text}")
  foreach(name IN LISTS names)
    if(name MATCHES "<semicolon>")
      string(REGEX REPLACE "[ \t\r\n]+" " " shown "${name}")
      string(REPLACE "<semicolon>" " (semicolon) " shown "${shown}")
      list(APPEND bad "${f}: a TEST_CASE name with a semicolon: ${shown}")
    elseif(name MATCHES "[*?\\]" OR name MATCHES "\\[" OR name MATCHES "\\]")
      string(REGEX REPLACE "[ \t\r\n]+" " " shown "${name}")
      list(APPEND bad "${f}: ${shown}")
    endif()
  endforeach()
endforeach()
if(bad)
  string(REPLACE ";" "\n" msg "${bad}")
  message(FATAL_ERROR "test names that CTest/doctest cannot select:\n${msg}")
endif()
message(STATUS "test names ok")
