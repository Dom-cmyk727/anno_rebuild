// Plain-text data files: Texte.dat and the keyed files in Data/
// (BGruppen.dat, Wachstum.dat, wegspeed.dat, Felder.dat, ...).
// Spec: docs/formats/data-text.md; reference: tools/text.py.
//
// - Windows-1252 bytes, CR LF line ends; this module works on raw bytes.
// - Keyed files: a line is blank, a `;` comment (never inside "..."), a
//   section header [NAME], or an entry `Key: value` (Key = [A-Za-z_][A-Za-z0-9_]*).
//   Values are comma-separated fields. Other lines are kept as Other.
// - Texte.dat: [NAME] headers (names may contain ':'), each followed by its
//   text lines verbatim. In [ANLAGEN] a line is `<id>:<name>`.
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <anno/formats/bytes.hpp>

namespace anno::formats::text {

enum class Ending { CrLf, Lf, None };

/// One line without its terminator.
struct Line {
    std::size_t number = 0; ///< 1-based
    Bytes text;
    Ending ending = Ending::None;
};

/// Splits at `\n` (a preceding `\r` belongs to the terminator); a file that
/// ends with a terminator yields a last, empty line without one.
std::vector<Line> lines(Bytes buf);
/// Joins lines with their terminators (byte-identical to the input of lines()).
ByteBuffer write_lines(const std::vector<Line>& lines);

/// The text before a `;` comment (a `;` inside "..." does not start one).
Bytes strip_comment(Bytes text);

enum class KeyedKindTag { Blank, Section, Entry, Other };

/// What a line of a keyed file holds.
struct KeyedKind {
    KeyedKindTag tag = KeyedKindTag::Blank;
    /// Section: the name; Entry: the key; Other: the trimmed code.
    Bytes name;
    /// Entry: the value, trimmed, comment removed.
    Bytes value;
};

KeyedKind keyed_kind(Bytes text);
std::vector<std::pair<Line, KeyedKind>> keyed(Bytes buf);

/// Splits a value at commas outside "..."; fields trimmed. Empty value = no fields.
std::vector<Bytes> fields(Bytes value);
/// A decimal integer field (optional '-').
std::optional<std::int64_t> to_int(Bytes field);
/// All integers of a value separated by commas and/or whitespace; nullopt if
/// a token is not an integer (reads Wachstum.dat line 765, OQ-029 g).
std::optional<std::vector<std::int64_t>> ints(Bytes value);
/// A "..." field without its quotes.
std::optional<Bytes> quoted(Bytes field);
/// A decimal field as an exact integer in units of 10^-digits (no floating
/// point); nullopt if not a decimal or more fractional digits than `digits`.
std::optional<std::int64_t> fixed(Bytes field, unsigned digits);

/// One `Key: value` entry.
struct Entry {
    std::size_t line = 0;
    Bytes key;
    Bytes value;
};

/// Entries under every [name] header (nullopt name = before the first header).
std::vector<Entry> section_entries(Bytes buf, std::optional<std::string_view> name);
/// Records starting at key `start`: (entries before the first, records).
std::pair<std::vector<Entry>, std::vector<std::vector<Entry>>>
split_records(const std::vector<Entry>& entries, std::string_view start);

/// A section of Texte.dat.
struct TextSection {
    Bytes name;
    std::size_t line = 0;
    std::vector<Bytes> lines;
};

struct Texte {
    std::vector<Bytes> preamble;
    std::vector<TextSection> sections;
    static Texte parse(Bytes buf);
    [[nodiscard]] const TextSection* section(std::string_view name) const;
};

/// `<id>:<text>` (decimal id), as in Texte.dat [ANLAGEN].
std::optional<std::pair<std::uint32_t, Bytes>> id_text(Bytes line);

/// True if `b` equals the ASCII string `s`.
bool equals(Bytes b, std::string_view s);

} // namespace anno::formats::text
