#include <algorithm>
#include <bit>
#include <limits>

#include <anno/sim_load/wildlife_geometry.hpp>

namespace anno::sim_load {
namespace {
using Error = WildlifeGeometryError;
using Stage = WildlifeGeometryStage;
bool valid(WildlifeFpProfile p) {
    return (p.precision == 24 || p.precision == 53 || p.precision == 64) &&
           (p.rounding == WildlifeRounding::Nearest || p.rounding == WildlifeRounding::Down ||
            p.rounding == WildlifeRounding::Up || p.rounding == WildlifeRounding::TowardZero);
}
unsigned width_of(std::uint64_t n) {
    return static_cast<unsigned>(std::bit_width(n));
}
// Internal exact workspace for <=64-bit significands and checked alignment/division.
// Eight u32 limbs cover <=128-bit products and the bounded alignment below; not a numeric API.
struct Wide {
    std::array<std::uint32_t, 8> w{};
    explicit Wide(std::uint64_t n = 0) {
        w[0] = static_cast<std::uint32_t>(n);
        w[1] = static_cast<std::uint32_t>(n >> 32);
    }
    unsigned width() const {
        for (unsigned i = 8; i > 0; --i)
            if (w[i - 1])
                return (i - 1) * 32 + width_of(w[i - 1]);
        return 0;
    }
    bool bit(unsigned i) const { return i < 256 && ((w[i / 32] >> (i % 32)) & 1U) != 0; }
    void set(unsigned i) { w[i / 32] |= std::uint32_t{1} << (i % 32); }
    int compare(const Wide& b) const {
        for (unsigned i = 8; i > 0; --i) {
            if (w[i - 1] < b.w[i - 1])
                return -1;
            if (w[i - 1] > b.w[i - 1])
                return 1;
        }
        return 0;
    }
    Wide left(unsigned n) const {
        Wide out;
        for (unsigned i = 0; i < width(); ++i)
            if (bit(i) && i + n < 256)
                out.set(i + n);
        return out;
    }
    Wide right(unsigned n) const {
        Wide out;
        for (unsigned i = n; i < width(); ++i)
            if (bit(i))
                out.set(i - n);
        return out;
    }
    Wide plus(const Wide& b) const {
        Wide out;
        std::uint64_t carry = 0;
        for (unsigned i = 0; i < 8; ++i) {
            const std::uint64_t sum = std::uint64_t{w[i]} + b.w[i] + carry;
            out.w[i] = static_cast<std::uint32_t>(sum);
            carry = sum >> 32;
        }
        return out;
    }
    Wide minus(const Wide& b) const {
        Wide out;
        std::uint64_t borrow = 0;
        for (unsigned i = 0; i < 8; ++i) {
            const auto v = (std::uint64_t{1} << 32) + w[i] - b.w[i] - borrow;
            out.w[i] = static_cast<std::uint32_t>(v);
            borrow = (v >> 32) == 0 ? 1U : 0U;
        }
        return out;
    }
    std::uint64_t low() const { return w[0] | (std::uint64_t{w[1]} << 32); }
};
struct Number {
    std::uint64_t sig = 0;
    std::int32_t exponent = 0;
    bool negative = false;
};
Number number(std::uint64_t sig, std::int32_t exponent = 0, bool negative = false) {
    if (sig) {
        const auto n = std::countr_zero(sig);
        sig >>= n;
        exponent += n;
    }
    return {sig, exponent, negative};
}
class Arithmetic {
  public:
    WildlifeFpProfile profile;
    std::optional<Error> error;
    explicit Arithmetic(WildlifeFpProfile p) : profile(p) {
        if (!valid(p))
            fail("unsupported precision/rounding");
    }
    void fail(std::string reason) {
        if (!error)
            error = Error{Stage::Numeric, std::move(reason)};
    }
    Number rounded(Wide raw, std::int32_t exponent, bool negative, unsigned precision) {
        if (error)
            return {};
        const unsigned width = raw.width();
        if (!width)
            return number(0, 0, negative);
        const auto e = exponent + static_cast<std::int32_t>(width) - 1;
        if (e < -126 || e > 100) {
            fail("normal finite arithmetic range exceeded");
            return {};
        }
        const auto shift = width > precision ? width - precision : 0;
        auto q = raw.right(shift);
        bool remainder = false, lower = false;
        for (unsigned i = 0; i < shift; ++i) {
            remainder |= raw.bit(i);
            if (i + 1 < shift)
                lower |= raw.bit(i);
        }
        bool increment = false;
        if (profile.rounding == WildlifeRounding::Nearest && shift)
            increment = raw.bit(shift - 1) && (lower || q.bit(0));
        else if (remainder)
            increment = (profile.rounding == WildlifeRounding::Up && !negative) ||
                        (profile.rounding == WildlifeRounding::Down && negative);
        if (increment)
            q = q.plus(Wide{1});
        auto out_exp = exponent + static_cast<std::int32_t>(shift);
        if (q.width() > precision) {
            q = q.right(1);
            ++out_exp;
        }
        return number(q.low(), out_exp, negative);
    }
    Number decode(std::uint32_t word) {
        const auto exp = (word >> 23) & 255U, frac = word & 0x7fffffU;
        if (exp == 0 && frac == 0)
            return number(0, 0, (word >> 31) != 0);
        if (exp == 0 || exp == 255) {
            fail("normal finite words or zero required");
            return {};
        }
        return number((1U << 23) | frac, static_cast<std::int32_t>(exp) - 150, (word >> 31) != 0);
    }
    Number add(Number a, Number b) {
        if (!a.sig && !b.sig)
            return number(0, 0,
                          a.negative == b.negative ? a.negative
                                                   : profile.rounding == WildlifeRounding::Down);
        const auto exp = std::min(a.exponent, b.exponent);
        const auto sa = static_cast<unsigned>(a.exponent - exp),
                   sb = static_cast<unsigned>(b.exponent - exp);
        if (sa + width_of(a.sig) > 255 || sb + width_of(b.sig) > 255) {
            fail("significand alignment outside bounded profile");
            return {};
        }
        const auto wa = Wide{a.sig}.left(sa), wb = Wide{b.sig}.left(sb);
        if (a.negative == b.negative)
            return rounded(wa.plus(wb), exp, a.negative, profile.precision);
        const auto cmp = wa.compare(wb);
        if (cmp == 0)
            return number(0, 0, profile.rounding == WildlifeRounding::Down);
        return rounded(cmp > 0 ? wa.minus(wb) : wb.minus(wa), exp,
                       cmp > 0 ? a.negative : b.negative, profile.precision);
    }
    Number sub(Number a, Number b) {
        b.negative = !b.negative;
        return add(a, b);
    }
    Number mul(Number a, Number b) {
        Wide product;
        for (unsigned i = 0; i < 64; ++i)
            if ((b.sig >> i) & 1U)
                product = product.plus(Wide{a.sig}.left(i));
        return rounded(product, a.exponent + b.exponent, a.negative != b.negative,
                       profile.precision);
    }
    Number div(Number a, Number b) {
        if (!b.sig) {
            fail("zero divisor");
            return {};
        }
        if (!a.sig)
            return number(0, 0, a.negative != b.negative);
        auto ratio =
            static_cast<std::int32_t>(width_of(a.sig)) - static_cast<std::int32_t>(width_of(b.sig));
        if ((ratio >= 0 ? Wide{a.sig}.compare(Wide{b.sig}.left(static_cast<unsigned>(ratio)))
                        : Wide{a.sig}.left(static_cast<unsigned>(-ratio)).compare(Wide{b.sig})) < 0)
            --ratio;
        const auto shift = static_cast<std::int32_t>(profile.precision) - 1 - ratio;
        const auto numerator =
            shift >= 0 ? Wide{a.sig}.left(static_cast<unsigned>(shift)) : Wide{a.sig};
        const auto denominator =
            shift < 0 ? Wide{b.sig}.left(static_cast<unsigned>(-shift)) : Wide{b.sig};
        Wide quotient, remainder;
        for (unsigned i = numerator.width(); i > 0; --i) {
            remainder = remainder.left(1);
            if (numerator.bit(i - 1))
                remainder.w[0] |= 1;
            if (remainder.compare(denominator) >= 0) {
                remainder = remainder.minus(denominator);
                quotient.set(i - 1);
            }
        }
        const auto comparison = remainder.left(1).compare(denominator);
        const bool negative = a.negative != b.negative;
        const bool increment = profile.rounding == WildlifeRounding::Nearest
                                   ? comparison > 0 || (comparison == 0 && quotient.bit(0))
                                   : remainder.width() != 0 &&
                                         ((profile.rounding == WildlifeRounding::Up && !negative) ||
                                          (profile.rounding == WildlifeRounding::Down && negative));
        if (increment)
            quotient = quotient.plus(Wide{1});
        auto exp = a.exponent - b.exponent - shift;
        if (quotient.width() > profile.precision) {
            quotient = quotient.right(1);
            ++exp;
        }
        const auto e = exp + static_cast<std::int32_t>(quotient.width()) - 1;
        if (e < -126 || e > 100) {
            fail("normal finite quotient range exceeded");
            return {};
        }
        return number(quotient.low(), exp, negative);
    }
    Number spill(Number a) { return rounded(Wide{a.sig}, a.exponent, a.negative, 24); }
    std::uint32_t word(Number a) {
        a = spill(a);
        if (!a.sig)
            return a.negative ? 0x80000000U : 0U;
        const auto width = width_of(a.sig);
        const auto e = a.exponent + static_cast<std::int32_t>(width) - 1;
        const auto sig = a.sig << (24 - width);
        return (a.negative ? 0x80000000U : 0U) | (static_cast<std::uint32_t>(e + 127) << 23) |
               static_cast<std::uint32_t>(sig & 0x7fffffU);
    }
    int compare(Number a, Number b) {
        if (!a.sig && !b.sig)
            return 0;
        if (a.negative != b.negative)
            return a.negative ? -1 : 1;
        const auto e = std::min(a.exponent, b.exponent);
        const auto sa = static_cast<unsigned>(a.exponent - e),
                   sb = static_cast<unsigned>(b.exponent - e);
        if (sa + width_of(a.sig) > 256 || sb + width_of(b.sig) > 256) {
            fail("comparison alignment outside bounded profile");
            return 0;
        }
        const auto c = Wide{a.sig}.left(sa).compare(Wide{b.sig}.left(sb));
        return a.negative ? -c : c;
    }
};
} // namespace
formats::Result<WildlifeNumericContext, Error>
prepare_wildlife_numeric_context(const WildlifeNumericInputs& input) {
    if (!input.table || !input.callback || !valid(*input.table) || !valid(*input.callback) ||
        input.provenance.empty())
        return Error{Stage::Context,
                     "explicit independent table/callback profiles and provenance required"};
    Arithmetic a{*input.table};
    Number acc;
    const auto c = a.decode(0x3d23d70aU);
    std::array<std::uint32_t, 256> table{};
    for (auto& word : table) {
        word = a.word(acc);
        acc = a.add(acc, c);
    }
    if (a.error)
        return *a.error;
    return WildlifeNumericContext{table, *input.table, *input.callback, input.provenance};
}
formats::Result<std::array<std::uint32_t, 2>, Error>
wildlife_ordinary_position(const WildlifePositionInput& in, WildlifeFpProfile profile) {
    if (!valid(profile))
        return Error{Stage::Context, "unsupported callback profile"};
    if (in.origin_x > 65535 || in.origin_z > 65535 || in.chunk_x > 255 || in.chunk_z > 255 ||
        in.packed > 255 || in.outcome > 3)
        return Error{Stage::Position, "outside original word/byte position profile"};
    const auto tx = in.origin_x + in.chunk_x * 8 + (in.packed & 7U),
               tz = in.origin_z + in.chunk_z * 8 + ((in.packed >> 3) & 7U);
    Arithmetic a{profile};
    const auto offset = a.decode(0x3e2aaaabU), one = number(1);
    const auto coordinate = [&](std::uint32_t tile, bool high) {
        // Welt 0x10092dcd: subtract scale*a AFTER (tile+1)*scale, no rounded 5/6.
        return a.word(high ? a.sub(a.mul(a.add(number(tile), one), one), a.mul(one, offset))
                           : a.mul(a.add(number(tile), offset), one));
    };
    const auto x = coordinate(tx, in.outcome == 1 || in.outcome == 2);
    const auto z = coordinate(tz, in.outcome == 2 || in.outcome == 3);
    if (a.error)
        return *a.error;
    return std::array<std::uint32_t, 2>{x, z};
}
formats::Result<WildlifeHeightResult, Error>
wildlife_ordinary_height(std::uint32_t xword, std::uint32_t zword,
                         const std::array<std::uint32_t, 4>& words, WildlifeFpProfile profile) {
    if (!valid(profile))
        return Error{Stage::Context, "unsupported callback profile"};
    Arithmetic a{profile};
    const auto x = a.decode(xword), z = a.decode(zword);
    std::array<Number, 4> heights;
    for (std::size_t i = 0; i < 4; ++i) {
        heights[i] = a.decode(words[i]);
        if (heights[i].negative)
            return Error{Stage::Height, "nonnegative source table words required"};
    }
    if (a.error)
        return *a.error;
    const auto integer = [&](Number n) -> std::optional<std::uint32_t> {
        if (n.negative)
            return {};
        if (n.exponent >= 0) {
            if (width_of(n.sig) + static_cast<unsigned>(n.exponent) > 32)
                return {};
            return static_cast<std::uint32_t>(n.sig << static_cast<unsigned>(n.exponent));
        }
        const auto shift = static_cast<unsigned>(-n.exponent);
        return shift >= 64 ? 0U : static_cast<std::uint32_t>(n.sig >> shift);
    };
    const auto ix = integer(x), iz = integer(z);
    if (!ix || !iz || *ix > 67582 || *iz > 67582)
        return Error{Stage::Height, "outside ordinary source-width coordinates"};
    const auto x0 = number(*ix), z0 = number(*iz), x1 = number(*ix + 1), z1 = number(*iz + 1);
    if (a.compare(x0, x) >= 0 || a.compare(x, x1) >= 0 || a.compare(z0, z) >= 0 ||
        a.compare(z, z1) >= 0)
        return Error{Stage::Height, "ordinary interior query required"};
    const auto edge = [&](Number h0, Number h1, Number delta, Number span) {
        return a.add(a.div(a.mul(a.sub(h1, h0), delta), span), h0);
    };
    const auto d = a.spill(a.sub(z1, z0)), e = a.spill(a.sub(z1, z0));
    const auto t = a.spill(a.sub(z, z0)), u = a.spill(a.sub(z, z0));
    auto left = a.spill(edge(x0, x0, t, d)), right = a.spill(edge(x1, x0, u, e));
    Number low, high;
    std::uint8_t triangle = 1;
    if (a.compare(left, x) <= 0 && a.compare(x, right) <= 0) {
        // Required FSTP at Welt 0x100b5649, not a retained extended left height.
        low = a.spill(edge(heights[0], heights[2], t, d));
        high = edge(heights[1], heights[2], u, e);
    } else {
        right = a.spill(edge(x1, x1, t, d));
        left = a.spill(edge(x1, x0, t, e));
        if (a.compare(left, x) > 0 || a.compare(x, right) > 0)
            return Error{Stage::Height, "ordinary interior triangle admission failed"};
        high = edge(heights[1], heights[3], t, d);
        low = edge(heights[1], heights[2], t, e);
        triangle = 2;
    }
    const auto y =
        a.compare(left, right) == 0
            ? high
            : a.add(a.div(a.mul(a.sub(high, low), a.sub(x, left)), a.sub(right, left)), low);
    if (a.error)
        return *a.error;
    if (y.sig == 0 && profile.rounding == WildlifeRounding::Down)
        return Error{Stage::Numeric, "downward zero endpoint outside reviewed reference profile"};
    const auto word = a.word(y);
    if (a.error)
        return *a.error;
    return WildlifeHeightResult{word, triangle};
}
formats::Result<WildlifeOrdinarySource, Error>
bind_wildlife_ordinary_source(const WildlifeFirstCallResult& prepared,
                              const WildlifeImportPlan& plan, const WildlifeNumericContext& numeric,
                              std::string provenance) {
    const auto& is = prepared.fill.island;
    const auto& tiles = is.resource.tiles;
    if (provenance.empty() || prepared.source_sha256.empty() || !is.complete_local_records ||
        prepared.source_sha256 != plan.imported.scene_sha256 ||
        prepared.source_sha256 != plan.final_facts.scene_sha256 ||
        prepared.ordinal != plan.final_facts.island_ordinal ||
        plan.imported.chunk_x != plan.final_facts.chunk_x ||
        plan.imported.chunk_y != plan.final_facts.chunk_y || tiles.x0() < 0 || tiles.y0() < 0 ||
        tiles.x0() > 65535 || tiles.y0() > 65535 || tiles.chunk_edge() != 8 ||
        plan.imported.chunk_x > 255 || plan.imported.chunk_y > 255)
        return Error{Stage::Source, "same completed fresh source/geometry/provenance required"};
    if (!is.terrain || is.terrain->height.size() != tiles.tile_count() ||
        !sim::terrain_matches(*is.terrain))
        return Error{Stage::Source, "complete immutable terrain columns/digest required"};
    if (std::uint64_t{plan.imported.chunk_x} * 8 + 8 > tiles.width() ||
        std::uint64_t{plan.imported.chunk_y} * 8 + 8 > tiles.height() ||
        plan.final_facts.world_x != std::int64_t{tiles.x0()} + plan.imported.chunk_x * 8 ||
        plan.final_facts.world_y != std::int64_t{tiles.y0()} + plan.imported.chunk_y * 8)
        return Error{Stage::Source, "source chunk outside matching prepared geometry"};
    WildlifeOrdinarySource out{numeric};
    out.terrain_ = is.terrain;
    out.x_ = static_cast<std::uint32_t>(tiles.x0());
    out.z_ = static_cast<std::uint32_t>(tiles.y0());
    out.width_ = tiles.width();
    out.height_ = tiles.height();
    out.cx_ = plan.imported.chunk_x;
    out.cz_ = plan.imported.chunk_y;
    out.source_ = prepared.source_sha256;
    out.corner_provenance_ = std::move(provenance);
    out.draws_ = plan.draws;
    const auto& records = plan.maintenance.completed.chunk.records;
    const auto& final_records =
        plan.materialization.completed.unchanged_chunk.completed.chunk.records;
    if (records != final_records)
        return Error{Stage::Selection, "maintenance/materialization snapshots differ"};
    std::map<std::uint64_t, const WildlifeChunkRecord*> indexed;
    for (const auto& r : records)
        if (!indexed.emplace(r.token, &r).second)
            return Error{Stage::Selection, "duplicate maintained token"};
    for (const auto& step : plan.materialization.completed.entries) {
        if (step.disposition != WildlifeMaterializeDisposition::Selected)
            continue;
        const auto found = indexed.find(step.token);
        if (found == indexed.end() || !found->second->position || *found->second->position > 63 ||
            !step.type || !step.type->prodtype || !step.tile || !step.tile->object_word ||
            step.type->object_word != *step.tile->object_word ||
            step.tile->position != *found->second->position || !step.selection ||
            !step.selection->creation)
            return Error{Stage::Selection, "complete selected source record/type required"};
        const auto pos = *found->second->position;
        const auto local_x = out.cx_ * 8 + (pos & 7U), local_z = out.cz_ * 8 + (pos >> 3);
        const auto cell = std::uint64_t{local_z} * tiles.width() + local_x;
        const auto source_type =
            std::find_if(plan.final_facts.source.types.begin(), plan.final_facts.source.types.end(),
                         [&](const auto& t) { return t.object_word == *step.tile->object_word; });
        if (tiles.object(cell) != *step.tile->object_word ||
            source_type == plan.final_facts.source.types.end() ||
            source_type->prodtype != step.type->prodtype ||
            source_type->output0 != step.type->output0)
            return Error{Stage::Source,
                         "selection actual tile/type differs from completed source facts"};
        const auto outcome = step.selection->choice.value.position;
        if (*step.type->prodtype == 0x145 || outcome < 0)
            return Error{Stage::Selection, "cave geometry outside ordinary profile"};
        if (outcome > 3 ||
            !out.requests_
                 .emplace(step.token,
                          WildlifeOrdinarySource::Request{
                              *found->second->position, static_cast<std::uint32_t>(outcome),
                              *step.tile->object_word, *step.type->prodtype})
                 .second)
            return Error{Stage::Selection, "invalid/duplicate ordinary selection"};
    }
    return out;
}
formats::Result<WildlifeOrdinaryGeometry, Error>
WildlifeOrdinarySource::project(std::uint64_t token) const {
    const auto found = requests_.find(token);
    if (found == requests_.end())
        return Error{Stage::Selection, "selected ordinary token unavailable"};
    const auto& request = found->second;
    const auto lx = cx_ * 8 + (request.position & 7U), lz = cz_ * 8 + (request.position >> 3);
    if (std::uint64_t{lx} + 1 >= width_ || std::uint64_t{lz} + 1 >= height_)
        return Error{Stage::Source, "four interior prepared corners required; border unsupported"};
    const std::uint64_t base = std::uint64_t{lz} * width_ + lx;
    std::uint32_t reads = 0;
    const auto read = [&](std::uint64_t index) {
        ++reads;
        return terrain_->height[index];
    };
    const std::array<std::uint8_t, 4> corners{read(base), read(base + 1), read(base + width_),
                                              read(base + width_ + 1)};
    const auto horizontal = wildlife_ordinary_position(
        {x_, z_, cx_, cz_, request.position, request.outcome}, numeric_.callback_profile());
    if (!horizontal)
        return horizontal.error();
    std::array<std::uint32_t, 4> values{};
    for (std::size_t i = 0; i < 4; ++i)
        values[i] = numeric_.table()[corners[i]];
    const auto height = wildlife_ordinary_height(horizontal.value()[0], horizontal.value()[1],
                                                 values, numeric_.callback_profile());
    if (!height)
        return height.error();
    return WildlifeOrdinaryGeometry{
        {horizontal.value()[0], height.value().word, horizontal.value()[1]},
        {x_ + lx, z_ + lz},
        corners,
        height.value().triangle,
        request.object,
        source_,
        numeric_.provenance(),
        corner_provenance_,
        terrain_->digest,
        numeric_.table_profile(),
        numeric_.callback_profile(),
        draws_,
        reads};
}
} // namespace anno::sim_load
