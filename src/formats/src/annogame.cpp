#include <anno/formats/annogame.hpp>

#include "pe_table_view.hpp"
namespace anno::formats::annogame {
Result<ResourceTables, std::string> read_resource_tables(Bytes dll, std::uint32_t climate) {
    const auto parsed = pe_tables_detail::read(dll, 0x44c0c30f, 0x2e000);
    if (!parsed)
        return std::string("resource tables: ") + annowelt::error_message(parsed.error());
    const auto& image = parsed.value();
    // P: supported AnnoGame.dll PE identity; D: selector 0x10001b30 tables.
    if (image.machine != 0x14c || image.base != 0x10000000 || image.stamp != 0x44c0c30f ||
        image.size != 0x2e000)
        return std::string("resource tables: unsupported image identity");
    // Physical row count follows the bounded slot region 0x2114c..0x2122c.
    constexpr std::uint32_t slot_rva = 0x2114c, slot_end = 0x2122c;
    if (climate >= (slot_end - slot_rva) / (16U * 2U))
        return std::string("resource tables: climate outside supported image rows");
    const auto read_row =
        [&](std::uint32_t rva,
            std::uint32_t width) -> Result<std::vector<std::uint16_t>, std::string> {
        const auto at = image.offset(rva + climate * width * 2U, width * 2U);
        if (!at)
            return std::string("resource tables: ") + annowelt::error_message(at.error());
        std::vector<std::uint16_t> out;
        for (std::uint32_t i = 0; i < width; ++i)
            out.push_back(u16_le(dll, at.value() + 2U * i));
        return out;
    };
    auto slots = read_row(slot_rva, 16), a = read_row(0x2129c, 8), b = read_row(0x2130c, 8),
         c = read_row(0x2122c, 8);
    if (!slots)
        return slots.error();
    if (!a)
        return a.error();
    if (!b)
        return b.error();
    if (!c)
        return c.error();
    return ResourceTables{climate, std::move(slots.value()), std::move(a.value()),
                          std::move(b.value()), std::move(c.value())};
}
} // namespace anno::formats::annogame
