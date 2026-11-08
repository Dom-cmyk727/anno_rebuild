// UI.2 C1/C2/C4/C5: the construction menu's click resolution, against the project's own real
// data/ui/hud.txt and data/ui/construction.txt (no game files needed - these are the project's
// own checked-in derived-fact files, like ui_layout_test.cpp).
#include <filesystem>

#include <doctest/doctest.h>

#include <anno/app/construction_menu.hpp>

namespace fs = std::filesystem;
namespace app = anno::app;

namespace {
fs::path data_ui_dir() {
    return fs::path(ANNO_REPO_DIR) / "data" / "ui";
}

struct Layouts {
    app::HudLayout hud;
    app::ConstructionLayout construction;
};

Layouts load() {
    const auto hud = app::load_hud_layout(data_ui_dir() / "hud.txt");
    const auto con = app::load_construction_layout(data_ui_dir() / "construction.txt");
    REQUIRE_MESSAGE(hud.ok(), (hud.ok() ? std::string() : hud.error()));
    REQUIRE_MESSAGE(con.ok(), (con.ok() ? std::string() : con.error()));
    return {hud.value(), con.value()};
}
} // namespace

TEST_CASE("construction menu: category_at finds each of the eight column bands, and nothing "
          "outside them") {
    const Layouts l = load();
    const app::ConstructionMenu menu(l.hud, l.construction);

    // Every category's own y (docs/re/ui.md section 3.1's table) is inside its own band.
    for (const auto& cat : l.construction.categories) {
        CAPTURE(cat.name);
        const auto hit = menu.category_at(790.0, static_cast<double>(cat.y));
        REQUIRE(hit.has_value());
        CHECK(*hit == cat.name);
    }
    // public's own y (325) centre hits public specifically.
    CHECK(menu.category_at(790.0, 325.0) == std::optional<std::string>("public"));
    // Left of the column (x < 779): no hit, even at a valid y.
    CHECK_FALSE(menu.category_at(700.0, 325.0).has_value());
    // Right of the 42 px gadget width (x >= 779 + 42): no hit.
    CHECK_FALSE(menu.category_at(825.0, 325.0).has_value());
    // Above the first band (public's band starts at 325 - 23 = 302): no hit.
    CHECK_FALSE(menu.category_at(790.0, 100.0).has_value());
}

TEST_CASE("construction menu: cell_at finds the grid's sprites, not the gutters, not outside "
          "the 4 x 8 grid") {
    const Layouts l = load();
    const app::ConstructionMenu menu(l.hud, l.construction);
    const auto& g = l.construction.grid;
    REQUIRE(g.origin_x == 808);
    REQUIRE(g.origin_y == 305);
    REQUIRE(g.cell_px == 52);
    REQUIRE(g.sprite == 50);

    // The centre of cell (0, 0) hits (0, 0).
    CHECK(menu.cell_at(808.0 + 25.0, 305.0 + 25.0) == std::make_pair(0, 0));
    // The centre of cell (3, 7) (the grid's far corner, 4 cols x 8 rows) hits (3, 7).
    CHECK(menu.cell_at(808.0 + 3 * 52.0 + 25.0, 305.0 + 7 * 52.0 + 25.0) == std::make_pair(3, 7));
    // Just inside the top-left corner of cell (1, 0)'s sprite.
    CHECK(menu.cell_at(808.0 + 52.0, 305.0) == std::make_pair(1, 0));
    // The 2 px gutter between cell (0, 0) and (1, 0) (stride 52, sprite 50: x in [50, 52)): no
    // cell.
    CHECK_FALSE(menu.cell_at(808.0 + 50.5, 305.0 + 25.0).has_value());
    // Before the grid's origin: no cell.
    CHECK_FALSE(menu.cell_at(800.0, 305.0).has_value());
    // Past the 4th column / 8th row: no cell.
    CHECK_FALSE(menu.cell_at(808.0 + 4 * 52.0 + 25.0, 305.0 + 25.0).has_value());
    CHECK_FALSE(menu.cell_at(808.0 + 25.0, 305.0 + 8 * 52.0 + 25.0).has_value());
}

TEST_CASE("construction menu: click resolves a category tab, then, with a category open, an "
          "empty/locked/armable cell, the pickaxe and cancel-X - never guessing eligibility open") {
    const Layouts l = load();
    const app::ConstructionMenu menu(l.hud, l.construction);

    // The menu is closed (open_category ""): a category-tab click still switches; a grid/pickaxe/
    // cancel-X click does nothing (those rectangles are not shown with no category open).
    {
        const auto r = menu.click(790.0, 325.0, "", false, nullptr);
        CHECK(r.kind == app::ConstructionClickKind::SwitchCategory);
        CHECK(r.category == "public");
    }
    {
        const auto r = menu.click(808.0 + 25.0, 305.0 + 25.0, "", false, nullptr);
        CHECK(r.kind == app::ConstructionClickKind::None);
    }

    // public (1, 0) is id 580 (locked at the Citizen start per construction.txt - but this test
    // never reads that field; it is reached only through the `eligibility` argument, exactly as
    // the sim's live construction_eligibility would be published).
    const auto* locked_cell = l.construction.cell("public", 1, 0);
    REQUIRE(locked_cell != nullptr);
    const auto [lx, ly] = *l.construction.grid.click_centre(1, 0);

    // No eligibility published at all: Locked, never guessed open.
    {
        const auto r =
            menu.click(static_cast<double>(lx), static_cast<double>(ly), "public", false, nullptr);
        CHECK(r.kind == app::ConstructionClickKind::Locked);
        CHECK(r.type.v == locked_cell->id);
    }
    // Published, but this id is absent from the list (a malformed/partial eligibility): Locked.
    {
        const std::vector<std::pair<anno::sim::TypeId, bool>> partial{};
        const auto r =
            menu.click(static_cast<double>(lx), static_cast<double>(ly), "public", false, &partial);
        CHECK(r.kind == app::ConstructionClickKind::Locked);
    }
    // Published and false: Locked.
    {
        const std::vector<std::pair<anno::sim::TypeId, bool>> elig{
            {anno::sim::TypeId{locked_cell->id}, false}};
        const auto r =
            menu.click(static_cast<double>(lx), static_cast<double>(ly), "public", false, &elig);
        CHECK(r.kind == app::ConstructionClickKind::Locked);
    }
    // Published and true: Arm, with the cell's own type id.
    {
        const std::vector<std::pair<anno::sim::TypeId, bool>> elig{
            {anno::sim::TypeId{locked_cell->id}, true}};
        const auto r =
            menu.click(static_cast<double>(lx), static_cast<double>(ly), "public", false, &elig);
        CHECK(r.kind == app::ConstructionClickKind::Arm);
        CHECK(r.type.v == locked_cell->id);
        CHECK(r.category == "public");
    }
    // An empty slot (public row 7, cols 1-3 are "-" per docs/re/ui.md section 3.3): Empty,
    // regardless of eligibility.
    {
        REQUIRE(l.construction.cell("public", 1, 7) == nullptr);
        const auto [ex, ey] = *l.construction.grid.click_centre(1, 7);
        const auto r =
            menu.click(static_cast<double>(ex), static_cast<double>(ey), "public", false, nullptr);
        CHECK(r.kind == app::ConstructionClickKind::Empty);
    }

    // The pickaxe (data/ui/hud.txt "abrissicon", (970, 710) 53 x 53), on the plain category grid
    // (placement_open false).
    {
        const auto r = menu.click(970.0 + 10.0, 710.0 + 10.0, "public", false, nullptr);
        CHECK(r.kind == app::ConstructionClickKind::ToggleDemolish);
    }
    // Cancel-X (data/ui/hud.txt "infoexit", (984, 725) 33 x 38), on the info/placement panel
    // (placement_open true) - the same point resolves ToggleDemolish instead when false, since
    // the two rectangles overlap and only the state tells them apart (see the header comment).
    {
        const auto r = menu.click(984.0 + 5.0, 725.0 + 5.0, "public", true, nullptr);
        CHECK(r.kind == app::ConstructionClickKind::Cancel);
    }
    {
        const auto r = menu.click(984.0 + 5.0, 725.0 + 5.0, "public", false, nullptr);
        CHECK(r.kind == app::ConstructionClickKind::ToggleDemolish);
    }
    // Outside every tracked rectangle: None.
    {
        const auto r = menu.click(10.0, 10.0, "public", false, nullptr);
        CHECK(r.kind == app::ConstructionClickKind::None);
    }
}

TEST_CASE("construction menu: cell_info reads cost live from the registry, never guesses "
          "upkeep (UI.2 C3, OQ-563)") {
    anno::sim::TypeRegistry::Builder b;
    anno::sim::BuildingType priced;
    priced.id = anno::sim::TypeId{501};
    priced.cost_gold = 250;
    priced.cost_goods = {{41, 300}, {42, 700}}; // tools, wood (8.8 fixed-point, as the registry
                                                // stores every cost amount)
    b.add(priced);
    const auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));

    const auto known = app::ConstructionMenu::cell_info(anno::sim::TypeId{501}, *built.value());
    CHECK(known.known);
    CHECK(known.cost_gold == 250);
    REQUIRE(known.cost_goods.size() == 2);
    CHECK(known.cost_goods[0].goods == 41);
    CHECK(known.cost_goods[0].amount_88 == 300);
    CHECK(known.cost_goods[1].goods == 42);
    CHECK(known.cost_goods[1].amount_88 == 700);
    // Never guessed: no published source exists yet for an unbuilt instance's upkeep (OQ-563).
    CHECK_FALSE(known.upkeep.has_value());

    const auto unknown = app::ConstructionMenu::cell_info(anno::sim::TypeId{9999}, *built.value());
    CHECK_FALSE(unknown.known);
    CHECK(unknown.cost_gold == 0);
    CHECK(unknown.cost_goods.empty());
    CHECK_FALSE(unknown.upkeep.has_value());
}
