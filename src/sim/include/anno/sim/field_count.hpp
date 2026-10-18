// Compile-time field count of an aggregate: the canary that ties a struct to
// its `fields()` function (spec: docs/sim/state.md §14.4). A test asserts
// `field_count<T>() == N` next to a count of the primitives `fields(T)`
// visits, so a member added to a persistent struct without a line in the
// field list fails the build or the test instead of silently dropping out of
// hash and save.
//
// The probe finds the largest n for which `T{any, any, ... n times}` is a
// valid aggregate initialisation, where `any` converts to every type. It
// counts members that are initialised by one initializer: scalars, enums,
// strings, vectors, optionals and nested aggregates or class types that
// `any` converts to as a whole; an array member counts one per element, so
// persistent structs use vectors instead. It needs an aggregate, hence the
// static_assert; bases and bit-fields are not supported. (The real payloads
// of production.hpp, with vector and nested members, are canaried in
// sim_test.cpp, "field count canary"; classes that are not aggregates, World
// and WorldRng, carry their own structured-binding canary next to their
// members.)
#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

namespace anno::sim {

namespace detail {

/// Converts to anything; only ever named in an unevaluated operand.
struct AnyField {
    template <class T> operator T() const noexcept;
};

template <class T, std::size_t... I> constexpr bool brace_constructible(std::index_sequence<I...>) {
    return requires { T{(static_cast<void>(I), AnyField{})...}; };
}

template <class T, std::size_t N> constexpr bool has_n_initializers() {
    return brace_constructible<T>(std::make_index_sequence<N>{});
}

template <class T, std::size_t N, std::size_t Max> constexpr std::size_t count_fields() {
    if constexpr (N < Max && has_n_initializers<T, N + 1>()) {
        return count_fields<T, N + 1, Max>();
    } else {
        return N;
    }
}

} // namespace detail

/// The number of members of the aggregate T. `Max` is a compile-time probe
/// bound (the probe instantiates one template per candidate count; 64 is far
/// above any persistent struct); a type that still accepts Max + 1
/// initializers fails the static_assert instead of reporting Max, and a
/// larger bound is a template argument away.
template <class T, std::size_t Max = 64> constexpr std::size_t field_count() {
    static_assert(std::is_aggregate_v<T>, "field_count needs an aggregate");
    static_assert(detail::count_fields<T, 0, Max>() < Max ||
                      !detail::has_n_initializers<T, Max + 1>(),
                  "more members than the probe bound Max: pass a larger Max");
    return detail::count_fields<T, 0, Max>();
}

} // namespace anno::sim
