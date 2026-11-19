#include <algorithm>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/annowelt.hpp>
#include <anno/sim_load/wildlife_ground_prepare.hpp>
#include <anno/test/wildlife_kernel_fixture.hpp>

TEST_CASE("wildlife kernel: owned authored two dimensional numerical connection") {
    auto image = anno::test::kernel_fixture::image();
    const auto kernel = anno::formats::annowelt::read_ground_kernel(image, 6);
    REQUIRE(kernel);
    std::fill(image.begin(), image.end(), 0);
    image.clear();
    image.shrink_to_fit();
    // Hand-derived b = [1,2,3 / 4,1,2 / 2,3,1]. Authored weights:
    // centre 12, east 24, south 12 -> low [9,9,5 / 8,8,3 / 8,5,1].
    // Unsaturated and asymmetric: orientation, stride and per-edge bounds matter.
    const std::vector<std::uint16_t> gfx{0, 4, 8, 12, 16, 20, 24, 28, 32};
    const std::vector<std::uint32_t> objects(9, 0);
    const std::vector<std::uint8_t> lookup{10, 20, 30, 40, 10, 20, 20, 30, 10};
    const std::vector<std::uint8_t> old{167, 183, 199, 215, 231, 247, 23, 39, 55};
    const std::vector<anno::sim_load::GroundObjectFact> facts{{}};
    const auto result = anno::sim_load::prepare_wildlife_ground(
        {{17, -11, 3, 3}, gfx, objects, old, lookup, kernel.value().weights, facts});
    REQUIRE(result);
    CHECK(result.value() == std::vector<std::uint8_t>{169, 185, 197, 216, 232, 243, 24, 37, 49});
    CHECK(old == std::vector<std::uint8_t>{167, 183, 199, 215, 231, 247, 23, 39, 55});
    // This proves an authored API connection, not original city/INFO/HEAD selection.
}
