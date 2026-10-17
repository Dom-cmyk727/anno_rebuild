#include <algorithm>

#include <anno/assets/figure.hpp>

namespace anno::assets::figure {

namespace fig = formats::figuren;

std::uint8_t pixel_alpha(fig::FrameFormat format, std::uint8_t level) {
    return fig::has_level(format) ? kLevelAlpha[level & 3U] : std::uint8_t{255};
}

bool player_coloured(const fig::Entry& entry) {
    const auto type = entry.record_str("BEWOHNTYPE");
    return type && std::find(kPlayerColouredTypes.begin(), kPlayerColouredTypes.end(), *type) !=
                       kPlayerColouredTypes.end();
}

Rgb recolour(Rgb rgb, Rgb player) {
    const std::int64_t r = rgb[0], g = rgb[1], b = rgb[2];
    // Values ≤ 3 · 255², exact in double; truncation toward zero as __ftol.
    const auto t = static_cast<std::int64_t>(static_cast<double>(r * r + b * b) * 1.5);
    if (g * g <= t) {
        return rgb;
    }
    const auto third = static_cast<double>(0.3333F);
    const std::int64_t v =
        static_cast<std::int64_t>(third * static_cast<double>(r) + third * static_cast<double>(g) +
                                  third * static_cast<double>(b)) &
        0xFF;
    const std::int64_t w =
        std::min<std::int64_t>(static_cast<std::int64_t>(static_cast<double>(v) * 3.0), 255);
    Rgb out{};
    for (std::size_t i = 0; i < 3; ++i) {
        out[i] = static_cast<std::uint8_t>(std::int64_t{player[i]} * w / 255);
    }
    return out;
}

std::optional<formats::Bytes> frame_data(const fig::Frame& frame, formats::Bytes volume) {
    const std::uint64_t start = frame.offset;
    const std::uint64_t end = start + frame.length;
    if (end > volume.size()) {
        return std::nullopt;
    }
    return volume.subspan(static_cast<std::size_t>(start), frame.length);
}

std::vector<std::uint8_t> pixels_to_rgba(const fig::Pixels& px, fig::FrameFormat format,
                                         const formats::tex::Palette* palette,
                                         std::optional<Rgb> player) {
    std::vector<std::uint8_t> out;
    out.reserve(px.size() * 4);
    for (const auto& p : px) {
        if (!p) {
            out.insert(out.end(), {0, 0, 0, 0});
        } else if (palette == nullptr) {
            out.insert(out.end(), {0, 0, 0, 255});
        } else {
            Rgb c = palette->rgb(p->index);
            if (player) {
                c = recolour(c, *player);
            }
            out.insert(out.end(), {c[0], c[1], c[2], pixel_alpha(format, p->level)});
        }
    }
    return out;
}

Result<Rgba, Error> to_rgba(const fig::Entry& entry, const fig::Frame& frame,
                            formats::Bytes volume) {
    const auto data = frame_data(frame, volume);
    if (!data) {
        return Error{Error::Kind::OutsideVolume, 0, {}};
    }
    return decode(entry, frame, *data);
}

Result<Rgba, Error> decode(const fig::Entry& entry, const fig::Frame& frame, formats::Bytes data,
                           std::optional<Rgb> player) {
    const auto format = frame.format();
    if (!format) {
        return Error{Error::Kind::UnknownFormat, 0, {}};
    }
    auto px = fig::decode_frame(*format, data, frame.width, frame.height);
    if (!px) {
        return Error{Error::Kind::Decode, 0, px.error()};
    }
    const formats::tex::Palette* palette = nullptr;
    if (*format != fig::FrameFormat::Alpha) {
        if (frame.palette >= entry.palettes.size()) {
            return Error{Error::Kind::MissingPalette, frame.palette, {}};
        }
        palette = &entry.palettes[frame.palette].palette;
    }
    return Rgba{frame.width, frame.height, pixels_to_rgba(px.value(), *format, palette, player)};
}

} // namespace anno::assets::figure
