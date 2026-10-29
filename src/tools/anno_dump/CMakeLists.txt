# anno-dump: inspect original files; its text output is the language-neutral
# contract shared with the Python references in tools/ (ADR 0001).
add_executable(anno_dump main.cpp dump.cpp dump_tex.cpp dump_figuren.cpp dump_insel.cpp dump_sze.cpp dump_text.cpp
  dump_bigbrother.cpp dump_annowelt.cpp)
set_target_properties(anno_dump PROPERTIES OUTPUT_NAME anno-dump)
target_link_libraries(anno_dump PRIVATE anno_formats)
anno_target_defaults(anno_dump)

# Agreement with the Python references (tools/agree.py): the synthetic
# fixtures always, the original corpus when ANNO1503_GAME_DIR is set. Grows
# with each ported command.
set(ANNO_DUMP_PORTED_COMMANDS "chunks,tex,inc,colors,figuren,frames,anlagen,insel,sze,keyed,texte,zei,bigbrother,walk-tables,fixed-map" CACHE STRING "anno-dump commands checked by tools/agree.py")
if(ANNO_BUILD_TESTS)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_FOUND)
    add_test(NAME anno_dump_agrees_with_python
      COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/agree.py
              --dump $<TARGET_FILE:anno_dump> --commands ${ANNO_DUMP_PORTED_COMMANDS})
    # Not "heavy": alone it raises the commit charge by about 0.7 GB (one
    # anno-dump at a time, measured 2026-10-23, T10b). Its bad_alloc under
    # `ctest -j 16` came from the heavy render cases beside it, which now
    # run alone (anno_add_doctest); a slot reservation here would only add
    # its 40 s to the serial part of the run.
  else()
    message(STATUS "python3 not found: anno_dump_agrees_with_python not registered")
  endif()
endif()
