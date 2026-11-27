#include <anno/render/chunk_instances.hpp>
#include <anno/render/frame.hpp>

namespace anno::render {

bool is_static_instance(const formats::anlagen::AnlagenFile& anlagen, std::uint32_t anlage_id) {
    const formats::figuren::Entry* entry = anlagen.entry(anlage_id);
    if (entry == nullptr || entry->gfx.empty()) {
        return true; // place_prepared always skips these too (failed/invisible), never time-varying
    }
    const auto [n, ms] = animation(entry->gfx.front());
    return n == 1 || ms == 0;
}

std::vector<StaticInstance> resolve_static_instances(std::span<const Prepared> list,
                                                     const formats::anlagen::AnlagenFile& anlagen,
                                                     std::size_t group, Lift lift) {
    std::vector<StaticInstance> out;
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (!is_static_instance(anlagen, list[i].first.anlage)) {
            continue;
        }
        std::vector<SpriteDraw> draws;
        // One object at a time, as Scene::object_frame already does: place_prepared's visible()
        // is only a culling filter, never part of the resolved result.
        place_prepared(
            list.subspan(i, 1), anlagen, group, lift, 0, [](int, int, int, int) { return true; },
            draws);
        for (SpriteDraw& d : draws) {
            out.push_back({i, std::move(d)});
        }
    }
    return out;
}

std::map<std::pair<int, int>, std::vector<StaticInstance>>
bucket_by_chunk(const std::vector<StaticInstance>& instances, std::pair<int, int> offset) {
    std::map<std::pair<int, int>, std::vector<StaticInstance>> out;
    for (const StaticInstance& inst : instances) {
        StaticInstance global = inst;
        global.draw.anchor.first += offset.first;
        global.draw.anchor.second += offset.second;
        const auto r = global.draw.reach();
        for (const auto& chunk : visible_chunks(Rect{r[0], r[1], r[2], r[3]})) {
            out[chunk].push_back(global);
        }
    }
    return out;
}

} // namespace anno::render
