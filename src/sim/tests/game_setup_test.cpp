// GameSetup: the wire, the digest, and what validate refuses (T36).
#include <cstdint>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/game_setup.hpp>

namespace sim = anno::sim;
using Bytes = std::vector<std::uint8_t>;

namespace {

void append_u64(Bytes& out, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
        value >>= 8U;
    }
}

sim::GameSetup sample() {
    sim::GameSetup setup;
    setup.seats.push_back(sim::Seat{sim::SeatKind::Human, "Ada", 1, 0});
    setup.seats.push_back(sim::Seat{sim::SeatKind::Computer, "Bea", 2, 4});
    setup.world.base_edge_4p = 1600;
    setup.world.size = sim::IslandSize::Large;
    setup.world.quantity = sim::IslandQuantity::Many;
    setup.world.start = sim::StartMode::Spread;
    setup.pirates = sim::PirateLevel::Aggressive;
    setup.money = sim::StartMoney::Lots;
    setup.seed = 0x0123456789ABCDEFULL;
    setup.rules = sim::RulesPreset::Original;
    return setup;
}

bool same(const sim::GameSetup& a, const sim::GameSetup& b) {
    if (a.seats.size() != b.seats.size() || a.world.base_edge_4p != b.world.base_edge_4p ||
        a.world.size != b.world.size || a.world.quantity != b.world.quantity ||
        a.world.start != b.world.start || a.pirates != b.pirates || a.money != b.money ||
        a.seed != b.seed || a.rules != b.rules) {
        return false;
    }
    for (std::size_t i = 0; i < a.seats.size(); ++i) {
        const sim::Seat& x = a.seats[i];
        const sim::Seat& y = b.seats[i];
        if (x.kind != y.kind || x.name != y.name || x.colour != y.colour || x.team != y.team) {
            return false;
        }
    }
    return true;
}

sim::GameSetup round_trip(const sim::GameSetup& setup) {
    sim::WriteArchive out;
    sim::fields(out, setup);
    const Bytes bytes = out.take();
    sim::ReadArchive in(bytes);
    sim::GameSetup back;
    sim::fields(in, back);
    REQUIRE_FALSE(in.failed());
    CHECK(in.remaining() == 0);
    return back;
}

} // namespace

TEST_CASE("game setup round-trips and each field changes the digest") {
    const sim::GameSetup setup = sample();
    CHECK(same(round_trip(setup), setup));
    const std::uint64_t base = sim::digest(setup);

    sim::GameSetup changed = setup;
    changed.seats[0].kind = sim::SeatKind::Computer;
    CHECK(sim::digest(changed) != base);
    changed = setup;
    changed.seats[0].name = "Ada2";
    CHECK(sim::digest(changed) != base);
    changed = setup;
    changed.seats[0].colour = 3;
    CHECK(sim::digest(changed) != base);
    changed = setup;
    changed.seats[0].team = 9;
    CHECK(sim::digest(changed) != base);
    changed = setup;
    changed.seats.pop_back();
    CHECK(sim::digest(changed) != base);

    changed = setup;
    changed.world.base_edge_4p = 1280;
    CHECK(sim::digest(changed) != base);
    changed = setup;
    changed.world.size = sim::IslandSize::Small;
    CHECK(sim::digest(changed) != base);
    changed = setup;
    changed.world.quantity = sim::IslandQuantity::Less;
    CHECK(sim::digest(changed) != base);
    changed = setup;
    changed.world.start = sim::StartMode::Together;
    CHECK(sim::digest(changed) != base);
    changed = setup;
    changed.pirates = sim::PirateLevel::None;
    CHECK(sim::digest(changed) != base);
    changed = setup;
    changed.money = sim::StartMoney::Little;
    CHECK(sim::digest(changed) != base);
    changed = setup;
    changed.seed = 1;
    CHECK(sim::digest(changed) != base);
    changed = setup;
    changed.rules = sim::RulesPreset::Enhanced;
    CHECK(sim::digest(changed) != base);
}

TEST_CASE("game setup validate rejects each illegal setup") {
    const sim::SetupLimits limits;
    sim::GameSetup setup = sample();
    CHECK(sim::validate(setup, limits, 8));

    setup.seats.clear();
    CHECK(sim::validate(setup, limits, 8).error() == "seats: none");

    setup = sample();
    while (setup.seats.size() < 7) {
        setup.seats.push_back(sim::Seat{sim::SeatKind::Computer, "",
                                        static_cast<std::uint32_t>(setup.seats.size() + 3), 0});
    }
    CHECK(sim::validate(setup, limits, 16).error() == "seats: above max_seats");

    setup = sample();
    setup.seats[1].colour = setup.seats[0].colour;
    CHECK(sim::validate(setup, limits, 8).error() == "seats: duplicate colour");

    setup = sample();
    setup.seats[0].colour = 8;
    CHECK(sim::validate(setup, limits, 8).error() == "seats: colour out of range");

    setup = sample();
    setup.seats[0].name = std::string(33, 'a');
    CHECK(sim::validate(setup, limits, 8).error() == "seats: name too long");
    setup.seats[0].name = std::string(32, 'a');
    CHECK(sim::validate(setup, limits, 8));

    setup = sample();
    setup.seats[0].kind = static_cast<sim::SeatKind>(9);
    CHECK(sim::validate(setup, limits, 8).error() == "seats: kind out of range");
    setup = sample();
    setup.world.size = static_cast<sim::IslandSize>(9);
    CHECK(sim::validate(setup, limits, 8).error() == "world.size out of range");
    setup = sample();
    setup.world.quantity = static_cast<sim::IslandQuantity>(9);
    CHECK(sim::validate(setup, limits, 8).error() == "world.quantity out of range");
    setup = sample();
    setup.world.start = static_cast<sim::StartMode>(9);
    CHECK(sim::validate(setup, limits, 8).error() == "world.start out of range");
    setup = sample();
    setup.pirates = static_cast<sim::PirateLevel>(9);
    CHECK(sim::validate(setup, limits, 8).error() == "pirates out of range");
    setup = sample();
    setup.money = static_cast<sim::StartMoney>(9);
    CHECK(sim::validate(setup, limits, 8).error() == "money out of range");
    setup = sample();
    setup.rules = static_cast<sim::RulesPreset>(9);
    CHECK(sim::validate(setup, limits, 8).error() == "rules out of range");
}

TEST_CASE("game setup max_seats 8 accepts eight seats") {
    sim::SetupLimits limits;
    limits.max_seats = 8;
    sim::GameSetup setup;
    for (std::uint32_t i = 0; i < 8; ++i) {
        setup.seats.push_back(sim::Seat{sim::SeatKind::Human, "s", i, 0});
    }
    CHECK(sim::validate(setup, limits, 8));
}

TEST_CASE("game setup limits round-trip and default enums are in range") {
    sim::GameSetup setup;
    setup.seats.push_back(sim::Seat{});
    CHECK(sim::validate(setup, sim::SetupLimits{}, 1));

    sim::SetupLimits limits;
    limits.max_seats = 9;
    limits.max_name_bytes = 4;
    sim::WriteArchive out;
    sim::fields(out, limits);
    const Bytes bytes = out.take();
    sim::ReadArchive in(bytes);
    sim::SetupLimits back;
    sim::fields(in, back);
    REQUIRE_FALSE(in.failed());
    CHECK(in.remaining() == 0);
    CHECK(back.max_seats == 9);
    CHECK(back.max_name_bytes == 4);
}

TEST_CASE("game setup refuses a seat count the remaining bytes cannot hold") {
    Bytes bytes;
    append_u64(bytes, 1000000);
    sim::ReadArchive in(bytes);
    sim::GameSetup setup;
    sim::fields(in, setup);
    CHECK(in.failed());
    CHECK(setup.seats.empty());
    CHECK(setup.seats.capacity() < 8);
}
