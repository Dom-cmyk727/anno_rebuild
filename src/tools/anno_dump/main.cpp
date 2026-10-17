// anno-dump: inspect original ANNO 1503 files. The text formats are
// specified in docs/formats/*.md ("Reference dump") and must equal the
// output of the Python references in tools/ (checked by tools/agree.py).
//
//   anno-dump chunks <file>...                 chunk tree (tools/chunks.py tree)
//   anno-dump tex [--full] <file>...           palettes and textures (tools/tex.py dump)
//   anno-dump inc <file>...                    ACCESS records (tools/tex.py inc)
//   anno-dump colors [--full] <Anlagen.dat>... COLORS tables (tools/tex.py colors)
//
// Each file's output starts with `# <path>` (the path as given); lines end
// with `\n` on every platform.
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <anno/formats/chunk.hpp>

#include "dump.hpp"

namespace {

using anno::formats::Bytes;

constexpr const char* kUsage = "usage: anno-dump chunks <file>...\n"
                               "       anno-dump tex [--full] <file>...\n"
                               "       anno-dump inc <file>...\n"
                               "       anno-dump colors [--full] <Anlagen.dat>...\n";

using DumpFn = bool (*)(Bytes, bool, std::string&);

struct Command {
    DumpFn fn;
    bool takes_full;
};

bool chunks(Bytes b, bool, std::string& o) {
    return anno::dump::dump_chunks(b, o);
}
bool inc(Bytes b, bool, std::string& o) {
    return anno::dump::dump_inc(b, o);
}

const std::map<std::string, Command>& commands() {
    static const std::map<std::string, Command> table = {
        {"chunks", {chunks, false}},
        {"tex", {anno::dump::dump_tex, true}},
        {"inc", {inc, false}},
        {"colors", {anno::dump::dump_colors, true}},
    };
    return table;
}

std::optional<std::vector<std::uint8_t>> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), {});
}

} // namespace

namespace anno::dump {

/// `chunks`: as `tools/chunks.py tree`, which prints `NOPARSE` for a file
/// that is not a chunk file and still exits 0.
bool dump_chunks(Bytes buf, std::string& out) {
    auto f = formats::chunk::parse_file(buf);
    out += f ? formats::chunk::tree(f.value()) : std::string("NOPARSE\n");
    return true;
}

} // namespace anno::dump

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty() || commands().count(args[0]) == 0) {
        std::fputs(kUsage, stderr);
        return 2;
    }
    const Command cmd = commands().at(args[0]);
    std::size_t first = 1;
    const bool full = cmd.takes_full && args.size() > 1 && args[1] == "--full";
    if (full) {
        first = 2;
    }
    if (args.size() <= first) {
        std::fputs(kUsage, stderr);
        return 2;
    }
    int status = 0;
    for (std::size_t i = first; i < args.size(); ++i) {
        const std::string& path = args[i];
        auto buf = read_file(path);
        if (!buf) {
            std::fprintf(stderr, "%s: cannot read\n", path.c_str());
            status = 1;
            continue;
        }
        std::string out = "# " + path + "\n";
        if (!cmd.fn(*buf, full, out)) {
            status = 1;
        }
        std::fwrite(out.data(), 1, out.size(), stdout);
    }
    return status;
}
