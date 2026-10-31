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
endif()
