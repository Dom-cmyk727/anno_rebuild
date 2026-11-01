// Figure-type facts the founding veto reads, as the registry loader produces them from
// FigurenHeader entries (task MAP-2 package A2): the hull half-extent is a class-specific fact.
// Ship-class types (BEWOHNTYPE SHIP, 113) read HEAD f32 at byte 0x10; every finite value below 2^31
// tiles is kept SIGNED and exact (the shipped type 391 holds the uninitialised fill 0xCCCCCCCC =
// -107374176.0, the original does float arithmetic with it); a HEAD without the field, a NaN or
// an infinity, or a value beyond the bound leaves the hull ABSENT and the veto refuses when it
// needs it. Other classes have no hull. Expected values are derived by hand.
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/figuren.hpp>
#include <anno/sim/figure_veto.hpp>
#include <anno/sim/types.hpp>

namespace sim = anno::sim;
namespace fmt = anno::formats;

namespace {

/// The chunk views `payload` (a span): the caller's vector must outlive the chunk.
fmt::chunk::Chunk record(const char* name, const std::vector<std::uint8_t>& payload) {
    fmt::chunk::Chunk c;
    std::copy_n(name, std::strlen(name), c.name_field.begin());
    c.payload = payload;
    return c;
}

/// A figure entry with `kind_name`, a HEAD of `head_len` bytes whose f32 at 0x10 (when it fits) is
/// `bits`, and LOGIC with byte 0x25 = `range`.
std::shared_ptr<const sim::TypeRegistry> registry_of(const char* kind_name, std::size_t head_len,
                                                     std::uint32_t bits, std::string& error) {
    std::vector<std::uint8_t> head(head_len, 0);
    for (int i = 0; i < 4 && 0x10 + i < static_cast<int>(head_len); ++i) {
        head[static_cast<std::size_t>(0x10 + i)] = static_cast<std::uint8_t>(bits >> (8 * i));
    }
    std::vector<std::uint8_t> name(kind_name, kind_name + std::strlen(kind_name) + 1);
    std::vector<std::uint8_t> logic(44, 0);
    logic[0x25] = 128;
    fmt::figuren::Entry e;
    e.id = 1502;
    e.records = {record("HEAD", head), record("BEWOHNTYPE", name), record("LOGIC", logic)};
    auto built = sim::TypeRegistry::build({}, sim::original_goods(), {}, std::span(&e, 1));
    if (!built.ok()) {
        error = built.error();
        return nullptr;
    }
    return built.value();
}

std::optional<std::int64_t> hull_of(const char* kind, std::size_t head_len, float value) {
    std::string error;
    const auto reg = registry_of(kind, head_len, std::bit_cast<std::uint32_t>(value), error);
    REQUIRE_MESSAGE(reg != nullptr, error);
    return reg->find_figure_type(1502)->hull_half_88;
}

} // namespace

TEST_CASE(
    "figure facts: the ship hull half-extent is exact, signed, or absent, never a silent zero") {
    // Valid original data: 1.55 -> 396.8 -> 397; 2.35 -> 601.6 -> 602 (1501); 0 is a valid hull.
    CHECK(hull_of("SHIP", 20, 1.55F) == std::optional<std::int64_t>(397));
    CHECK(hull_of("SHIP", 20, 2.35F) == std::optional<std::int64_t>(602));
    CHECK(hull_of("SHIP", 20, 0.0F) == std::optional<std::int64_t>(0));
    // 1/512 tile = 0.5 raw rounds to 1 (llround away from zero); one raw unit is 1/256.
    CHECK(hull_of("SHIP", 20, 1.0F / 512.0F) == std::optional<std::int64_t>(1));
    // Negative values are kept exactly (an inverted box in the original's float arithmetic).
    CHECK(hull_of("SHIP", 20, -1.0F) == std::optional<std::int64_t>(-256));
    // The uninitialised fill of the shipped type 391: -107374176.0 tiles * 256.
    CHECK(hull_of("SHIP", 20, std::bit_cast<float>(0xCCCCCCCCU)) ==
          std::optional<std::int64_t>(-27487789056LL));
    // Large but inside the bound: 65536 tiles.
    CHECK(hull_of("SHIP", 20, 65536.0F) == std::optional<std::int64_t>(16777216));
    // Absent: the HEAD has no such field (8 and 16 bytes), NaN, infinities, beyond 2^31 tiles.
    CHECK_FALSE(hull_of("SHIP", 8, 1.55F).has_value());
    CHECK_FALSE(hull_of("SHIP", 16, 1.55F).has_value());
    CHECK_FALSE(hull_of("SHIP", 20, std::numeric_limits<float>::quiet_NaN()).has_value());
    CHECK_FALSE(hull_of("SHIP", 20, std::numeric_limits<float>::infinity()).has_value());
    CHECK_FALSE(hull_of("SHIP", 20, -std::numeric_limits<float>::infinity()).has_value());
    CHECK_FALSE(hull_of("SHIP", 20, 4294967296.0F).has_value()); // 2^32 tiles
    // Just inside the bound (2^30 tiles) is kept.
    CHECK(hull_of("SHIP", 20, 1073741824.0F) == std::optional<std::int64_t>(1073741824LL * 256));
    // A class without a hull never reads the field, whatever is there: absent, load succeeds.
    CHECK_FALSE(hull_of("ERKUNDER", 20, std::numeric_limits<float>::quiet_NaN()).has_value());
    CHECK_FALSE(hull_of("ERKUNDER", 20, 1.55F).has_value());
    CHECK_FALSE(hull_of("ESEL", 16, 0.0F).has_value());
}

TEST_CASE("figure facts: the veto's behaviour for present, negative, zero and absent hulls") {
    // Footprint [50,54) x [30,34); an own ship at (55.5, 31.5) is outside the rectangle.
    const auto verdict = [](std::optional<std::int64_t> hull, std::int32_t x_raw) {
        sim::veto::NearbyFigure f;
        f.registration = sim::veto::Registration::SectorCell;
        f.kind = 113;
        f.pos = sim::FixedPos{x_raw, 31 * 256 + 128};
        f.owner = {sim::veto::Fact::True, sim::PlayerId{0}};
        f.hull_half_88 = hull;
        return sim::veto::nearby_figures(sim::PlayerId{0}, {50, 30}, 4, 4,
                                         std::span<const sim::veto::NearbyFigure>(&f, 1))
            .verdict;
    };
    const std::int32_t outside = 55 * 256 + 128;
    CHECK(verdict(397, outside) == sim::veto::Verdict::Vetoed);     // 53.95 -> 53 < 54: overlaps
    CHECK(verdict(0, outside) == sim::veto::Verdict::Clear);        // a zero hull: point only
    CHECK(verdict(-7 * 256, outside) == sim::veto::Verdict::Clear); // inverted box
    CHECK(verdict(-27487789056LL, outside) == sim::veto::Verdict::Clear); // the type-391 fill
    CHECK(verdict(std::nullopt, outside) == sim::veto::Verdict::MissingFact);
    // The point test needs no hull: inside the rectangle an absent hull still vetoes.
    CHECK(verdict(std::nullopt, 53 * 256 + 128) == sim::veto::Verdict::Vetoed);
}
