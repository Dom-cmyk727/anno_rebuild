// A value or an error, for parsers that must not throw (ADR 0001: no
// exceptions in formats/sim). Minimal on purpose; replace with
// std::expected once the project moves to C++23.
#pragma once

#include <cassert>
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
        assert(ok());
        return std::get<0>(v_);
    }
    const T& value() const& {
        assert(ok());
        return std::get<0>(v_);
    }
    T&& value() && {
        assert(ok());
        return std::get<0>(std::move(v_));
    }
    const E& error() const {
        assert(!ok());
        return std::get<1>(v_);
    }

  private:
    std::variant<T, E> v_;
};

} // namespace anno::formats
