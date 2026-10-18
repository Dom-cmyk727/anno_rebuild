// <cfloat> before doctest: with libc++ 22 doctest's own include order leaves
// DBL_EPSILON undefined in its implementation.
#include <cfloat>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
