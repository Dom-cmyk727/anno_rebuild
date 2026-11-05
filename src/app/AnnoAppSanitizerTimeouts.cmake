# Read by CTest after the discovery of anno_app_tests' cases (src/app/CMakeLists.txt appends it to
# TEST_INCLUDE_FILES in a sanitizer build only). The two full-corpus cases are in the "heavy" suite
# (play_test.cpp); under ASan+UBSan alone each needs about 5100-5400 s and 2300 s (W4-WIN-CLOSE C3),
# above the gate's default of 3400 s, so they get an 8000 s per-test timeout. A property of these two
# stable names only: no other case, no global timeout and no scenario is reduced. The names are looked
# up in the heavy cases the discovery recorded (anno_app_tests_HEAVY_TESTS, the list the heavy label is
# set on), because CTest's script reader does not evaluate if(TEST ...).
foreach(name
    "anno_app_tests.play: islands are the ones the sim placed"
    "anno_app_tests.play: tick 0 matches the viewer except objects the sim did not place")
  # IN_LIST needs policy CMP0057, which CTest's script-mode if() reader (used while
  # including this file during test discovery) does not set; list(FIND) has no such
  # policy dependency.
  list(FIND anno_app_tests_HEAVY_TESTS "${name}" anno_app_tests_heavy_index)
  if(anno_app_tests_heavy_index GREATER -1)
    set_tests_properties("${name}" PROPERTIES TIMEOUT 8000)
  endif()
endforeach()
