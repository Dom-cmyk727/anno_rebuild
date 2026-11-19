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
Result<AnbauNames, std::string> read_anbau_names(Bytes dll) {
    const auto parsed = pe_tables_detail::read(dll, 0x44c0c30f, 0x2e000);
    if (!parsed)
        return std::string("anbau names: ") + annowelt::error_message(parsed.error());
    const auto& image = parsed.value();
    if (image.machine != 0x14c || image.base != 0x10000000)
        return std::string("anbau names: unsupported image identity");
    // Welt 0x100898dc..0x1008992f calls goods-name indices 56..68.
    // Game 0x10013c5c reads table+index*4, null -> 0x100261e8 at 0x10013ce2.
    constexpr std::uint32_t first_goods = 56;
    constexpr std::uint32_t first_rva = 0x251f8 + first_goods * 4;
    constexpr std::uint32_t end_rva = 0x251f8 + 69 * 4;
    AnbauNames out{first_goods, {}};
    for (auto rva = first_rva; rva < end_rva; rva += 4) {
        const auto at = image.offset(rva, 4);
        if (!at)
            return std::string("anbau names: pointer outside image data");
        auto pointer = u32_le(dll, at.value());
        if (pointer == 0)
            pointer = 0x100261e8;
        if (pointer < image.base || pointer - image.base >= image.size)
            return std::string("anbau names: invalid string pointer");
        std::string name;
        auto cursor = pointer - image.base;
        bool terminated = false;
        for (; cursor < image.size; ++cursor) {
            const auto pos = image.offset(cursor, 1);
            if (!pos)
                return std::string("anbau names: string outside image data");
            const auto byte = dll[pos.value()];
            if (byte == 0) {
                terminated = true;
                break;
            }
            name.push_back(static_cast<char>(byte));
        }
        if (!terminated)
            return std::string("anbau names: unterminated string");
        out.names.push_back(std::move(name));
    }
    return out;
}
} // namespace anno::formats::annogame
