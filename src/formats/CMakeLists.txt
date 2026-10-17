# anno_formats: readers and writers for the original file formats.
# Specs: docs/formats/. Standard library only.
add_library(anno_formats
  src/anlagen.cpp
  src/chunk.cpp
  src/colors.cpp
  src/figuren.cpp
  src/inc.cpp
  src/insel.cpp
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
    tests/chunk_test.cpp
    tests/chunk_corpus_test.cpp
    tests/tex_test.cpp
    tests/tex_corpus_test.cpp
    tests/figuren_corpus_test.cpp
    tests/figuren_test.cpp
    tests/insel_corpus_test.cpp
    tests/sze_test.cpp
    tests/sze_corpus_test.cpp
    tests/text_zei_test.cpp
  )
  target_link_libraries(anno_formats_tests PRIVATE anno_formats anno_test_support doctest::doctest)
  anno_target_defaults(anno_formats_tests)
  add_test(NAME anno_formats_tests COMMAND anno_formats_tests)
endif()
