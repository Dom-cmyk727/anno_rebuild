// UI-HARVEST-ICON-1 stage B: the status icons over buildings (status_icons.hpp, docs/re/ui.md
// §11). Every expected value is derived by hand from the RE note, not copied from a run:
// - the icon is the lowest of {Zzz, bit 1, bit 2, bit 3} (0x100730d3-0x100730e4), Zzz only when
//   every line is off (0x100690b0 / 0x1006910c), nothing when the owner's Volk is not 1
//   (0x1007302f);
// - a repeat above 65 535 never falls due (0x10072f1a, ISS-150), through the sim's own
//   StatusTracker::report;
// - frame = (ms / 40) mod 16 (OQ-722 default);
// - the game-file case compares the layer's decoded frames with tools/figuren.py's (an
//   independent decoder): size, anchor, opaque count and the sum of R + G + B over opaque pixels.
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/status_icons.hpp>
#include <anno/sim/building.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>
#include <anno/test/game_dir.hpp>

namespace app = anno::app;
namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
namespace render = anno::render;

namespace {

sim::StatusTracker with_icons(std::uint8_t icons) {
    sim::StatusTracker s;
    s.icons = icons;
    return s;
}

sim::Workshop workshop(bool on, std::uint8_t icons) {
    sim::Workshop w;
    w.switched_on = on;
    w.status = with_icons(icons);
    return w;
}

sim::ScenarioSpec::BuildingSpec place(sim::TypeId type, std::int32_t x, std::int32_t y,
                                      sim::Payload payload) {
    sim::ScenarioSpec::BuildingSpec b;
    b.type = type;
    b.x = x;
    b.y = y;
    b.payload = std::move(payload);
    return b;
}

/// A 1 x 1 frame of colour `tag` with anchor (0, 0): which frame a unit carries is visible in it.
render::UnitSprite tagged(std::uint8_t tag) {
    render::UnitSprite s;
    for (std::size_t g = 0; g < render::kGroups; ++g) {
        auto image = std::make_shared<anno::assets::Rgba>();
        image->width = 1;
        image->height = 1;
        image->pixels = {tag, static_cast<std::uint8_t>(g), 0, 255};
        anno::formats::figuren::Frame f;
        f.width = 1;
        f.height = 1;
        f.anchor_x = 0;
        f.anchor_y = static_cast<std::int16_t>(10 + g);
        s.frames.emplace_back(render::UnitFrame{f, image, render::UnitFrameId{}});
    }
    return s;
}

} // namespace

TEST_CASE("status icons: the icon of a payload, lowest bit first, Zzz first") {
    CHECK_FALSE(app::status_icon_of(sim::Payload{workshop(true, 0)}, true).has_value());
    CHECK(app::status_icon_of(sim::Payload{workshop(false, 0)}, true) == app::StatusIcon::Off);
    CHECK(app::status_icon_of(sim::Payload{workshop(true, 0x02)}, true) ==
          app::StatusIcon::MissingInput);
    CHECK(app::status_icon_of(sim::Payload{workshop(true, 0x04)}, true) ==
          app::StatusIcon::StorageFull);
    // A set bit 1 hides a later bit 2 (docs/re/ui.md 11.3 step 4).
    CHECK(app::status_icon_of(sim::Payload{workshop(true, 0x06)}, true) ==
          app::StatusIcon::MissingInput);
    // Zzz wins over a still-set bit 1; switched on again, bit 1 shows at once.
    CHECK(app::status_icon_of(sim::Payload{workshop(false, 0x02)}, true) == app::StatusIcon::Off);
    // Bit 3 (code 3, never set by the sim yet) maps to the road icon; bit 0 alone is not read.
    CHECK(app::status_icon_of(sim::Payload{workshop(true, 0x08)}, true) == app::StatusIcon::NoRoad);
    CHECK_FALSE(app::status_icon_of(sim::Payload{workshop(true, 0x01)}, true).has_value());
    // The owner gate: no icon at all, not even Zzz.
    CHECK_FALSE(app::status_icon_of(sim::Payload{workshop(false, 0x02)}, false).has_value());
    // Classes without icons.
    CHECK_FALSE(app::status_icon_of(sim::Payload{sim::Inert{}}, true).has_value());
    sim::Inert inert;
    inert.status = with_icons(0x02);
    CHECK_FALSE(app::status_icon_of(sim::Payload{inert}, true).has_value());
    CHECK_FALSE(app::status_icon_of(sim::Payload{sim::Market{}}, true).has_value());
}

TEST_CASE("status icons: a two-line building shows Zzz only when every line is off") {
    sim::WeaponsWorkshop w;
    w.line_on = {true, false};
    CHECK_FALSE(app::status_icon_of(sim::Payload{w}, true).has_value());
    w.line_on = {false, false};
    CHECK(app::status_icon_of(sim::Payload{w}, true) == app::StatusIcon::Off);
    sim::SmallFarm s;
    s.line_on = {false, true};
    s.status = with_icons(0x04);
    CHECK(app::status_icon_of(sim::Payload{s}, true) == app::StatusIcon::StorageFull);
    s.line_on = {false, false};
    CHECK(app::status_icon_of(sim::Payload{s}, true) == app::StatusIcon::Off);
    sim::Farm f;
    f.switched_on = false;
    CHECK(app::status_icon_of(sim::Payload{f}, true) == app::StatusIcon::Off);
    sim::Mine m;
    m.status = with_icons(0x02);
    CHECK(app::status_icon_of(sim::Payload{m}, true) == app::StatusIcon::MissingInput);
}

TEST_CASE("status icons: the 16-bit due test, through the sim's own StatusTracker") {
    // A farm missing input (60 000) gets icon 1; a small farm (75 000) never does (ISS-150).
    sim::StatusTracker farm, small;
    bool farm_due = false, small_due = false;
    for (std::uint32_t t = 500; t <= 200000; t += 500) {
        farm_due = farm.report(sim::Status::MissingInput, t, sim::kRepeatFarmMissing) || farm_due;
        small_due =
            small.report(sim::Status::MissingInput, t, sim::kRepeatSmallFarmMissing) || small_due;
    }
    CHECK(farm_due);
    CHECK_FALSE(small_due);
    sim::Farm f;
    f.status = farm;
    sim::SmallFarm s;
    s.line_on = {true, true};
    s.status = small;
    CHECK(app::status_icon_of(sim::Payload{f}, true) == app::StatusIcon::MissingInput);
    CHECK(small.code == sim::Status::MissingInput);                      // the code is set ...
    CHECK_FALSE(app::status_icon_of(sim::Payload{s}, true).has_value()); // ... the icon is not
}

TEST_CASE("status icons: collected for the viewer's own buildings only") {
    auto types = syn::make_synthetic_types();
    REQUIRE(types.ok());
    const syn::SyntheticTypes& t = types.value();
    const sim::TypeRegistry& reg = *t.registry;
    sim::Workshop off = sim::make_workshop(reg.find(t.weaver)->prod);
    off.switched_on = false;
    sim::Farm missing;
    missing.status = with_icons(0x02);
    sim::Farm quiet;
    sim::ScenarioSpec spec = syn::basic_scenario();
    spec.islands[0].cities[0].buildings = {
        place(t.weaver, 110, 110, sim::Payload{off}),
        place(t.farm, 114, 110, sim::Payload{missing}),
        place(t.farm, 118, 110, sim::Payload{quiet}),
    };
    {
        auto world = sim::World::create(spec, t.registry);
        REQUIRE_MESSAGE(world.ok(), (world.ok() ? std::string() : world.error()));
        const auto icons = app::collect_status_icons(world.value(), sim::PlayerId{0});
        REQUIRE(icons.size() == 2);
        CHECK(icons[0].icon == app::StatusIcon::Off);
        CHECK(icons[0].x == 110);
        CHECK(icons[0].y == 110);
        CHECK(icons[1].icon == app::StatusIcon::MissingInput);
        CHECK(icons[1].x == 114);
        const sim::BuildingType* farm_type = reg.find(t.farm);
        REQUIRE(farm_type != nullptr);
        const auto [fw, fh] = sim::footprint(*farm_type, 0);
        CHECK(icons[1].w == fw);
        CHECK(icons[1].h == fh);
        // Another player sees none of them (OQ-723 default).
        CHECK(app::collect_status_icons(world.value(), sim::PlayerId{1}).empty());
    }
    // The owner gate: a Volk other than 1 shows nothing.
    spec.players[0].volk = 2;
    auto world = sim::World::create(spec, t.registry);
    REQUIRE_MESSAGE(world.ok(), (world.ok() ? std::string() : world.error()));
    CHECK(app::collect_status_icons(world.value(), sim::PlayerId{0}).empty());
}

TEST_CASE("status icons: frame (ms / 40) mod 16 and the units of a layer") {
    CHECK(app::status_icon_frame(0, 16) == 0);
    CHECK(app::status_icon_frame(39, 16) == 0);
    CHECK(app::status_icon_frame(40, 16) == 1);
    CHECK(app::status_icon_frame(639, 16) == 15);
    CHECK(app::status_icon_frame(640, 16) == 0);
    CHECK(app::status_icon_frame(1000, 0) == 0);

    std::map<app::StatusIcon, std::vector<render::UnitSprite>> sets;
    for (const app::StatusIcon code : {app::StatusIcon::Off, app::StatusIcon::MissingInput}) {
        std::vector<render::UnitSprite> frames;
        for (std::uint8_t k = 0; k < 16; ++k) {
            frames.push_back(
                tagged(static_cast<std::uint8_t>(static_cast<unsigned>(code) * 16 + k)));
        }
        sets.emplace(code, std::move(frames));
    }
    const auto layer = app::StatusIconLayer::from_prepared(std::move(sets));
    app::PlacedStatusIcon a;
    a.x = 11.5F;
    a.y = 7.5F;
    a.icon = app::StatusIcon::MissingInput;
    a.raise = {30, 15, 7};
    app::PlacedStatusIcon b = a;
    b.icon = app::StatusIcon::StorageFull; // not in this layer: skipped
    const std::vector<app::PlacedStatusIcon> placed{a, b};
    const auto units = layer->units(placed, 13 * 40 + 5);
    REQUIRE(units.size() == 1);
    CHECK(units[0].x == doctest::Approx(11.5F));
    CHECK(units[0].y == doctest::Approx(7.5F));
    REQUIRE(units[0].frames.size() == render::kGroups);
    for (std::size_t g = 0; g < render::kGroups; ++g) {
        REQUIRE(units[0].frames[g]);
        CHECK(units[0].frames[g]->image->pixels[0] == 16 + 13); // MissingInput, frame 13
        CHECK(units[0].frames[g]->frame.anchor_y == static_cast<int>(10 + g) + a.raise[g]);
    }
}

TEST_CASE("status icons: the layer's frames are the original's (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto loaded = app::StatusIconLayer::load(*dir);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    const auto& layer = *loaded.value();
    CHECK(layer.sets() == 4);
    struct Expect {
        app::StatusIcon icon;
        std::size_t frame;
        std::size_t group;
        int w, h, ax, ay;
        std::size_t opaque;
        std::uint64_t rgb_sum;
    };
    // tools/figuren.py (cache script ref.py of the packet): figure, frame k, zoom group.
    const Expect expect[] = {
        {app::StatusIcon::Off, 0, 0, 20, 51, 10, 51, 520, 193503},
        {app::StatusIcon::Off, 0, 2, 4, 13, 2, 13, 33, 12334},
        {app::StatusIcon::MissingInput, 13, 0, 48, 44, 24, 35, 1143, 363529},
        {app::StatusIcon::MissingInput, 13, 1, 24, 23, 12, 18, 295, 93673},
        {app::StatusIcon::MissingInput, 13, 2, 12, 11, 6, 9, 80, 25641},
        {app::StatusIcon::StorageFull, 5, 0, 42, 45, 21, 34, 1192, 458950},
        {app::StatusIcon::StorageFull, 5, 1, 22, 23, 11, 17, 311, 118531},
    };
    for (const Expect& e : expect) {
        CAPTURE(static_cast<int>(e.icon));
        CAPTURE(e.group);
        app::PlacedStatusIcon p;
        p.icon = e.icon;
        const std::vector<app::PlacedStatusIcon> one{p};
        const auto units = layer.units(one, e.frame * app::kStatusIconFrameMs);
        REQUIRE(units.size() == 1);
        REQUIRE(units[0].frames.size() > e.group);
        REQUIRE(units[0].frames[e.group]);
        const render::UnitFrame& f = *units[0].frames[e.group];
        CHECK(f.frame.width == e.w);
        CHECK(f.frame.height == e.h);
        CHECK(f.frame.anchor_x == e.ax);
        CHECK(f.frame.anchor_y == e.ay); // raise 0
        std::size_t opaque = 0;
        std::uint64_t sum = 0;
        const auto& px = f.image->pixels;
        for (std::size_t i = 0; i + 3 < px.size(); i += 4) {
            if (px[i + 3] != 0) {
                ++opaque;
                sum += std::uint64_t{px[i]} + px[i + 1] + px[i + 2];
            }
        }
        CHECK(opaque == e.opaque);
        CHECK(sum == e.rgb_sum);
    }
}
