# bench_picture: the window-side cost of the building picture (W4.3, round dv; docs/perf/README.md).
add_executable(bench_picture main.cpp)
target_link_libraries(bench_picture PRIVATE anno_app_core)
# --gpu 1 (round eb): the GPU compositor, when it is built.
if(TARGET anno_render_gpu)
  target_link_libraries(bench_picture PRIVATE anno_render_gpu)
  target_compile_definitions(bench_picture PRIVATE ANNO_BENCH_GPU)
endif()
anno_target_defaults(bench_picture)
if(ANNO_BUILD_TESTS AND UNIX)
  # The matrix runner's and the aggregate's contracts (round dz, 1f): no game files, seconds.
  add_test(NAME picture_matrix_contract
    COMMAND bash ${PROJECT_SOURCE_DIR}/tools/bench/picture_matrix_test.sh $<TARGET_FILE:bench_picture>)
  # The window matrix table's contract (round ec, D): a synthetic window case and its faults.
  add_test(NAME window_table_contract
    COMMAND bash ${PROJECT_SOURCE_DIR}/tools/bench/window_table_test.sh $<TARGET_FILE:bench_picture>)
endif()
if(ANNO_BUILD_TESTS AND WIN32 AND Python3_FOUND)
  # The native Windows window tools' public-command contracts (W4-WIN-CLOSE C4): watchdog, an
  # unreadable peak and a refused capture, each with a control, against tiny fixtures (the UNIX
  # window tools have the shell contracts above). The capture cases need Pillow and a desktop and
  # report a skip without them.
  add_test(NAME window_tools_contract
    COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/window_tools_contract_test.py)
  set_tests_properties(window_tools_contract PROPERTIES TIMEOUT 300)
endif()
if(ANNO_BUILD_TESTS AND Python3_FOUND)
  # tools/test/viewer_wiring.py's verdict (W4-WIN-FINAL F1): a child that exits 0 passes only when the
  # session record shows the invoked script ran. Tiny fixtures, no game files, every platform.
  add_test(NAME viewer_wiring_contract
    COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/viewer_wiring_contract_test.py)
  set_tests_properties(viewer_wiring_contract PROPERTIES TIMEOUT 300)
endif()
