# Helpers shared by the test suites: the game directory and file lists.
add_library(anno_test_support src/game_dir.cpp src/memory.cpp src/budget.cpp src/coord.cpp
  src/coord_server.cpp)
target_include_directories(anno_test_support PUBLIC include)
target_include_directories(anno_test_support PRIVATE src)
target_compile_features(anno_test_support PUBLIC cxx_std_20)
if(WIN32)
  target_link_libraries(anno_test_support PRIVATE psapi ws2_32)
endif()
anno_target_defaults(anno_test_support)

# The coordinator's build id: the first 64 bits of a SHA-256 over the sources
# that make up the coordinator. It is provenance only (T10c F3, D1): logged in
# every gate line, and the WSL coordinator a Windows client compiles is named by
# it. Compatibility is the compatibility version (wire::kCompatVersion), so two
# builds with different ids share one budget when their versions agree. Editing
# one of these files reconfigures, so the id follows the sources.
set(ANNO_COORD_SOURCES
  src/coord_server.cpp src/coord_wire.hpp src/admit_detail.hpp src/budget_coord.cpp
  src/memory.cpp include/anno/test/budget.hpp include/anno/test/parallel.hpp)
set(_anno_coord_hashes "")
foreach(_anno_coord_source ${ANNO_COORD_SOURCES})
  file(SHA256 ${CMAKE_CURRENT_SOURCE_DIR}/${_anno_coord_source} _anno_coord_hash)
  string(APPEND _anno_coord_hashes "${_anno_coord_hash}")
endforeach()
string(SHA256 _anno_coord_id "${_anno_coord_hashes}")
string(SUBSTRING "${_anno_coord_id}" 0 16 ANNO_COORD_BUILD_ID)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ANNO_COORD_SOURCES})
target_compile_definitions(anno_test_support PRIVATE ANNO_COORD_BUILD=0x${ANNO_COORD_BUILD_ID}ULL)
message(STATUS "Test budget coordinator build ${ANNO_COORD_BUILD_ID}")

if(ANNO_BUILD_TESTS)
  add_executable(anno_budget_coord src/budget_coord.cpp)
  target_link_libraries(anno_budget_coord PRIVATE anno_test_support)
  anno_target_defaults(anno_budget_coord)

  add_executable(anno_budget_probe tests/budget_probe.cpp)
  target_link_libraries(anno_budget_probe PRIVATE anno_test_support)
  target_include_directories(anno_budget_probe PRIVATE src)
  anno_target_defaults(anno_budget_probe)

  add_executable(anno_test_support_tests tests/main.cpp tests/game_dir_test.cpp
    tests/parallel_test.cpp tests/budget_test.cpp tests/budget_f1_test.cpp
    tests/budget_f2_test.cpp tests/budget_f3_test.cpp tests/budget_f4_test.cpp
    tests/budget_f5_test.cpp
    tests/host_test.cpp)
  target_link_libraries(anno_test_support_tests PRIVATE anno_test_support doctest::doctest)
  target_include_directories(anno_test_support_tests PRIVATE src)
  anno_target_defaults(anno_test_support_tests)
  anno_add_doctest(anno_test_support_tests)
endif()
