// The goods table of the original (spec: docs/sim/state.md Appendix A).
//
// D: AnnoGame.dll goods name lookup 0x10013be0 maps a goods number to a name
// through the pointer tables at 0x100251d0, 0x100251e4, 0x100251f8, 0x10025234
// and 0x10025178; the numbers 0..0x8e are covered and an id without a name
// returns "UNUSED". The names below were read from those tables on 2026-10-19
// (the table is also listed in docs/re/production.md §7). The research
// requirements are BGruppen.dat [WAREN] Forsch (docs/open-questions.md OQ-034).
#include <array>
#include <string_view>

#include <anno/sim/types.hpp>

namespace anno::sim {

namespace {

/// Names by goods number, five to a row. Goods 67 is spelled with the Latin-1
/// byte 0xC4 (A umlaut) in AnnoGame.dll and in Anlagen.dat; the files are
/// matched byte for byte, so the escape is the exact spelling.
// clang-format off
constexpr std::array<std::string_view, 143> kGoodsNames = {
    "UNUSED",        "EISENERZ",         "GOLD",             "SALZSTEIN",    "EDELSTEINE",
    "STEINE",        "MAMORSTEIN",       "UNUSED",           "UNUSED",       "WOLLE",
    "ZUCKER",        "TABAK",            "FLEISCH",          "KORN",         "MEHL",
    "EISEN",         "FLACHS",           "HOPFEN",           "TIERHAEUTE",   "SEILE",
    "SEIDE",         "FARBSTOFFE",       "WALSPECK",         "HOLZKOHLE",    "UNUSED",
    "UNUSED",        "NAHRUNG",          "LEDER",            "SALZ",         "ALKOHOL",
    "TABAKWAREN",    "GEWUERZE",         "HEILKRAEUTER",     "STOFFE",       "SEIDESTOFFE",
    "LAMPENOEL",     "KLEIDUNG",         "SCHMUCK",          "PELZE",        "WEIN",
    "UNUSED",        "WERKZEUG",         "HOLZ",             "ZIEGEL",       "MAMOR",
    "UNUSED",        "SCHWERT",          "BOGEN",            "ARMBRUST",     "MUSKETE",
    "RUESTUNG",      "LANZE",            "AXT",              "KANONESHIP",   "UNUSED",
    "UNUSED",        "GETREIDE",         "TABAKBAUM",        "GEWUERZBAUM",  "FLACHSBAUM",
    "ZUCKERROHR",    "BAUMWOLLE",        "WEINTRAUBEN",      "KAKAOBAUM",    "HOPFENBAUM",
    "SEIDENRAUPE",   "FARBBAUM",         "KR\xC4UTER",       "KARTOFFEL",    "GRAS",
    "BAUM",          "FISCH",            "EISFISCH",         "UNUSED",       "UNUSED",
    "UNUSED",        "WIRTSHAUS",        "BADEHAUS",         "THEATER",      "KLINIK",
    "SCHULE",        "HOCHSCHULE",       "KAPELLE",          "KIRCHE",       "KATHETRALE",
    "PARK",          "BIBLIOTHEK",       "FEUERWEHR",        "GERICHT",      "GALGEN",
    "UNUSED",        "UNUSED",           "UNUSED",           "UNUSED",       "UNUSED",
    "UNUSED",        "SPIESSTRAEGER",    "SCHWERTKAEMPFER",  "BOGENSCHUETZE", "LANZENKNECHT",
    "KAVALERIE",     "ARMBRUSTSCHUETZE", "MUSKETENSCHUETZE", "MUSKETIER",    "PIONIER",
    "BEDIENUNG",     "MEDICUS",          "TRAGTIER",         "KATAPULT",     "KATAPULT_GRP",
    "KANONE",        "KANONE_GRP",       "MOERSER",          "MOERSER_GRP",  "BELAGERTURM",
    "BELAGERTURM_GRP", "STATIST",        "WELTBEHERRSCHER",  "UNUSED",       "UNUSED",
    "UNUSED",        "UNUSED",           "HANDELSCHIFF1",    "HANDELSCHIFF2", "HANDELSCHIFF3",
    "KRIEGSCHIFF1",  "KRIEGSCHIFF2",     "KRIEGSCHIFF3",     "TRADERSCHIFF", "PIRATSCHIFF",
    "UNUSED",        "UNUSED",           "SCHATZ",           "UNUSED",       "UNUSED",
    "UNUSED",        "UNUSED",           "FLEISCHTIER",      "PELZTIER",     "WALFISCH",
    "UNUSED",        "UNUSED",           "ALL"};
// clang-format on

} // namespace

std::vector<GoodsInfo> original_goods() {
    std::vector<GoodsInfo> out;
    out.reserve(kGoodsNames.size());
    for (const std::string_view name : kGoodsNames) {
        out.push_back(GoodsInfo{std::string(name), 0});
    }
    // Goods with a research requirement (BGruppen [WAREN] Forsch, Appendix A):
    // SCHWERT 1, BOGEN 4, ARMBRUST 9, MUSKETE 12, RUESTUNG 1, LANZE 3, AXT 6,
    // KANONESHIP 38.
    out[46].forsch = 1;
    out[47].forsch = 4;
    out[48].forsch = 9;
    out[49].forsch = 12;
    out[50].forsch = 1;
    out[51].forsch = 3;
    out[52].forsch = 6;
    out[53].forsch = 38;
    return out;
}

} // namespace anno::sim
