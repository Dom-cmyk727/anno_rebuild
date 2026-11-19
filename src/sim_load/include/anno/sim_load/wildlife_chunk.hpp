// Owned pre-materialization maintenance; docs/sim/wildlife-chunk-plan.md.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <anno/formats/result.hpp>

namespace anno::sim_load {
struct WildlifeChunkRecord {
    std::uint64_t token = 0; // Caller-local record identity, never a FigureId.
    std::uint32_t category = 0;
    std::optional<std::uint8_t> position; // Original packed low 6 bits, x | (z << 3).
    std::optional<bool> occupied, high_bit;
    std::optional<std::uint8_t> timestamp;
    std::optional<std::uint16_t> saved_id, goods;
    std::optional<std::uint8_t> byte11;
    bool operator==(const WildlifeChunkRecord&) const = default;
};
struct WildlifeChunkSnapshot {
    std::uint16_t flags = 0;
    std::vector<WildlifeChunkRecord> records; // Exact linked-list traversal order.
    bool operator==(const WildlifeChunkSnapshot&) const = default;
};
enum class WildlifeChunkAllocation { NormalSuccess };
struct WildlifeChunkInputs {
    WildlifeChunkSnapshot chunk;
    // Original 8 x 8 local geometry, not an engine map/content bound. Missing
    // outcomes are not false; only actual required query positions need a value.
    std::array<std::optional<bool>, 64> habitat{};
    std::uint32_t time = 0;
    WildlifeChunkAllocation allocation = WildlifeChunkAllocation::NormalSuccess;
    std::vector<std::uint64_t> fresh_tokens; // Successful abstract allocations only.
    bool operator==(const WildlifeChunkInputs&) const = default;
};
struct WildlifeChunkAttempt {
    std::uint32_t raw = 0, modulus = 0;
    std::uint8_t position = 0;
    std::optional<std::uint64_t> collision;
    WildlifeChunkRecord reserved_after_probe; // Before insertion/time/occupancy writes.
    bool operator==(const WildlifeChunkAttempt&) const = default;
};
struct WildlifeChunkProgress {
    WildlifeChunkSnapshot chunk;
    std::vector<WildlifeChunkRecord> reuse, recycled_order, discarded, inserted;
    std::optional<WildlifeChunkRecord> reserved;
    std::vector<std::uint8_t> candidates;
    std::vector<WildlifeChunkAttempt> attempts;
    std::uint64_t retained = 0, retained_before_replenish = 0;
    std::uint32_t desired = 0;
    std::size_t consumed = 0, fresh_used = 0;
    bool operator==(const WildlifeChunkProgress&) const = default;
};
struct WildlifeChunkPlan {
    WildlifeChunkProgress completed; // Only returned at materialization entry 0x10093767.
    bool operator==(const WildlifeChunkPlan&) const = default;
};
enum class WildlifeChunkFault { InvalidFacts, DrawsExhausted, Oq572CandidatesExhausted };
enum class WildlifeChunkStage { Validate, Reserve, SelectCandidate };
struct WildlifeChunkFailure {
    WildlifeChunkFault fault;
    WildlifeChunkStage stage;
    std::string reason;
    WildlifeChunkProgress diagnostic_only; // Never a successful plan or original rollback.
};
[[nodiscard]] formats::Result<WildlifeChunkPlan, WildlifeChunkFailure>
prepare_wildlife_chunk(const WildlifeChunkInputs&, std::span<const std::uint32_t> draws);
} // namespace anno::sim_load
