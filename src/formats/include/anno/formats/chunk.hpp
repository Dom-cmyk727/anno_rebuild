// Max Design chunk container, shared by .sze, .scp, .tex, .inc, .zei and
// Anlagen.dat. Spec: docs/formats/chunk-container.md; reference:
// tools/chunks.py (same semantics, including its two heuristics: the name
// pattern [A-Z][A-Z0-9_]{1,11} and the nesting limit kMaxDepth).
//
// A chunk is a 12-byte name field, a u32 little-endian payload size and the
// payload. A payload is a container when it parses exactly into one or more
// valid chunks. Writing re-encodes containers from their children and copies
// leaf payloads and name fields verbatim; every corpus file round-trips
// byte-identically.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/result.hpp>

namespace anno::formats::chunk {

/// Size of the name field.
inline constexpr std::size_t kNameLen = 12;
/// Size of a chunk header (name field + u32 size).
inline constexpr std::size_t kHeaderLen = kNameLen + 4;
/// Payloads are probed for children only up to this depth (as tools/chunks.py).
inline constexpr std::size_t kMaxDepth = 6;

using NameField = std::array<std::uint8_t, kNameLen>;

/// One chunk; `payload` points into the parsed buffer.
struct Chunk {
    /// The raw name field, including the NUL and any bytes after it.
    NameField name_field{};
    /// Absolute offset of the header in the file.
    std::size_t offset = 0;
    Bytes payload;
    /// Whether the payload parsed as a chunk sequence (then `children` holds it).
    bool is_container = false;
    std::vector<Chunk> children;

    /// The name up to the first NUL (validated ASCII).
    [[nodiscard]] std::string_view name() const;
    /// Length when written: header plus re-encoded payload.
    [[nodiscard]] std::size_t encoded_len() const;
};

/// A parsed file: the top-level chunks and any bytes after the root chunk.
struct ChunkFile {
    std::vector<Chunk> chunks;
    /// Bytes after the root chunk when the file is not an exact chunk
    /// sequence (60 bytes on 444 .scp; OQ-001).
    Bytes trailing;
    std::size_t trailing_offset = 0;
};

enum class ErrorKind { Truncated, InvalidName, SizeOutOfBounds };

struct Error {
    ErrorKind kind;
    std::size_t offset;
    std::uint32_t size = 0;
    [[nodiscard]] std::string message() const;
};

/// Parses a whole file: an exact chunk sequence, or else the root chunk plus
/// trailing bytes.
Result<ChunkFile, Error> parse_file(Bytes buf);

/// Parses `data` as an exact chunk sequence (for payloads with a prefix that
/// generic detection cannot see into); offsets relative to `data`.
std::optional<std::vector<Chunk>> parse_chunks(Bytes data);

/// A zero-padded name field for a new chunk; nullopt if `name` does not
/// match the name pattern.
std::optional<NameField> name_field(std::string_view name);

/// Appends one chunk; containers are rebuilt from their children. False if a
/// payload exceeds u32.
bool write_chunk(const Chunk& chunk, ByteBuffer& out);

/// The whole file: chunks, then the trailing bytes. False as write_chunk.
bool write_file(const ChunkFile& file, ByteBuffer& out);

/// The tree in the line format of `tools/chunks.py tree`: one
/// `<indent>NAME off=<offset> size=<size>` line per chunk (two spaces per
/// level), then `TRAILING off=<offset> len=<len>` if trailing bytes exist.
std::string tree(const ChunkFile& file);

} // namespace anno::formats::chunk
