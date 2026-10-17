# anno_render: isometric renderer (CPU reference path; the GPU compositor
# follows, ADR 0001). Consumes anno_assets; never mutates simulation state.
find_package(Threads REQUIRED)
add_library(anno_render
  src/frame.cpp
  src/iso.cpp
  src/scene.cpp
  src/sea.cpp
)
target_include_directories(anno_render PUBLIC include)
target_link_libraries(anno_render PUBLIC anno_assets Threads::Threads)
target_compile_features(anno_render PUBLIC cxx_std_20)
anno_target_defaults(anno_render)

if(ANNO_BUILD_TESTS)
  add_executable(anno_render_tests
    tests/main.cpp
    tests/render_corpus_test.cpp
    tests/render_test.cpp
    tests/sprite_fidelity_test.cpp
  )
  target_link_libraries(anno_render_tests PRIVATE anno_render anno_test_support doctest::doctest)
  anno_target_defaults(anno_render_tests)
  add_test(NAME anno_render_tests COMMAND anno_render_tests)
endif()
