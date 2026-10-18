// game_dir_problem: the reasons a folder is not the game folder.
#include <filesystem>
#include <fstream>
#include <string>

#include <doctest/doctest.h>

#include <anno/test/game_dir.hpp>

namespace fs = std::filesystem;

TEST_CASE("game_dir_problem names why a folder is not the game folder") {
    const fs::path root = fs::temp_directory_path() / "anno_game_dir_test";
    fs::remove_all(root);
    fs::create_directories(root / "inner");

    CHECK(anno::test::game_dir_problem(root / "missing") == "the folder does not exist");
    CHECK(anno::test::game_dir_problem(root) == "no Anlagen.dat: not the game folder");

    std::ofstream(root / "file.txt") << "x";
    CHECK(anno::test::game_dir_problem(root / "file.txt") == "not a folder");

    std::ofstream(root / "inner" / "Anlagen.dat") << "x";
    CHECK(anno::test::game_dir_problem(root) ==
          "no Anlagen.dat here, but in " + (root / "inner").string());
    CHECK(anno::test::game_dir_problem(root / "inner").empty());

    fs::remove_all(root);
}
