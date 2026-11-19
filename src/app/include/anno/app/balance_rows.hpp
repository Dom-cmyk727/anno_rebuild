// UI-TABS-1 stage B: the figures of the two balance pages, the player view's overview (its sub-tab
// 0, `SPIELER_BILANZ_BK`) and the "?" view's city status page (its sub-tab 0,
// `STADT_BILANZ_BK`), computed from the play snapshot the way the original's page updates compute
// them (GOG View.dll 0x1007f8f0 and 0x1006c0f0; docs/re/ui.md section 5.5). Where each row is
// drawn, in which font and colour, is hud.txt data (`value`, `page`; data/ui/README.md); this file
// only holds the arithmetic and the decimal text. A row whose source the snapshot does not hold
// has no value (never a zero in its place). No SDL, no drawing.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <anno/app/play.hpp>

namespace anno::app {

/// What the two pages read: the local player's cities (null or empty: the player has none), the
/// city the "?" page shows (null: none), whether the sim keeps the cities' accounts
/// (PlaySnapshot::accounts_live) and the shown scale of statistics 0, 4 and 5
/// (PlaySnapshot::stat_shown_num / stat_shown_den).
struct BalanceInputs {
    const std::vector<CityCounters>* cities = nullptr;
    const CityCounters* city = nullptr;
    bool accounts_live = false;
    std::uint32_t stat_num = 0, stat_den = 0;
};

/// The row values, keyed `player.<k>` (k = 0..6, the overview's rows top to bottom) and
/// `city.<k>` (k = 0..7, the city page's rows); a key that is absent has no source here and its
/// row stays empty. The rules (docs/re/ui.md 5.5, with the addresses):
/// - city.0 the shown city's population; city.1 statistic 0 (resident sales) shown; city.2 the
///   civil plus the military cost bucket; city.3 statistic 4 shown; city.4 statistic 5 shown;
///   city.5 the military cost bucket; city.6 = city.1 - city.2 - city.3 + city.4 - city.5 (the
///   military bucket counts twice, as the original computes it). city.1 .. city.6 only while the
///   accounts are live. city.7 (the city's knowledge, a research object the sim does not hold):
///   never.
/// - player.0 the sum of the cities' populations; player.1 the sum of the shown statistic 0;
///   player.2 the sum of the civil cost buckets; with no city both sums are 0, as the original's
///   loop over an empty list gives. player.1 and player.2 with a city only while the accounts
///   are live. player.3 .. player.6 add the player's own statistics and unit upkeep (player
///   getter 0x1001c0f0 indices 2 .. 5) the sim does not feed: never.
/// A value that does not fit an int64 (or a zero `stat_den`) leaves its row, and the balance
/// built from it, empty.
[[nodiscard]] std::map<std::string, std::int64_t> balance_row_values(const BalanceInputs& in);

/// `value` as the original prints it: `_itoa(value, buffer, 10)` (View.dll 0x1008d919: a minus
/// sign for a negative value, decimal digits, no thousands separator).
[[nodiscard]] std::string balance_text(std::int64_t value);

} // namespace anno::app
