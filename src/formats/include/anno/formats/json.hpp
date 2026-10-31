// A strict JSON reader (RFC 8259) and a canonical writer, for our own tool
// files (the ship-start manifest, docs/sim/ship-start-integration.md). Not an
// original format. No exceptions.
//
// Strictness, all of it rejecting, never repairing:
//   - the grammar of RFC 8259 and nothing else (no comments, no trailing commas,
//     no NaN / Infinity, no leading zeros, no leading '+', no BOM);
//   - an object with a repeated key is an error, at any depth;
//   - a number without a fraction and without an exponent is an Integer and must
//     fit int64; every other number is Real. A `true` is a Bool, never an Integer;
//   - a string is valid UTF-8 without raw control characters; a \u escape that is
//     half of a surrogate pair is an error (Python's json accepts it);
//   - nesting deeper than Limits::max_depth is an error.
// Path to raise a limit: the Limits value, per call.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/result.hpp>

namespace anno::formats::json {

enum class Kind : std::uint8_t { Null, Bool, Integer, Real, String, Array, Object };

struct Value {
    Kind kind = Kind::Null;
    bool boolean = false;
    std::int64_t integer = 0;
    /// String: the UTF-8 text. Real: the number as written.
    std::string text;
    std::vector<Value> items;
    /// Object members in file order; keys are unique.
    std::vector<std::pair<std::string, Value>> members;

    /// The member named `key`, or nullptr (also when this is not an object).
    [[nodiscard]] const Value* find(std::string_view key) const;

    static Value make_integer(std::int64_t v);
    static Value make_string(std::string s);
    static Value make_array();
    static Value make_object();
};

struct Error {
    /// Byte offset of the offending input.
    std::size_t offset = 0;
    std::string message;
};

struct Limits {
    /// Container nesting. The manifest is 4 deep; a hostile file must not make
    /// the recursion deep (Python's own limit is about 1000).
    std::uint32_t max_depth = 64;
};

[[nodiscard]] Result<Value, Error> parse(Bytes text, Limits limits = {});

/// Compact separators ',' and ':', object keys sorted by code point, arrays in
/// order, every non-ASCII character as \uXXXX (surrogate pairs above U+FFFF) and
/// '"', '\\', \b \f \n \r \t, and every other character outside ' '..'~' as
/// \u00XX: Python's `json.dumps(v, sort_keys=True, separators=(',', ':'),
/// ensure_ascii=True)`. The result is ASCII. Real values are not supported (the
/// manifest has none) and are written as their text.
[[nodiscard]] std::string canonical(const Value& value);

} // namespace anno::formats::json
