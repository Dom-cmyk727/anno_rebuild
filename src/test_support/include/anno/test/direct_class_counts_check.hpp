// RENDER-DIRECT-3 stage 3a scope question (ORCH III, 2026-11-27): how many static instances on a
// scene have object_flags::depth_rule(flags).cls == depth::Class::None (3a's scope) versus a real
// depth class, and how many of the None ones carry the shadow bit (depth.hpp: Class::None is
// shared by "shadows; units", so None is not just "ordinary objects" - counted separately here
// rather than assumed).
#pragma once

#include <array>
#include <cstddef>
#include <utility>

#include <doctest/doctest.h>

#include <anno/render/chunk_draws.hpp>
#include <anno/render/chunk_instances.hpp>
#include <anno/render/depth.hpp>
#include <anno/render/scene.hpp>

namespace anno::test {

struct DepthClassCounts {
    std::array<std::size_t, 4> by_class{}; // indexed by depth::Class
    std::size_t none_shadow = 0;
    std::size_t none_not_shadow = 0;
    std::size_t total = 0;
};

/// Resolves every island's static instances at group 0 (the depth class is a property of the
/// frame's own pixel format, render::depth::rule, not of the zoom group) and tallies them by
/// depth class. Prints a table via MESSAGE; does not assert (the caller decides what, if
/// anything, to CHECK against the count).
inline DepthClassCounts count_depth_classes(const render::Scene& scene, const char* label) {
    DepthClassCounts out;
    constexpr std::size_t kGroup = 0;
    for (std::size_t island_i = 0; island_i < scene.islands.size(); ++island_i) {
        const auto& list = scene.island_lists(island_i).other;
        if (list.empty()) {
            continue;
        }
        std::pair<int, int> offset{0, 0};
        bool placed = false;
        for (const auto& p : scene.layouts[kGroup]) {
            if (p.index == island_i) {
                offset = p.offset;
                placed = true;
                break;
            }
        }
        if (!placed) {
            continue;
        }
        const auto statics =
            render::resolve_static_instances(list, scene.anlagen, kGroup, scene.lift);
        const render::depth::Params dp = render::depth::params(kGroup, scene.lift);
        for (const auto& inst : statics) {
            const auto rec = scene.object_record(inst.draw, {0, 0}, kGroup, scene.depth_test, dp);
            if (!rec) {
                continue; // an undecodable frame: object_record itself declines it (not a class)
            }
            const auto rule = render::object_flags::depth_rule(rec->flags);
            ++out.by_class[static_cast<std::size_t>(rule.cls)];
            ++out.total;
            if (rule.cls == render::depth::Class::None) {
                if (inst.draw.is_shadow()) {
                    ++out.none_shadow;
                } else {
                    ++out.none_not_shadow;
                }
            }
        }
    }
    MESSAGE(label << ": " << out.total << " static instances - None: "
                  << out.by_class[static_cast<std::size_t>(render::depth::Class::None)]
                  << " (shadow: " << out.none_shadow << ", not: " << out.none_not_shadow
                  << "), Z: " << out.by_class[static_cast<std::size_t>(render::depth::Class::Z)]
                  << ", V: " << out.by_class[static_cast<std::size_t>(render::depth::Class::V)]
                  << ", H: " << out.by_class[static_cast<std::size_t>(render::depth::Class::H)]);
    return out;
}

} // namespace anno::test
