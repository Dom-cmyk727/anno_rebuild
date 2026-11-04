#include <anno/sim/inn.hpp>

#include <algorithm>

namespace anno::sim {

InnTickResult inn_step(Inn& inn, std::uint32_t tick, std::int32_t limit) {
    InnTickResult r;
    const std::uint32_t diff = (tick - std::uint32_t{inn.last}) & 0xffffU;
    if (diff < kInnGatePeriod) {
        return r;
    }
    r.stepped = true;
    inn.last = static_cast<std::uint16_t>(inn.last + kInnGatePeriod);
    // (31 * taken) / 32 from the zero-extended u16: the signed divide of the original equals the
    // floor for these non-negative values.
    inn.taken = (31U * (inn.taken & 0xffffU)) / 32U;
    if (inn.suspended) {
        return r;
    }
    if (!(limit > static_cast<std::int32_t>(inn.carriers_out))) {
        return r;
    }
    const std::uint32_t stock = inn.tavern & 0xffffU;
    r.send = stock < kInnRefillStock || stock < (inn.taken >> 1);
    return r;
}

std::int64_t inn_take(Inn& inn, std::int64_t request) {
    const std::int64_t stock = inn.tavern & 0xffffU;
    const std::int64_t n = std::min(request, stock);
    inn.taken = static_cast<std::uint32_t>((std::int64_t{inn.taken} + n) & 0xffff);
    inn.tavern = static_cast<std::uint32_t>((stock - n) & 0xffff);
    return n;
}

void inn_unload(Inn& inn, std::uint32_t amount) {
    inn.tavern = (inn.tavern + amount) & 0xffffU;
}

std::optional<std::int32_t> base_priority(std::uint32_t stock, std::uint32_t need) {
    if (need == 0) {
        return std::nullopt;
    }
    const std::uint32_t steps = stock / need;
    if (steps < 30) {
        return 0x80;
    }
    if (steps < 60) {
        return 0x60;
    }
    return -1;
}

} // namespace anno::sim
