// A lookup result that must exist: REQUIRE it, then use it. Test files dereference the pointers of
// find/lookup calls through this, so that a missing object fails the case with a message and GCC
// -O3 -Werror=null-dereference sees a checked pointer (no pragma, no suppression).
#pragma once

#include <doctest/doctest.h>

namespace test_must {

template <class P> inline P must(P p) {
    REQUIRE(p != nullptr);
    return p;
}

} // namespace test_must
