#include <limits>

#include <anno/formats/chunk.hpp>

namespace anno::formats::chunk {

namespace {

/// Bytes of the name field up to the first NUL.
std::string_view name_bytes(const std::uint8_t* field) {
    std::size_t len = 0;
    while (len < kNameLen && field[len] != 0) {
        ++len;
    }
    return {reinterpret_cast<const char*>(field), len};
}

bool upper(char c) {
    return c >= 'A' && c <= 'Z';
}
bool digit(char c) {
    return c >= '0' && c <= '9';
}

/// [A-Z][A-Z0-9_]{1,11}, the pattern of tools/chunks.py (up to 12
/// characters, i.e. a field without NUL is accepted when reading).
bool valid_name(std::string_view n) {
    if (n.size() < 2 || n.size() > kNameLen || !upper(n[0])) {
        return false;
    }
    for (char c : n.substr(1)) {
        if (!upper(c) && !digit(c) && c != '_') {
            return false;
        }
    }
    return true;
}

struct Header {
    NameField field;
    std::size_t size;
};

/// Reads and validates the header at `off` in `buf[0..end)`.
Result<Header, Error> read_header(Bytes buf, std::size_t off, std::size_t end) {
    if (end < off || end - off < kHeaderLen) {
        return Error{ErrorKind::Truncated, off};
    }
    Header h{};
    for (std::size_t i = 0; i < kNameLen; ++i) {
        h.field[i] = buf[off + i];
    }
    if (!valid_name(name_bytes(h.field.data()))) {
        return Error{ErrorKind::InvalidName, off};
    }
    const std::uint32_t size = u32_le(buf, off + kNameLen);
    if (size > end - off - kHeaderLen) {
        return Error{ErrorKind::SizeOutOfBounds, off, size};
    }
    h.size = size;
    return h;
}

std::optional<std::vector<Chunk>> parse_seq(Bytes buf, std::size_t start, std::size_t end,
                                            std::size_t depth);

void probe_children(Bytes buf, Chunk& c, std::size_t start, std::size_t end, std::size_t depth) {
    if (depth < kMaxDepth && end - start >= kHeaderLen) {
        if (auto kids = parse_seq(buf, start, end, depth + 1)) {
            c.is_container = true;
            c.children = std::move(*kids);
        }
    }
}

std::optional<std::vector<Chunk>> parse_seq(Bytes buf, std::size_t start, std::size_t end,
                                            std::size_t depth) {
    std::vector<Chunk> out;
    std::size_t off = start;
    while (off < end) {
        auto h = read_header(buf, off, end);
        if (!h) {
            return std::nullopt;
        }
        const std::size_t payload_start = off + kHeaderLen;
        const std::size_t payload_end = payload_start + h.value().size;
        Chunk c;
        c.name_field = h.value().field;
        c.offset = off;
        c.payload = buf.subspan(payload_start, h.value().size);
        probe_children(buf, c, payload_start, payload_end, depth);
        out.push_back(std::move(c));
        off = payload_end;
    }
    return out;
}

void tree_lines(const std::vector<Chunk>& chunks, std::size_t depth, std::string& out) {
    for (const Chunk& c : chunks) {
        out.append(depth * 2, ' ');
        out.append(c.name());
        out.append(" off=" + std::to_string(c.offset) +
                   " size=" + std::to_string(c.payload.size()));
        out.push_back('\n');
        if (c.is_container) {
            tree_lines(c.children, depth + 1, out);
        }
    }
}

} // namespace

std::string_view Chunk::name() const {
    return name_bytes(name_field.data());
}

std::size_t Chunk::encoded_len() const {
    if (!is_container) {
        return kHeaderLen + payload.size();
    }
    std::size_t n = kHeaderLen;
    for (const Chunk& k : children) {
        n += k.encoded_len();
    }
    return n;
}

std::string Error::message() const {
    switch (kind) {
    case ErrorKind::Truncated:
        return "truncated chunk header at " + std::to_string(offset);
    case ErrorKind::InvalidName:
        return "invalid chunk name at " + std::to_string(offset);
    case ErrorKind::SizeOutOfBounds:
        return "chunk at " + std::to_string(offset) + " declares size " + std::to_string(size) +
               " past end of data";
    }
    return "chunk error";
}

Result<ChunkFile, Error> parse_file(Bytes buf) {
    // An exact sequence of at least one chunk (an empty file is not one).
    if (!buf.empty()) {
        if (auto seq = parse_seq(buf, 0, buf.size(), 0)) {
            return ChunkFile{std::move(*seq), buf.subspan(buf.size()), buf.size()};
        }
    }
    auto h = read_header(buf, 0, buf.size());
    if (!h) {
        return h.error();
    }
    const std::size_t end = kHeaderLen + h.value().size;
    Chunk root;
    root.name_field = h.value().field;
    root.offset = 0;
    root.payload = buf.subspan(kHeaderLen, h.value().size);
    probe_children(buf, root, kHeaderLen, end, 0);
    ChunkFile file;
    file.chunks.push_back(std::move(root));
    file.trailing = buf.subspan(end);
    file.trailing_offset = end;
    return file;
}

std::optional<std::vector<Chunk>> parse_chunks(Bytes data) {
    // An empty range is an empty sequence (as the Rust reader and
    // tools/chunks.py parse).
    return parse_seq(data, 0, data.size(), 1);
}

std::optional<NameField> name_field(std::string_view name) {
    // New chunks get a NUL inside the field.
    if (name.size() >= kNameLen || !valid_name(name)) {
        return std::nullopt;
    }
    NameField f{};
    for (std::size_t i = 0; i < name.size(); ++i) {
        f[i] = static_cast<std::uint8_t>(name[i]);
    }
    return f;
}

bool write_chunk(const Chunk& chunk, ByteBuffer& out) {
    const std::size_t payload_len = chunk.encoded_len() - kHeaderLen;
    if (payload_len > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    out.insert(out.end(), chunk.name_field.begin(), chunk.name_field.end());
    put_u32_le(out, static_cast<std::uint32_t>(payload_len));
    if (!chunk.is_container) {
        put_bytes(out, chunk.payload);
        return true;
    }
    for (const Chunk& k : chunk.children) {
        if (!write_chunk(k, out)) {
            return false;
        }
    }
    return true;
}

bool write_file(const ChunkFile& file, ByteBuffer& out) {
    for (const Chunk& c : file.chunks) {
        if (!write_chunk(c, out)) {
            return false;
        }
    }
    put_bytes(out, file.trailing);
    return true;
}

std::string tree(const ChunkFile& file) {
    std::string out;
    tree_lines(file.chunks, 0, out);
    if (!file.trailing.empty()) {
        out += "TRAILING off=" + std::to_string(file.trailing_offset) +
               " len=" + std::to_string(file.trailing.size()) + "\n";
    }
    return out;
}

} // namespace anno::formats::chunk
