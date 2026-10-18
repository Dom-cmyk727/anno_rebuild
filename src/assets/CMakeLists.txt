# anno_assets: decoded, engine-ready images (textures, figures, buildings,
# island ground, scenario units) built from anno_formats. No I/O.
add_library(anno_assets
  src/building.cpp
  src/figure.cpp
  src/island.cpp
  src/texture.cpp
  src/unit.cpp
)
target_include_directories(anno_assets PUBLIC include)
target_link_libraries(anno_assets PUBLIC anno_formats)
target_compile_features(anno_assets PUBLIC cxx_std_20)
anno_target_defaults(anno_assets)

if(ANNO_BUILD_TESTS)
  add_executable(anno_assets_tests
    tests/main.cpp
    tests/assets_test.cpp
    tests/assets_corpus_test.cpp
  )
  target_link_libraries(anno_assets_tests PRIVATE anno_assets anno_test_support doctest::doctest)
  # Our own synthetic files (fixtures/synthetic, shared with the Python references).
  target_compile_definitions(anno_assets_tests PRIVATE
    ANNO_FIXTURES_DIR="${PROJECT_SOURCE_DIR}/fixtures/synthetic")
  anno_target_defaults(anno_assets_tests)
  add_test(NAME anno_assets_tests COMMAND anno_assets_tests)
endif()
