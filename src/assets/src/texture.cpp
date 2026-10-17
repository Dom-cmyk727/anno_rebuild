#include <anno/assets/texture.hpp>

namespace anno::assets {

std::string Error::message() const {
    switch (kind) {
    case Kind::MissingTexturePalette:
        return "palette " + std::to_string(id) + " not in file";
    case Kind::UnknownFormat:
        return "unknown frame format";
    case Kind::MissingPalette:
        return "colour table " + std::to_string(id) + " not in entry";
    case Kind::OutsideVolume:
        return "frame data outside the volume";
    case Kind::Decode:
        break;
    }
    return detail;
}

Result<Rgba, Error> to_rgba(const formats::tex::TexFile& file,
                            const formats::tex::Texture& texture) {
    const formats::tex::Palette* palette = file.palette(texture.head.palette_id);
    if (palette == nullptr) {
        return Error{Error::Kind::MissingTexturePalette, texture.head.palette_id, {}};
    }
    auto indices = texture.decode();
    if (!indices) {
        return Error{Error::Kind::Decode, 0, indices.error()};
    }
    Rgba out{texture.head.width, texture.head.height, {}};
    out.pixels.reserve(indices.value().size() * 4);
    for (const auto& p : indices.value()) {
        if (p) {
            const auto rgb = palette->rgb(*p);
            out.pixels.insert(out.pixels.end(), {rgb[0], rgb[1], rgb[2], 255});
        } else {
            out.pixels.insert(out.pixels.end(), {0, 0, 0, 0});
        }
    }
    return out;
}

} // namespace anno::assets
