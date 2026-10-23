// Islands and their tiles (spec: docs/sim/state.md §4.8, §9.2, §9.6, stages S5
// and S6).
//
// An island owns its tile layer (the original's 8-byte tile records of the
// island object, D: AnnoWelt.dll island +0xc: u16 object id, flag bytes, the
// owner nibble ((u32 at +4) >> 25) & 0xf, the growth phase), the deposits of
// its ore objects, the state of its incremental sweeps, the city ring, the
// object manager, the drought and (S6) its cities with their buildings. The
// scheduler that ticks an island is stage S7 (island_tick.hpp, scheduler.cpp); the
// growth sweep's tile body and the drought machine follow with stage S13.
//
// Tiles are stored as columns, so that a chunk digest is one linear pass. The
// owner is a u32 (the original nibble is 4 bits, F3): city_slots and
// neutral_owner are configuration. EVERY writer goes through a setter, which
// marks the tile's chunk dirty; the cached chunk digests are derived state
// (never saved) and TileLayer::cache_consistent() is the check that none was
// missed. Hashing a const world never writes the cache (two threads may hash one
// snapshot at the same time); only the non-const refresh / prepare_hash /
// chunk_digest fill it. A chunk is `chunk_edge` x `chunk_edge` tiles (SimConfig, original 8,
// D: 0x1008b760); an island whose width or height is not a multiple of it has
// edge chunks that are clipped to the in-bounds tiles (A14, F20: the
// original's visit loops 8 x 8 without a bounds check and over-reads).
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <anno/sim/city.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/executor.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/production.hpp>

namespace anno::sim {

/// Bytes one tile takes in the save and in the raw hash: 4 (object) + 1 (phase)
/// + 4 (owner) + 4 (aux) (docs/sim/state.md §9.2).
inline constexpr std::uint32_t kTileBytes = 13;

/// The static terrain of an island: shared, immutable, identified in the state
/// by its digest (the blobs travel in the save frame, §9.5). STAND-IN (O-53,
/// OQ-060): the shape of the original island's terrain is not established, so the
/// three columns are kept as given and the simulation reads none of them.
/// A Terrain with empty columns and a digest is the placeholder that a read of the
/// flat state stream leaves (the blobs travel only in the save frame): a world in
/// that state is for hashing only, and World::save() must not be called on it
/// (World::load would refuse the frame: "the blobs do not match the digest").
struct Terrain {
    std::vector<std::uint8_t> height;
    std::vector<std::uint16_t> gfx;
    std::vector<std::uint8_t> fertility;
    /// terrain_digest of the three columns; never 0 (0 means "no terrain" in the
    /// state stream).
    std::uint64_t digest = 0;
};

/// The digest the state stores for a raw FNV result: 0 is the stream's "no
/// terrain", so a raw 0 is stored as 1 (F30).
[[nodiscard]] constexpr std::uint64_t nonzero_digest(std::uint64_t raw) {
    return raw == 0 ? 1 : raw;
}

/// FNV-1a 64 of the three columns (each as u64 length + elements, little
/// endian); a result of 0 is returned as 1, because 0 is the stream's "no
/// terrain" (F30).
[[nodiscard]] std::uint64_t terrain_digest(const Terrain& t);

/// Whether a terrain's stored digest is the digest of its columns.
[[nodiscard]] inline bool terrain_matches(const Terrain& t) {
    return t.digest == terrain_digest(t);
}

/// A terrain with its digest computed.
[[nodiscard]] std::shared_ptr<const Terrain> make_terrain(std::vector<std::uint8_t> height,
                                                          std::vector<std::uint16_t> gfx,
                                                          std::vector<std::uint8_t> fertility);

struct TileIO;

/// The tile columns of one island with their chunk digest cache.
class TileLayer {
  public:
    /// An empty 0 x 0 layer: every persistent type is default-constructible
    /// (docs/sim/state.md §4.3).
    TileLayer() = default;
    /// reset() as a constructor; the layer is empty when reset fails.
    TileLayer(std::int32_t x0, std::int32_t y0, std::uint32_t w, std::uint32_t h,
              std::uint32_t chunk_edge);

    /// Re-dimension and zero-fill (the loader, and ReadArchive after the
    /// dimensions were checked against remaining()). Origin (x0, y0) is the
    /// island's top-left tile in world coordinates. Fails (returns false, layer
    /// unchanged) when chunk_edge is 0 or w · h exceeds kIslandTileBound; a
    /// width or height of 0 is valid (an empty layer). Every chunk is dirty
    /// afterwards and the ignored-writes counter restarts at 0.
    bool reset(std::int32_t x0, std::int32_t y0, std::uint32_t w, std::uint32_t h,
               std::uint32_t chunk_edge);

    [[nodiscard]] std::int32_t x0() const { return x0_; }
    [[nodiscard]] std::int32_t y0() const { return y0_; }
    [[nodiscard]] std::uint32_t width() const { return w_; }
    [[nodiscard]] std::uint32_t height() const { return h_; }
    [[nodiscard]] std::uint32_t chunk_edge() const { return edge_; }
    [[nodiscard]] std::uint64_t tile_count() const { return std::uint64_t{w_} * h_; }
    /// ceil(w / chunk_edge) and ceil(h / chunk_edge) (A4); 0 for an empty layer.
    [[nodiscard]] std::uint32_t chunks_x() const { return cx_; }
    [[nodiscard]] std::uint32_t chunks_y() const { return cy_; }
    [[nodiscard]] std::uint64_t chunk_count() const { return std::uint64_t{cx_} * cy_; }

    /// Whether the world tile (x, y) is on this layer.
    [[nodiscard]] bool contains(std::int32_t x, std::int32_t y) const;
    /// Row-major index of the world tile (x, y), or kNoSlot when outside.
    [[nodiscard]] Slot index_of(std::int32_t x, std::int32_t y) const;

    // Reads are total: an index beyond the layer reads 0.
    /// Type id of the object on the tile (D: island slot +0x94).
    [[nodiscard]] std::uint32_t object(std::uint64_t idx) const;
    /// Growth phase (GrowthTile::phase).
    [[nodiscard]] std::uint8_t phase(std::uint64_t idx) const;
    /// City number, or neutral_owner. A u32 (F3).
    [[nodiscard]] std::uint32_t owner(std::uint64_t idx) const;
    /// The remaining record bits (flag 0x80 of byte +3, ...): O-11.
    [[nodiscard]] std::uint32_t aux(std::uint64_t idx) const;

    // The writers mark the chunk of the tile dirty. An index beyond the layer is
    // defined behaviour, not a feature to rely on: the write is dropped and
    // counted in ignored_writes(), so a stray index of a later stage (S6
    // footprints, S7 growth) shows in a test as a non-zero counter instead of as
    // a state that only a perturbation test could find.
    void set_object(std::uint64_t idx, std::uint32_t v);
    void set_phase(std::uint64_t idx, std::uint8_t v);
    void set_owner(std::uint64_t idx, std::uint32_t v);
    void set_aux(std::uint64_t idx, std::uint32_t v);
    /// The number of writes dropped for an index beyond the layer since the last
    /// reset(); 0 in every correct run. A diagnostic, not state: never hashed or
    /// saved.
    [[nodiscard]] std::uint64_t ignored_writes() const { return ignored_writes_; }

    /// Index of the chunk a tile index is in (row-major chunk order);
    /// chunk_count() (one beyond the last chunk) for idx >= tile_count(), which
    /// includes every index of an empty layer, so the call is total like the
    /// reads.
    [[nodiscard]] std::uint64_t chunk_of(std::uint64_t idx) const;
    /// The digest of one chunk: FNV-1a 64 over (object u32, phase u8, owner u32,
    /// aux u32) of the chunk's in-bounds tiles, row-major (docs/sim/state.md
    /// §9.2). 0 for a chunk beyond the layer. This const overload NEVER writes
    /// the cache: a clean chunk returns its cached value, a dirty one is computed
    /// from the columns and not stored, so any number of threads may hash one
    /// const layer (World::state_hash is const and calls this one).
    [[nodiscard]] std::uint64_t chunk_digest(std::uint64_t chunk) const;
    /// The same value, and the chunk's cache entry is brought up to date. Needs
    /// exclusive access to the layer (refresh() is the parallel door).
    [[nodiscard]] std::uint64_t chunk_digest(std::uint64_t chunk);
    /// The same value computed from the columns, bypassing the cache (the
    /// reference of the cache assertion).
    [[nodiscard]] std::uint64_t chunk_digest_uncached(std::uint64_t chunk) const;
    /// Computes the digest of every dirty chunk on `ex`, one chunk per job (each
    /// job writes only its own cache entry); the result does not depend on the
    /// executor. Needs exclusive access to the layer.
    void refresh(const Executor& ex);
    /// The number of chunks whose cached digest is out of date.
    [[nodiscard]] std::uint64_t dirty_chunks() const;
    /// Whether every clean chunk's cached digest equals the recomputed one (the
    /// invariant that no writer skipped the dirty mark, docs/sim/state.md §9.2).
    [[nodiscard]] bool cache_consistent() const;

  private:
    friend struct TileIO;

    void mark(std::uint64_t idx);

    std::int32_t x0_ = 0, y0_ = 0;
    std::uint32_t w_ = 0, h_ = 0, edge_ = 0, cx_ = 0, cy_ = 0;
    std::vector<std::uint32_t> object_;
    std::vector<std::uint8_t> phase_;
    std::vector<std::uint32_t> owner_;
    std::vector<std::uint32_t> aux_;
    // Derived, never saved: one digest and one dirty byte per chunk. Only
    // non-const members write them, so a const layer can be read by several
    // threads at once.
    std::vector<std::uint64_t> digest_;
    std::vector<std::uint8_t> dirty_;
    // Diagnostic, not state: writes dropped for an index beyond the layer.
    std::uint64_t ignored_writes_ = 0;
};

/// D: island +0x9a (stamp = the world tick at island init, D 0x1008c2e6),
/// +0x9c/+0x9e (growth sweep position), +0xa0/+0xa2 (fauna sweep position).
/// The positions are widened (the original's are u16).
struct SweepState {
    std::uint16_t stamp = 0;
    std::uint32_t grow_x = 0, grow_y = 0, fauna_x = 0, fauna_y = 0;
};
/// D: island +0x38 + 0x44 (stamp), +0x46 (cursor).
struct CityRing {
    std::uint16_t stamp = 0;
    std::uint32_t cursor = 0;
};
/// Stamp: D manager +0x14. Cursor: N, a stand-in index into Island::deposits
/// (the original cursor is a tile, A10).
struct ObjectManagerState {
    std::uint16_t stamp = 0;
    std::uint32_t cursor = 0;
};
/// The number of u32 words of a drought's city mask for `city_slots` city slots:
/// ceil(city_slots / 32) (O-55; the original's mask is one u32 for its 16 slots).
[[nodiscard]] constexpr std::uint64_t drought_mask_words(std::uint32_t city_slots) {
    return (std::uint64_t{city_slots} + 31U) / 32U;
}

/// D: the object at island +0x84, 0x18 bytes, constructor 0x10087280, ticked by
/// drought_tick (0x1008aeb0, stage S13, docs/sim/state.md §5.8). validate_islands
/// accepts the states the machine leaves behind between two ticks (§4.8): x and y
/// multiples of chunk_edge below the world's edges, 0 <= ring <= target_ring,
/// phase < drought_phases, step < drought_steps · chunk_edge, counter <
/// drought_plateau_count (the original resets at the limit in the same tick: `jb`
/// at 0x1008b4f7, 0x1008b516, 0x1008b5be), and a city_mask of
/// drought_mask_words(city_slots) words with no bit beyond the city slots.
struct Drought {
    /// Multiples of chunk_edge (original 8).
    std::uint32_t x = 0, y = 0;
    /// radius / chunk_edge.
    std::uint32_t target_ring = 6;
    std::int32_t ring = 0;
    /// One bit per city slot (D +0xc, a u32 for the original's 16 slots): bit k % 32
    /// of word k / 32 for city slot k, drought_mask_words(city_slots) words (O-55,
    /// widened in S13; saved as u64 count + words). start_drought_at sizes it from
    /// the configuration; the default of one word fits up to 32 city slots.
    std::vector<std::uint32_t> city_mask = std::vector<std::uint32_t>(1, 0U);
    std::uint8_t phase = 0, step = 0;
    /// 1 expanding, 0 plateau, 0xff retreating.
    std::uint8_t state = 1;
    /// Plateau counter, 1000-tick units.
    std::uint16_t counter = 0;
    /// A u16 tick, the catch-up gate (§5.8).
    std::uint16_t stamp = 0;
};

/// One strip of the drought machine (D 0x1008aeb0, docs/sim/state.md §5.8), what
/// Seams::drought_tiles is called with once per strip. The rectangle is in world
/// tiles, half-open, as the original computes it: ring 0 the centre chunk (not
/// clipped), ring r >= 1 the side `phase` (0 top, 1 right, 2 bottom, 3 left) of the
/// ring clipped to the island; its chunk anchors are (rect.x0 + i · chunk_edge,
/// rect.y0 + j · chunk_edge) inside it, row-major. Empty (w or h 0) when the clipped
/// side holds no tile, and for a phase of 4 or more (no rectangle in the original;
/// reachable only with SimConfig::drought_phases above 4). `step` is the index into the
/// original's 64 in-chunk tile offsets (table 0x100ca0b8): the strip covers the
/// offsets step .. step + chunk_edge - 1 of every anchor's chunk. A tile access of a
/// seam must be clipped to the island (F36).
struct DroughtStrip {
    TileRect rect;
    std::int32_t ring = 0;
    std::uint32_t phase = 0, step = 0;
    /// The record's state is 1 (expanding); otherwise it retreats (0xff).
    bool expanding = true;
};

/// DERIVED, never saved or hashed (docs/sim/state.md §4.8, §9.1): rebuilt by
/// rebuild_island_index (create.hpp) on load, kept up to date by
/// create_building / remove_building, compared with a recomputation by
/// World::check_invariants.
struct IslandIndexCache {
    /// Every building of the island by id, with the place it sits (city, list,
    /// slot).
    std::map<BuildingId, BuildingKey> by_id;
    /// Per ListId: the serial of the building on each tile (row-major tile
    /// index), 0 for none. Empty until the list holds a building, then
    /// tile_count() entries: O(1) point lookup with no limit on the footprint's
    /// height (F3: the original assumed at most 8 rows).
    std::vector<std::vector<std::uint32_t>> grids;
};

struct Island {
    /// The slot in the world's island table.
    IslandId index;
    TileLayer tiles;
    /// May be null.
    std::shared_ptr<const Terrain> terrain;
    /// D: +0xc0 (property 7): 0 POLAR 1 TUNDRA 2 NORD 3 PRAERIE 4 DSCHUNGEL 5 STEPPE
    /// 6 WUESTE.
    std::uint32_t climate = 0;
    /// Append-only (production.hpp): the ore objects of the island.
    std::vector<Deposit> deposits;
    /// The building serial counter (stage S6); serial 0 means "none", so >= 1.
    std::uint32_t next_serial = 1;
    SweepState sweep;
    /// D: +0xa4, a game-seconds stamp.
    std::uint16_t event300_stamp = 0;
    std::optional<Drought> drought;
    ObjectManagerState objmgr;
    CityRing ring;
    /// Size SimConfig::city_slots; the slot is the city number (the owner value
    /// written to tiles, SimConfig::neutral_owner = none). A slot is freed only by
    /// demolishing the city (D9) and reused by a later founding.
    std::vector<std::optional<City>> cities;
    /// Flames, plague, volcano and wildlife objects (O-12): 0 until ported; the
    /// slot keeps the stream stable.
    std::uint64_t reserved_object_count = 0;
    /// Derived (see IslandIndexCache).
    IslandIndexCache cache;

    /// Row-major tile index of the world tile (x, y), or kNoSlot when outside.
    [[nodiscard]] Slot tile_index(std::int32_t x, std::int32_t y) const {
        return tiles.index_of(x, y);
    }
    /// The city in slot `no`, or nullptr (no such slot, or the slot is empty).
    [[nodiscard]] const City* city(CityNo no) const {
        return no.v < cities.size() && cities[no.v].has_value() ? &*cities[no.v] : nullptr;
    }
    [[nodiscard]] City* city(CityNo no) {
        return no.v < cities.size() && cities[no.v].has_value() ? &*cities[no.v] : nullptr;
    }

    // ---- const queries (docs/sim/state.md §4.8). The World members of the same
    // names locate the island and delegate here; City has the same set. They
    // return the canonical order (D2). `layout` is the world's SimConfig::layout;
    // an Island holds no configuration, so the lookup and rect orders and the
    // list masks come in as an argument.

    /// The building with this id (the by_id cache), or nullptr.
    [[nodiscard]] const Building* find_building(BuildingId id) const;
    /// The building of city `no` on the tile (x, y) in the lists of `list_mask`,
    /// searched in layout.lookup_order (the first list with a building there
    /// wins); nullptr when there is none or the tile is outside the island.
    [[nodiscard]] const Building* building_at(const ListLayout& layout, CityNo no, std::int32_t x,
                                              std::int32_t y, std::uint32_t list_mask = ~0U) const;
    /// Clears `out`, then appends the ids of the buildings of city `no` in the
    /// masked lists whose footprint shares a tile with `rect`: ordered by (list
    /// position in layout.rect_order, y, x, id). The membership rule is the
    /// original's (D: 0x10023380 accepts a building iff its footprint overlaps the
    /// rectangle; the 8-row start bound of 0x100230d0 is not modelled, F3); the
    /// order is D2. Allocation-free except for growing `out`.
    void rect_query(const ListLayout& layout, CityNo no, const TileRect& rect,
                    std::uint32_t list_mask, std::vector<BuildingId>& out) const;
    /// The island's rectangle in world tiles.
    [[nodiscard]] TileRect rect() const {
        return TileRect{tiles.x0(), tiles.y0(), tiles.width(), tiles.height()};
    }
};

struct WorldSettings;
class TypeRegistry;

/// The rectangle of one island slot: all the world builder needs to decide
/// whether a set of islands fits the world before it allocates a single tile.
struct SlotRect {
    std::uint64_t slot = 0;
    std::int32_t x0 = 0, y0 = 0;
    std::uint32_t w = 0, h = 0;
};

/// The rectangle facts of an island table, on rectangles alone: every rectangle
/// is inside the world (WorldSettings::world_width x world_height; a rectangle
/// with a zero side counts as inside when its origin is), the rectangles cover no
/// more tiles than the world has, and no two share a tile. World::create runs it
/// on the scenario BEFORE any tile column is allocated (so the memory a scenario
/// can ask for is at most the world's area, docs/sim/state.md §10 step 1, F30);
/// validate_islands runs it again on the built or loaded table. Empty when the
/// facts hold, else one message "islands[<slot>].tiles: ..." or "islands: ...".
[[nodiscard]] std::string validate_island_rects(const std::vector<SlotRect>& rects,
                                                const WorldSettings& settings);

/// The structural facts of an island table that the scheduler and the loader
/// rely on (docs/sim/state.md §9.6; World::check_invariants calls this and adds
/// the facts that need the player table and the derived indexes): the table
/// has at most SimConfig::island_slots entries; per island the index is its
/// slot, the rectangle is non-empty, inside the world (WorldSettings::world_width
/// x world_height) and disjoint from every other island's (validate_island_rects),
/// the tile layer has SimConfig::chunk_edge and at most kIslandTileBound tiles,
/// the climate indexes SimConfig::monopoly_climate_mask, next_serial is at least
/// 1, every tile owner is the number of a present city of the island or equal to
/// SimConfig::neutral_owner (S6), the cities and their buildings are structurally
/// sound (validate_cities in island.cpp: the city table has SimConfig::city_slots
/// slots, every config-sized container of a city has its configured size, every
/// list has at least one element with its hint in -1 .. n and its cursor below n,
/// every building's type exists, its rotation is below 4, it sits in the list
/// its type belongs to, its id names this island with a serial below next_serial,
/// its payload matches its type, a mine's deposit index is inside the deposits),
/// the sweep positions are inside the island and multiples of chunk_edge (the
/// original starts them at 0 and advances them by the chunk edge), the ring
/// cursor is inside city_slots, the object manager cursor is inside the deposits
/// (0 without), every deposit is inside the rectangle with a goods name the
/// registry knows, reserved_object_count is 0 until the island objects are
/// ported (O-12), a drought record is a state its machine can leave behind (see
/// Drought) within the widths of the configuration, and the chunk digest cache is
/// consistent.
/// The terrain is not checked here (a state read from the stream alone holds a
/// placeholder that carries only the digest): terrain_matches() is the check the
/// builder and the save frame apply once the columns exist. Empty when the facts
/// hold, else one message that names the path ("islands[1].tiles: ...").
[[nodiscard]] std::string validate_islands(const std::vector<std::optional<Island>>& islands,
                                           const SimConfig& cfg, const WorldSettings& settings,
                                           const TypeRegistry& types);

} // namespace anno::sim
