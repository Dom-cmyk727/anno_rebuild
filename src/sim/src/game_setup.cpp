#include <utility>
#include <vector>

#include <anno/sim/game_setup.hpp>

namespace anno::sim {
namespace {

template <class E> bool enum_ok(E value) {
    return EnumInfo<E>::valid(static_cast<std::uint8_t>(value));
}

} // namespace

formats::Result<std::monostate, std::string>
validate(const GameSetup& setup, const SetupLimits& limits, std::uint32_t colour_count) {
    if (setup.seats.empty()) {
        return std::string("seats: none");
    }
    if (setup.seats.size() > limits.max_seats) {
        return std::string("seats: above max_seats");
    }
    std::vector<std::uint32_t> seen;
    seen.reserve(setup.seats.size());
    for (const Seat& seat : setup.seats) {
        if (!enum_ok(seat.kind)) {
            return std::string("seats: kind out of range");
        }
        if (seat.name.size() > limits.max_name_bytes) {
            return std::string("seats: name too long");
        }
        if (seat.colour >= colour_count) {
            return std::string("seats: colour out of range");
        }
        for (const std::uint32_t colour : seen) {
            if (colour == seat.colour) {
                return std::string("seats: duplicate colour");
            }
        }
        seen.push_back(seat.colour);
    }
    if (!enum_ok(setup.world.size)) {
        return std::string("world.size out of range");
    }
    if (!enum_ok(setup.world.quantity)) {
        return std::string("world.quantity out of range");
    }
    if (!enum_ok(setup.world.start)) {
        return std::string("world.start out of range");
    }
    if (!enum_ok(setup.pirates)) {
        return std::string("pirates out of range");
    }
    if (!enum_ok(setup.money)) {
        return std::string("money out of range");
    }
    if (!enum_ok(setup.rules)) {
        return std::string("rules out of range");
    }
    return std::monostate{};
}

std::uint64_t digest(const GameSetup& setup) {
    HashArchive archive(HashOptions{nullptr, 0, true});
    fields(archive, setup);
    return archive.finish();
}

} // namespace anno::sim
