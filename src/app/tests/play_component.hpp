// Test-only original-scene projection. Never include this from production code.
#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <utility>

#include <anno/app/play.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/scenario_load.hpp>

#include "../../sim/tests/original_component.hpp"
namespace anno::test {
// Explicit geometry/renderer component input, selected before any playable load.
// No production loader can select this actor-free projection. Unsupported whole
// scripts remain a verified refusal and Missing permission context in this fixture.
inline anno::formats::Result<anno::app::PlayLoad, std::string>
load_scene_component(const std::filesystem::path& dir, const std::filesystem::path& file) {
    auto registry =
        anno::sim_load::load_original_registry(dir, anno::sim_load::FigureEntries::Required);
    if (!registry.error.empty())
        return registry.error;
    auto templates = anno::sim_load::scan_island_templates(dir);
    auto loaded = anno::sim_load::load_scenario_file(file, dir, templates, *registry.types, {}, {},
                                                     original_component::actor_free_units());
    if (!loaded)
        return loaded.error();
    auto component = original_component::import_only(loaded.value().loaded.spec, registry.types);
    if (!component)
        return component.error();
    auto world = anno::sim::World::create(component.value().spec, registry.types);
    if (!world)
        return world.error();
    anno::app::PlayLoad out;
    out.world = std::move(world).value();
    out.registry = registry.types;
    std::map<std::uint32_t, std::string> keys;
    for (std::size_t i = 0; i < loaded.value().resolution.links.size(); ++i) {
        const auto& link = loaded.value().resolution.links[i];
        if (!link.candidate || *link.candidate >= templates.files.size())
            continue;
        const auto number = loaded.value().loaded.links[i].number;
        keys[number & 0xffffU] = templates.summaries[*link.candidate].name;
        anno::app::LinkTemplate chosen;
        chosen.number = number;
        chosen.file = templates.files[*link.candidate];
        out.links.push_back(std::move(chosen));
    }
    for (const auto& is : component.value().spec.islands) {
        if (!is.terrain)
            continue;
        const auto key = keys.find(is.slot.v);
        out.islands.push_back(
            {is.slot.v, is.x0, is.y0, is.w, is.h, key == keys.end() ? "embedded" : key->second});
    }
    return out;
}

} // namespace anno::test
