#include <anno/formats/anlagen.hpp>

namespace anno::formats::anlagen {

Result<AnlagenFile, std::string> AnlagenFile::parse(Bytes buf) {
    auto cs = colors::parse_anlagen(buf);
    if (!cs) {
        return cs.error();
    }
    auto f = chunk::parse_file(buf);
    if (!f) {
        return std::string("Anlagen.dat: no root");
    }
    const chunk::Chunk& root = f.value().chunks.front();
    if (!root.is_container) {
        return std::string("Anlagen.dat: root is not a container");
    }
    AnlagenFile out;
    out.colors = std::move(cs).value();
    const chunk::Chunk* anl = nullptr;
    for (const auto& c : root.children) {
        if (c.name() == "ANLAGEN") {
            anl = &c;
            break;
        }
    }
    if (anl == nullptr) {
        return std::string("Anlagen.dat: no ANLAGEN");
    }
    auto entries = chunk::parse_chunks(anl->payload);
    if (!entries) {
        return std::string("Anlagen.dat: ANLAGEN is not a chunk sequence");
    }
    out.entry_chunks = std::move(*entries);
    for (const auto& c : out.entry_chunks) {
        auto e = figuren::Entry::parse(c);
        if (!e) {
            return e.error();
        }
        out.entries.push_back(std::move(e).value());
    }
    for (const auto& c : root.children) {
        if (c.name() != "TEXTURES") {
            continue;
        }
        if (c.payload.size() < kVolumePrefixLen) {
            return std::string("Anlagen.dat: TEXTURES shorter than 8");
        }
        out.volumes.push_back(
            {u32_le(c.payload, 0), u32_le(c.payload, 4), c.payload.subspan(kVolumePrefixLen)});
    }
    return out;
}

const Volume* AnlagenFile::volume(std::uint32_t group) const {
    for (const auto& v : volumes) {
        if (v.group == group) {
            return &v;
        }
    }
    return nullptr;
}

const figuren::Entry* AnlagenFile::entry(std::uint32_t id) const {
    for (const auto& e : entries) {
        if (e.id == id) {
            return &e;
        }
    }
    return nullptr;
}

std::optional<std::pair<std::uint8_t, std::uint8_t>>
AnlagenFile::footprint(std::uint32_t id) const {
    const figuren::Entry* e = entry(id);
    if (e == nullptr) {
        return std::nullopt;
    }
    auto head = e->record("HEAD");
    if (!head || head->size() < 2) {
        return std::nullopt;
    }
    return std::pair{(*head)[0], (*head)[1]};
}

} // namespace anno::formats::anlagen
