// `walk-tables` (docs/formats/annowelt-walk-tables.md, "Reference dump";
// reference: tools/annowelt_tables.py dump).
#include <cstdio>
#include <string>

#include <anno/formats/annowelt.hpp>

#include "dump.hpp"

namespace anno::dump {

namespace wt = formats::annowelt;

bool dump_walk_tables(Bytes buf, bool, std::string& out) {
    auto t = wt::read_walk_tables(buf);
    if (!t) {
        std::fprintf(stderr, "NOPARSE: %s\n", wt::error_message(t.error()));
        out += "NOPARSE\n";
        return false;
    }
    const wt::WalkTables& tables = t.value();
    char ident[80];
    std::snprintf(ident, sizeof ident, "TimeDateStamp=0x%08x SizeOfImage=0x%08x\n",
                  tables.time_date_stamp, tables.size_of_image);
    out += ident;
    for (std::size_t L = 0; L < tables.layers.size(); ++L) {
        const std::vector<std::uint8_t>& layer = tables.layers[L];
        std::size_t admitted = 0;
        const std::size_t n =
            layer.size() < wt::kAdmissionCount ? layer.size() : wt::kAdmissionCount;
        for (std::size_t c = 0; c < n; ++c) {
            if (layer[c] != 0) {
                ++admitted;
            }
        }
        out += "layer " + std::to_string(L) + ": admitted=" + std::to_string(admitted);
        for (unsigned code : {0u, 1u, 2u, 4u}) {
            out += " code" + std::to_string(code) + "=";
            if (code < layer.size()) {
                out += std::to_string(layer[code]);
            }
            const std::size_t base = wt::kAdmissionCount + wt::kCostStride * code;
            for (std::size_t j = 0; j < wt::kCostStride; ++j) {
                out += ",";
                if (base + j < layer.size()) {
                    out += std::to_string(layer[base + j]);
                }
            }
        }
        out += "\n";
    }
    return true;
}

} // namespace anno::dump
