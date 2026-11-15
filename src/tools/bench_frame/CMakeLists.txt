# bench_frame: frame benchmark of the viewer pipeline (docs/perf/README.md).
add_executable(bench_frame main.cpp)
target_link_libraries(bench_frame PRIVATE anno_app_core)
anno_target_defaults(bench_frame)
if(ANNO_HAVE_WGPU)
  target_link_libraries(bench_frame PRIVATE anno_render_gpu)
  target_compile_definitions(bench_frame PRIVATE ANNO_BENCH_GPU)
  if(ANNO_BUILD_TESTS AND Python3_FOUND)
    # ZOOM-SMOOTH-1 step 8, ZOOM-MEASURE-1 review note 1: the sweep's "everything resident"
    # control, readiness counters only (--readiness-only: the busy-time criterion is not yet
    # decided for driver jitter, docs/perf/README.md; ZOOM-SMOOTH-3's charge). Opt-in
    # (ANNO_ZOOM_SWEEP_PROOF=1): needs a real GPU adapter and real wall-clock frame pacing, so it
    # does not run in `gate` by default, unlike the always-failing camera-sweep profiles this
    # deliberately does not register (relaxing those would hide a regression).
    add_test(NAME zoom_sweep_control_readiness
      COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test/zoom_sweep_contract.py
              $<TARGET_FILE:bench_frame> ${PROJECT_SOURCE_DIR}/game/app
              ${PROJECT_SOURCE_DIR}/data/maps/lan5.txt --control --readiness-only
              --require-env ANNO_ZOOM_SWEEP_PROOF)
    set_tests_properties(zoom_sweep_control_readiness PROPERTIES
      SKIP_RETURN_CODE 77 TIMEOUT 300 LABELS "gpu" RESOURCE_LOCK gpu)
  endif()
endif()
