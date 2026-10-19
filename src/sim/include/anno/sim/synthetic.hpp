// Synthetic scenarios: small, code-built type data and, in later stages,
// islands and cities, so the tests and the goldens need no game files
// (spec: docs/sim/state.md §14.3; CI has none).
//
// The production types use the per-step values the original's Anlagen.dat
// holds for the entries named in docs/re/production.md §9 (F1: weaver 6 / 11 /
// 5 t, grain farm 37 / 80, fur trapper 26 / 25, small farm 13, 4 / 128, 50,
// bow maker 9, iron mine 21), under ids in a synthetic range (9001...) that no
// Anlagen.dat entry uses.
#pragma once

#include <memory>
#include <string>

#include <anno/formats/result.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim::synthetic {

/// The synthetic building types and their ids.
struct SyntheticTypes {
    std::shared_ptr<const TypeRegistry> registry;
    /// HANDWERK weaver: STOFFE from WOLLE, 6 per step for 11, 5 t.
    TypeId weaver;
    /// PLANTAGE grain farm: KORN from GETREIDE, 37 for 80.
    TypeId farm;
    /// PELZJAEGER: PELZE from PELZTIER, 26 for 25.
    TypeId fur_trapper;
    /// UNIVERSAL small farm, two lines: 13 and 4 for 128 and 50.
    TypeId small_farm;
    /// HANDWERKWAFFEN bow maker: BOGEN and ARMBRUST.
    TypeId bow_maker;
    /// MINE: EISENERZ, 21 per step.
    TypeId iron_mine;
    /// PIONIER house (PRODTYPE 97).
    TypeId pioneer_house;
    /// KONTOR (PRODTYPE 35, the market list).
    TypeId kontor;
    /// MARKTSTAND (PRODTYPE 161, the stall list, never scheduled).
    TypeId market_stall;
    /// STRASSE (PRODTYPE 193): a tile object, not a building.
    TypeId road;
};

/// The registry of the types above on the original goods and class tables.
formats::Result<SyntheticTypes, std::string> make_synthetic_types();

} // namespace anno::sim::synthetic
