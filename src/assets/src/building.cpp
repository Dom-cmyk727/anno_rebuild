#include <limits>

#include <anno/assets/building.hpp>
#include <anno/assets/figure.hpp>

namespace anno::assets::building {

namespace fig = formats::figuren;

Result<Rgba, Error> to_rgba(const formats::anlagen::AnlagenFile& file, const fig::Entry& entry,
                            std::uint32_t group, const fig::Frame& frame) {
    const auto format = frame.format();
    if (!format) {
        return Error{Error::Kind::UnknownFormat, 0, {}};
    }
    const formats::anlagen::Volume* volume = file.volume(group);
    const auto data = volume != nullptr ? figure::frame_data(frame, volume->data) : std::nullopt;
    if (!data) {
        return Error{Error::Kind::OutsideVolume, 0, {}};
    }
    auto decoded = fig::decode_frame_extended(*format, *data, frame.width, frame.height);
    if (!decoded) {
        return Error{Error::Kind::Decode, 0, decoded.error()};
    }
    const auto& [px, width] = decoded.value();
    const formats::tex::Palette* palette = nullptr;
    if (*format != fig::FrameFormat::Alpha) {
        if (frame.palette >= entry.palette_ids.size()) {
            return Error{Error::Kind::MissingPalette, frame.palette, {}};
        }
        const std::uint32_t id = entry.palette_ids[frame.palette];
        for (const auto& t : file.colors) {
            if (t.palette.id == id && id != 0) {
                palette = &t.palette;
                break;
            }
        }
        if (palette == nullptr) {
            return Error{Error::Kind::MissingPalette, frame.palette, {}};
        }
    }
    if (width > std::numeric_limits<std::uint16_t>::max()) {
        return Error{Error::Kind::OutsideVolume, 0, {}};
    }
    return Rgba{static_cast<std::uint16_t>(width), frame.height,
                figure::pixels_to_rgba(px, *format, palette, std::nullopt)};
}

} // namespace anno::assets::building
