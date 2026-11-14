// <cfloat> before doctest: with libc++ 22 doctest's own include order leaves
// DBL_EPSILON undefined in its implementation.
#include <cfloat>

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "golden.hpp"
int main(int argc, char** argv) {
    if (anno::test::update_requested() && anno::test::in_ci()) {
        std::cerr << "ANNO_UPDATE_GOLDEN is set in a CI run: refused, unset it\n";
        return 2;
    }
    doctest::Context context(argc, argv);
    return context.run();
}
