// anno-dump: inspect original ANNO 1503 files.
//
//   anno-dump chunks <file>...   chunk tree (same output as `tools/chunks.py tree`)
//
// Each file's output starts with `# <path>` (the path as given). Output uses
// `\n` line ends on every platform, as the Python references.
#include <cstdio>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <anno/formats/chunk.hpp>

namespace {

namespace chunk = anno::formats::chunk;
using anno::formats::Bytes;

constexpr std::string_view kUsage = "usage: anno-dump chunks <file>...\n";

std::optional<std::vector<std::uint8_t>> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), {});
}

void put(const std::string& s) {
    std::fwrite(s.data(), 1, s.size(), stdout);
}

/// Runs `dump` on every file; returns the process exit code.
int dump_files(const std::vector<std::string>& files,
               const std::function<bool(Bytes, std::string&)>& dump) {
    int status = 0;
    for (const auto& path : files) {
        auto buf = read_file(path);
        if (!buf) {
            std::fprintf(stderr, "%s: cannot read\n", path.c_str());
            status = 1;
            continue;
        }
        std::string out = "# " + path + "\n";
        if (!dump(*buf, out)) {
            status = 1;
        }
        put(out);
    }
    return status;
}

/// `chunks`: as `tools/chunks.py tree`, which prints `NOPARSE` for a file
/// that is not a chunk file and still exits 0.
bool dump_chunks(Bytes buf, std::string& out) {
    auto f = chunk::parse_file(buf);
    if (!f) {
        out += "NOPARSE\n";
        return true;
    }
    out += chunk::tree(f.value());
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.size() < 2) {
        std::fputs(kUsage.data(), stderr);
        return 2;
    }
    const std::string cmd = args[0];
    const std::vector<std::string> files(args.begin() + 1, args.end());
    if (cmd == "chunks") {
        return dump_files(files, dump_chunks);
    }
    std::fputs(kUsage.data(), stderr);
    return 2;
}
