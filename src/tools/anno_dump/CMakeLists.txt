# anno-dump: inspect original files; its text output is the language-neutral
# contract shared with the Python references in tools/ (ADR 0001).
add_executable(anno_dump main.cpp)
set_target_properties(anno_dump PROPERTIES OUTPUT_NAME anno-dump)
target_link_libraries(anno_dump PRIVATE anno_formats)
anno_target_defaults(anno_dump)

# Agreement with the Python references (tools/agree.py): the synthetic
# fixtures always, the original corpus when ANNO1503_GAME_DIR is set. Grows
# with each ported command.
set(ANNO_DUMP_PORTED_COMMANDS "chunks" CACHE STRING "anno-dump commands checked by tools/agree.py")
if(ANNO_BUILD_TESTS)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_FOUND)
    add_test(NAME anno_dump_agrees_with_python
      COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/agree.py
              --dump $<TARGET_FILE:anno_dump> --commands ${ANNO_DUMP_PORTED_COMMANDS})
  else()
    message(STATUS "python3 not found: anno_dump_agrees_with_python not registered")
  endif()
endif()
