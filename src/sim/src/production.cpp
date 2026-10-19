#include <algorithm>
#include <array>
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

/// The first kWareRecordNames names, up to the last one that is not UNUSED (or
/// empty). The count `index + 1 of the last non-UNUSED name` is what the
/// ROHSTOFF branch stores (type +0x1fb, D: AnnoGame.dll 0x1000a2c9-0x1000a2cf);
/// the WARE branch (0x1000a156-0x1000a205) stores no count, so for WARE this
/// trailing cut is the rebuild's own convention (the goods bytes of an UNUSED
/// name are 0 either way).
std::vector<std::string> used(std::vector<std::string> v) {
    if (v.size() > kWareRecordNames) {
        v.resize(kWareRecordNames);
    }
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

std::vector<std::string> ware_names(formats::Bytes record) {
    return used(names(record));
}

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
        n_working += working[j] ? 1U : 0U;
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

StepResult fur_trapper_step(Farm& f, const ProductionType& type, std::uint32_t t) {
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
    const std::uint32_t rate = at(type.output_per_step, 0);
    const std::uint32_t need = at(type.input_per_step, 0);
    // The ratio starts at 0 while a trapper is out (no production), else at
    // 128 (0x1006e637–0x1006e65f).
    std::uint32_t ratio = f.workers_out > 0 ? 0 : 128;
    std::uint32_t made = 0;
    if (ratio != 0) {
        if (need != 0) {
            ratio = std::min(ratio, (f.raw << 7) / need);
        }
        if (ratio < 64) {
            r.status = Status::MissingInput;
        } else if (f.out >= at(type.capacity, 0)) {
            r.status = Status::StorageFull;
        } else {
            made = rate * ratio >> 7;
            f.out += made;
            f.activity = static_cast<std::uint16_t>(f.activity + (made << 4));
            f.raw -= std::min(f.raw, need * ratio >> 7);
            f.status.clear(0x0E);
            r.blocked_changed = f.blocked;
            f.blocked = false;
        }
        if (r.status != Status::None) {
            // Both codes repeat after 45,000 ticks (0x1006e862, 0x1006e8d4).
            r.message = f.status.report(r.status, t, kRepeatWorkshop);
            r.blocked_changed = !f.blocked;
            f.blocked = true;
        }
    }
    r.made = {made};
    if (ratio < 128 && f.workers_out == 0 && rate != 0 && need != 0) {
        r.send_figure = 384 / rate >= f.raw / need;
    }
    return r;
}

SmallFarm make_small_farm(const ProductionType& type, const SmallFarmAnimals& animals) {
    SmallFarm f;
    const std::size_t lines = std::max<std::size_t>(type.outputs.size(), 1);
    f.line_on.assign(lines, true);
    f.out.assign(lines, 0);
    f.raw.assign(lines, 0);
    f.activity.assign(lines, 0);
    // The original has five counters (+0x2a–+0x2e: line 0's workers and four
    // animal kinds); a modded table that can pick a kind further up gets the
    // counters it needs.
    std::size_t counters = 5;
    for (const std::uint16_t kind : animals.kind_by_draw) {
        if (kind >= animals.base) {
            counters =
                std::max(counters, static_cast<std::size_t>(kind - animals.base) + std::size_t{1});
        }
    }
    f.figures_out.assign(counters, 0);
    return f;
}

namespace {

/// One small-farm line (D: 0x1006f440(line, rate, need, t)); returns what
/// it made.
std::uint32_t small_farm_line(SmallFarm& f, const ProductionType& type, std::size_t line,
                              std::uint32_t t, StepResult& r) {
    const std::uint32_t cap = at(type.capacity, 0);
    auto report = [&](Status s, std::uint32_t repeat) {
        r.status = s;
        r.message = f.status.report(s, t, repeat) || r.message;
        f.blocked = true;
    };
    if (f.out[line] >= cap) {
        report(Status::StorageFull, kRepeatWorkshop);
        return 0;
    }
    const std::uint32_t rate = at(type.output_per_step, line);
    const std::uint32_t need = at(type.input_per_step, line);
    // need 0: no limit (the original divides by it; corpus test).
    const std::uint32_t q = need == 0 ? 128 : (f.raw[line] << 7) / need;
    if (q < 64) {
        report(Status::MissingInput, kRepeatSmallFarmMissing);
        return 0;
    }
    const std::uint32_t ratio = std::min<std::uint32_t>(q, 128);
    const std::uint32_t used = ratio * need >> 7;
    // A step that would use nothing makes nothing and reports nothing
    // (0x1006f4b3, the u16 of the amount).
    if ((used & 0xFFFF) == 0) {
        return 0;
    }
    f.raw[line] -= std::min(f.raw[line], used);
    const std::uint32_t made = ratio * rate >> 7;
    f.out[line] += made;
    f.activity[line] = static_cast<std::uint16_t>(f.activity[line] + (made << 4));
    // The status is cleared unless the other line is switched on and full
    // (0x1006f4da–0x1006f4f0).
    const std::size_t other = line ^ 1U;
    const bool other_blocks =
        other < f.out.size() && f.out[other] >= cap && other < f.line_on.size() && f.line_on[other];
    if (!other_blocks) {
        f.status.clear(0x0E);
        f.blocked = false;
    }
    return made;
}

/// 384 / rate ≥ raw / need: about one and a half output tons of raw stock
/// in work (farm rule, 0x1006f75b–0x1006f779).
bool wants_raw(std::uint32_t rate, std::uint32_t need, std::uint32_t raw) {
    return rate != 0 && need != 0 && 384 / rate >= raw / need;
}

} // namespace

StepResult small_farm_step(SmallFarm& f, const ProductionType& type, std::uint32_t t, DrawRef draw,
                           const SmallFarmAnimals& animals) {
    StepResult r;
    // The per-line vectors have one size and there is a counter for line 0
    // (SmallFarm invariant); a farm that breaks it does nothing, so no index
    // below can leave its vector.
    if (f.out.size() != f.line_on.size() || f.raw.size() != f.line_on.size() ||
        f.activity.size() != f.line_on.size() || f.figures_out.empty()) {
        return r;
    }
    if (!gate(f.last, t)) {
        return r;
    }
    r.stepped = true;
    const auto on = [&](std::size_t line) { return line < f.line_on.size() && f.line_on[line]; };
    if (!on(0) && !on(1)) {
        return r;
    }
    const auto age = [&](std::size_t line) {
        if (line < f.activity.size()) {
            f.activity[line] = static_cast<std::uint16_t>(f.activity[line] * 255U >> 8);
        }
    };
    if (f.suspended) {
        // Both lines age, switched on or not (0x1006f6ab–0x1006f6e2).
        age(0);
        age(1);
        return r;
    }
    r.made.assign(f.out.size(), 0);
    const bool blocked_before = f.blocked;
    // Net change of the blocked flag; each set or clear notifies the city.
    const auto done = [&]() -> StepResult {
        r.blocked_changed = f.blocked != blocked_before;
        return r;
    };
    // The original tests only the low 16 bits of what a line made (0x1006f739,
    // 0x1006f857); made <= rate, so the difference from the full u32 is
    // unreachable.
    const auto made_any = [](std::uint32_t made) { return static_cast<std::uint16_t>(made) != 0; };
    if (on(0)) {
        age(0);
        const std::uint32_t made = small_farm_line(f, type, 0, t, r);
        r.made[0] = made;
        if (!made_any(made) && f.figures_out[0] < 1 &&
            wants_raw(at(type.output_per_step, 0), at(type.input_per_step, 0), f.raw[0])) {
            r.send_figure = true;
        }
    }
    if (on(1)) {
        age(1);
        const std::uint32_t made = small_farm_line(f, type, 1, t, r);
        r.made[1] = made;
        // The draw is taken here, once, whatever the limits say (D
        // 0x1006f810–0x1006f838): after the line step, before the limit tests.
        const std::uint16_t drawn = draw();
        // The next three returns are defensive: the config validation
        // rejects an empty table and kinds outside base..base+counters. The
        // original reads outside its tables there; here the draw is consumed
        // and nothing is sent (fail-closed deviation, docs/sim/state.md F21).
        if (animals.kind_by_draw.empty()) {
            return done();
        }
        const std::uint16_t kind =
            animals.kind_by_draw[static_cast<std::size_t>(drawn) % animals.kind_by_draw.size()];
        if (kind < animals.base) {
            return done();
        }
        const std::size_t idx = static_cast<std::size_t>(kind - animals.base);
        if (idx >= f.figures_out.size() || idx >= animals.limit.size()) {
            return done();
        }
        // A line that made something counts one animal fewer (0x1006f855–
        // 0x1006f86b); signed as the original.
        const std::int64_t out_now =
            static_cast<std::int64_t>(f.figures_out[idx]) - (made_any(made) ? 1 : 0);
        if (static_cast<std::int64_t>(animals.limit[idx]) <= out_now) {
            return done();
        }
        std::uint32_t animals_out = 0;
        for (std::size_t i = 1; i < f.figures_out.size(); ++i) {
            animals_out += f.figures_out[i];
        }
        if (at(type.max_figures, 0) <= animals_out) {
            return done();
        }
        if (wants_raw(at(type.output_per_step, 1), at(type.input_per_step, 1), f.raw[1])) {
            r.send_animal = true;
            r.animal_kind = kind;
        }
    }
    return done();
}

std::uint32_t deposit_remaining(const Deposit& d, bool deep) {
    return d.reserve && !deep ? d.remaining - kDepositReserve : d.remaining;
}

std::uint32_t deposit_extract(Deposit& d, std::uint32_t rate, bool deep, bool depletes) {
    if (d.reserve) {
        if (deep || !depletes) {
            return rate;
        }
        // Stop at the reserve: take remaining − reserve when the rate would
        // reach into it (unsigned compare, 0x1006369f–0x100636a9).
        const std::uint32_t take =
            d.remaining - rate < kDepositReserve ? d.remaining - kDepositReserve : rate;
        d.remaining -= take;
        return take;
    }
    if (!depletes) {
        return rate;
    }
    const std::uint32_t take = std::min(rate, d.remaining);
    d.remaining -= take;
    return take;
}

bool deposit_in_reach(const MineArea& area, std::int32_t x, std::int32_t y) {
    // Half-width of the radius-7 circle per row beyond the footprint
    // (emulated from 0x100787cb–0x10078805).
    static constexpr std::array<std::int32_t, 8> kHalfWidth = {7, 7, 7, 6, 6, 5, 4, 2};
    // Rows: above the footprint the distance is area.y − y, below it
    // y − (area.y + h − 1); inside, it is not positive.
    std::int32_t dy = area.y - y;
    if (dy < 0) {
        dy = 1 - static_cast<std::int32_t>(area.h) - dy;
    }
    if (dy > 7) {
        return false;
    }
    dy = std::max(dy, 0);
    std::int32_t dx = area.x - x;
    if (dx < 0) {
        dx = 1 - static_cast<std::int32_t>(area.w) - dx;
    }
    return dx <= kHalfWidth[static_cast<std::size_t>(dy)];
}

std::optional<std::size_t> find_deposit(std::span<const Deposit> deposits, const std::string& goods,
                                        bool deep, const MineArea& area) {
    std::optional<std::size_t> best;
    for (std::size_t i = 0; i < deposits.size(); ++i) {
        const Deposit& d = deposits[i];
        if (d.goods != goods || !deposit_in_reach(area, d.x, d.y)) {
            continue;
        }
        // The first candidate is taken as it is; later ones only if they
        // hold more (0x100788f3–0x10078923).
        if (!best || deposit_remaining(deposits[*best], deep) < deposit_remaining(d, deep)) {
            best = i;
        }
    }
    return best;
}

bool mine_is_deep(const ProductionType& type, std::uint32_t research_id) {
    // Goods 1 is EISENERZ (docs/re/production.md §7); research 0x39 is
    // TIEFE_ERZMINE.
    const bool iron = !type.outputs.empty() && type.outputs[0] == "EISENERZ";
    return !iron || research_id == 0x39;
}

StepResult mine_step(Mine& m, const ProductionType& type, std::uint32_t t,
                     std::vector<Deposit>& deposits, const MineArea& area, bool depletes) {
    StepResult r;
    if (!gate(m.last, t)) {
        return r;
    }
    r.stepped = true;
    if (!m.switched_on) {
        return r;
    }
    m.activity = static_cast<std::uint16_t>(m.activity * 63U >> 6);
    r.made = {0};
    const bool blocked_before = m.blocked;
    auto report = [&](Status s, std::uint32_t repeat) {
        r.status = s;
        r.message = m.status.report(s, t, repeat);
        m.blocked = true;
        r.blocked_changed = !blocked_before;
    };
    if (m.out >= at(type.capacity, 0)) {
        report(Status::StorageFull, kRepeatWorkshop);
        return r;
    }
    if (!m.deposit || *m.deposit >= deposits.size()) {
        report(Status::MissingInput, kRepeatMineMissing);
        return r;
    }
    const std::uint32_t rate = at(type.output_per_step, 0);
    Deposit& dep = deposits[*m.deposit];
    const std::uint32_t left = deposit_remaining(dep, m.deep);
    const std::uint32_t got = deposit_extract(dep, rate, m.deep, depletes);
    // Search again when the step was short although something was left, or
    // when this step used the rest up (0x1006df9a–0x1006dfac).
    const bool short_step = got < rate && left != 0;
    if (short_step || (got != 0 && rate >= left)) {
        const std::string goods = type.outputs.empty() ? std::string() : type.outputs[0];
        m.deposit = find_deposit(deposits, goods, m.deep, area);
        const bool found = m.deposit && deposit_remaining(deposits[*m.deposit], m.deep) != 0;
        if (!found && goods == "EISENERZ") {
            // 0x15b when the deposit found still has deep ore, else 0x19.
            const bool deep_left = m.deposit && deposit_remaining(deposits[*m.deposit], true) != 0;
            r.notice = deep_left ? std::uint16_t{0x15B} : std::uint16_t{0x19};
        }
    }
    if (got == 0) {
        report(Status::MissingInput, kRepeatMineMissing);
        return r;
    }
    m.out += got;
    m.activity = static_cast<std::uint16_t>(m.activity + (got << 4));
    r.made = {got};
    m.status.clear(0x0E);
    m.blocked = false;
    r.blocked_changed = blocked_before;
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
    // D: 0x1002da76–0x1002da85: if capacity < stock + amount, amount = capacity −
    // stock (u32, wraps when the stock is above the capacity). The sum is formed in
    // u64 here (F35): the original's u32 sum wraps for an amount near 2^32, which
    // lowers the stock; no amount a game reaches is that large, so the two agree on
    // every original state.
    if (std::uint64_t{capacity} < std::uint64_t{stock[g]} + amount) {
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
