#include <algorithm>

#include <anno/sim/wildlife.hpp>

namespace anno::sim {
namespace {
struct Draws {
    std::span<const std::uint32_t> values;
    std::size_t consumed = 0;
    std::optional<std::uint32_t> take() {
        if (consumed == values.size())
            return {};
        return values[consumed++];
    }
    WildlifeError exhausted() const { return {WildlifeFault::DrawsExhausted, consumed}; }
};
template <class T> bool contains(std::span<const T> values, T value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}
// AnnoWelt 0x100519d0/0x10051b00 (New), 0x100532b0/0x10053410 (Wolf).
WildlifeResult<WildlifeTransition> transition(WildlifeTransitionInput in,
                                              std::span<const std::uint32_t> draws, bool wolf) {
    Draws d{draws};
    if (in.event == WildlifeEvent::Fallback) {
        std::uint8_t next = 0;
        if (in.state == 27)
            next = 28;
        else if (!wolf && in.state == 26)
            next = 27;
        else if (wolf && in.state == 30)
            next = 27;
        else if (wolf && in.state == 31)
            next = 30;
        return WildlifeDecision<WildlifeTransition>{{next, {}}, 0};
    }
    if (in.event == WildlifeEvent::Idle) {
        if (in.state != 28 || in.variants < 1 || in.variants > 3)
            return WildlifeError{WildlifeFault::InvalidInput};
        // 0x10051441: modulus one still draws.
        const auto r = d.take();
        if (!r)
            return d.exhausted();
        return WildlifeDecision<WildlifeTransition>{
            {28, static_cast<std::uint8_t>(*r % in.variants)}, d.consumed};
    }
    const bool supported = in.state == 25 || in.state == 27 || in.state == 28 ||
                           (wolf ? in.state == 30 || in.state == 31 : in.state == 26);
    if (in.event != WildlifeEvent::Complete || !supported ||
        (((!wolf && in.state == 26) || in.state == 27 || in.state == 28) && !in.success))
        return WildlifeError{WildlifeFault::InvalidInput};
    std::uint8_t next = in.state;
    if (wolf) {
        if (in.state == 25)
            next = in.success ? 27 : 25;
        else if (in.state == 30)
            next = in.success ? 31 : 30;
        else if (in.state == 31)
            next = in.success ? 25 : 31;
        else {
            const auto r = d.take();
            if (!r)
                return d.exhausted();
            if (in.state == 27) {
                const auto remainder = *r % 7;
                next = remainder == 0 ? 28 : (remainder <= 3 ? 30 : 27);
            } else
                next = (*r & 1) != 0 ? 30 : 27;
        }
    } else if (in.state == 25 && !in.success)
        next = 25;
    else if (in.state == 26)
        next = 25;
    else {
        const auto r = d.take();
        if (!r)
            return d.exhausted();
        next =
            in.state == 27 ? (*r % 5 != 0 ? 26 : 28) : static_cast<std::uint8_t>((~*r & 1U) | 26U);
    }
    // 0x10051bf6 / 0x10053546: after the draw, strict >, truncated third first.
    if (next == (wolf ? 30 : 26) && in.quantity > 2U * (in.capacity / 3U))
        next = 27;
    return WildlifeDecision<WildlifeTransition>{{next, {}}, d.consumed};
}
} // namespace

WildlifeResult<std::uint32_t> wildlife_density(std::uint32_t n) {
    // 0x100938a0–0x10093963: exact initialized values for the 16-cell sweep only.
    if (n > 16)
        return WildlifeError{WildlifeFault::InvalidInput};
    const std::uint32_t desired = n == 0 ? 0 : n < 4 ? 1 : n < 8 ? 2 : n < 12 ? 3 : n < 14 ? 4 : 5;
    return WildlifeDecision<std::uint32_t>{desired, 0};
}
WildlifeResult<bool> wildlife_habitat(WildlifeHabitatFact center,
                                      std::span<const WildlifeHabitatFact> neighbors,
                                      std::uint32_t property7) {
    // 0x10093030–0x10093472: eight geometric neighbours, double matches count twice.
    if (neighbors.size() != 8)
        return WildlifeError{WildlifeFault::InvalidInput};
    std::uint32_t score = 1;
    for (const auto& n : neighbors)
        if (n.object.v != 0) {
            score += n.goods == 0x46 ? 1U : 0U;
            score += n.kind == 0x145 ? 1U : 0U;
        }
    return WildlifeDecision<bool>{center.object.v != 0 &&
                                      (center.goods == 0x46 || center.kind == 0x145) &&
                                      score >= (property7 == 5 ? 1U : 4U),
                                  0};
}
WildlifeResult<WildlifeReplenishment> wildlife_replenish(std::span<const std::uint32_t> candidates,
                                                         std::uint8_t deficit,
                                                         std::span<const std::uint32_t> occupied,
                                                         std::span<const std::uint32_t> draws) {
    // 0x100935e0–0x10093725: local sweep geometry, compact by swap-pop before collision retry.
    if (candidates.size() > 16)
        return WildlifeError{WildlifeFault::InvalidInput};
    for (std::size_t i = 0; i < candidates.size(); ++i)
        if (contains(candidates.subspan(i + 1), candidates[i]))
            return WildlifeError{WildlifeFault::InvalidInput};
    WildlifeReplenishment out{{}, {candidates.begin(), candidates.end()}};
    Draws d{draws};
    while (out.chosen.size() < deficit) {
        if (out.remaining.empty())
            return WildlifeError{WildlifeFault::Oq572CandidatesExhausted, d.consumed};
        const auto r = d.take();
        if (!r)
            return d.exhausted();
        const auto index = *r % out.remaining.size();
        const auto cell = out.remaining[index];
        out.remaining[index] = out.remaining.back();
        out.remaining.pop_back();
        if (!contains(occupied, cell))
            out.chosen.push_back(cell);
    }
    return WildlifeDecision<WildlifeReplenishment>{std::move(out), d.consumed};
}
WildlifeResult<WildlifeCreation> wildlife_create(bool cave, std::size_t entrances,
                                                 std::span<const WildlifeSpecies> species,
                                                 GoodsId saved,
                                                 std::span<const std::uint32_t> draws) {
    // 0x10092cfd–0x10092f30: mask-indexed climate table, not a world registry size.
    if (species.size() != 32)
        return WildlifeError{WildlifeFault::InvalidInput};
    if (cave && entrances == 0)
        return WildlifeError{WildlifeFault::Oq574CaveWithoutEntrance};
    Draws d{draws};
    std::int32_t position = -1;
    if (!cave) {
        const auto r = d.take();
        if (!r)
            return d.exhausted();
        position = static_cast<std::int32_t>(*r & 3U);
    }
    const auto r = d.take();
    if (!r)
        return d.exhausted();
    std::uint32_t index = *r & 31U, inspected = 1;
    if (saved != 255)
        while (inspected < 18 && species[index].goods != saved) {
            index = (index + 1) & 31U;
            ++inspected;
        }
    return WildlifeDecision<WildlifeCreation>{{position, index, species[index].type, inspected},
                                              d.consumed};
}
std::uint32_t wildlife_load_base(std::uint16_t raw_stamp) {
    // 0x100946e9–0x10094705; physical time units remain unproved.
    return static_cast<std::uint32_t>(raw_stamp) / 500U;
}
WildlifeResult<WildlifeLoad> wildlife_load(std::uint8_t entry4, std::uint32_t base,
                                           std::uint8_t mode, GoodsId goods,
                                           std::span<const std::uint32_t> draws) {
    // 0x100947d3–0x10094816: caller-divided base, byte addition and mode bit zero.
    // The decoded u16 stamp divided by 500 admits at most 131, not a world limit.
    if (base > 131)
        return WildlifeError{WildlifeFault::InvalidInput};
    Draws d{draws};
    std::uint32_t jitter = 0;
    if ((entry4 & 1) == 0) {
        const auto r = d.take();
        if (!r)
            return d.exhausted();
        jitter = *r & 31U;
    }
    return WildlifeDecision<WildlifeLoad>{
        {static_cast<std::uint8_t>((base + jitter) & 255U), (mode & 1) != 0 ? 255U : goods},
        d.consumed};
}
WildlifeResult<WildlifeTransition> wildlife_animal(WildlifeTransitionInput input,
                                                   std::span<const std::uint32_t> draws) {
    return transition(input, draws, false);
}
WildlifeResult<WildlifeTransition> wildlife_wolf(WildlifeTransitionInput input,
                                                 std::span<const std::uint32_t> draws) {
    return transition(input, draws, true);
}
WildlifeResult<WildlifeWalk> wildlife_walk(std::uint8_t attempt, std::uint8_t behavior,
                                           std::uint8_t facing,
                                           std::span<const std::uint32_t> draws) {
    // 0x10051515 / 0x10058360–0x100583ff: radius then angle, three candidates.
    Draws d{draws};
    bool doubled = false;
    if (behavior == 7) {
        const auto r = d.take();
        if (!r)
            return d.exhausted();
        doubled = *r % 6 == 1;
    }
    const std::uint32_t minimum = doubled ? 80 : 40, modulus = doubled ? 64 : 32;
    const std::uint32_t half = attempt < 2 ? 128 : 256;
    const std::uint32_t base = ((std::uint32_t{facing} + 2) << 6) & 255U;
    WildlifeWalk out{};
    for (std::size_t i = 0; i < out.radii.size(); ++i) {
        const auto radius = d.take();
        if (!radius)
            return d.exhausted();
        const auto angle = d.take();
        if (!angle)
            return d.exhausted();
        out.radii[i] = minimum + *radius % modulus;
        out.angles[i] = (base + *angle % (2 * half + 1) + 512U - half) & 511U;
    }
    out.attempt = static_cast<std::uint8_t>(std::uint32_t{attempt} + 1);
    return WildlifeDecision<WildlifeWalk>{out, d.consumed};
}
WildlifeResult<std::uint8_t> wildlife_route_old(std::uint16_t quantity,
                                                std::span<const std::uint32_t> draws) {
    // 0x1005e2b0–0x1005e37f: quantity is already post-action.
    if (quantity != 0)
        return WildlifeDecision<std::uint8_t>{12, 0};
    Draws d{draws};
    const auto r = d.take();
    if (!r)
        return d.exhausted();
    return WildlifeDecision<std::uint8_t>{static_cast<std::uint8_t>((*r & 1) != 0 ? 7 : 12),
                                          d.consumed};
}
WildlifeResult<bool> wildlife_prey(TypeId predator, TypeId prey, std::uint32_t kind,
                                   bool zero_hp_unresolved, WildlifePreyLists lists) {
    // 0x100349c8–0x100349d2: no corpse inference for unresolved zero-HP prey.
    if (zero_hp_unresolved)
        return WildlifeError{WildlifeFault::Oq585ZeroHpPrey};
    // 0x100536b0–0x10053732 and 0x10098513: mask 8, then supplied membership lists.
    const bool extended = contains(lists.extended_predators, predator);
    return WildlifeDecision<bool>{
        kind >= 49 && kind <= 64 &&
            (contains(lists.short_list, prey) || (extended && contains(lists.extended_list, prey))),
        0};
}
} // namespace anno::sim
