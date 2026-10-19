// The archives against hand-derived bytes and hostile input (docs/sim/state.md
// §4.3, §9.1, §14.2): one field list drives hash, save, load and trace.
#include <array>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/hash.hpp>

namespace sim = anno::sim;
using Bytes = std::vector<std::uint8_t>;

namespace {

enum class Colour : std::uint8_t { Red = 0, Green = 1, Blue = 2 };
enum class Mode : std::uint8_t { Off = 0, Slow = 2, Fast = 3 }; // 1 is not a valid value

} // namespace

namespace anno::sim {
template <> struct EnumInfo<Colour> : ContiguousEnum<3> {};
template <> struct EnumInfo<Mode> {
    static constexpr bool valid(std::uint8_t v) { return v == 0 || v == 2 || v == 3; }
};
} // namespace anno::sim

namespace {

struct Inner {
    std::uint32_t a = 0;
    std::int32_t b = 0;
    friend bool operator==(const Inner&, const Inner&) = default;
};

/// A large element that takes 8 bytes on the wire (its padding is derived).
struct Big {
    std::uint64_t id = 0;
    std::array<std::uint8_t, 512> pad{};
};
template <class A, sim::Same<Big> S> void fields(A& a, S& s) {
    a.u64(s.id);
}

/// An element with a member that is derived on load and not in the stream.
struct Cached {
    std::uint32_t a = 0;
    std::uint32_t cache = 0;
};
template <class A, sim::Same<Cached> S> void fields(A& a, S& s) {
    a.u32(s.a);
}

/// An alternative whose construction throws (makes a variant valueless). The
/// move constructor may throw too, so that libstdc++ does not build the new
/// alternative in a temporary first (which would keep the variant valued).
struct Boom {
    explicit Boom(int) { throw 1; }
    Boom(Boom&&) noexcept(false) {}
};
constexpr std::array<std::uint8_t, 2> kBoomTags = {5, 6};

using Var = std::variant<std::uint32_t, Inner, std::string>;
// Explicit tags, deliberately not the alternative indexes 0, 1, 2.
constexpr std::array<std::uint8_t, 3> kVarTags = {7, 3, 9};
static_assert(sim::ar::tags_are_distinct<kVarTags>());
// A duplicate tag would make the second alternative unreachable on read; the
// variant helper refuses it at compile time through this predicate.
static_assert(!sim::ar::tags_are_distinct<std::array<std::uint8_t, 3>{1, 1, 2}>());
static_assert(!sim::ar::tags_are_distinct<std::array<std::uint8_t, 3>{4, 5, 4}>());
static_assert(sim::ar::tags_are_distinct<std::array<std::uint8_t, 2>{0, 255}>());
// A scope name is a string literal (ScopeName has a consteval constructor, so a
// name built at run time does not compile; compilers disagree on whether that
// can be probed in a requires-expression, so only the accepted side is checked).
static_assert(requires { sim::ScopeName{"literal"}; });

struct Sample {
    std::uint8_t b8 = 0;
    std::uint16_t b16 = 0;
    std::uint32_t b32 = 0;
    std::uint64_t b64 = 0;
    std::int32_t i32 = 0;
    std::int64_t i64 = 0;
    bool flag = false;
    Colour colour = Colour::Red;
    Mode mode = Mode::Off;
    std::string text;
    std::array<std::uint8_t, 3> raw{};
    std::vector<Inner> items;
    std::vector<bool> bits;
    std::optional<Inner> maybe;
    std::map<std::uint32_t, std::string> table;
    Var var;
    friend bool operator==(const Sample&, const Sample&) = default;
};

template <class A, sim::Same<Inner> S> void fields(A& a, S& s) {
    a.u32(s.a);
    a.i32(s.b);
}

template <class A, class T> void var_field(A& a, T& alternative) {
    using U = std::remove_const_t<T>;
    if constexpr (std::is_same_v<U, std::uint32_t>) {
        a.u32(alternative);
    } else if constexpr (std::is_same_v<U, Inner>) {
        fields(a, alternative);
    } else {
        a.str(alternative);
    }
}

template <class A, sim::Same<Sample> S> void fields(A& a, S& s) {
    a.u8(s.b8);
    a.u16(s.b16);
    a.u32(s.b32);
    a.u64(s.b64);
    a.i32(s.i32);
    a.i64(s.i64);
    a.boolean(s.flag);
    a.enumv(s.colour);
    a.enumv(s.mode);
    a.str(s.text);
    a.bytes(std::span(s.raw));
    a.scope("items", [&] { sim::ar::seq(a, s.items, [&](auto& e) { fields(a, e); }); });
    a.scope("bits", [&] { sim::ar::bools(a, s.bits); });
    sim::ar::opt(a, s.maybe, [&](auto& e) { fields(a, e); });
    sim::ar::map(a, s.table, [&](auto& k) { a.u32(k); }, [&](auto& v) { a.str(v); });
    sim::ar::variant<kVarTags>(a, s.var, [&](auto& alternative) { var_field(a, alternative); });
}

Sample make_sample() {
    Sample s;
    s.b8 = 0xAB;
    s.b16 = 0x1234;
    s.b32 = 0xDEADBEEFU;
    s.b64 = 0x0102030405060708ULL;
    s.i32 = -2;
    s.i64 = -3;
    s.flag = true;
    s.colour = Colour::Blue;
    s.mode = Mode::Fast;
    s.text = "hi";
    s.raw = {9, 8, 7};
    s.items = {Inner{1, -1}, Inner{2, -2}};
    s.bits = {true, false, true};
    s.maybe = Inner{5, 6};
    s.table = {{3, "x"}, {9, "yz"}};
    s.var = Inner{40, -41};
    return s;
}

// The stream of make_sample(), derived by hand from the encoding rules:
// little-endian fixed widths, bool/enum one byte, count and string length u64.
Bytes sample_bytes() {
    return {
        0xAB,                                                                // u8
        0x34, 0x12,                                                          // u16
        0xEF, 0xBE, 0xAD, 0xDE,                                              // u32
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,                      // u64
        0xFE, 0xFF, 0xFF, 0xFF,                                              // i32 -2
        0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,                      // i64 -3
        0x01,                                                                // bool true
        0x02,                                                                // Colour::Blue
        0x03,                                                                // Mode::Fast
        0x02, 0,    0,    0,    0,    0,    0,    0,    'h', 'i',            // string "hi"
        0x09, 0x08, 0x07,                                                    // raw bytes, no length
        0x02, 0,    0,    0,    0,    0,    0,    0,                         // items: count
        0x01, 0,    0,    0,    0xFF, 0xFF, 0xFF, 0xFF,                      //   {1, -1}
        0x02, 0,    0,    0,    0xFE, 0xFF, 0xFF, 0xFF,                      //   {2, -2}
        0x03, 0,    0,    0,    0,    0,    0,    0,                         // bits: count
        0x01, 0x00, 0x01,                                                    //   true false true
        0x01,                                                                // maybe: present
        0x05, 0,    0,    0,    0x06, 0,    0,    0,                         //   {5, 6}
        0x02, 0,    0,    0,    0,    0,    0,    0,                         // table: count
        0x03, 0,    0,    0,    0x01, 0,    0,    0,    0,   0,   0, 0, 'x', //   3 -> "x"
        0x09, 0,    0,    0,    0x02, 0,    0,    0,    0,   0,   0, 0, 'y', 'z', // 9 -> "yz"
        0x03,                                           // variant tag of Inner (kVarTags[1])
        0x28, 0,    0,    0,    0xD7, 0xFF, 0xFF, 0xFF, //   {40, -41}
    };
}

Bytes le32(std::uint32_t v) {
    return {static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(v >> 8),
            static_cast<std::uint8_t>(v >> 16), static_cast<std::uint8_t>(v >> 24)};
}
Bytes le64(std::uint64_t v) {
    Bytes b = le32(static_cast<std::uint32_t>(v));
    const Bytes hi = le32(static_cast<std::uint32_t>(v >> 32));
    b.insert(b.end(), hi.begin(), hi.end());
    return b;
}
Bytes cat(std::initializer_list<Bytes> parts) {
    Bytes out;
    for (const Bytes& p : parts) {
        out.insert(out.end(), p.begin(), p.end());
    }
    return out;
}

Bytes write_sample(const Sample& s) {
    sim::WriteArchive w;
    fields(w, s);
    return w.take();
}

/// Reads `bytes` into `out`; empty on success (all input consumed), else the error.
std::string read_sample(Sample& out, const Bytes& bytes) {
    sim::ReadArchive r(bytes);
    fields(r, out);
    if (r.failed()) {
        return r.error();
    }
    return r.remaining() == 0 ? std::string() : "trailing bytes";
}

} // namespace

TEST_CASE("archive primitives round trip") {
    const Sample s = make_sample();
    const Bytes bytes = write_sample(s);
    // Hand-derived bytes: widths, byte order, counts and tags.
    CHECK(bytes == sample_bytes());
    Sample back;
    CHECK(read_sample(back, bytes) == "");
    CHECK(back == s);
    // save(load(save(x))) == save(x), byte for byte.
    CHECK(write_sample(back) == bytes);

    SUBCASE("every variant alternative and an absent optional round trip") {
        Sample t = s;
        t.maybe.reset();
        t.var = std::uint32_t{0xCAFEBABEU};
        Sample u;
        CHECK(read_sample(u, write_sample(t)) == "");
        CHECK(u == t);
        CHECK_FALSE(u.maybe);
        t.var = std::string("variant text");
        CHECK(read_sample(u, write_sample(t)) == "");
        CHECK(u == t);
    }
    SUBCASE("empty containers and extreme values round trip") {
        Sample t;
        t.b8 = 0xFF;
        t.b16 = 0xFFFF;
        t.b32 = 0xFFFFFFFFU;
        t.b64 = 0xFFFFFFFFFFFFFFFFULL;
        t.i32 = -2147483647 - 1;
        t.i64 = static_cast<std::int64_t>(0x8000000000000000ULL);
        Sample u;
        CHECK(read_sample(u, write_sample(t)) == "");
        CHECK(u == t);
        CHECK(u.i32 == -2147483647 - 1);
        CHECK(u.i64 < 0);
    }
    SUBCASE("a populated target is replaced, not merged") {
        Sample u = make_sample();
        u.items.resize(7);
        u.table[100] = "stale";
        Sample t;
        t.b8 = 1;
        CHECK(read_sample(u, write_sample(t)) == "");
        CHECK(u == t);
    }
}

TEST_CASE("archive prefixes fail cleanly") {
    const Sample s = make_sample();
    const Bytes bytes = write_sample(s);
    for (std::size_t n = 0; n < bytes.size(); ++n) {
        const Bytes prefix(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(n));
        Sample out;
        sim::ReadArchive r(prefix);
        fields(r, out);
        INFO("prefix length " << n);
        REQUIRE(r.failed());
        CHECK_FALSE(r.error().empty());
        // After a failure every primitive stores 0 and reads nothing.
        std::uint32_t after = 77;
        const std::uint64_t at = r.position();
        r.u32(after);
        CHECK(after == 0);
        CHECK(r.position() == at);
    }
    // One byte too many is the caller's to detect (remaining() != 0).
    Bytes longer = bytes;
    longer.push_back(0);
    Sample out;
    CHECK(read_sample(out, longer) == "trailing bytes");
}

TEST_CASE("archive hostile counts and enums fail") {
    SUBCASE("a bool byte of 2") {
        const Bytes b = {2};
        sim::ReadArchive r(b);
        bool v = true;
        r.boolean(v);
        CHECK(r.failed());
        CHECK_FALSE(v);
        CHECK(r.error().find("bool") != std::string::npos);
    }
    SUBCASE("bool bytes 0 and 1 are accepted") {
        const Bytes b = {0, 1};
        sim::ReadArchive r(b);
        bool x = true, y = false;
        r.boolean(x);
        r.boolean(y);
        CHECK_FALSE(r.failed());
        CHECK_FALSE(x);
        CHECK(y);
    }
    SUBCASE("a count within the remaining bytes whose elements run out") {
        // Claims 3 elements of Inner, carries 8 bytes (one element would fit).
        Bytes b = cat({le64(3), le32(1), le32(2)});
        sim::ReadArchive r(b);
        std::vector<Inner> v(5);
        sim::ar::seq(r, v, [&](auto& e) { fields(r, e); });
        // 3 <= 8 remaining bytes: the count check passes (every element needs
        // at least one byte) and the element reads then run out of input.
        CHECK(r.failed());
    }
    SUBCASE("a count that exceeds the input fails before anything is sized") {
        const Bytes b = le64(0xFFFFFFFFFFFFFFFFULL);
        sim::ReadArchive r(b);
        std::vector<Inner> v(5);
        sim::ar::seq(r, v, [&](auto& e) { fields(r, e); });
        CHECK(r.failed());
        CHECK(v.empty());
        CHECK(v.capacity() < 1000);
        CHECK(r.error().find("count") != std::string::npos);
    }
    SUBCASE("a count one above the remaining bytes") {
        Bytes b = {4, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3}; // 3 bytes remain
        sim::ReadArchive r(b);
        std::uint64_t n = 99;
        r.count(n);
        CHECK(r.failed());
        CHECK(n == 0);
        Bytes ok = {3, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3}; // 3 bytes remain: allowed
        sim::ReadArchive r2(ok);
        r2.count(n);
        CHECK_FALSE(r2.failed());
        CHECK(n == 3);
    }
    SUBCASE("a count bounded by the smallest element: no allocation from a lie") {
        // 1000 claimed elements, 1000 bytes left. Each Big element needs 8
        // bytes on the wire, so at most 125 could be there: refused before any
        // element (512 bytes in memory) is made.
        Bytes b = le64(1000);
        b.resize(b.size() + 1000, 0);
        sim::ReadArchive r(b);
        std::vector<Big> v;
        sim::ar::seq(r, v, 8, [&](auto& e) { fields(r, e); });
        CHECK(r.failed());
        CHECK(v.empty());
        CHECK(v.capacity() == 0);
        CHECK(r.error().find("count") != std::string::npos);
        // The bound is exact: 3 elements of 8 bytes pass, 4 do not.
        Bytes ok = le64(3);
        ok.resize(ok.size() + 24, 1);
        sim::ReadArchive r3(ok);
        std::vector<Big> three;
        sim::ar::seq(r3, three, 8, [&](auto& e) { fields(r3, e); });
        CHECK_FALSE(r3.failed());
        CHECK(three.size() == 3);
        CHECK(three[2].id == 0x0101010101010101ULL);
        Bytes four = le64(4);
        four.resize(four.size() + 24, 1);
        sim::ReadArchive r4(four);
        std::vector<Big> none;
        sim::ar::seq(r4, none, 8, [&](auto& e) { fields(r4, e); });
        CHECK(r4.failed());
        CHECK(none.empty());
        // A multiplication would overflow here; the division does not.
        const Bytes huge = le64(0xFFFFFFFFFFFFFFFFULL);
        sim::ReadArchive rh(huge);
        std::vector<Big> h;
        sim::ar::seq(rh, h, 0x8000000000000000ULL, [&](auto& e) { fields(rh, e); });
        CHECK(rh.failed());
        CHECK(h.empty());
    }
    SUBCASE("memory tracks the bytes consumed, not the claimed count") {
        // With the default 1 byte per element, 100 claimed elements pass the
        // count check, but the elements are made one at a time and the read
        // stops at the first failure: 12 whole ones fit 100 bytes, the 13th
        // runs out. No 100 x 512 bytes are ever allocated.
        Bytes b = le64(100);
        b.resize(b.size() + 100, 0);
        sim::ReadArchive r(b);
        std::vector<Big> v;
        sim::ar::seq(r, v, [&](auto& e) { fields(r, e); });
        CHECK(r.failed());
        CHECK(v.size() <= 13);
        CHECK(v.capacity() <= 32);
    }
    SUBCASE("a sequence read replaces the target and default-constructs its elements") {
        // `cache` is derived data: not in the stream, so a read leaves the
        // default there, never the stale value of the target.
        std::vector<Cached> v = {{1, 99}, {2, 99}, {3, 99}, {4, 99}};
        const Bytes b = cat({le64(2), le32(7), le32(8)});
        sim::ReadArchive r(b);
        sim::ar::seq(r, v, 4, [&](auto& e) { fields(r, e); });
        CHECK_FALSE(r.failed());
        REQUIRE(v.size() == 2);
        CHECK(v[0].a == 7);
        CHECK(v[1].a == 8);
        CHECK(v[0].cache == 0);
        CHECK(v[1].cache == 0);
        // A failed read still leaves no stale element behind.
        std::vector<Cached> w = {{1, 99}, {2, 99}};
        const Bytes bad = cat({le64(2), le32(5)}); // second element missing
        sim::ReadArchive rb(bad);
        sim::ar::seq(rb, w, 4, [&](auto& e) { fields(rb, e); });
        CHECK(rb.failed());
        for (const Cached& c : w) {
            CHECK(c.cache == 0);
        }
    }
    SUBCASE("a valueless variant is written as its first alternative") {
        std::variant<std::uint32_t, Boom> v;
        try {
            v.emplace<1>(1);
        } catch (const int&) {
        }
        REQUIRE(v.valueless_by_exception());
        sim::WriteArchive w;
        sim::ar::variant<kBoomTags>(w, std::as_const(v), [&](auto& alt) {
            using Alt = std::remove_const_t<std::remove_reference_t<decltype(alt)>>;
            if constexpr (std::is_same_v<Alt, std::uint32_t>) {
                w.u32(alt);
            }
        });
        CHECK(w.data() == Bytes{5, 0, 0, 0, 0});
    }
    SUBCASE("an enum out of range") {
        const Bytes b = {3};
        sim::ReadArchive r(b);
        Colour c = Colour::Blue;
        r.enumv(c);
        CHECK(r.failed());
        CHECK(c == Colour::Red);
    }
    SUBCASE("an enum with a gap") {
        for (unsigned v = 0; v < 6; ++v) {
            const Bytes b = {static_cast<std::uint8_t>(v)};
            sim::ReadArchive r(b);
            Mode m = Mode::Off;
            r.enumv(m);
            const bool valid = v == 0 || v == 2 || v == 3;
            CHECK(r.failed() == !valid);
            if (valid) {
                CHECK(static_cast<unsigned>(m) == v);
            }
        }
    }
    SUBCASE("a duplicate map key") {
        // count 2, keys 5 and 5, empty strings
        const Bytes b = cat({le64(2), le32(5), le64(0), le32(5), le64(0)});
        sim::ReadArchive r(b);
        std::map<std::uint32_t, std::string> m;
        sim::ar::map(r, m, [&](auto& k) { r.u32(k); }, [&](auto& v) { r.str(v); });
        CHECK(r.failed());
        CHECK(r.error().find("ascending") != std::string::npos);
    }
    SUBCASE("map keys out of order") {
        const Bytes b = cat({le64(2), le32(6), le64(0), le32(5), le64(0)});
        // (the same two keys in ascending order are accepted)
        const Bytes good = cat({le64(2), le32(5), le64(0), le32(6), le64(0)});
        sim::ReadArchive rg(good);
        std::map<std::uint32_t, std::string> mg;
        sim::ar::map(rg, mg, [&](auto& k) { rg.u32(k); }, [&](auto& v) { rg.str(v); });
        CHECK_FALSE(rg.failed());
        CHECK(mg.size() == 2);
        sim::ReadArchive r(b);
        std::map<std::uint32_t, std::string> m;
        sim::ar::map(r, m, [&](auto& k) { r.u32(k); }, [&](auto& v) { r.str(v); });
        CHECK(r.failed());
    }
    SUBCASE("a variant tag no alternative has") {
        for (const std::uint8_t tag : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{2},
                                       std::uint8_t{4}, std::uint8_t{8}, std::uint8_t{255}}) {
            // Tags 0, 1 and 2 are the alternative indexes, which are not the tags.
            const Bytes b = {tag, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            sim::ReadArchive r(b);
            Var v;
            sim::ar::variant<kVarTags>(r, v, [&](auto& alt) { var_field(r, alt); });
            INFO("tag " << static_cast<unsigned>(tag));
            CHECK(r.failed());
        }
        for (std::size_t i = 0; i < kVarTags.size(); ++i) {
            const Bytes b = {kVarTags[i], 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            sim::ReadArchive r(b);
            Var v;
            sim::ar::variant<kVarTags>(r, v, [&](auto& alt) { var_field(r, alt); });
            INFO("tag " << static_cast<unsigned>(kVarTags[i]));
            CHECK(v.index() == i);
        }
    }
    SUBCASE("a string length that lies") {
        Bytes b = {100, 0, 0, 0, 0, 0, 0, 0, 'a', 'b', 'c'};
        sim::ReadArchive r(b);
        std::string s = "stale";
        r.str(s);
        CHECK(r.failed());
        CHECK(s.empty());
    }
    SUBCASE("an optional presence byte of 2 and a bool element of 2") {
        Bytes b = {2};
        sim::ReadArchive r(b);
        std::optional<Inner> o = Inner{1, 1};
        sim::ar::opt(r, o, [&](auto& e) { fields(r, e); });
        CHECK(r.failed());
        CHECK_FALSE(o);
        Bytes c = {2, 0, 0, 0, 0, 0, 0, 0, 2, 0};
        sim::ReadArchive r2(c);
        std::vector<bool> bits;
        sim::ar::bools(r2, bits);
        CHECK(r2.failed());
    }
    SUBCASE("raw bytes past the end are zero filled and fail") {
        const Bytes b = {1, 2};
        sim::ReadArchive r(b);
        std::array<std::uint8_t, 3> out = {9, 9, 9};
        r.bytes(std::span(out));
        CHECK(r.failed());
        CHECK(out == std::array<std::uint8_t, 3>{0, 0, 0});
        CHECK(r.position() == 0);
    }
    SUBCASE("the error names the scope path and the byte, and the first error stays") {
        const Bytes b = {1, 2, 3};
        sim::ReadArchive r(b);
        r.scope("outer", [&] {
            r.scope("inner", 2, [&] {
                std::uint8_t x = 0;
                r.u8(x);
                std::uint32_t y = 0;
                r.u32(y); // needs 4 bytes, 2 remain
                r.fail("a later error");
            });
        });
        CHECK(r.failed());
        CHECK(r.error() == "outer/inner[2] (byte 1): unexpected end of input");
        // Outside any scope.
        const Bytes empty;
        sim::ReadArchive r2(empty);
        std::uint8_t z = 5;
        r2.u8(z);
        CHECK(r2.error() == "stream (byte 0): unexpected end of input");
        CHECK(z == 0);
    }
}

namespace {

// The stream used for the hash tree tests, written against any archive.
template <class A, bool Renamed = false> void tree_stream(A& a) {
    constexpr const char* beta = Renamed ? "gamma" : "beta";
    a.u32(1);
    a.scope("alpha", [&] { a.u8(2); });
    a.scope(beta, 0, [&] {
        a.u16(3);
        a.scope("deep", [&] { a.u8(4); });
        a.u16(5);
    });
    a.scope(beta, 1, [&] { a.u8(6); });
    a.u32(7);
}

/// The hierarchical digests of tree_stream, computed with StateHasher alone.
struct TreeExpect {
    std::uint64_t alpha, deep, beta0, beta1, root;
};
TreeExpect expected_tree() {
    TreeExpect e{};
    sim::StateHasher alpha;
    alpha.u8(2);
    e.alpha = alpha.value();
    sim::StateHasher deep;
    deep.u8(4);
    e.deep = deep.value();
    sim::StateHasher beta0;
    beta0.u16(3);
    beta0.u64(e.deep);
    beta0.u16(5);
    e.beta0 = beta0.value();
    sim::StateHasher beta1;
    beta1.u8(6);
    e.beta1 = beta1.value();
    sim::StateHasher root;
    root.u32(1);
    root.u64(e.alpha);
    root.u64(e.beta0);
    root.u64(e.beta1);
    root.u32(7);
    e.root = root.value();
    return e;
}

} // namespace

TEST_CASE("hash tree root equals flat hash") {
    const TreeExpect e = expected_tree();

    sim::HashArchive flat;
    tree_stream(flat);
    // A scope contributes its own digest at the position where it appears.
    CHECK(flat.finish() == e.root);

    SUBCASE("the tree records the scopes down to the depth") {
        sim::HashTree tree;
        sim::HashArchive a(sim::HashOptions{&tree, 1, false});
        tree_stream(a);
        CHECK(a.finish() == e.root);
        CHECK(tree.root.hash == e.root);
        CHECK(tree.root.name == "world");
        REQUIRE(tree.root.children.size() == 3);
        const sim::HashNode& alpha = tree.root.children[0];
        const sim::HashNode& beta0 = tree.root.children[1];
        const sim::HashNode& beta1 = tree.root.children[2];
        CHECK(alpha.name == "alpha");
        CHECK_FALSE(alpha.indexed);
        CHECK(alpha.hash == e.alpha);
        CHECK(beta0.name == "beta");
        CHECK(beta0.indexed);
        CHECK(beta0.index == 0);
        CHECK(beta0.hash == e.beta0);
        CHECK(beta1.index == 1);
        CHECK(beta1.hash == e.beta1);
        REQUIRE(beta0.children.size() == 1);
        CHECK(beta0.children[0].name == "deep");
        CHECK(beta0.children[0].hash == e.deep);
        CHECK(alpha.children.empty());
    }
    SUBCASE("finish does not consume the archive") {
        sim::HashTree tree;
        sim::HashArchive a(sim::HashOptions{&tree, 1, false});
        tree_stream(a);
        const std::uint64_t first = a.finish();
        REQUIRE(tree.root.children.size() == 3);
        const std::uint64_t second = a.finish();
        CHECK(second == first);
        CHECK(tree.root.hash == first);
        REQUIRE(tree.root.children.size() == 3);
        CHECK(tree.root.children[1].children.size() == 1);
    }
    SUBCASE("depth 0 records only the top-level scopes") {
        sim::HashTree tree;
        sim::HashArchive a(sim::HashOptions{&tree, 0, false});
        tree_stream(a);
        CHECK(a.finish() == e.root);
        REQUIRE(tree.root.children.size() == 3);
        CHECK(tree.root.children[1].children.empty());
    }
    SUBCASE("names and ordinals are addresses, not hashed") {
        sim::HashArchive a;
        tree_stream<sim::HashArchive, true>(a);
        CHECK(a.finish() == e.root);
    }
    SUBCASE("a change inside a nested scope reaches the root") {
        sim::HashArchive a;
        a.u32(1);
        a.scope("alpha", [&] { a.u8(2); });
        a.scope("beta", 0, [&] {
            a.u16(3);
            a.scope("deep", [&] { a.u8(44); }); // 4 -> 44
            a.u16(5);
        });
        a.scope("beta", 1, [&] { a.u8(6); });
        a.u32(7);
        CHECK(a.finish() != e.root);
    }
    SUBCASE("a scope at a different position is a different hash") {
        sim::HashArchive a;
        a.scope("alpha", [&] { a.u8(2); });
        a.u32(1);
        a.scope("beta", 0, [&] {
            a.u16(3);
            a.scope("deep", [&] { a.u8(4); });
            a.u16(5);
        });
        a.scope("beta", 1, [&] { a.u8(6); });
        a.u32(7);
        CHECK(a.finish() != e.root);
    }
    SUBCASE("raw mode is transparent: the digest is the FNV of the written bytes") {
        sim::HashArchive raw(sim::HashOptions{nullptr, 0, true});
        tree_stream(raw);
        sim::WriteArchive w;
        tree_stream(w);
        sim::StateHasher over_bytes;
        over_bytes.bytes(w.data());
        CHECK(raw.finish() == over_bytes.value());
        CHECK(raw.finish() != e.root);
    }
    SUBCASE("a deep nest beyond the reserved frames still hashes") {
        sim::HashArchive a;
        struct Rec {
            sim::HashArchive& a;
            void go(int depth) {
                if (depth == 0) {
                    a.u8(1);
                    return;
                }
                a.scope("n", [&] { go(depth - 1); });
            }
        };
        Rec{a}.go(100);
        sim::StateHasher h;
        h.u8(1);
        std::uint64_t d = h.value();
        for (int i = 0; i < 100; ++i) {
            sim::StateHasher p;
            p.u64(d);
            d = p.value();
        }
        CHECK(a.finish() == d);
    }
}

TEST_CASE("archives visit the same primitive sequence") {
    const Sample s = make_sample();
    sim::WriteArchive w;
    fields(w, s);
    sim::TraceArchive t;
    fields(t, s);
    // The trace keeps the byte stream a WriteArchive produces ...
    CHECK(t.bytes_written() == w.data());
    // ... and a raw HashArchive hashes exactly those bytes.
    sim::HashArchive h(sim::HashOptions{nullptr, 0, true});
    fields(h, s);
    sim::StateHasher over_bytes;
    over_bytes.bytes(w.data());
    CHECK(h.finish() == over_bytes.value());

    const auto& e = t.entries();
    REQUIRE(e.size() > 20);
    CHECK(e[0] == sim::TraceEntry{sim::Prim::U8, 1, 0xAB, {}, 0});
    CHECK(e[1] == sim::TraceEntry{sim::Prim::U16, 2, 0x1234, {}, 0});
    CHECK(e[2] == sim::TraceEntry{sim::Prim::U32, 4, 0xDEADBEEFU, {}, 0});
    CHECK(e[3] == sim::TraceEntry{sim::Prim::U64, 8, 0x0102030405060708ULL, {}, 0});
    CHECK(e[4] == sim::TraceEntry{sim::Prim::I32, 4, 0xFFFFFFFEU, {}, 0});
    CHECK(e[5] == sim::TraceEntry{sim::Prim::I64, 8, 0xFFFFFFFFFFFFFFFDULL, {}, 0});
    CHECK(e[6] == sim::TraceEntry{sim::Prim::Bool, 1, 1, {}, 0});
    CHECK(e[7] == sim::TraceEntry{sim::Prim::Enum, 1, 2, {}, 0});
    CHECK(e[8] == sim::TraceEntry{sim::Prim::Enum, 1, 3, {}, 0});
    CHECK(e[9] == sim::TraceEntry{sim::Prim::StrLen, 8, 2, {}, 0});
    CHECK(e[10] == sim::TraceEntry{sim::Prim::StrBytes, 2, 0, {}, 0});
    CHECK(e[11] == sim::TraceEntry{sim::Prim::Bytes, 3, 0, {}, 0});
    CHECK(e[12] == sim::TraceEntry{sim::Prim::ScopeOpen, 0, 0, "items", 0});
    CHECK(e[13] == sim::TraceEntry{sim::Prim::Count, 8, 2, {}, 0});
    // The scopes are balanced and in order.
    std::vector<std::string_view> open;
    for (const sim::TraceEntry& x : e) {
        if (x.kind == sim::Prim::ScopeOpen) {
            open.push_back(x.name);
        } else if (x.kind == sim::Prim::ScopeClose) {
            REQUIRE_FALSE(open.empty());
            CHECK(open.back() == x.name);
            open.pop_back();
        }
    }
    CHECK(open.empty());
}

TEST_CASE("fnv vectors unchanged") {
    // FNV-1a 64 test vectors (draft-eastlake-fnv).
    auto fnv = [](std::string_view s) {
        sim::StateHasher h;
        for (const char c : s) {
            h.u8(static_cast<std::uint8_t>(c));
        }
        return h.value();
    };
    CHECK(fnv("") == 0xcbf29ce484222325ULL);
    CHECK(fnv("a") == 0xaf63dc4c8601ec8cULL);
    CHECK(fnv("foobar") == 0x85944171f73967e8ULL);
    // Multi-byte feeds are little endian.
    sim::StateHasher a, b;
    a.u16(0x0201);
    a.u64(0x0807060504030201ULL);
    const std::uint8_t bytes[] = {1, 2, 1, 2, 3, 4, 5, 6, 7, 8};
    b.bytes(bytes);
    CHECK(a.value() == b.value());
    // The archive feeds the same bytes as the hasher.
    sim::HashArchive h;
    h.u16(0x0201);
    h.u64(0x0807060504030201ULL);
    CHECK(h.finish() == b.value());
}
