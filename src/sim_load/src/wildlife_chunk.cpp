#include <algorithm>
#include <limits>
#include <set>
#include <utility>

#include <anno/sim/wildlife.hpp>
#include <anno/sim_load/wildlife_chunk.hpp>

namespace anno::sim_load {
formats::Result<WildlifeChunkPlan, WildlifeChunkFailure>
prepare_wildlife_chunk(const WildlifeChunkInputs& input, std::span<const std::uint32_t> draws) {
    WildlifeChunkProgress work;
    work.chunk = input.chunk;
    const auto fail = [&](WildlifeChunkFault fault, WildlifeChunkStage stage, std::string reason) {
        return WildlifeChunkFailure{fault, stage, std::move(reason), work};
    };
    using F = WildlifeChunkFault;
    using S = WildlifeChunkStage;
    if (input.allocation != WildlifeChunkAllocation::NormalSuccess)
        return fail(F::InvalidFacts, S::Validate, "chunk plan: unsupported allocation profile");
    std::set<std::uint64_t> tokens;
    std::uint64_t original_count = 0;
    for (const auto& record : input.chunk.records) {
        if (!tokens.insert(record.token).second)
            return fail(F::InvalidFacts, S::Validate, "chunk plan: duplicate record token");
        if (record.category != 0)
            continue;
        if (++original_count > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
            return fail(F::InvalidFacts, S::Validate,
                        "chunk plan: outside original signed count profile");
        if (!record.position || *record.position > 63 || !record.occupied)
            return fail(F::InvalidFacts, S::Validate, "chunk plan: unknown required record fields");
        if (!*record.occupied && !input.habitat[*record.position])
            return fail(F::InvalidFacts, S::Validate, "chunk plan: missing retention habitat");
    }
    for (const auto token : input.fresh_tokens)
        if (!tokens.insert(token).second)
            return fail(F::InvalidFacts, S::Validate, "chunk plan: ambiguous fresh record token");
    for (unsigned z = 1; z < 8; z += 2)
        for (unsigned x = 1; x < 8; x += 2)
            if (!input.habitat[x + 8 * z])
                return fail(F::InvalidFacts, S::Validate, "chunk plan: missing candidate habitat");
    // Welt 0x100934f7–0x10093578: cache traversal order; rejected records prepend reuse.
    work.chunk.records.clear();
    for (const auto& record : input.chunk.records) {
        if (record.category != 0) {
            work.chunk.records.push_back(record);
            continue;
        }
        if (*record.occupied || *input.habitat[*record.position]) {
            ++work.retained;
            work.chunk.records.push_back(record);
        } else
            work.reuse.push_back(record);
    }
    std::reverse(work.reuse.begin(),
                 work.reuse.end()); // Equivalent head-prepend order, linear work.
    work.retained_before_replenish = work.retained;
    work.recycled_order = work.reuse;
    // Welt 0x1009357a–0x100935de: y outer, x inner; no actual terrain query here.
    for (std::uint8_t z = 1; z < 8; z += 2)
        for (std::uint8_t x = 1; x < 8; x += 2) {
            const auto position = static_cast<std::uint8_t>(x + 8U * z);
            if (*input.habitat[position])
                work.candidates.push_back(position);
        }
    const auto density = sim::wildlife_density(static_cast<std::uint32_t>(work.candidates.size()));
    if (!density)
        return fail(F::InvalidFacts, S::Validate, "chunk plan: invalid density domain");
    work.desired = density.value().value;
    // The old replenish helper has no record writes or partial failure snapshot.
    // This phase retains those effects rather than reconstructing them from counts.
    while (work.retained < work.desired) {
        if (!work.reuse.empty()) {
            work.reserved = work.reuse.front();
            work.reuse.erase(work.reuse.begin());
        } else {
            if (work.fresh_used == input.fresh_tokens.size())
                return fail(F::InvalidFacts, S::Reserve,
                            "chunk plan: missing successful allocation fact");
            WildlifeChunkRecord fresh;
            fresh.token = input.fresh_tokens[work.fresh_used++];
            work.reserved = fresh; // Table/category/next only initialized; other fields unknown.
        }
        for (;;) {
            if (work.candidates.empty())
                return fail(F::Oq572CandidatesExhausted, S::SelectCandidate,
                            "chunk plan: OQ-572 candidate exhaustion");
            if (work.consumed == draws.size())
                return fail(F::DrawsExhausted, S::SelectCandidate,
                            "chunk plan: supplied draws exhausted");
            const auto raw = draws[work.consumed++];
            const auto modulus = static_cast<std::uint32_t>(work.candidates.size());
            const auto index = raw % modulus;
            const auto position = work.candidates[index];
            work.candidates[index] = work.candidates.back();
            work.candidates.pop_back();
            // Welt 0x10093684 /0x100936b2: these writes precede collision lookup.
            work.reserved->goods = 255;
            work.reserved->position = position;
            std::optional<std::uint64_t> collision;
            if ((work.chunk.flags & 1U) != 0)
                for (const auto& record : work.chunk.records)
                    if (record.category == 0 && record.position == position) {
                        collision = record.token;
                        break;
                    }
            work.attempts.push_back({raw, modulus, position, collision, *work.reserved});
            if (collision)
                continue; // SAME reserved record, with pre-insertion timestamp unchanged.
            work.chunk.records.insert(work.chunk.records.begin(), *work.reserved);
            work.chunk.flags = static_cast<std::uint16_t>(work.chunk.flags | 1U);
            auto& inserted = work.chunk.records.front();
            inserted.occupied = false;
            inserted.timestamp = static_cast<std::uint8_t>(input.time / 1000U);
            work.inserted.push_back(inserted);
            work.reserved.reset();
            ++work.retained;
            break;
        }
    }
    work.discarded = std::move(work.reuse); // Semantic discard only, no native destructor.
    work.reuse.clear();
    if (work.retained == 0)
        work.chunk.flags = static_cast<std::uint16_t>(work.chunk.flags & 0xfffeU);
    return WildlifeChunkPlan{std::move(work)};
}
} // namespace anno::sim_load
