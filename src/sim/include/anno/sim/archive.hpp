// The four archives: one field list drives hash, save, load and trace (spec:
// docs/sim/state.md §4.3, §9.1).
//
// A persistent type T has exactly one function
//     template <class A, Same<T> S> void fields(A& a, S& s)
// (state_fields.hpp) that names its fields in canonical order by calling the
// archive's primitives. The archive decides what that means:
//   HashArchive   feeds FNV-1a 64, hierarchical through scope()
//   WriteArchive  appends little-endian bytes
//   ReadArchive   fills the state from bytes, latching the first error
//   TraceArchive  test only: records (kind, width, value) of every primitive
// so the hash, the save and the load cannot disagree about which fields exist
// or in which order. Encodings (N, own design; original saves are not read):
// fixed-width little-endian integers, bool = one byte 0/1, enum = its u8
// value, count / string length = u64, no padding, no struct images.
//
// Write-side primitives take their value by value, the read side by
// reference, so `a.u32(s.field)` compiles for every archive whether `s` is
// const (hash, write) or not (read). `A::kReading` selects the read-only
// parts of a field function.
#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <anno/sim/hash.hpp>
#include <anno/sim/hash_tree.hpp>

namespace anno::sim {

template <class S, class T>
concept Same = std::same_as<std::remove_const_t<S>, T>;

/// Which byte values are valid for a state enum, specialised next to the
/// field function that uses it: `static constexpr bool valid(std::uint8_t)`.
/// Every state enum has the underlying type u8.
template <class E> struct EnumInfo;

/// EnumInfo base for an enum whose valid values are 0 .. N-1.
template <std::uint32_t N> struct ContiguousEnum {
    static constexpr bool valid(std::uint8_t v) { return v < N; }
};

/// The name of a scope: a string literal, checked at compile time. Scope names
/// are kept as views by HashTree, TraceArchive and the ReadArchive error path,
/// so they must outlive the archive; a consteval constructor from `const
/// char*` accepts only a constant expression, i.e. a literal, and a name built
/// at run time does not compile.
struct ScopeName {
    consteval ScopeName(const char* literal) : view(literal) {}
    std::string_view view;
};

/// The kinds of primitive an archive sees (TraceArchive records them).
enum class Prim : std::uint8_t {
    U8,
    U16,
    U32,
    U64,
    I32,
    I64,
    Bool,
    Count,
    Enum,
    StrLen,
    StrBytes,
    Bytes,
    ScopeOpen,
    ScopeClose
};

namespace detail {

/// The bytes of a string. char and std::uint8_t are both byte types, so the
/// view aliases legally; this is the only cast in the archives.
inline std::span<const std::uint8_t> bytes_of(const std::string& s) {
    return {reinterpret_cast<const std::uint8_t*>(s.data()), s.size()};
}

/// The write-side primitives, shared by HashArchive, WriteArchive and
/// TraceArchive. A derived class provides
///   void emit(Prim, std::uint64_t value, std::uint32_t width)   // little endian
///   void emit_bytes(Prim, std::span<const std::uint8_t>)
template <class D> class Writer {
  public:
    static constexpr bool kReading = false;

    void u8(std::uint8_t v) { self().emit(Prim::U8, v, 1); }
    void u16(std::uint16_t v) { self().emit(Prim::U16, v, 2); }
    void u32(std::uint32_t v) { self().emit(Prim::U32, v, 4); }
    void u64(std::uint64_t v) { self().emit(Prim::U64, v, 8); }
    void i32(std::int32_t v) {
        self().emit(Prim::I32, static_cast<std::uint64_t>(static_cast<std::uint32_t>(v)), 4);
    }
    void i64(std::int64_t v) { self().emit(Prim::I64, static_cast<std::uint64_t>(v), 8); }
    void boolean(bool v) { self().emit(Prim::Bool, v ? 1U : 0U, 1); }
    /// Element count of a container: u64 prefix. `min_wire_bytes` is the
    /// smallest encoding of one element; only the read side uses it (the same
    /// call shape for every archive).
    void count(std::uint64_t v, std::uint64_t /*min_wire_bytes*/ = 1) {
        self().emit(Prim::Count, v, 8);
    }
    template <class E> void enumv(E v) {
        static_assert(std::is_enum_v<E> && std::is_same_v<std::underlying_type_t<E>, std::uint8_t>,
                      "state enums have the underlying type u8");
        self().emit(Prim::Enum, static_cast<std::uint8_t>(v), 1);
    }
    /// u64 length, then the bytes.
    void str(const std::string& s) {
        self().emit(Prim::StrLen, s.size(), 8);
        self().emit_bytes(Prim::StrBytes, bytes_of(s));
    }
    /// Raw bytes, no length (fixed-size or bulk data whose size the caller knows).
    void bytes(std::span<const std::uint8_t> b) { self().emit_bytes(Prim::Bytes, b); }

  private:
    D& self() { return static_cast<D&>(*this); }
};

} // namespace detail

/// FNV-1a 64 of the stream, hierarchical (docs/sim/state.md §9.1): the digest
/// of a scope covers its own primitives, and every child scope contributes
/// its 64-bit digest (u64 LE) at the position where it appears. Names and
/// ordinals are for addressing only and are not hashed. In raw mode scopes
/// are transparent: every primitive feeds the one root hasher, so the digest
/// equals the FNV-1a of the WriteArchive bytes of the same stream (the
/// invariant that WriteArchive, HashArchive and TraceArchive visit the same
/// primitive sequence).
struct HashOptions {
    /// If set, the scopes down to `depth` are recorded (hash_tree.hpp).
    HashTree* tree = nullptr;
    /// Deepest scope level recorded: 0 = the top-level sections, 1 = their
    /// children, ... (docs/sim/state.md §9.4).
    std::uint32_t depth = 0;
    bool raw = false;
};

class HashArchive : public detail::Writer<HashArchive> {
  public:
    HashArchive() : HashArchive(HashOptions{}) {}
    explicit HashArchive(HashOptions o) : tree_(o.tree), depth_(o.depth), raw_(o.raw) {
        // Scope nesting of the world stream is a handful deep: no allocation
        // after this in the hot path (docs/sim/state.md §9.4).
        stack_.reserve(32);
        stack_.emplace_back();
    }

    void emit(Prim /*kind*/, std::uint64_t v, std::uint32_t width) {
        StateHasher& h = stack_.back().h;
        switch (width) {
        case 1:
            h.u8(static_cast<std::uint8_t>(v));
            break;
        case 2:
            h.u16(static_cast<std::uint16_t>(v));
            break;
        case 4:
            h.u32(static_cast<std::uint32_t>(v));
            break;
        default:
            h.u64(v);
            break;
        }
    }
    void emit_bytes(Prim /*kind*/, std::span<const std::uint8_t> b) { stack_.back().h.bytes(b); }

    template <class F> void scope(ScopeName name, std::uint32_t index, F&& f) {
        open(name.view, index, true);
        f();
        close();
    }
    template <class F> void scope(ScopeName name, F&& f) {
        open(name.view, 0, false);
        f();
        close();
    }

    /// Raw mode: scopes are transparent (HashOptions::raw). Field lists that feed
    /// derived digests instead of bulk data in hierarchical mode (the tile layer's
    /// chunk digests, state_fields.hpp) feed the data itself in raw mode.
    [[nodiscard]] bool raw() const { return raw_; }

    /// The digest of the root scope; fills the tree (root named "world") when
    /// one was requested. Call after every scope is closed. It does not
    /// consume the archive: a second call returns the same digest and fills
    /// the same tree.
    std::uint64_t finish() {
        const Frame& root = stack_.front();
        const std::uint64_t h = root.h.value();
        if (tree_ != nullptr) {
            tree_->root.name = "world";
            tree_->root.index = 0;
            tree_->root.indexed = false;
            tree_->root.hash = h;
            tree_->root.children = root.children;
        }
        return h;
    }

  private:
    struct Frame {
        StateHasher h;
        std::string_view name;
        std::uint32_t index = 0;
        bool indexed = false;
        std::vector<HashNode> children;
    };

    void open(std::string_view name, std::uint32_t index, bool indexed) {
        if (raw_) {
            return;
        }
        Frame& f = stack_.emplace_back();
        f.name = name;
        f.index = index;
        f.indexed = indexed;
    }
    void close() {
        if (raw_) {
            return;
        }
        Frame child = std::move(stack_.back());
        stack_.pop_back();
        const std::uint64_t digest = child.h.value();
        Frame& parent = stack_.back();
        parent.h.u64(digest);
        // The child's level: 0 for a top-level section.
        const std::uint64_t level = stack_.size() - 1;
        if (tree_ != nullptr && level <= depth_) {
            parent.children.push_back(HashNode{child.name, child.index, child.indexed, digest,
                                               std::move(child.children)});
        }
    }

    std::vector<Frame> stack_;
    HashTree* tree_;
    std::uint32_t depth_;
    bool raw_;
};

/// Appends the little-endian stream (the save body).
class WriteArchive : public detail::Writer<WriteArchive> {
  public:
    void emit(Prim /*kind*/, std::uint64_t v, std::uint32_t width) {
        for (std::uint32_t i = 0; i < width; ++i) {
            out_.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
        }
    }
    // Element by element: vector::insert with a runtime range of a span trips
    // false -Warray-bounds / -Wstringop-overflow in GCC 12/13 at -O3.
    void emit_bytes(Prim /*kind*/, std::span<const std::uint8_t> b) {
        out_.reserve(out_.size() + b.size());
        for (const std::uint8_t x : b) {
            out_.push_back(x);
        }
    }
    template <class F> void scope(ScopeName /*name*/, std::uint32_t /*index*/, F&& f) { f(); }
    template <class F> void scope(ScopeName /*name*/, F&& f) { f(); }
    [[nodiscard]] const std::vector<std::uint8_t>& data() const { return out_; }
    [[nodiscard]] std::vector<std::uint8_t> take() { return std::move(out_); }

  private:
    std::vector<std::uint8_t> out_;
};

/// One recorded primitive (or scope marker) of a TraceArchive.
struct TraceEntry {
    Prim kind = Prim::U8;
    /// Bytes the primitive occupies in the stream (0 for scope markers).
    std::uint64_t width = 0;
    /// The value as the little-endian stream carries it (0 for byte runs).
    std::uint64_t value = 0;
    std::string_view name;
    std::uint32_t index = 0;
    friend bool operator==(const TraceEntry&, const TraceEntry&) = default;
};

/// Test only: records every primitive and scope, and keeps the byte stream a
/// WriteArchive would have produced.
class TraceArchive : public detail::Writer<TraceArchive> {
  public:
    void emit(Prim kind, std::uint64_t v, std::uint32_t width) {
        entries_.push_back(TraceEntry{kind, width, v, {}, 0});
        for (std::uint32_t i = 0; i < width; ++i) {
            bytes_.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
        }
    }
    void emit_bytes(Prim kind, std::span<const std::uint8_t> b) {
        entries_.push_back(TraceEntry{kind, b.size(), 0, {}, 0});
        bytes_.reserve(bytes_.size() + b.size());
        for (const std::uint8_t x : b) {
            bytes_.push_back(x);
        }
    }
    template <class F> void scope(ScopeName name, std::uint32_t index, F&& f) {
        entries_.push_back(TraceEntry{Prim::ScopeOpen, 0, 0, name.view, index});
        f();
        entries_.push_back(TraceEntry{Prim::ScopeClose, 0, 0, name.view, index});
    }
    template <class F> void scope(ScopeName name, F&& f) {
        scope(name, 0, std::forward<F>(f));
    }
    [[nodiscard]] const std::vector<TraceEntry>& entries() const { return entries_; }
    [[nodiscard]] const std::vector<std::uint8_t>& bytes_written() const { return bytes_; }

  private:
    std::vector<TraceEntry> entries_;
    std::vector<std::uint8_t> bytes_;
};

/// Fills state from bytes. Never throws and never allocates more than the
/// input justifies: it latches the first error (failed() / error() name the
/// scope path and byte offset), and after that every primitive stores 0 / an
/// empty value and reads nothing (docs/sim/state.md §4.3). Containers are
/// filled one element at a time (ar::seq, ar::map), so the memory they take
/// tracks the bytes actually consumed, never `count * sizeof(element)` of a
/// claimed count; strings and bool vectors take one byte per element on the
/// wire and so at most as much memory as input.
class ReadArchive {
  public:
    static constexpr bool kReading = true;

    explicit ReadArchive(std::span<const std::uint8_t> in) : in_(in) {}

    void u8(std::uint8_t& v) { v = static_cast<std::uint8_t>(take(1)); }
    void u16(std::uint16_t& v) { v = static_cast<std::uint16_t>(take(2)); }
    void u32(std::uint32_t& v) { v = static_cast<std::uint32_t>(take(4)); }
    void u64(std::uint64_t& v) { v = take(8); }
    void i32(std::int32_t& v) {
        v = static_cast<std::int32_t>(static_cast<std::uint32_t>(take(4)));
    }
    void i64(std::int64_t& v) { v = static_cast<std::int64_t>(take(8)); }
    /// A byte 0 or 1; any other byte is an error.
    void boolean(bool& v) {
        const std::uint64_t x = take(1);
        if (x > 1) {
            fail("bool byte is neither 0 nor 1");
            v = false;
            return;
        }
        v = x == 1;
    }
    /// A container length. Every element takes at least `min_wire_bytes`
    /// (>= 1) bytes in the stream, so a count larger than the remaining input
    /// divided by that is a lie and fails before anything is sized from it.
    /// The division cannot overflow, unlike `n * min_wire_bytes`.
    void count(std::uint64_t& n, std::uint64_t min_wire_bytes = 1) {
        n = take(8);
        const std::uint64_t per = min_wire_bytes == 0 ? 1 : min_wire_bytes;
        if (!failed_ && n > remaining() / per) {
            fail("count exceeds the remaining input");
        }
        if (failed_) {
            n = 0;
        }
    }
    template <class E> void enumv(E& e) {
        static_assert(std::is_enum_v<E> && std::is_same_v<std::underlying_type_t<E>, std::uint8_t>,
                      "state enums have the underlying type u8");
        const auto x = static_cast<std::uint8_t>(take(1));
        if (!failed_ && !EnumInfo<E>::valid(x)) {
            fail("enum value out of range");
        }
        e = failed_ ? E{} : static_cast<E>(x);
    }
    void str(std::string& s) {
        std::uint64_t n = 0;
        count(n);
        s.clear();
        if (failed_) {
            return;
        }
        s.resize(static_cast<std::size_t>(n));
        for (std::size_t i = 0; i < s.size(); ++i) {
            s[i] = static_cast<char>(in_[pos_ + i]);
        }
        pos_ += s.size();
    }
    /// Fills `out` with the next out.size() bytes (the size is the caller's).
    void bytes(std::span<std::uint8_t> out) {
        if (!failed_ && remaining() < out.size()) {
            fail("unexpected end of input");
        }
        for (std::size_t i = 0; i < out.size(); ++i) {
            out[i] = failed_ ? std::uint8_t{0} : in_[pos_ + i];
        }
        if (!failed_) {
            pos_ += out.size();
        }
    }

    template <class F> void scope(ScopeName name, std::uint32_t index, F&& f) {
        path_.push_back(PathItem{name.view, index, true});
        f();
        path_.pop_back();
    }
    template <class F> void scope(ScopeName name, F&& f) {
        path_.push_back(PathItem{name.view, 0, false});
        f();
        path_.pop_back();
    }

    /// Skips `n` bytes (a length-prefixed section whose content another reader
    /// handles); fails when fewer remain.
    void skip(std::uint64_t n) {
        if (failed_) {
            return;
        }
        if (remaining() < n) {
            fail("unexpected end of input");
            return;
        }
        pos_ += static_cast<std::size_t>(n);
    }

    /// Latches `message` as the error unless one is already latched.
    void fail(std::string_view message) {
        if (failed_) {
            return;
        }
        failed_ = true;
        std::string where;
        for (const PathItem& p : path_) {
            if (!where.empty()) {
                where += '/';
            }
            where += p.name;
            if (p.indexed) {
                where += '[' + std::to_string(p.index) + ']';
            }
        }
        error_ = (where.empty() ? std::string("stream") : where) + " (byte " +
                 std::to_string(pos_) + "): " + std::string(message);
    }
    [[nodiscard]] bool failed() const { return failed_; }
    [[nodiscard]] const std::string& error() const { return error_; }
    [[nodiscard]] std::uint64_t remaining() const {
        return static_cast<std::uint64_t>(in_.size() - pos_);
    }
    [[nodiscard]] std::uint64_t position() const { return pos_; }

  private:
    struct PathItem {
        std::string_view name;
        std::uint32_t index = 0;
        bool indexed = false;
    };

    /// The next `width` bytes as a little-endian value; 0 once failed.
    std::uint64_t take(std::uint32_t width) {
        if (failed_) {
            return 0;
        }
        if (remaining() < width) {
            fail("unexpected end of input");
            return 0;
        }
        std::uint64_t v = 0;
        for (std::uint32_t i = 0; i < width; ++i) {
            v |= static_cast<std::uint64_t>(in_[pos_ + i]) << (8 * i);
        }
        pos_ += width;
        return v;
    }

    std::span<const std::uint8_t> in_;
    std::size_t pos_ = 0;
    bool failed_ = false;
    std::string error_;
    std::vector<PathItem> path_;
};

/// Container helpers: one code path for every archive (docs/sim/state.md
/// §4.3). Reading default-constructs the elements and then reads their
/// fields, so every persistent type is default-constructible.
namespace ar {

namespace detail {
template <std::size_t I = 0, class Var> bool emplace_alternative(Var& v, std::size_t index) {
    if constexpr (I < std::variant_size_v<Var>) {
        if (index == I) {
            v.template emplace<I>();
            return true;
        }
        return emplace_alternative<I + 1>(v, index);
    } else {
        static_cast<void>(v);
        static_cast<void>(index);
        return false;
    }
}
} // namespace detail

/// `std::vector<T>` (or any sequence with `emplace_back`): u64 count, then
/// `f(element)` for each element in order. Reading clears the target and then
/// default-constructs and fills one element at a time, stopping at the first
/// error, so the memory used tracks the bytes consumed and not the claimed
/// count (a hostile 10 MB snapshot cannot make `count * sizeof(T)` of
/// elements). `min_wire_bytes` is the smallest encoding of one element (the
/// least the field list writes for it; default 1): a count that cannot fit
/// the remaining input at that size fails before anything is allocated.
template <class A, class V, class F>
void seq(A& a, V& v, std::uint64_t min_wire_bytes, F&& f) {
    std::uint64_t n = v.size();
    a.count(n, min_wire_bytes);
    if constexpr (A::kReading) {
        v.clear();
        for (std::uint64_t i = 0; i < n && !a.failed(); ++i) {
            v.emplace_back();
            f(v.back());
        }
    } else {
        for (auto& e : v) {
            f(e);
        }
    }
}
template <class A, class V, class F> void seq(A& a, V& v, F&& f) {
    seq(a, v, std::uint64_t{1}, std::forward<F>(f));
}

/// `std::vector<bool>`: u64 count, then one byte per element, visited through
/// a local bool so no proxy reference escapes the archive.
template <class A, class V> void bools(A& a, V& v) {
    std::uint64_t n = v.size();
    a.count(n);
    if constexpr (A::kReading) {
        v.assign(static_cast<std::size_t>(n), false);
    }
    for (std::size_t i = 0; i < v.size(); ++i) {
        bool b = v[i];
        a.boolean(b);
        if constexpr (A::kReading) {
            v[i] = b;
        }
    }
}

/// `std::optional<T>`: a presence byte, then `f(value)` when present.
template <class A, class O, class F> void opt(A& a, O& o, F&& f) {
    bool has = o.has_value();
    a.boolean(has);
    if constexpr (A::kReading) {
        if (!has) {
            o.reset();
            return;
        }
        o.emplace();
    } else {
        if (!has) {
            return;
        }
    }
    f(*o);
}

/// `std::map<K, V>`: u64 count, then (key, value) in key order. Reading
/// requires strictly ascending keys, which rejects duplicates and also the
/// one non-canonical ordering a re-save could not reproduce. Like seq(), it
/// fills one entry at a time and takes `min_wire_bytes` (key plus value).
template <class A, class M, class FK, class FV>
void map(A& a, M& m, std::uint64_t min_wire_bytes, FK&& fk, FV&& fv) {
    std::uint64_t n = m.size();
    a.count(n, min_wire_bytes);
    if constexpr (A::kReading) {
        m.clear();
        typename M::key_type prev{};
        for (std::uint64_t i = 0; i < n; ++i) {
            typename M::key_type k{};
            fk(k);
            if (a.failed()) {
                return;
            }
            if (i > 0 && !(prev < k)) {
                a.fail("map keys are not strictly ascending (duplicate or unordered key)");
                return;
            }
            auto it = m.emplace_hint(m.end(), k, typename M::mapped_type{});
            fv(it->second);
            prev = k;
        }
    } else {
        for (auto& [k, v] : m) {
            fk(k);
            fv(v);
        }
    }
}
template <class A, class M, class FK, class FV> void map(A& a, M& m, FK&& fk, FV&& fv) {
    map(a, m, std::uint64_t{1}, std::forward<FK>(fk), std::forward<FV>(fv));
}

/// The tag table of a variant: `Tags[alternative index]` is the u8 written
/// for that alternative. A constant of the field list; pairwise distinct tags
/// are checked at compile time (a duplicate would make the second alternative
/// unreachable on read, and a round trip would change the alternative).
template <auto Tags> constexpr bool tags_are_distinct() {
    for (std::size_t i = 0; i < Tags.size(); ++i) {
        for (std::size_t j = i + 1; j < Tags.size(); ++j) {
            if (Tags[i] == Tags[j]) {
                return false;
            }
        }
    }
    return true;
}

/// `std::variant`: an explicit u8 tag (`Tags[alternative index]`, constants of
/// the field list, never `index()`), then `f(alternative)`. Reading rejects a
/// tag no alternative has. A valueless variant (only reachable through an
/// exception, which the sim has none of) is written as a value-initialised
/// first alternative instead of indexing out of bounds.
///
///     ar::variant<kTags>(a, s.var, [&](auto& alt) { ... });
template <std::array Tags, class A, class Var, class F> void variant(A& a, Var& v, F&& f) {
    using V = std::remove_const_t<Var>;
    static_assert(Tags.size() == std::variant_size_v<V>,
                  "one tag per alternative of the variant");
    static_assert(tags_are_distinct<Tags>(), "variant tags must be pairwise distinct");
    static_assert(std::is_same_v<typename decltype(Tags)::value_type, std::uint8_t>,
                  "variant tags are u8");
    std::uint8_t tag = 0;
    if constexpr (!A::kReading) {
        if (v.valueless_by_exception()) {
            V fallback{};
            variant<Tags>(a, fallback, std::forward<F>(f));
            return;
        }
        tag = Tags[v.index()];
    }
    a.u8(tag);
    if constexpr (A::kReading) {
        if (a.failed()) {
            return;
        }
        std::size_t index = Tags.size();
        for (std::size_t i = 0; i < Tags.size(); ++i) {
            if (Tags[i] == tag) {
                index = i;
                break;
            }
        }
        if (!detail::emplace_alternative(v, index)) {
            a.fail("variant tag out of range");
            return;
        }
    }
    std::visit([&](auto& alternative) { f(alternative); }, v);
}

} // namespace ar

} // namespace anno::sim
