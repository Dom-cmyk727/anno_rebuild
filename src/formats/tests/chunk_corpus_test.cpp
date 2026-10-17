// Chunk-container corpus test over the original files (needs
// ANNO1503_GAME_DIR; skips otherwise). Figures: docs/formats/chunk-container.md.
#include <map>
#include <string>

#include <doctest/doctest.h>

#include <anno/formats/chunk.hpp>
#include <anno/test/game_dir.hpp>

namespace chunk = anno::formats::chunk;

namespace {

struct Stats {
    std::size_t files = 0, trailing = 0, chunks = 0, dirty_names = 0;
};

void visit(const std::vector<chunk::Chunk>& chunks, Stats& s) {
    for (const auto& c : chunks) {
        ++s.chunks;
        const std::size_t nul = c.name().size();
        REQUIRE(nul < chunk::kNameLen);
        for (std::size_t i = nul + 1; i < chunk::kNameLen; ++i) {
            if (c.name_field[i] != 0) {
                ++s.dirty_names;
                break;
            }
        }
        if (c.is_container) {
            visit(c.children, s);
        }
    }
}

void check_file(const std::filesystem::path& path, Stats& s) {
    const auto buf = anno::test::read_file(path);
    auto f = chunk::parse_file(buf);
    INFO(path.string());
    REQUIRE(f);
    REQUIRE_FALSE(f.value().chunks.empty());
    anno::formats::ByteBuffer out;
    REQUIRE(chunk::write_file(f.value(), out));
    CHECK(out == buf);
    ++s.files;
    if (!f.value().trailing.empty()) {
        ++s.trailing;
    }
    visit(f.value().chunks, s);
}

} // namespace

TEST_CASE("chunk corpus") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping chunk corpus: ANNO1503_GAME_DIR not set");
        return;
    }
    // (extension, files, files with bytes after the root chunk)
    const std::map<std::string, std::pair<std::size_t, std::size_t>> expected = {
        {"sze", {50, 0}},
        {"tex", {1214, 0}},
        {"inc", {1213, 0}},
        {"zei", {11, 0}},
        {"scp", {465, 444}}};
    Stats total;
    for (const auto& [ext, want] : expected) {
        Stats s;
        for (const auto& path : anno::test::files_with_ext(*dir, ext)) {
            check_file(path, s);
        }
        INFO("." << ext);
        CHECK(s.files == want.first);
        CHECK(s.trailing == want.second);
        total.files += s.files;
        total.chunks += s.chunks;
        total.dirty_names += s.dirty_names;
    }
    Stats anlagen;
    check_file(*dir / "Anlagen.dat", anlagen);
    CHECK(anlagen.trailing == 0);
    CHECK(total.files + anlagen.files == 2954);
    CHECK(total.chunks + anlagen.chunks == 17529);
    CHECK(total.dirty_names + anlagen.dirty_names == 22);
}
