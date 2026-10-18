#include <algorithm>
#include <bit>
#include <cmath>

#include <anno/formats/bytes.hpp>
#include <anno/sim/production.hpp>

namespace anno::sim {

namespace fig = formats::figuren;

namespace {

/// MSVC __ftol (x87 fistp with truncation): the value truncated toward
/// zero, or the "integer indefinite" 0x80000000 when it is not finite or
/// outside int32 (Intel SDM, FIST/FISTP).
std::uint32_t ftol(double v) {
    if (!std::isfinite(v) || v >= 2147483648.0 || v <= -2147483649.0) {
        return 0x80000000U;
    }
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(std::trunc(v)));
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
    if (p == "HANDWERKWAFFEN") {
        return BuildingClass::TwoOutputWorkshop;
    }
    if (p == "PLANTAGE" || p == "WEIDETIER" || p == "STEINMETZ" || p == "SCHAFFARM" ||
        p == "FISCHER" || p == "PELZJAEGER") {
        return BuildingClass::Farm;
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
    if (T1 == 0.0F) {
        T1 = T0; // fcomps 0.0; equal → T1 = T0
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
    // Growth period (D: 0x100088b0) and the display rate derived from it
    // (0x100096ad–0x100096db, 0xEA6000 = 15,360,000 = 60,000 · 256).
    if (t.kind == BuildingClass::GrowingItem && t.stages > 1 && t.stage_shift < 31) {
        const std::uint64_t div = (std::uint64_t{1} << t.stage_shift) * t.stages * 128;
        t.growth_period = static_cast<std::uint32_t>(std::uint64_t{t.time0_128} * 1000 / div);
        if (t.growth_period != 0) {
            t.display_rate0 = low16(static_cast<std::uint32_t>(
                15360000ULL / ((std::uint64_t{1} << t.stage_shift) * t.stages * t.growth_period)));
        }
    }
    return std::optional<ProductionType>(std::move(t));
}

bool StatusTracker::report(Status s, std::uint32_t t, std::uint32_t repeat) {
    const bool raise = s != code || t - last_t >= repeat;
    if (raise) {
        code = s;
        last_t = t;
    }
    return raise;
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
        r.message = w.status.report(r.status, t, kRepeatWorkshop);
    } else if (w.out[0] >= at(type.capacity, 0)) {
        r.status = Status::StorageFull;
        r.message = w.status.report(r.status, t, kRepeatWorkshop);
    } else {
        const std::uint32_t made = at(type.output_per_step, 0) * ratio >> 7;
        w.out[0] += made;
        w.activity[0] += made << 3;
        r.made[0] = made;
        for (std::size_t i = 0; i < n_in; ++i) {
            w.in[i] -= std::min(w.in[i], at(type.input_per_step, i) * ratio >> 7);
        }
        w.status.code = Status::None;
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
    f.activity = f.activity * 255 >> 8;
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
            f.activity += made << 4;
            f.status.code = Status::None;
        }
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
    std::uint32_t counter = ((tile.phase & 0x3FU) + 1) & 63U;
    bool changed = false;
    if (counter % (1U << type.stage_shift) == 0 && counter >= (type.stages << type.stage_shift)) {
        tile.object += 1;
        counter = 0;
        changed = true;
    }
    tile.phase =
        static_cast<std::uint8_t>((tile.phase & 0x80U) | (std::uint32_t{parity} << 6U) | counter);
    return changed;
}

std::uint32_t CityStock::add(std::uint32_t g, std::uint32_t amount) {
    if (g >= stock.size()) {
        return 0;
    }
    const std::uint32_t room = stock[g] < capacity ? capacity - stock[g] : 0;
    const std::uint32_t added = std::min(amount, room);
    stock[g] += added;
    return added;
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
