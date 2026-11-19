#include <algorithm>
#include <limits>
#include <set>

#include <anno/sim_load/wildlife_create_batch.hpp>

namespace anno::sim_load {
formats::Result<WildlifeCreateBatch, WildlifeCreateBatchFailure>
prepare_wildlife_create_batch(const WildlifeFirstCallResult& prepared,
                              const WildlifeImportPlan& plan, const WildlifeNumericContext& numeric,
                              std::span<const WildlifeManagerFact> managers,
                              std::string provenance) {
    using Scope = WildlifeCreateBatchScope;
    using Stage = WildlifeCreateBatchStage;
    const auto admission = [](Stage stage, std::string reason,
                              std::optional<std::uint64_t> token = {}) {
        return WildlifeCreateBatchFailure{Scope::Admission,  stage, token,
                                          std::move(reason), {},    {}};
    };
    const auto& records = plan.maintenance.completed.chunk.records;
    const auto& steps = plan.materialization.completed.entries;
    if (records.size() != steps.size())
        return admission(Stage::Shape,
                         "batch: one materialization step per maintained record required");
    if (plan.draws.imported_prefix != prepared.fill.prefix_consumed ||
        plan.imported.prefix_draws != plan.draws.imported_prefix ||
        plan.draws.maintenance != plan.maintenance.completed.consumed ||
        plan.draws.selector_only != plan.materialization.completed.selector_consumed)
        return admission(Stage::Shape, "batch: recorded draw domains disagree");
    std::size_t cumulative = 0;
    std::set<std::uint64_t> selected, tokens;
    for (std::size_t i = 0; i < steps.size(); ++i) {
        const auto& step = steps[i];
        const auto& record = records[i];
        if (step.token != record.token || !tokens.insert(record.token).second)
            return admission(Stage::Shape, "batch: materialization/list order or token mismatch",
                             step.token);
        if (step.selector_consumed > std::numeric_limits<std::size_t>::max() - cumulative)
            return admission(Stage::Shape, "batch: selector count overflow", step.token);
        cumulative += step.selector_consumed;
        if (step.cumulative_selector_consumed != cumulative ||
            (step.disposition == WildlifeMaterializeDisposition::Selected &&
             (!step.selection || step.selection->choice.consumed != step.selector_consumed)))
            return admission(Stage::Shape, "batch: per-entry selector accounting mismatch",
                             step.token);
        if (step.disposition != WildlifeMaterializeDisposition::Selected) {
            if (step.selector_consumed != 0 ||
                (step.disposition != WildlifeMaterializeDisposition::WrongCategory &&
                 step.disposition != WildlifeMaterializeDisposition::Occupied &&
                 step.disposition != WildlifeMaterializeDisposition::NotReady &&
                 step.disposition != WildlifeMaterializeDisposition::NullType &&
                 step.disposition != WildlifeMaterializeDisposition::NonqualifyingType))
                return admission(Stage::Descriptor,
                                 "batch: incomplete materialization cannot be executed",
                                 step.token);
            continue;
        }
        selected.insert(step.token);
        if (record.category != 0 || !record.occupied || *record.occupied || !step.selection ||
            !step.selection->creation)
            return admission(Stage::Descriptor,
                             "batch: selected unoccupied spawn and sealed descriptor required",
                             step.token);
        const auto& choice = *step.selection;
        const auto& cap = *choice.creation;
        const auto& fact = choice.selected;
        if (!plan.materialization.completed.catalogue ||
            choice.choice.value.index >= plan.materialization.completed.catalogue->slots().size() ||
            plan.materialization.completed.catalogue->slots()[choice.choice.value.index] != fact)
            return admission(Stage::Descriptor,
                             "batch: selection differs from owned source catalogue", step.token);

        if (choice.disposition != WildlifePlanDisposition::SupportedNew || cap.id() != fact.id ||
            choice.choice.value.species != fact.id || cap.kind() != fact.kind ||
            cap.hp() != fact.hp || cap.yield() != fact.yield ||
            cap.image_sha256() != fact.image_sha256 || cap.source_sha256() != fact.source_sha256)
            return admission(Stage::Descriptor, "batch: selected descriptor identity mismatch",
                             step.token);
    }
    if (cumulative != plan.draws.selector_only)
        return admission(Stage::Shape, "batch: selector total disagrees");
    std::map<std::uint64_t, const WildlifeManagerFact*> indexed;
    for (const auto& manager : managers) {
        if (!selected.contains(manager.token) || !indexed.emplace(manager.token, &manager).second)
            return admission(Stage::Manager, "batch: duplicate or unselected manager token",
                             manager.token);
    }
    auto source = bind_wildlife_ordinary_source(prepared, plan, numeric, provenance);
    if (!source) {
        auto error = admission(Stage::Source, source.error().reason);
        error.geometry_error = source.error();
        return error;
    }
    const auto& tiles = prepared.fill.island.resource.tiles;
    WildlifeCreateBatchProgress progress;
    progress.chunk = plan.maintenance.completed.chunk;
    progress.sidecars = plan.imported.sidecars;
    progress.unchanged_draws = plan.draws;
    progress.unused_maintenance = plan.unused_maintenance;
    progress.unused_selectors = plan.materialization.completed.unused_draws;
    progress.scene_sha256 = prepared.source_sha256;
    progress.corner_provenance = std::move(provenance);
    progress.origin_x = static_cast<std::uint32_t>(tiles.x0());
    progress.origin_z = static_cast<std::uint32_t>(tiles.y0());
    progress.columns = tiles.chunks_x();
    progress.rows = tiles.chunks_y();
    progress.chunk_x = plan.imported.chunk_x;
    progress.chunk_z = plan.imported.chunk_y;
    const auto reached = [&](Stage stage, std::uint64_t token, std::string reason) {
        return WildlifeCreateBatchFailure{Scope::ReachedEntry, stage,    token,
                                          std::move(reason),   progress, {}};
    };
    for (std::size_t i = 0; i < steps.size(); ++i) {
        const auto& step = steps[i];
        auto& record = progress.chunk.records[i];
        WildlifeCreateBatchEntry entry;
        entry.token = record.token;
        entry.prior = record;
        if (step.disposition != WildlifeMaterializeDisposition::Selected) {
            progress.entries.push_back(std::move(entry));
            continue;
        }
        auto geometry = source.value().project(step.token);
        if (!geometry) {
            auto error = reached(Stage::Geometry, step.token, geometry.error().reason);
            error.geometry_error = geometry.error();
            return error;
        }
        const auto manager = indexed.find(step.token);
        if (manager == indexed.end() ||
            (!manager->second->outcome || manager->second->provenance.empty()))
            return reached(Stage::Manager, step.token,
                           "batch: reached manager outcome/provenance unavailable");
        const auto& supplied = *manager->second;
        const WildlifeCreationOutcomeInputs input{
            *supplied.outcome,
            supplied.assigned_id,
            supplied.creation_time,
            geometry.value().words,
            {{tiles.x0(), tiles.y0(), tiles.width(), tiles.height()},
             geometry.value().home[0],
             geometry.value().home[1],
             WildlifeCreationPlace::Ordinary,
             0},
            plan.supplied.time,
            static_cast<std::uint64_t>(step.selector_consumed)};
        auto outcome = project_wildlife_creation_outcome(*step.selection->creation, input);
        if (!outcome)
            return reached(Stage::Projection, step.token, outcome.error());
        entry.manager = supplied;
        entry.geometry = std::move(geometry.value());
        entry.descriptor = *step.selection->creation;
        entry.disposition = outcome.value().animal ? WildlifeCreateBatchDisposition::Created
                                                   : WildlifeCreateBatchDisposition::Null;
        if (outcome.value().writes) {
            const auto& writes = *outcome.value().writes;
            // No packed-byte construction: only the proved source fields are overwritten.
            record.saved_id = writes.id;
            record.timestamp = writes.timestamp;
            record.occupied = true;
            record.goods = writes.goods;
        }
        entry.effects = std::move(outcome.value());
        progress.entries.push_back(std::move(entry));
    }
    return WildlifeCreateBatch{std::move(progress)};
}
} // namespace anno::sim_load
