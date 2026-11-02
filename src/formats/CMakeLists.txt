# anno_formats: readers and writers for the original file formats.
# Specs: docs/formats/. Standard library only.
add_library(anno_formats
  src/anlagen.cpp
  src/annowelt.cpp
  src/bigbrother.cpp
  src/chunk.cpp
  src/colors.cpp
  src/figuren.cpp
  src/inc.cpp
  src/fixed_map.cpp
  src/insel.cpp
  src/json.cpp
  src/sha256.cpp
  src/ship_manifest.cpp
  src/sze.cpp
  src/tagged.cpp
  src/text.cpp
  src/zei.cpp
  src/tex.cpp
)
target_include_directories(anno_formats PUBLIC include)
target_compile_features(anno_formats PUBLIC cxx_std_20)
anno_target_defaults(anno_formats)

if(ANNO_BUILD_TESTS)
  add_executable(anno_formats_tests
    tests/main.cpp
    tests/annowelt_test.cpp
    tests/bigbrother_test.cpp
    tests/bigbrother_corpus_test.cpp
    tests/chunk_test.cpp
    tests/chunk_corpus_test.cpp
    tests/tex_test.cpp
    tests/tex_corpus_test.cpp
    tests/figuren_corpus_test.cpp
    tests/figuren_test.cpp
    tests/fixed_map_test.cpp
    tests/insel_corpus_test.cpp
    tests/json_test.cpp
    tests/ship_manifest_test.cpp
    tests/sze_test.cpp
    tests/sze_corpus_test.cpp
    tests/text_zei_test.cpp
  )
  target_link_libraries(anno_formats_tests PRIVATE anno_formats anno_test_support doctest::doctest)
  # Our own synthetic files (fixtures/synthetic, shared with the Python references).
  target_compile_definitions(anno_formats_tests PRIVATE
    ANNO_FIXTURES_DIR="${PROJECT_SOURCE_DIR}/fixtures/synthetic"
    ANNO_REPO_DIR="${PROJECT_SOURCE_DIR}")
  anno_target_defaults(anno_formats_tests)
  anno_stage_budget_coord(anno_formats_tests)
  anno_add_doctest(anno_formats_tests)
endif()
