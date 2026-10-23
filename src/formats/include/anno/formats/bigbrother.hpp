// Scenario script: the BIGBROTHER chunk of .sze scenarios and savegames.
// Spec: docs/formats/bigbrother.md; reference: tools/bigbrother.py.
//
// - Plain Latin-1 text. Every byte is one of the tokens of §2 (blanks, line
//   ends and `$` comments are tokens too), so `emit` re-creates the input
//   byte for byte: CRLF and LF, blanks, comments and touching tokens as read.
// - Sections of §3: the six known ones, each once and in this order, are
//   read (`known_sections`); any other `[NAME]` section (the interpreter also
//   knows STARTUP, OQ-122) is kept as read and not interpreted.
// - The kernel grammar of §3.4. A call is a name plus values: no action,
//   condition or query names of §4-§6 are known here (a later stage binds
//   names to handlers).
// - The interpreter's reading (§3.4, docs/re/annoscene.md §12): `grouped`
//   (`&&`/`||` of equal rank, grouped to the right) and `read_on` (the block
//   statements without `;` and what each swallows). No evaluation here.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/result.hpp>

namespace anno::formats::bigbrother {

/// A refusal: the byte offset in the script (the chunk's payload), its line
/// (1-based, counted in LF bytes) and what was wrong.
struct Error {
    std::size_t offset = 0;
    std::size_t line = 0;
    std::string message;
    /// `line <line>, offset <offset>: <message>`.
    [[nodiscard]] std::string text() const;
};

// ---- tokens (§2) ----------------------------------------------------------

enum class TokenKind { Newline, Blank, Comment, Section, String, Number, Name, Operator };

/// The kind's name in the reference (tools/bigbrother.py) and the dump:
/// NEWLINE, WS, COMMENT, SECTION, STRING, NUMBER, IDENT, OP.
std::string_view kind_name(TokenKind kind);

struct Token {
    TokenKind kind = TokenKind::Blank;
    /// Offset of the first byte in the script.
    std::size_t offset = 0;
    /// The bytes as read (Latin-1).
    std::string text;
};

/// Every byte of `text` in exactly one token, in order. Refuses the first
/// byte that starts no token.
Result<std::vector<Token>, Error> lex(Bytes text);

// ---- the tree -------------------------------------------------------------

enum class ValueKind { Bool, Number, String };

struct Value {
    ValueKind kind = ValueKind::Bool;
    /// As written: TRUE/FALSE, the digits, or the string with its quotes.
    std::string text;
    /// A string without its quotes (the text itself for the other kinds).
    [[nodiscard]] std::string_view contents() const;
};

/// `SET NAME = value [;]` at the top level, `NAME = value [;]` in a block.
struct Assign {
    std::string name;
    Value value;
    bool is_set = false;
    /// Four shipped statements have no `;` (§3.4); accepted only at a line end.
    bool semicolon = true;
    /// Offset of `SET`, or of the name in a block.
    std::size_t offset = 0;
};

/// `NAME( value, ... ) [;]`, or `NAME;` without parentheses.
struct Call {
    std::string name;
    std::vector<Value> args;
    bool parens = true;
    bool semicolon = true;
    /// Offset of the name.
    std::size_t offset = 0;
};

using Statement = std::variant<Assign, Call>;

[[nodiscard]] std::size_t offset_of(const Statement& s);
[[nodiscard]] bool has_semicolon(const Statement& s);

/// A term of a condition: `SUBJECT "name" QUERY ["arg"] [compare value]`
/// with SUBJECT one of `subjects()`, or `NAME [compare value]` (a variable
/// or a timer).
struct Primary {
    /// Empty for a variable.
    std::string subject;
    /// The subject's string without quotes, or the variable's name.
    std::string name;
    /// Empty for a variable.
    std::string query;
    /// The query's string argument without quotes.
    std::optional<std::string> arg;
    /// `==`, `<` or `>`; empty without a comparison.
    std::string op;
    std::optional<Value> value;
    /// Offset of the first token.
    std::size_t offset = 0;
};

enum class Connective { And, Or };

struct Condition;

/// A primary, or a parenthesised condition (then `group` holds it).
struct Term {
    Primary primary;
    /// One element for `( condition )`, empty for a primary.
    std::vector<Condition> group;
};

/// Terms joined by `&&` / `||` as written (`connectives.size() + 1 ==
/// terms.size()`); see `grouped` for the interpreter's reading.
struct Condition {
    std::vector<Term> terms;
    std::vector<Connective> connectives;
};

struct InitBlock {
    std::vector<Statement> body;
    std::size_t offset = 0;
};

struct If {
    Condition condition;
    std::vector<Statement> then_block;
    bool has_else = false;
    std::vector<Statement> else_block;
    std::size_t offset = 0;
};

using KernelItem = std::variant<InitBlock, Assign, If>;

/// Numbers in define lines are kept as written (`-?[0-9]+`, no length
/// limit); `to_integer` converts one that fits.
struct ObjectDefine {
    /// One of `object_kinds()`.
    std::string kind;
    /// Without quotes.
    std::string name;
    std::vector<std::string> numbers;
    std::size_t offset = 0;
};

struct AreaDefine {
    std::string name;
    /// Four numbers (two opposite corners, likely; §3.2).
    std::vector<std::string> corners;
    std::size_t offset = 0;
};

struct PlayerMapping {
    std::string name;
    std::string slot;
    std::size_t offset = 0;
};

struct Signal {
    std::string name;
    bool value = false;
    std::size_t offset = 0;
};

struct Camera {
    std::string x, y;
    std::size_t offset = 0;
};

enum class SectionKind { Objects, Areas, Players, Kernel, Signals, Camera, Unknown };

struct KnownSection {
    std::string_view name;
    SectionKind kind;
};

/// The sections every shipped script has, in their order (§3, P: 50 of 50).
/// Data, not a limit: other names are kept as `SectionKind::Unknown`.
std::span<const KnownSection> known_sections();

struct ObjectKind {
    std::string_view name;
    std::size_t numbers;
};

/// The define-line kinds of `[OBJECT_DEFINE]` and their number counts (§3.1).
std::span<const ObjectKind> object_kinds();

/// The subjects of a condition's primary (§3.4 grammar).
std::span<const std::string_view> subjects();

struct Section {
    /// The header as written, brackets included.
    std::string name;
    SectionKind kind = SectionKind::Unknown;
    /// Every token of the section, the header first.
    std::vector<Token> tokens;
};

struct Script {
    /// In file order; together they hold every byte of the script.
    std::vector<Section> sections;
    std::vector<ObjectDefine> objects;
    std::vector<AreaDefine> areas;
    std::vector<PlayerMapping> players;
    std::vector<KernelItem> kernel;
    std::vector<Signal> signals;
    Camera camera;
};

/// Parses a script (the BIGBROTHER payload). Anything outside the grammar
/// read from the 50 shipped scripts is refused, with the offending offset
/// and line.
Result<Script, Error> parse(Bytes text);

/// The script's bytes: its tokens in order (the input of `parse`).
ByteBuffer emit(const Script& script);

/// The BIGBROTHER payload of a scenario (root ANNO/ANNOADDON1 > SCENE >
/// BIGBROTHER) or a savegame (root > BIGBROTHER); exactly one must exist.
Result<Bytes, std::string> find_payload(Bytes file);

/// A `-?[0-9]+` number that fits in 64 bits.
std::optional<std::int64_t> to_integer(std::string_view number);
/// A number as its decimal value: leading zeros dropped, `-0` as `0`.
std::string canonical_integer(std::string_view number);

// ---- the interpreter's reading (docs/re/annoscene.md §12) ----------------

/// A condition as the interpreter groups it: a term (`leaf`), or a
/// connective with two operands.
struct Grouped {
    const Primary* leaf = nullptr;
    Connective connective = Connective::And;
    /// Empty for a leaf; else the left and the right operand.
    std::vector<Grouped> operands;
};

/// `&&` and `||` have equal rank and group to the right: the first
/// connective takes the whole rest as its right operand, `a op b op c` =
/// `a op (b op c)`; parentheses group as written; a group of one term is that
/// term (condition parser AnnoScene 0x10022bf0). The leaves point into
/// `condition`.
Grouped grouped(const Condition& condition);

/// Whether a condition's own connectives mix `&&` and `||`, or one of its
/// groups does.
bool is_mixed(const Condition& condition);

/// `PLAYER "A" QUERY "arg" > 1`, `NAME == TRUE`, as the reference prints it.
std::string describe(const Primary& primary);
/// `(a && (b) || c)`: the condition as written, in parentheses.
std::string describe(const Condition& condition);
/// `(a && (b || c))`: the grouping, each connective in parentheses.
std::string describe(const Grouped& grouped);

/// A block statement without `;` as the interpreter reads it (reader
/// AnnoScene 0x1001c9e0): it ends at the next `;` of the kernel text,
/// wherever that is (strpbrk, 0x1001cbca): in a later statement, in a comment
/// or past the block's `}`.
struct ReadOn {
    const Statement* statement = nullptr;
    /// Offset of that `;`; none when no `;` follows in the kernel text (the
    /// interpreter then refuses the whole kernel).
    std::optional<std::size_t> end;
    /// The statements of the same block that start before `end`: read into
    /// the statement and lost. A `;` past the `}` loses nothing.
    std::vector<const Statement*> lost;
};

/// Every block statement without `;` (`INITBLOCK`, THEN, ELSE, in file
/// order) that is not itself lost. A top-level `SET` without `;` loses
/// nothing (the reader takes the next word) and is not listed. The kernel
/// text ends at the next `[` after its header (0x1000b3f0). The pointers
/// point into `script`.
std::vector<ReadOn> read_on(const Script& script);

} // namespace anno::formats::bigbrother
