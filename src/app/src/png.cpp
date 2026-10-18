#include <array>
#include <fstream>
#include <vector>

#include <anno/app/png.hpp>

namespace anno::app {

namespace {

std::array<std::uint32_t, 256> crc_table() {
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t n = 0; n < 256; ++n) {
        std::uint32_t c = n;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1U) != 0 ? 0xEDB88320U ^ (c >> 1) : c >> 1;
        }
        t[n] = c;
    }
    return t;
}

void put_u32_be(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int s = 24; s >= 0; s -= 8) {
        out.push_back(static_cast<std::uint8_t>(v >> s));
    }
}

/// One chunk: length, type, data, CRC over type and data.
void write_chunk(std::ofstream& f, const char (&type)[5], std::span<const std::uint8_t> data) {
    std::vector<std::uint8_t> head;
    put_u32_be(head, static_cast<std::uint32_t>(data.size()));
    std::vector<std::uint8_t> body(type, type + 4);
    body.insert(body.end(), data.begin(), data.end());
    std::vector<std::uint8_t> crc;
    put_u32_be(crc, crc32(body));
    f.write(reinterpret_cast<const char*>(head.data()), 4);
    f.write(reinterpret_cast<const char*>(body.data()), static_cast<std::streamsize>(body.size()));
    f.write(reinterpret_cast<const char*>(crc.data()), 4);
}

} // namespace

std::uint32_t crc32(std::span<const std::uint8_t> data, std::uint32_t crc) {
    static const auto table = crc_table();
    std::uint32_t c = crc ^ 0xFFFFFFFFU;
    for (const std::uint8_t b : data) {
        c = table[(c ^ b) & 0xFFU] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFU;
}

std::string write_png(const std::filesystem::path& path, std::uint32_t width, std::uint32_t height,
                      std::span<const std::uint8_t> rgba) {
    const std::size_t row = std::size_t{width} * 4;
    if (width == 0 || height == 0) {
        return "PNG images need a width and height of at least 1";
    }
    if (rgba.size() != row * height) {
        return "pixel buffer does not match the image size";
    }
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        return "cannot create " + path.string();
    }
    const std::array<std::uint8_t, 8> signature = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    f.write(reinterpret_cast<const char*>(signature.data()), signature.size());
    std::vector<std::uint8_t> ihdr;
    put_u32_be(ihdr, width);
    put_u32_be(ihdr, height);
    // Bit depth 8, colour type 6 (RGBA), deflate, adaptive filtering, no interlace.
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
    write_chunk(f, "IHDR", ihdr);
    // zlib stream: header 78 01, stored blocks of ≤ 65,535 bytes over the
    // filtered rows (filter type 0 per row), Adler-32.
    std::vector<std::uint8_t> z = {0x78, 0x01};
    std::uint32_t a = 1, b = 0;
    std::vector<std::uint8_t> block;
    constexpr std::size_t kMaxBlock = 65535;
    // Flushes `block` as a stored block; `last` sets BFINAL.
    auto flush = [&](bool last) {
        const auto n = static_cast<std::uint16_t>(block.size());
        z.push_back(last ? 1 : 0);
        z.push_back(static_cast<std::uint8_t>(n & 0xFF));
        z.push_back(static_cast<std::uint8_t>(n >> 8));
        z.push_back(static_cast<std::uint8_t>(~n & 0xFF));
        z.push_back(static_cast<std::uint8_t>((~n >> 8) & 0xFF));
        z.insert(z.end(), block.begin(), block.end());
        block.clear();
        // Keep IDAT chunks around 1 MiB.
        if (z.size() >= (1U << 20) || last) {
            write_chunk(f, "IDAT", z);
            z.clear();
        }
    };
    auto put = [&](std::uint8_t v) {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
        block.push_back(v);
        if (block.size() == kMaxBlock) {
            flush(false);
        }
    };
    for (std::uint32_t y = 0; y < height; ++y) {
        put(0);
        for (std::size_t i = 0; i < row; ++i) {
            put(rgba[std::size_t{y} * row + i]);
        }
    }
    // The final block (possibly empty), then the Adler-32 in its own IDAT.
    flush(true);
    std::vector<std::uint8_t> adler;
    put_u32_be(adler, b << 16 | a);
    write_chunk(f, "IDAT", adler);
    write_chunk(f, "IEND", {});
    return f ? std::string() : "write failed: " + path.string();
}

} // namespace anno::app
