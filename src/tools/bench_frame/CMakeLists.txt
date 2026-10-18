# bench_frame: frame benchmark of the viewer pipeline (docs/perf/README.md).
add_executable(bench_frame main.cpp)
target_link_libraries(bench_frame PRIVATE anno_app_core)
anno_target_defaults(bench_frame)
if(ANNO_HAVE_WGPU)
  target_link_libraries(bench_frame PRIVATE anno_render_gpu)
  target_compile_definitions(bench_frame PRIVATE ANNO_BENCH_GPU)
endif()
