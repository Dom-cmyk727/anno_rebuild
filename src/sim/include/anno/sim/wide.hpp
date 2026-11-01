// Portable 128-bit unsigned arithmetic for the ship movement integrator (MAP-NAV-2 package B;
// docs/sim/ship-navigation.md 12.4). The simulation must not assume a compiler 128-bit integer
// (MSVC has none), and the products of the exact-rational integrator exceed 64 bits: a budget
// B = ticks * speed_q32 * V is below 2^51, a diagonal axis step multiplies it by a constant below
// 2^24 (up to 2^75), and the conversion of leftover time between two speeds multiplies two such
// values (up to 2^86). Only what that integrator needs exists here: a full 64 x 64 -> 128 product,
// an add, a right shift, a comparison and a 128 / 64 division. Everything is constexpr, integer
// only and defined for every input (no undefined behaviour; division by zero is reported, not
// executed). Checked against Python big integers by fixtures/synthetic/sim/nav/wide_vectors.txt.
#pragma once

#include <cstdint>

namespace anno::sim::wide {

struct U128 {
    std::uint64_t hi = 0;
    std::uint64_t lo = 0;
    friend constexpr bool operator==(const U128&, const U128&) = default;
};

/// a * b as a full 128-bit value.
[[nodiscard]] constexpr U128 mul(std::uint64_t a, std::uint64_t b) {
    const std::uint64_t a0 = a & 0xFFFFFFFFULL, a1 = a >> 32;
    const std::uint64_t b0 = b & 0xFFFFFFFFULL, b1 = b >> 32;
    const std::uint64_t p00 = a0 * b0;
    const std::uint64_t p01 = a0 * b1;
    const std::uint64_t p10 = a1 * b0;
    const std::uint64_t p11 = a1 * b1;
    // The middle column cannot overflow: three terms each below 2^33 in the low 32-bit sense.
    const std::uint64_t mid = (p00 >> 32) + (p01 & 0xFFFFFFFFULL) + (p10 & 0xFFFFFFFFULL);
    U128 r;
    r.lo = (p00 & 0xFFFFFFFFULL) | (mid << 32);
    r.hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
    return r;
}

/// a + b; `carry` is set when the sum does not fit 128 bits (the value wraps modulo 2^128).
[[nodiscard]] constexpr U128 add(U128 a, U128 b, bool& carry) {
    U128 r;
    r.lo = a.lo + b.lo;
    const bool c1 = r.lo < a.lo;
    const std::uint64_t hi1 = a.hi + b.hi;
    const bool c2 = hi1 < a.hi;
    r.hi = hi1 + (c1 ? 1U : 0U);
    const bool c3 = r.hi < hi1;
    carry = c2 || c3;
    return r;
}

/// a >> n for n in 0..127 (an n of 128 or more gives 0).
[[nodiscard]] constexpr U128 shr(U128 a, unsigned n) {
    if (n == 0) {
        return a;
    }
    if (n >= 128) {
        return {};
    }
    if (n >= 64) {
        return {0, a.hi >> (n - 64)};
    }
    return {a.hi >> n, (a.lo >> n) | (a.hi << (64 - n))};
}

[[nodiscard]] constexpr bool less(U128 a, U128 b) {
    return a.hi != b.hi ? a.hi < b.hi : a.lo < b.lo;
}

/// n / d and n % d for d != 0 (restoring division, 128 steps). Returns false and leaves the
/// outputs untouched for d == 0.
[[nodiscard]] constexpr bool divmod(U128 n, std::uint64_t d, U128& quotient,
                                    std::uint64_t& remainder) {
    if (d == 0) {
        return false;
    }
    U128 q;
    std::uint64_t r = 0;
    for (int i = 127; i >= 0; --i) {
        const std::uint64_t bit = i >= 64 ? (n.hi >> (i - 64)) & 1U : (n.lo >> i) & 1U;
        const bool top = (r >> 63) != 0; // the shift below would lose this bit
        r = (r << 1) | bit;
        if (top || r >= d) {
            r -= d; // modulo 2^64 is exact when `top` was set (the true value is < 2 * d)
            if (i >= 64) {
                q.hi |= std::uint64_t{1} << (i - 64);
            } else {
                q.lo |= std::uint64_t{1} << i;
            }
        }
    }
    quotient = q;
    remainder = r;
    return true;
}

/// floor(a / d) for a signed a and d > 0 (the rounding toward negative infinity that the
/// published Q24.8 position uses). d == 0 is a precondition violation and returns 0.
[[nodiscard]] constexpr std::int64_t floor_div(std::int64_t a, std::int64_t d) {
    if (d <= 0) {
        return 0;
    }
    std::int64_t q = a / d;
    const std::int64_t r = a % d;
    if (r != 0 && r < 0) {
        --q;
    }
    return q;
}

} // namespace anno::sim::wide
