// test_threads(): the ANNO_TEST_THREADS request is bounded by free memory.
#include <cstdlib>
#include <string>

#include <doctest/doctest.h>

#include <anno/test/parallel.hpp>

namespace {

void set_env(const char* name, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

} // namespace

TEST_CASE("test_threads is bounded by free physical memory") {
    set_env("ANNO_TEST_THREADS", "64");
    set_env("ANNO_TEST_MB_PER_THREAD", "0");
    CHECK(anno::test::test_threads() == 64); // bound off: the request wins

    // A budget larger than any machine's RAM leaves exactly one thread.
    set_env("ANNO_TEST_MB_PER_THREAD", "1000000000");
    if (anno::test::free_physical_bytes() > 0) {
        CHECK(anno::test::test_threads() == 1);
    }

    set_env("ANNO_TEST_MB_PER_THREAD", "1024");
    const std::size_t n = anno::test::test_threads();
    CHECK(n >= 1);
    CHECK(n <= 64);
    set_env("ANNO_TEST_THREADS", "");
    set_env("ANNO_TEST_MB_PER_THREAD", "");
}
