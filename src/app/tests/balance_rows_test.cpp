// UI-TABS-1 stage B: the balance pages' figures (balance_rows.hpp; docs/re/ui.md section 5.5) and
// their hud.txt rows. Expected values are hand-derived from the GOG View.dll / AnnoWelt.dll reads
// cited at each case; no game files needed.
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/balance_rows.hpp>
#include <anno/app/ui_layout.hpp>

namespace fs = std::filesystem;
namespace app = anno::app;

namespace {

/// A city of `population` with the five account values (sim::CityAccounts order: sales = stat
/// 0, trade_sales = stat 4, trade_purchases = stat 5, civil = stat 1, military = stat 2).
app::CityCounters city(std::optional<std::int64_t> population, std::uint64_t sales,
                       std::uint64_t stat4, std::uint64_t stat5, std::uint64_t civil,
                       std::uint64_t military) {
    app::CityCounters c;
    c.population = population;
    c.accounts.sales = sales;
    c.accounts.trade_sales = stat4;
    c.accounts.trade_purchases = stat5;
    c.accounts.civil_cost = civil;
    c.accounts.military_cost = military;
    return c;
}

/// The original's scale (AnnoWelt 0x1002dc70: 15 x / 105), live accounts.
app::BalanceInputs live(const std::vector<app::CityCounters>* cities, const app::CityCounters* c) {
    return app::BalanceInputs{cities, c, true, 15, 105};
}

std::string row(const std::map<std::string, std::int64_t>& rows, const std::string& key) {
    const auto it = rows.find(key);
    return it == rows.end() ? std::string("empty") : app::balance_text(it->second);
}

app::HudLayout real_hud() {
    const auto loaded = app::load_hud_layout(fs::path(ANNO_REPO_DIR) / "data" / "ui" / "hud.txt");
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    return loaded.value();
}

std::string synthetic(const std::string& element) {
    return "anno1503-ui-hud 1\nresolution 1024 768\n" + element + "\n";
}

} // namespace

TEST_CASE("balance rows: the text is the original's itoa, radix 10, no separator") {
    // View.dll 0x1008d919 -> 0x1008d946: '-' then the digits of the negated value.
    CHECK(app::balance_text(0) == "0");
    CHECK(app::balance_text(15) == "15");
    CHECK(app::balance_text(-31) == "-31");
    CHECK(app::balance_text(1234567) == "1234567");
    CHECK(app::balance_text(-1000000) == "-1000000");
    CHECK(app::balance_text(std::numeric_limits<std::int64_t>::max()) == "9223372036854775807");
    CHECK(app::balance_text(std::numeric_limits<std::int64_t>::min()) == "-9223372036854775808");
}

TEST_CASE("balance rows: the city page reproduces the History Edition's figures at ui_5 6:45") {
    // HE ui_5.mp4 405 s (Bad Aussee): 120, 91, 248, 0, 0, 0, -157, 9. Statistic 0 = 637 shows
    // 15 * 637 / 105 = 91; civil 248, military 0; balance 91 - 248 = -157. Row 7 (knowledge, a
    // research object the sim does not hold) stays empty.
    const app::CityCounters c = city(120, 637, 0, 0, 248, 0);
    const auto rows = app::balance_row_values(live(nullptr, &c));
    CHECK(row(rows, "city.0") == "120");
    CHECK(row(rows, "city.1") == "91");
    CHECK(row(rows, "city.2") == "248");
    CHECK(row(rows, "city.3") == "0");
    CHECK(row(rows, "city.4") == "0");
    CHECK(row(rows, "city.5") == "0");
    CHECK(row(rows, "city.6") == "-157");
    CHECK(row(rows, "city.7") == "empty");
}

TEST_CASE(
    "balance rows: the city balance subtracts the military bucket twice, as 0x1006c2ef does") {
    // Row 2 = +0x6c(1) + +0x6c(2) = 10 + 5, row 5 = +0x6c(2) = 5, row 6 = row4 - row5 - row3 -
    // row2 + row1 = 0 - 5 - 0 - 15 + 0 = -20. Counting the military bucket once would give -15.
    const app::CityCounters c = city(0, 0, 0, 0, 10, 5);
    const auto rows = app::balance_row_values(live(nullptr, &c));
    CHECK(row(rows, "city.2") == "15");
    CHECK(row(rows, "city.5") == "5");
    CHECK(row(rows, "city.6") == "-20");
}

TEST_CASE("balance rows: row 3 is statistic 4 (a cost), row 4 statistic 5 (a revenue)") {
    // 0x1006c234 prints +0x6c(4) on row 3, 0x1006c275 +0x6c(5) on row 4; the balance adds row 4
    // and subtracts row 3. Statistic 4 = 105 shows 15, statistic 5 = 210 shows 30: balance +15.
    // Mapping by the sim's field names instead (trade_purchases on row 3) would give 30, 15, -15.
    const app::CityCounters c = city(0, 0, 105, 210, 0, 0);
    const auto rows = app::balance_row_values(live(nullptr, &c));
    CHECK(row(rows, "city.3") == "15");
    CHECK(row(rows, "city.4") == "30");
    CHECK(row(rows, "city.6") == "15");
}

TEST_CASE("balance rows: a statistic shows 15 x / 105, truncated") {
    // AnnoWelt 0x1002dc8a..0x1002dc9f: 15 x, then the unsigned divide by 105 (magic 0x38138139).
    for (const auto& [x, shown] : std::vector<std::pair<std::uint64_t, const char*>>{
             {0, "0"}, {6, "0"}, {7, "1"}, {13, "1"}, {14, "2"}, {700000, "100000"}}) {
        CAPTURE(x);
        const app::CityCounters c = city(0, x, 0, 0, 0, 0);
        CHECK(row(app::balance_row_values(live(nullptr, &c)), "city.1") == shown);
    }
}

TEST_CASE("balance rows: without live accounts only the populations and the empty sums show") {
    // Rules::city_booking off: the cost buckets and statistics are not kept, so rows 1..6 of the
    // city page and rows 1, 2 of the overview stay empty; the populations stay.
    const std::vector<app::CityCounters> cities = {city(40, 700, 0, 0, 30, 0)};
    app::BalanceInputs in = live(&cities, &cities.front());
    in.accounts_live = false;
    const auto rows = app::balance_row_values(in);
    CHECK(row(rows, "city.0") == "40");
    for (int k = 1; k <= 7; ++k)
        CHECK(row(rows, "city." + std::to_string(k)) == "empty");
    CHECK(row(rows, "player.0") == "40");
    CHECK(row(rows, "player.1") == "empty");
    CHECK(row(rows, "player.2") == "empty");

    // No city at all: the original's loop over an empty list leaves 0 in rows 0..2, whatever the
    // rules (HE HE_UI_1.mp4 44.5 s: 0, 0, 0 on the ship start).
    const auto none = app::balance_row_values(app::BalanceInputs{nullptr, nullptr, false, 15, 105});
    CHECK(row(none, "player.0") == "0");
    CHECK(row(none, "player.1") == "0");
    CHECK(row(none, "player.2") == "0");
    CHECK(row(none, "city.0") == "empty");
}

TEST_CASE("balance rows: the overview sums the cities and leaves the player's own terms empty") {
    // 0x1007f977..0x1007f9ee: row 0 sums the populations, row 1 the shown statistic 0, row 2 the
    // civil buckets. Rows 3..6 add the player's getter 0x1001c0f0 (indices 2..5), which the sim
    // does not feed: empty.
    const std::vector<app::CityCounters> cities = {city(10, 105, 0, 0, 3, 50),
                                                   city(20, 210, 0, 0, 4, 60)};
    const auto rows = app::balance_row_values(live(&cities, nullptr));
    CHECK(row(rows, "player.0") == "30");
    CHECK(row(rows, "player.1") == "45");
    CHECK(row(rows, "player.2") == "7");
    for (int k = 3; k <= 6; ++k)
        CHECK(row(rows, "player." + std::to_string(k)) == "empty");
    CHECK(row(rows, "city.0") == "empty"); // no city shown
}

TEST_CASE(
    "balance rows: a source that does not fit leaves its rows empty, never a wrapped figure") {
    const std::uint64_t huge = std::numeric_limits<std::uint64_t>::max();
    const app::CityCounters c = city(5, 105, 0, 0, huge, 0);
    const auto rows = app::balance_row_values(live(nullptr, &c));
    CHECK(row(rows, "city.1") == "15");
    CHECK(row(rows, "city.2") == "empty");
    CHECK(row(rows, "city.6") == "empty");
    // A zero scale denominator: the shown statistics and the balance are empty.
    app::BalanceInputs zero = live(nullptr, &c);
    zero.stat_den = 0;
    const auto z = app::balance_row_values(zero);
    CHECK(row(z, "city.1") == "empty");
    CHECK(row(z, "city.3") == "empty");
    CHECK(row(z, "city.6") == "empty");
    // A city whose population is unknown: the overview's sum is empty.
    const std::vector<app::CityCounters> cities = {city(std::nullopt, 0, 0, 0, 0, 0)};
    CHECK(row(app::balance_row_values(live(&cities, nullptr)), "player.0") == "empty");
}

TEST_CASE("balance rows: hud.txt holds the rows at the layout functions' positions") {
    // City layout 0x1006c4c0 / player 0x1007fcf0: x = 779 + 35, y = 244 + C - 244 with C =
    // 323, 353, 383, 413, 443, 473, 533, 633; gadgets 195 x 15; the balance row (6) turns red.
    const app::HudLayout hud = real_hud();
    const int ys[] = {323, 353, 383, 413, 443, 473, 533, 633};
    for (const auto& [prefix, page, count] : std::vector<std::tuple<std::string, std::string, int>>{
             {"player", "player", 7}, {"city", "info", 8}}) {
        int seen = 0;
        for (const app::UiElement& el : hud.elements) {
            if (el.value.rfind(prefix + ".", 0) != 0)
                continue;
            CAPTURE(el.name);
            const int k = std::stoi(el.value.substr(prefix.size() + 1));
            REQUIRE(k >= 0);
            REQUIRE(k < count);
            ++seen;
            CHECK(el.page == page);
            CHECK(el.pos_x == 814);
            CHECK(el.pos_y == ys[k]);
            CHECK(el.size_w == 195u);
            CHECK(el.size_h == 15u);
            CHECK(el.align == "right");
            CHECK(el.text_dy == -3);
            CHECK(el.clip_box);
            CHECK(el.anchor == app::UiAnchor::Right);
            CHECK(el.negative_rgb.has_value() == (k == 6));
            if (el.negative_rgb)
                CHECK(*el.negative_rgb == 0xff0000u);
        }
        CHECK(seen == count);
    }
}

TEST_CASE("balance rows: the row keys refuse malformed values") {
    CHECK(app::parse_hud_layout(synthetic("element r pos 1 1 size 2 2 align right text_dy -3 "
                                          "clip box value player.0 page player negative_rgb "
                                          "ff0000"))
              .ok());
    CHECK_FALSE(app::parse_hud_layout(synthetic("element r align left")).ok());
    CHECK_FALSE(app::parse_hud_layout(synthetic("element r clip text")).ok());
    CHECK_FALSE(app::parse_hud_layout(synthetic("element r text_dy up")).ok());
    CHECK_FALSE(app::parse_hud_layout(synthetic("element r negative_rgb red")).ok());
    CHECK_FALSE(app::parse_hud_layout(synthetic("element r negative_rgb ff00001")).ok());
    CHECK_FALSE(app::parse_hud_layout(synthetic("element r value")).ok());
}
