// test_threads(): the ANNO_TEST_THREADS request is bounded by free memory.
#include <atomic>
#include <cstdlib>
#include <new>
#include <stdexcept>
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

TEST_CASE("parallel_indexed runs an item again after std::bad_alloc and keeps the order") {
    set_env("ANNO_TEST_THREADS", "4");
    set_env("ANNO_TEST_MB_PER_THREAD", "0");
    std::atomic<int> failures{0};
    const auto out = anno::test::parallel_indexed(8, [&](std::size_t i) {
        if (i % 3 == 0 && failures++ < 3) {
            throw std::bad_alloc();
        }
        return static_cast<int>(i * i);
    });
    REQUIRE(out.size() == 8);
    for (std::size_t i = 0; i < out.size(); ++i) {
        CHECK(out[i] == static_cast<int>(i * i));
    }
    set_env("ANNO_TEST_THREADS", "");
    set_env("ANNO_TEST_MB_PER_THREAD", "");
}

TEST_CASE("parallel_indexed rethrows other exceptions") {
    set_env("ANNO_TEST_THREADS", "2");
    set_env("ANNO_TEST_MB_PER_THREAD", "0");
    CHECK_THROWS_AS(anno::test::parallel_indexed(4,
                                                 [](std::size_t i) -> int {
                                                     if (i == 2) {
                                                         throw std::runtime_error("x");
                                                     }
                                                     return 0;
                                                 }),
                    std::runtime_error);
    set_env("ANNO_TEST_THREADS", "");
    set_env("ANNO_TEST_MB_PER_THREAD", "");
}
