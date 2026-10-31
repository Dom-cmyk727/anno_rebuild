// picture_check (round dx, DX1): see the header.
#include <anno/app/picture_check.hpp>

namespace anno::app {

ChunkDiff compare_chunks(const std::vector<render::ChunkKey>& keys, const ChunkLookup& candidate,
                         const ChunkLookup& reference) {
    ChunkDiff diff;
    for (const render::ChunkKey& key : keys) {
        const render::Chunk* want = reference(key);
        const render::Chunk* got = candidate(key);
        if (want == nullptr || got == nullptr) {
            ++diff.missing;
            continue;
        }
        ++diff.compared;
        diff.with_objects += want->objects.empty() ? 0U : 1U;
        diff.ground += want->ground == got->ground ? 0U : 1U;
        diff.objects += want->objects == got->objects ? 0U : 1U;
        diff.terrain += want->terrain == got->terrain ? 0U : 1U;
        diff.field += want->field == got->field ? 0U : 1U;
    }
    return diff;
}

} // namespace anno::app
