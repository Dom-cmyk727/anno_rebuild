# anno_render: isometric renderer (CPU reference path; the GPU compositor
# follows, ADR 0001). Consumes anno_assets; never mutates simulation state.
find_package(Threads REQUIRED)
add_library(anno_render
  src/chunk_draws.cpp
  src/chunk_reduce.cpp
  src/fluid.cpp
  src/frame.cpp
  src/frame_table.cpp
  src/iso.cpp
  src/profile.cpp
  src/residency.cpp
  src/resample.cpp
  src/river.cpp
  src/river_lattice.cpp
  src/river_solve.cpp
  src/scene.cpp
  src/sea.cpp
)
target_include_directories(anno_render PUBLIC include)
target_link_libraries(anno_render PUBLIC anno_assets Threads::Threads)
target_compile_features(anno_render PUBLIC cxx_std_20)
anno_target_defaults(anno_render)

if(ANNO_BUILD_TESTS)
  add_executable(anno_render_tests
    tests/buffer_equiv_test.cpp
    tests/chunk_draws_corpus_test.cpp
    tests/chunk_draws_test.cpp
    tests/chunk_reduce_test.cpp
    tests/fluid_test.cpp
    tests/main.cpp
    tests/render_corpus_test.cpp
    tests/render_test.cpp
    tests/residency_test.cpp
    tests/river_flow_corpus_test.cpp
    tests/river_flow_test.cpp
    tests/river_shade_test.cpp
    tests/sprite_fidelity_test.cpp
  )
  target_link_libraries(anno_render_tests PRIVATE anno_render anno_test_support doctest::doctest)
  anno_target_defaults(anno_render_tests)
  anno_stage_budget_coord(anno_render_tests)
  anno_add_doctest(anno_render_tests)
endif()

# anno_render_gpu: the GPU compositor over wgpu-native (separate, so the CPU
# renderer and everything below it build without a GPU dependency).
if(ANNO_HAVE_WGPU)
  set(_gen ${CMAKE_CURRENT_BINARY_DIR}/generated)
  anno_embed_text(${_gen}/gpu_wgsl.cpp kGpuWgsl ${CMAKE_CURRENT_SOURCE_DIR}/shaders/gpu.wgsl)
  anno_embed_text(${_gen}/fluid_wgsl.cpp kFluidWgsl ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fluid.wgsl)
  add_library(anno_render_gpu
    src/gpu.cpp
    src/gpu_fluid.cpp
    src/wgpu.cpp
    ${_gen}/gpu_wgsl.cpp
    ${_gen}/fluid_wgsl.cpp
  )
  target_include_directories(anno_render_gpu PUBLIC include)
  target_link_libraries(anno_render_gpu PUBLIC anno_render wgpu::native)
  target_compile_features(anno_render_gpu PUBLIC cxx_std_20)
  anno_target_defaults(anno_render_gpu)
  if(ANNO_BUILD_TESTS)
    add_executable(anno_render_gpu_tests tests/main.cpp tests/gpu_test.cpp
      tests/gpu_chunk_build_test.cpp tests/play_gpu_test.cpp)
    target_link_libraries(anno_render_gpu_tests PRIVATE anno_render_gpu anno_app_core
      anno_test_support doctest::doctest)
    # Our own synthetic files (fixtures/synthetic): a scene without islands.
    target_compile_definitions(anno_render_gpu_tests PRIVATE
      ANNO_FIXTURES_DIR="${PROJECT_SOURCE_DIR}/fixtures/synthetic")
    anno_target_defaults(anno_render_gpu_tests)
    anno_stage_budget_coord(anno_render_gpu_tests)
    # One GPU case at a time: each opens its own device. The label "gpu" lets a
    # host without a useful adapter leave them out (`ctest -LE gpu`): CI's
    # windows-msvc job, whose only adapter is the WARP software rasterizer,
    # spent 43 of its 54 minutes on them (about 50 s per case) while the
    # deterministic GPU parity job is linux-gcc on lavapipe (OQ-089).
    anno_add_doctest(anno_render_gpu_tests LABELS gpu PROPERTIES RESOURCE_LOCK gpu)
  endif()
endif()
