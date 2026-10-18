#include <algorithm>
#include <bit>
#include <cmath>

#include <anno/formats/bytes.hpp>
#include <anno/sim/production.hpp>

namespace anno::sim {

namespace fig = formats::figuren;

namespace {

/// MSVC __ftol (AnnoGame.dll 0x10016d60): fistp to a 64-bit integer with
/// truncation, returning its low 32 bits. Non-finite values and values
/// outside int64 give the "integer indefinite" 0x8000000000000000, i.e. 0
/// (Intel SDM, FISTP).
std::uint32_t ftol(double v) {
    if (!std::isfinite(v) || v >= 9223372036854775808.0 || v < -9223372036854775808.0) {
        return 0;
    }
    return static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(static_cast<std::int64_t>(std::trunc(v))));
}

/// The low 16 bits, as the loader stores most results (movw).
std::uint32_t low16(std::uint32_t v) {
    return v & 0xFFFFU;
}

float f32_at(formats::Bytes b, std::size_t o) {
    return std::bit_cast<float>(formats::u32_le(b, o));
}

/// NUL-terminated names in a WARE/ROHSTOFF record.
std::vector<std::string> names(formats::Bytes b) {
    std::vector<std::string> out;
    std::string cur;
    for (const std::uint8_t c : b) {
        if (c == 0) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(static_cast<char>(c));
        }
    }
    return out;
}

/// Names up to the last one that is not UNUSED (the original's count, type
/// +0x1fb = index + 1 of the last non-UNUSED name; D: AnnoGame.dll
/// 0x10009240 ROHSTOFF branch).
std::vector<std::string> used(std::vector<std::string> v) {
    while (!v.empty() && (v.back() == "UNUSED" || v.back().empty())) {
        v.pop_back();
    }
    return v;
}

BuildingClass class_of(const std::string& p) {
    if (p == "HANDWERK") {
        return BuildingClass::Workshop;
    }
    if (p == "HANDWERKDUAL") {
        return BuildingClass::TwoOutputWorkshop;
    }
    if (p == "HANDWERKWAFFEN") {
        return BuildingClass::WeaponsWorkshop;
    }
    if (p == "PLANTAGE" || p == "WEIDETIER" || p == "STEINMETZ" || p == "SCHAFFARM" ||
        p == "FISCHER") {
        return BuildingClass::Farm;
    }
    if (p == "PELZJAEGER") {
        return BuildingClass::FurTrapper;
    }
    if (p == "MINE") {
        return BuildingClass::Mine;
    }
    if (p == "UNIVERSAL") {
        return BuildingClass::SmallFarm;
    }
    if (p == "HAUPT" || p == "MARKT" || p == "KONTOR" || p == "MARKTKOMBI") {
        return BuildingClass::Market;
    }
    if (p == "ROHSTWACHS" || p == "ROHSTDUERR") {
        return BuildingClass::GrowingItem;
    }
    return BuildingClass::Other;
}

} // namespace

formats::Result<std::optional<ProductionType>, std::string>
production_type(const fig::Entry& entry) {
    const auto prod3 = entry.record("PROD3");
    if (!prod3 || prod3->size() != 68) {
        return std::optional<ProductionType>{};
    }
    const auto head = entry.record("HEAD");
    if (!head || head->size() < 0x1a) {
        return "entry " + std::to_string(entry.id) + ": HEAD too short";
    }
    const formats::Bytes p = *prod3;
    ProductionType t;
    t.id = entry.id;
    t.prodtype = entry.record_str("PRODTYPE").value_or("");
    t.kind = class_of(t.prodtype);
    if (const auto w = entry.record("WARE")) {
        t.outputs = used(names(*w));
    }
    if (const auto r = entry.record("ROHSTOFF")) {
        t.inputs = used(names(*r));
    }
    // Load-time conversion, in the order of AnnoGame.dll 0x100095c7–0x10009717
    // (x87 at 53-bit precision = double; docs/sim/production.md).
    const float T0 = f32_at(p, 0x08);
    float T1 = f32_at(p, 0x0c);
    if (!(T1 != 0.0F)) {
        T1 = T0; // fcomps 0.0: equal or unordered (NaN) sets C3 → T1 = T0
    }
    const float I0 = f32_at(p, 0x10), I1 = f32_at(p, 0x14);
    t.time0_128 = ftol(static_cast<double>(T0) * 128.0);
    t.time1_128 = ftol(static_cast<double>(T1) * 128.0);
    const double q0 = 256.0 / static_cast<double>(T0);
    const double q1 = 256.0 / static_cast<double>(T1);
    t.output_per_step = {low16(ftol(q0 + 0.5)), low16(ftol(q1 + 0.5))};
    // Line 0 multiplies the quotient still on the x87 stack; line 1 the one
    // stored to an f32 before (fsts 0x1c(%esp), 0x1000964f).
    t.input_per_step = {
        low16(ftol(q0 * static_cast<double>(I0))),
        low16(ftol(static_cast<double>(static_cast<float>(q1)) * static_cast<double>(I1)))};
    t.capacity = {std::uint32_t{p[0x04]} << 8, std::uint32_t{p[0x06]} << 8};
    t.display_rate0 = low16(ftol(static_cast<double>(f32_at(p, 0x20)) * 256.0));
    t.display_rate1 = low16(ftol(static_cast<double>(f32_at(p, 0x24)) * 256.0));
    for (std::size_t g = 0; g < 3; ++g) {
        t.figure_type_count.push_back(p[0x28 + 3 * g]);
        t.max_figures.push_back(p[0x28 + 3 * g + 1]);
    }
    for (std::size_t k = 0; k < 9; ++k) {
        t.figure_ids.push_back(formats::u16_le(p, 0x32 + 2 * k));
    }
    t.stages = (*head)[0x10];
    t.stage_shift = (*head)[0x18];
    // Growth period (D: 0x100088b0): u32 arithmetic (shll masks the shift to
    // 5 bits, imull/divl wrap), stored as u16. Then the display rate from it
    // (0x100096ad–0x100096db: signed division of 0xEA6000 = 15,360,000 =
    // 60,000 · 256), only when the u16 period is not 0.
    if (t.kind == BuildingClass::GrowingItem && t.stages >= 2) {
        const std::uint32_t div = ((1U << (t.stage_shift & 31U)) * t.stages) << 7;
        if (div != 0) {
            t.growth_period = low16((t.time0_128 * 1000U) / div);
        }
        if (t.growth_period != 0) {
            const auto d = static_cast<std::int32_t>((1U << (t.stage_shift & 31U)) * t.stages *
                                                     t.growth_period);
            if (d != 0) {
                t.display_rate0 = low16(static_cast<std::uint32_t>(15360000 / d));
            }
        }
    }
    return std::optional<ProductionType>(std::move(t));
}

bool StatusTracker::report(Status s, std::uint32_t t, std::uint32_t repeat) {
    if (s < code) {
        return false;
    }
    if (s > code) {
        time = static_cast<std::uint16_t>(t);
        code = s;
    }
    const auto elapsed = static_cast<std::uint16_t>(static_cast<std::uint16_t>(t) - time);
    if (repeat > elapsed) {
        return false;
    }
    if (shows_icons) {
        icons = static_cast<std::uint8_t>(icons | (1U << static_cast<unsigned>(code)));
    }
    time = static_cast<std::uint16_t>(t - repeat);
    return true;
}

void StatusTracker::clear(std::uint8_t mask) {
    if ((icons & mask & 0x1FU) == 0) {
        return;
    }
    for (unsigned c = 0; c < 4; ++c) {
        if ((mask & (1U << c)) == 0) {
            continue;
        }
        if (static_cast<unsigned>(code) == c) {
            code = Status::None;
        }
        icons = static_cast<std::uint8_t>(icons & ~(1U << c));
    }
}

namespace {

/// The step gate: one step per kStepTicks; `last` advances by exactly that
/// (16-bit arithmetic as the original's u16 field).
bool gate(std::uint16_t& last, std::uint32_t t) {
    if (static_cast<std::uint16_t>(static_cast<std::uint16_t>(t) - last) < kStepTicks) {
        return false;
    }
    last = static_cast<std::uint16_t>(last + kStepTicks);
    return true;
}

std::uint32_t at(const std::vector<std::uint32_t>& v, std::size_t i) {
    return i < v.size() ? v[i] : 0;
}

} // namespace

Workshop make_workshop(const ProductionType& type) {
    Workshop w;
    w.out.assign(std::max<std::size_t>(type.outputs.size(), 1), 0);
    w.in.assign(type.inputs.size(), 0);
    w.activity.assign(w.out.size(), 0);
    return w;
}

WeaponsWorkshop make_weapons_workshop(const ProductionType& type) {
    WeaponsWorkshop w;
    const std::size_t lines = std::max<std::size_t>(type.outputs.size(), 1);
    w.line_on.assign(lines, true);
    w.unlocked.assign(lines, true);
    w.out.assign(lines, 0);
    w.in.assign(type.inputs.size(), 0);
    w.activity.assign(lines, 0);
    return w;
}

StepResult weapons_workshop_step(WeaponsWorkshop& w, const ProductionType& type, std::uint32_t t) {
    StepResult r;
    if (!gate(w.last, t)) {
        return r;
    }
    r.stepped = true;
    const std::size_t lines = w.out.size();
    std::vector<bool> working(lines, false);
    std::uint32_t n_working = 0;
    for (std::size_t j = 0; j < lines; ++j) {
        working[j] = j < w.line_on.size() && w.line_on[j] && j < w.unlocked.size() && w.unlocked[j];
        n_working += working[j] ? 1 : 0;
    }
    if (n_working == 0) {
        return r;
    }
    // Only working lines age their activity (0x10069644–0x1006968a).
    for (std::size_t j = 0; j < lines; ++j) {
        if (working[j]) {
            w.activity[j] = static_cast<std::uint16_t>(w.activity[j] * 127 >> 7);
        }
    }
    if (w.suspended) {
        return r;
    }
    const std::size_t n_in = w.in.size();
    std::uint32_t ratio = 128;
    for (std::size_t i = 0; i < n_in; ++i) {
        const std::uint32_t need = at(type.input_per_step, i);
        // need 0: no limit, as in workshop_step (the original divides by it).
        if (need != 0) {
            ratio = std::min(ratio, (w.in[i] << 7) / need);
        }
    }
    r.made.assign(lines, 0);
    const bool blocked_before = w.blocked;
    if (n_in > 0 && ratio < 64) {
        r.status = Status::MissingInput;
        r.message = w.status.report(r.status, t, kRepeatWorkshop);
        w.blocked = true;
    } else {
        // Each working line's share of the step: the original computes
        // (other line working ? 0x40 : 0x80) for its two lines
        // (0x1006973d–0x10069746, 0x10069824–0x10069831); 128 / working
        // lines is the same for two and extends it to other line counts.
        const std::uint32_t share = 128 / n_working;
        // Every line checks the output-0 capacity (type +0xc0), not its own.
        const std::uint32_t cap = at(type.capacity, 0);
        bool made_any = false;
        for (std::size_t j = 0; j < lines; ++j) {
            if (w.out[j] < cap) {
                if (working[j]) {
                    const std::uint32_t part = (share * ratio >> 7) & 0xFF;
                    const std::uint32_t made = at(type.output_per_step, j) * part >> 7;
                    w.out[j] += made;
                    w.activity[j] = static_cast<std::uint16_t>(w.activity[j] + (made << 4));
                    r.made[j] = made;
                    made_any = true;
                }
            } else {
                // Reported for every full line, working or not; the blocked
                // flag is set (and the city told) on the transition.
                r.status = Status::StorageFull;
                r.message = w.status.report(Status::StorageFull, t, kRepeatWorkshop) || r.message;
                w.blocked = true;
            }
        }
        if (made_any) {
            for (std::size_t i = 0; i < n_in; ++i) {
                w.in[i] -= std::min(w.in[i], at(type.input_per_step, i) * ratio >> 7);
            }
            w.status.clear(0x0E);
            w.blocked = false;
        }
    }
    // Net change; a step in which one line is full and another produces sets
    // and clears the flag, so the original notifies the city twice.
    r.blocked_changed = w.blocked != blocked_before;
    r.send_figure = w.carriers_out < at(type.max_figures, 0) && n_in > 0;
    return r;
}

StepResult workshop_step(Workshop& w, const ProductionType& type, std::uint32_t t) {
    StepResult r;
    if (!gate(w.last, t)) {
        return r;
    }
    r.stepped = true;
    if (!w.switched_on) {
        return r;
    }
    for (auto& a : w.activity) {
        a = a * 127 >> 7;
    }
    if (w.suspended) {
        return r;
    }
    const std::size_t n_in = w.in.size();
    std::uint32_t ratio = 128;
    for (std::size_t i = 0; i < n_in; ++i) {
        const std::uint32_t need = at(type.input_per_step, i);
        // need 0: no limit (the original would divide by zero; no workshop
        // of the original data has an input with need 0 — corpus test).
        if (need != 0) {
            ratio = std::min(ratio, (w.in[i] << 7) / need);
        }
    }
    r.made.assign(w.out.size(), 0);
    if (n_in > 0 && ratio < 64) {
        r.status = Status::MissingInput;
    } else if (w.out[0] >= at(type.capacity, 0)) {
        r.status = Status::StorageFull;
    }
    if (r.status != Status::None) {
        r.message = w.status.report(r.status, t, kRepeatWorkshop);
        // The blocked flag is set (and the city told) on the transition
        // (0x10068276–0x10068330).
        r.blocked_changed = !w.blocked;
        w.blocked = true;
    } else {
        const std::uint32_t made = at(type.output_per_step, 0) * ratio >> 7;
        w.out[0] += made;
        w.activity[0] += made << 3;
        r.made[0] = made;
        for (std::size_t i = 0; i < n_in; ++i) {
            w.in[i] -= std::min(w.in[i], at(type.input_per_step, i) * ratio >> 7);
        }
        w.status.clear(0x0E);
        r.blocked_changed = w.blocked;
        w.blocked = false;
    }
    r.send_figure = w.carriers_out < at(type.max_figures, 0) && n_in > 0;
    return r;
}

StepResult farm_step(Farm& f, const ProductionType& type, std::uint32_t t) {
    StepResult r;
    if (!gate(f.last, t)) {
        return r;
    }
    r.stepped = true;
    if (!f.switched_on) {
        return r;
    }
    f.activity = static_cast<std::uint16_t>(f.activity * 255U >> 8);
    if (f.suspended) {
        return r;
    }
    const std::uint32_t max_w = at(type.max_figures, 0);
    const std::uint32_t rate = at(type.output_per_step, 0);
    const std::uint32_t need = at(type.input_per_step, 0);
    std::uint32_t made = 0;
    if (max_w == 1 && f.workers_out > 0) {
        // The single worker is away: no production this step.
    } else if (f.out >= at(type.capacity, 0)) {
        r.status = Status::StorageFull;
        r.message = f.status.report(r.status, t, kRepeatWorkshop);
    } else {
        const std::uint32_t ratio =
            need == 0 ? 128 : std::min<std::uint32_t>(128, (f.raw << 7) / need);
        if (ratio < 64) {
            r.status = Status::MissingInput;
            r.message = f.status.report(r.status, t, kRepeatFarmMissing);
        } else {
            f.raw -= std::min(f.raw, need * ratio >> 7);
            made = rate * ratio >> 7;
            f.out += made;
            f.activity = static_cast<std::uint16_t>(f.activity + (made << 4));
            f.status.clear(0x0E);
            r.blocked_changed = f.blocked;
            f.blocked = false;
        }
    }
    if (r.status != Status::None) {
        // As the workshop (D: 0x1006ecc9, 0x1006ed2a).
        r.blocked_changed = !f.blocked;
        f.blocked = true;
    }
    r.made = {made};
    if ((max_w > 1 || made == 0) && f.workers_out < max_w && rate != 0 && need != 0) {
        // Keep about one and a half output tons of raw stock in work.
        r.send_figure = 384 / rate >= f.raw / need;
    }
    return r;
}

bool grow(GrowthTile& tile, const ProductionType& type, std::uint32_t t) {
    const std::uint32_t half = type.growth_period / 2;
    if (half == 0) {
        return false;
    }
    const std::uint8_t parity = static_cast<std::uint8_t>((t / half) & 1U);
    if (parity == ((tile.phase >> 6) & 1U)) {
        return false;
    }
    const std::uint32_t counter = ((tile.phase & 0x3FU) + 1) & 63U;
    // As 0x1008b6a4–0x1008b6c2: shift masked to 5 bits, an 8-bit multiple-of
    // test, a signed comparison with S << k.
    const std::uint32_t k = type.stage_shift & 31U;
    const auto mask = static_cast<std::uint8_t>((1U << k) - 1U);
    const auto limit = static_cast<std::int32_t>(type.stages << k);
    if ((counter & mask) == 0 && static_cast<std::int32_t>(counter) >= limit) {
        // Next growth object; counter and parity bit are cleared, bit 7 kept
        // (andb $0x80 at 0x1008b6ca).
        tile.object += 1;
        tile.phase = static_cast<std::uint8_t>(tile.phase & 0x80U);
        return true;
    }
    tile.phase =
        static_cast<std::uint8_t>((tile.phase & 0x80U) | (std::uint32_t{parity} << 6U) | counter);
    return false;
}

std::uint32_t CityStock::add(std::uint32_t g, std::uint32_t amount) {
    if (g >= stock.size()) {
        return 0;
    }
    // D: 0x1002da76–0x1002da85: if capacity < stock + amount (u32), amount =
    // capacity − stock (u32, wraps when the stock is above the capacity).
    if (capacity < stock[g] + amount) {
        amount = capacity - stock[g];
    }
    stock[g] += amount;
    return amount;
}

std::uint32_t CityStock::remove(std::uint32_t g, std::uint32_t amount) {
    if (g >= stock.size()) {
        return 0;
    }
    const std::uint32_t removed = std::min(amount, stock[g]);
    stock[g] -= removed;
    return removed;
}

} // namespace anno::sim
