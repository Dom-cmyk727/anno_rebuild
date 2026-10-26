// A value or an error, for parsers that must not throw (ADR 0001: no
// exceptions in formats/sim). Minimal on purpose; replace with
// std::expected once the project moves to C++23.
#pragma once

#include <cstdlib>
#include <utility>
#include <variant>

namespace anno::formats {

template <class T, class E> class Result {
  public:
    Result(T value) : v_(std::in_place_index<0>, std::move(value)) {}
    Result(E error) : v_(std::in_place_index<1>, std::move(error)) {}

    [[nodiscard]] bool ok() const { return v_.index() == 0; }
    explicit operator bool() const { return ok(); }

    T& value() & {
        require(ok());
        return std::get<0>(v_);
    }
    const T& value() const& {
        require(ok());
        return std::get<0>(v_);
    }
    /// On a temporary, the value itself (moved out), not a reference: a
    /// range-for over `f().value()` would otherwise dangle (C++20 does not
    /// extend the temporary's lifetime there).
    T value() && {
        require(ok());
        return std::get<0>(std::move(v_));
    }
    const E& error() const {
        require(!ok());
        return std::get<1>(v_);
    }

  private:
    // NDEBUG removes assert, and std::get then throws std::bad_variant_access.
    static void require(bool condition) {
        if (!condition) [[unlikely]] {
            std::abort();
        }
    }

    std::variant<T, E> v_;
};

} // namespace anno::formats
