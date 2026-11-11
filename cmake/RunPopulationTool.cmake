# Runs `anno-sim-replay population` on one scenario of the game directory (W4-NEXT-2 X):
#   cmake -DPROG=<exe> -DSCENE=<path relative to Scenes> [-DEXPECT=<regex>]
#         -P RunPopulationTool.cmake
# Skips (the "skipping:" line, SKIP_REGULAR_EXPRESSION) when ANNO1503_GAME_DIR is not set;
# requires exit 0 and the default four checkpoints (t=0s/10s/30s/60s), each with a
# residents= and town_store_tons= field.
if(NOT PROG OR NOT SCENE)
  message(FATAL_ERROR "RunPopulationTool.cmake: pass -DPROG and -DSCENE")
endif()
if("$ENV{ANNO1503_GAME_DIR}" STREQUAL "")
  message("skipping: ANNO1503_GAME_DIR not set")
  return()
endif()
execute_process(COMMAND "${PROG}" population "$ENV{ANNO1503_GAME_DIR}/Scenes/${SCENE}"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "exit status ${rc}:\n${out}${err}")
endif()
foreach(tag t=0s t=10s t=30s t=60s)
  if(NOT out MATCHES "${tag} tick=[0-9]+ residents=[0-9]+ town_store_tons=[0-9]+")
    message(FATAL_ERROR "missing or malformed '${tag}' checkpoint:\n${out}${err}")
  endif()
endforeach()
if(DEFINED EXPECT AND NOT out MATCHES "${EXPECT}")
  message(FATAL_ERROR "the output does not match '${EXPECT}':\n${out}${err}")
endif()
message(STATUS "${out}")
