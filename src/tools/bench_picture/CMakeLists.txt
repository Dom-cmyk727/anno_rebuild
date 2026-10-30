# bench_picture: the window-side cost of the building picture (W4.3, round dv; docs/perf/README.md).
add_executable(bench_picture main.cpp)
target_link_libraries(bench_picture PRIVATE anno_app_core)
anno_target_defaults(bench_picture)
