#include <future>
#include <latch>

#include <doctest/doctest.h>

#include "golden.hpp"

TEST_CASE("pins writer validates, records atomically, sorts and protects oracles") {
    const auto dir = std::filesystem::temp_directory_path() /
                     ("anno-pins-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    REQUIRE(std::filesystem::create_directory(dir));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::filesystem::remove_all(path); }
    } cleanup{dir};
    const auto path = dir / "pins.txt";
    {
        std::ofstream out(path);
        out << "# self-authored contract\nz PIN " << anno::sim::kStateHashVersion
            << " 0000000000000001\n";
    }
    std::string line;
    const auto check = [&](const std::string& name, std::uint64_t value, bool record,
                           bool ci = false) {
        return anno::test::check_golden_at(path, name, "PIN", value, line, record, ci);
    };
    CHECK(check("z", 1, false).empty());
    CHECK_FALSE(check("z", 2, false).empty());
    CHECK_FALSE(check("z", 2, true, true).empty());
    CHECK(anno::test::read_goldens(path)[0].value == 1);
    CHECK(check("z", 2, true).empty());
    CHECK(check("a", 3, true).empty());
    const auto pins = anno::test::read_goldens(path);
    REQUIRE(pins.size() == 2);
    CHECK(pins[0].name == "a");
    CHECK(pins[1].value == 2);
    CHECK_FALSE(std::filesystem::exists(path.string() + ".tmp"));
    CHECK_FALSE(std::filesystem::exists(path.string() + ".lock"));
    CHECK_FALSE(check("oracle_private", 4, true).empty());
    CHECK(anno::test::read_goldens(path).size() == 2);
    {
        std::ofstream out(path);
        out << "bad PIN 138 nothex\n";
    }
    CHECK_FALSE(check("z", 2, true).empty());
}
TEST_CASE("pins concurrent writers retain both updates") {
    const auto path = std::filesystem::temp_directory_path() /
                      ("anno-pins-parallel-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    {
        std::ofstream out(path);
        out << "# self-authored contract\n";
    }
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::filesystem::remove(path); }
    } cleanup{path};
    std::latch ready(2), start(1);
    std::future<std::string> a, b;
    // Destroy this guard before either future: task creation failure must release
    // an already started peer before its future joins it.
    struct Release {
        std::latch& start;
        bool pending = true;
        void open() noexcept {
            if (pending) {
                pending = false;
                start.count_down();
            }
        }
        ~Release() { open(); }
    } release{start};
    const auto write = [&](std::string name, std::uint64_t value) {
        ready.count_down();
        start.wait();
        std::string line;
        return anno::test::check_golden_at(path, name, "PIN", value, line, true, false);
    };
    a = std::async(std::launch::async, write, "a", 1);
    b = std::async(std::launch::async, write, "b", 2);
    ready.wait();
    release.open();
    CHECK(a.get().empty());
    CHECK(b.get().empty());
    const auto pins = anno::test::read_goldens(path);
    REQUIRE(pins.size() == 2);
    CHECK(pins[0].name == "a");
    CHECK(pins[0].value == 1);
    CHECK(pins[1].name == "b");
    CHECK(pins[1].value == 2);
}
