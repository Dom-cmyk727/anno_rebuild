# river_qa: gap and connectivity measurement of enhanced river water (see main.cpp).
add_executable(river_qa main.cpp shade.cpp)
target_link_libraries(river_qa PRIVATE anno_app_core)
anno_target_defaults(river_qa)

# Gap patch survey (tools/river_qa_gap.py): the pure functions always, the tool itself when
# ANNO1503_GAME_DIR is set (skipped otherwise).
if(ANNO_BUILD_TESTS)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_FOUND)
    add_test(NAME river_qa_gap_survey
      COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/test_river_qa_gap.py
              --exe $<TARGET_FILE:river_qa>)
  endif()
endif()
