#include <string>
#include <utility>

#include <doctest/doctest.h>

#include <anno/formats/result.hpp>

TEST_CASE("Result value and error return the stored objects") {
    anno::formats::Result<int, std::string> good(7);
    CHECK(good.ok());
    CHECK(static_cast<bool>(good));
    CHECK(good.value() == 7);
    const anno::formats::Result<int, std::string>& good_const = good;
    CHECK(good_const.ok());
    CHECK(static_cast<bool>(good_const));
    CHECK(good_const.value() == 7);
    CHECK(std::move(good).value() == 7);

    const anno::formats::Result<int, std::string> bad(std::string("missing"));
    CHECK_FALSE(bad.ok());
    CHECK_FALSE(static_cast<bool>(bad));
    CHECK(bad.error() == "missing");
}
