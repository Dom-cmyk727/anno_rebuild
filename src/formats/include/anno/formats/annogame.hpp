// Original GOG AnnoGame resource-selector table layout, not an executable loader.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/result.hpp>
namespace anno::formats::annogame {
struct ResourceTables {
    std::uint32_t climate = 0;
    std::vector<std::uint16_t> slots, tag104, tag201, tag404;
};
// Only the addressed GOG image/layout is supported. Its seven physical rows are
// an image-layout fact, never an engine climate limit. Original arrays stay runtime data.
Result<ResourceTables, std::string> read_resource_tables(Bytes dll, std::uint32_t climate);
struct AnbauNames {
    std::uint32_t first_goods = 0;
    std::vector<std::string> names;
};
// GOG ANBAU's addressed name window, including the getter's null-pointer fallback.
// Layout/rule bounds are not an engine crop/population capacity (wildlife-anbau.md).
Result<AnbauNames, std::string> read_anbau_names(Bytes dll);
} // namespace anno::formats::annogame
