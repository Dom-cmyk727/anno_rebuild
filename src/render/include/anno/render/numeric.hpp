// Float and integer helpers with fixed, documented semantics, so that the
// renderer gives the same pixels on every compiler and as the Rust reference
// (crates/render): float-to-integer casts saturate and map NaN to 0 (a plain
// static_cast is undefined there), min/max ignore a NaN operand (IEEE
// minNum/maxNum), clamp keeps NaN, Euclidean division for negative screen
// coordinates. Rendering only; the simulation uses no floats.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace anno::render {

/// `v` truncated toward zero and saturated to T's range; NaN becomes 0.
template <class T> constexpr T sat_cast(float v) {
    static_assert(std::is_integral_v<T>);
    if (std::isnan(v)) {
        return 0;
    }
    // Compare in double: float cannot represent every bound exactly.
    const double d = static_cast<double>(v);
    if (d <= static_cast<double>(std::numeric_limits<T>::min())) {
        return std::numeric_limits<T>::min();
    }
    if (d >= static_cast<double>(std::numeric_limits<T>::max())) {
        return std::numeric_limits<T>::max();
    }
    return static_cast<T>(d);
}

/// The larger operand; if one is NaN, the other.
inline float fmaxf(float a, float b) {
    return std::fmax(a, b);
}
/// The smaller operand; if one is NaN, the other.
inline float fminf(float a, float b) {
    return std::fmin(a, b);
}
/// `v` limited to [lo, hi]; NaN stays NaN.
inline float clampf(float v, float lo, float hi) {
    if (v < lo) {
        v = lo;
    }
    if (v > hi) {
        v = hi;
    }
    return v;
}
/// Rounds half away from zero.
inline float roundf(float v) {
    return std::round(v);
}
inline float sq(float v) {
    return v * v;
}

/// Floor division and the matching non-negative remainder.
template <class T> constexpr T div_euclid(T a, T b) {
    const T q = a / b;
    return (a % b < 0) ? (b > 0 ? q - 1 : q + 1) : q;
}
template <class T> constexpr T rem_euclid(T a, T b) {
    const T r = a % b;
    return r < 0 ? (b < 0 ? r - b : r + b) : r;
}

/// IEEE 754 total order of two floats (−NaN < −∞ < … < −0 < +0 < … < NaN).
inline bool total_less(float a, float b) {
    auto key = [](float f) {
        std::int32_t i = 0;
        std::memcpy(&i, &f, sizeof i);
        return i ^ static_cast<std::int32_t>(static_cast<std::uint32_t>(i >> 31) >> 1);
    };
    return key(a) < key(b);
}

} // namespace anno::render
