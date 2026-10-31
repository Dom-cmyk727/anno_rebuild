# Original endless starts and the five-player ship start

2026-10-31. Orchestrator investigation of the owner's GOG A.D. 2.0.0.5
files. Main checkout inspected at `67a48eca9da5028091caef700916815e303cf4e1`.
This is an evidence report, not a release of ships, combat or map integration.

## 1. Conclusion and current owner direction

**Starting aboard a ship and choosing the first settlement matches the eight
standard endless presets.** It is not an invented multiplayer start. All eight
give every competing party one small trading ship and no city. The human also
has a scout and its donkey aboard. The beginner scenario is a deliberate
exception with a prebuilt Kontor; the three expansion scenarios have their own
conditions.

Owner decisions in this conversation, 2026-10-31:

- Players start with ships and race to establish their first settlement.
- Natural differences between islands, available building space and resource
  opportunities are part of the game. Exact equality of land or distances is
  not the acceptance goal.
- Large islands should permit multiple players' settlements and the resulting
  cooperation, competition, land borders and military opportunities.
- **Sharing must remain optional. Players must have viable opportunities to
  settle separate islands.** Do not solve density or map balance by forcing
  two players onto one island.

These decisions supersede the earlier five assigned northern Kontors and the
strict per-seat ±5% land/distance interpretation. They do not select candidate
A, B or C, set starting gold, choose clustered versus dispersed ship positions,
or release new simulation implementation. Older PLAN/MVP/MAP text needs an
explicit reconciliation at the next specification checkpoint.

## 2. Scope and strength of evidence

Read and independently parsed:

- All nine `.sze` files in `game/app/Scenes/0010SingleEndlos/` and all three in
  `Scenes/0011SingleEndlosAddon/`, including both player series, ships, crew,
  cargo, cities, island rectangles and scenario scripts.
- All twelve associated description pages, matched to the files rather than
  relying on their sometimes stale page titles.
- `FigurenHeader.dat` metadata for ship/scout capacity, cargo slots and goods
  classes; `Anlagen.dat` metadata for the starting Kontor.
- Original help about founding, warehouses, service areas, scouts, land trade
  and the suitability of different climates for a first settlement.
- Original `AnnoWelt.dll` disassembly for unit position loading and the cargo
  reader/writer; a focused reinspection of `AnnoFrame.dll`'s start function.
  Island randomization uses the existing detailed disassembly findings in
  `docs/re/annoscene.md` §3–§5; that whole routine was not re-proven here.

The reproducible audit is
`cache/orch-original-starts-2026-10-31/audit_endless_starts.py`. Run it with
`python3` from any directory. It uses the existing `tools/sze.py`, `insel.py`
and `figuren.py`, requires no additional packages, and writes only to cache.
`start-conditions.json` contains derived values and source hashes;
`checks.txt` and `run.log` contain the result. Focused disassembly is saved in
`cargo-load.asm`, `cargo-save.asm`, `unit-load.asm` and `frame-start.asm`.

Checks passed over all twelve scenarios: 51 ships have matching enclosing
owners and positions inside the world; 73 ship/crew cargo records match their
figure type's hold count; all 263 cargo slots fit the type capacity; island
numbers are unique and their rectangles are in bounds and do not overlap.
All twelve human players have one ship with one or two scouts and matching
donkey records. The nine base scripts and `endless03` have empty script kernels.
These are file/disassembly checks, not an original-game runtime capture.

No game assets were changed. No engine or map implementation was changed.
The original Windows executable was not run on this Mac.

## 3. Human starting conditions, all twelve scenarios

Cargo values are **tons aboard the ship**, summed across holds. The scout's
own two cargo slots are empty in every human start, so they add no supplies.
Money is the player-header amount, not a qualitative description-page label.

| Scenario | Gold | Human city at load | Ship | Scouts | Ship cargo: wood / tools / food / other |
|---|---:|---:|---|---:|---|
| `00_1024_easyentry_01` | 500,000 | 1 | large trading ship | 1 | 50 / 100 / 50 / — |
| `01_1024_easy_256` | 50,000 | 0 | small trading ship | 1 | 50 / 100 / 10 / — |
| `02_1024_easy_128` | 45,000 | 0 | small trading ship | 1 | 50 / 80 / 10 / — |
| `03_1024_med_256` | 30,000 | 0 | small trading ship | 1 | 50 / 80 / 10 / — |
| `04_1024_med_128` | 30,000 | 0 | small trading ship | 1 | 50 / 80 / 10 / — |
| `05_800_Med_128` | 30,000 | 0 | small trading ship | 1 | 50 / 80 / 10 / — |
| `06_1024_heavy_256` | 25,000 | 0 | small trading ship | 1 | 50 / 80 / 10 / — |
| `07_1024_heavy_128` | 20,000 | 0 | small trading ship | 1 | 50 / 80 / 10 / — |
| `08_800_heavy_128` | 20,000 | 0 | small trading ship | 1 | 50 / 80 / 10 / — |
| addon `endless01` | 10,000,000 | 0 | large trading ship | 2 | 100 / 150 / 100 / 50 bricks |
| addon `endless02` | 300,000 | 0 | large trading ship | 2 | 100 / 100 / 100 / — |
| addon `endless03` | 23,000 | 0 | small trading ship | 1 | 32 / 75 / 0 / 15 spices |

Identifiers and data:

- Small trading ship: figure **1502**, `WARE=HANDELSCHIFF1`, four cargo
  holds of 50 t each and two crew slots.
- Large trading ship: figure **1503**, eight cargo holds of 50 t each and
  three crew slots; the existing figure/ship specifications identify its
  trading-ship class.
- Scout: figure **717**, `BEWOHNTYPE=ERKUNDER`, two cargo slots of 10 t.
  Its nested `PRIVAT/ESEL` record identifies figure **718**, the donkey.
- The common 80 t tools load occupies **two holds, 50 + 30**, not one
  over-capacity hold. The ordinary load is four occupied holds: wood 50,
  food 10, tools 50, tools 30.

### The beginner exception

`00_1024_easyentry_01` contains three competing parties, each with one city
and one Kontor, plus a ship. The human's city holds an additional 50 t each
of food, tools and wood. Each of these three cities has only one building
entry. The embedded island `PROD2` objects independently identify that building
as **1302**, `PRODTYPE=KONTOR`.

Thus the beginner preset is a settlement head start, not evidence that the
standard endless start grants a home island. Nor does it justify the rebuild's
developer rule that assigns every tile of the island to that city.

### Expansion scenarios are separate rule packages

- `endless01` is a generous building scenario: ten million gold, a full
  large ship, two scouts, five native parties and no competing type-12 AI.
  Its description promises broad research/building availability. The player
  has 38 `FORSCH/ENTRY` records, and the script explicitly enables construction
  draw states. Menu visibility and research state remain distinct mechanisms.
- `endless02` starts the human at sea but gives its one competing AI an
  established city with **166 building entries**, 36 separately stored soldiers
  and a ship. Its description emphasizes continents and very large cities.
- `endless03` is a constrained-building-space challenge. The human's unusual
  supplies include spices and no food. Four AI opponents have 50,000 gold each,
  versus the human's 23,000, and start with ships and no cities.

The developer resources borrowed from the expansion files are valid developer
test settings; they are not the common expedition load.

## 4. Newly verified cargo interpretation

The earlier `docs/formats/endless-presets.md` explicitly left ship `WAREN`
unread. The amounts above are new findings, not a repetition of that table.

Original module: `AnnoWelt.dll`, image base `0x10000000`.

- Ship vtable `0x100c4da4`, slot `+0x6c`, points to reader `0x10031870`.
- At `0x10031902` it matches `WAREN` (string `0x100dce08`).
- `0x1003192b` divides payload length by eight. Each iteration reads eight
  bytes and obtains the indexed hold through unit slot `+0x84`.
- `0x10031971`–`0x10031981` take the **low byte at entry +0** as the file
  goods number and map it through the load context. It is not necessary to
  assume a u16 goods identifier merely because byte +1 is zero in these files.
- `0x10031997`–`0x100319a9` read the u16 at **entry +2** and shift it left
  eight, converting whole tons into the internal 8.8 amount.
- `0x100319ac`–`0x100319c9` clamp to the figure type's per-hold capacity,
  then set the amount. Unmapped goods or a missing hold are skipped.
- Writer `0x10031ba2`–`0x10031c78` obtains the hold count from the type,
  allocates eight bytes per hold, writes the goods byte and divides the
  internal quantity by 256 into the u16 at +2.

The goods names are resolved from each scenario's own `WARETYPE` table:
26 food, 31 spices, 41 tools, 42 wood, 43 bricks. Entry byte +1 and bytes +4..7
are zero in the audited records and are not used for these amounts.

The player money field is `SPIELER/HEAD+4`: the original player loader copies
it to object `+0x74` (`docs/re/annoscene.md` §6), and the money getter reads
that field (`AnnoWelt 0x1001cfc0`, `docs/re/players.md`).

## 5. Geography, scarcity and starting positions

The base endless worlds combine **authored island positions, sizes and
climates** with randomized island templates, resources, ores and treasure.
They are not freshly generated, equalized archipelagos. `INSELLNK` records
name dummy templates; `AnnoScene 0x10009e0f` and `0x10009550` choose compatible
real islands from the seed-driven pool. Expansion endless scenarios instead
contain embedded islands, with no `INSELLNK` records.

Complete counts below include both linked and embedded islands. Percentages
measure **bounding rectangles**, including water inside those rectangles;
they are not buildable land or actual coastline area.

| Scenario | World | Links + embedded | Rectangle share |
|---|---|---:|---:|
| `00` | 1024² | 16 + 4 = 20 | 39.38% |
| `01` | 1024² | 27 + 1 = 28 | 46.17% |
| `02` | 1024² | 28 + 1 = 29 | 29.64% |
| `03` | 1024² | 27 + 0 | 45.56% |
| `04` | 1024² | 28 + 0 | 29.03% |
| `05` | 800² | 23 + 0 | 37.88% |
| `06` | 1024² | 27 + 0 | 45.56% |
| `07` | 1024² | 28 + 0 | 29.03% |
| `08` | 800² | 25 + 0 | 28.36% |
| addon `01` | 800² | 0 + 23 | 58.64% |
| addon `02` | 1024² | 0 + 30 | 71.56% |
| addon `03` | 800² | 0 + 12 | 39.12% |

The previous links-only range of 16–28 must not be reused as total island
counts, particularly when calibrating our map's density against the beginner
scenario.

**A useful scarcity example is `03_1024_med_256`:** five competing parties,
but only four 256×256 island rectangles—two temperate and two jungle. The
other 23 islands are smaller. This supports a contest for especially large
islands; it does not imply that a player must share an island. There are many
other islands, and the original help recommends both temperate and jungle
climates for the first settlement.

Ship starts are explicit coordinates in each party's `SCHIFF` record. The
reader creates the ship from those coordinates plus the load origin
(`AnnoWelt 0x1001df97`–`0x1001e013`). For `03`, the competing positions are:

| Party | Ship position (world tiles) |
|---|---|
| Human 0 | (450.5, 422.5) |
| AI 1 | (344.5, 254.5) |
| AI 2 | (605.5, 434.5) |
| AI 3 | (692.5, 534.5) |
| AI 4 | (556.5, 550.5) |

Their pairwise straight-line distances range from 125.9 to 446.7 tiles.
Other presets differ substantially: `01` has a closest competing pair about
39 tiles apart; `04`'s closest pair is about 236 tiles apart. These are stored
coordinate measurements, **not sailing route lengths, coast access times or
proof of equal opportunity**.

The empty base script kernels contain no later scripted start-position
randomizer or settlement grant. The single-player part of `AnnoFrame
0x10029100` selects the first human slot and assigns identity/name/colour
(`0x10029379`–`0x10029465`). We found no evidence here for an option promising
equal-distance spawns. Complete runtime observation of startup and every
indirect call remains outside this investigation.

**Ship start and spawn distribution are separate choices.** The owner's
choice of a ship start does not itself choose a common starting fleet,
dispersed starts, or randomized seat-to-position assignment.

## 6. Founding and multiple settlements on one island

The shipped help provides particularly direct evidence:

- `help/IH_Kontorfehlt_wie.htm`: sail close to a coast, then place a warehouse
  near the ship when construction becomes available.
- `help/Scout.htm`: the embarked scout explores resources and can establish
  a main market building inland.
- `help/IH_Insel_Falsch_{Steppe,Tundra,Praerie}.htm`: these climates have
  limited agriculture for a first settlement; temperate **or jungle** is
  recommended. There is no recommendation here to reserve northern islands
  for every player.
- `help/Gesamteinzugsbereich.htm`: the city's building territory is the
  combined service area of its warehouses and main markets. It expands by
  adding those buildings.
- `help/H_Landweg.htm`: land trade between players requires both to have
  settled on the same island. This explicitly establishes that coexistence is
  supported.
- `help/Kontor_uebernehmen.htm`: taking an enemy warehouse gives the buildings
  in its service area, not an automatic transfer of the whole island.

The basic warehouse's costs agree between the help and `Anlagen.dat` entry
1302: **350 gold, 12 t wood, 5 t tools**, with a 4×4 footprint. This verifies
the type data, not every condition and atomic operation in the original
ship-to-city founding command.

For our map, first arrival should establish a foothold and advantage. It
must not silently become an exclusive whole-island reservation. Equally,
large islands that permit shared settlement must not be an excuse for too few
separate viable settlement opportunities. The owner explicitly wants both
choices available. Combat is part of that long-term design; this report does
not claim that combat or ship-based founding is already implemented.

## 7. What transfers to our multiplayer design

| Topic | Original evidence | Implication for our game |
|---|---|---|
| Initial settlement | None for ordinary competitors | Start at sea; establish the first city through gameplay |
| Equipment | Small trading ship, human scout and donkey, finite cargo | Include this complete expedition, not an empty decorative ship |
| Gold | Varies by preset; AI may have different gold | Pick a named baseline; equal human equipment is a multiplayer design choice |
| Island quality | Mixed climates, sizes, templates and resources | Preserve meaningful differences and scarce attractive locations |
| Climate | Temperate or jungle recommended | Avoid compulsory northern home-island assignment |
| Ownership | City service areas; multiple cities/players per island | Preserve local territory and the possibility of rival footholds |
| Sharing | Supported by the rules | Optional, with sufficient separate viable settlement opportunities |
| Expansion | Open settlement opportunities | No preassigned southern island or per-seat climate entitlement |
| Spawns | Authored ship positions, varied spacing | Define navigable starting positions separately from ownership |

The inspected build's `Scenes/0500MultiEndlos` and `Scenes/0700Multi`
directories exist but contain no files. The binary has network setup paths,
but those do not provide a shipped, tested multiplayer endless ruleset to
copy. Distinguish **reproducing single-player endless mechanics** from
**designing their deterministic multiplayer adaptation**.

For a concrete baseline, `03_1024_med_256` is a strong reference: an ordinary
large-island medium preset with five competing parties. A proposed human
starting package is its 30,000 gold, one small trading ship, one scout/donkey,
50 t wood, 80 t tools and 10 t food. This is a recommendation, not a new owner
decision. Original resource, difficulty and disaster settings should remain
separate named preset data rather than being inferred from this loadout.

## 8. Current implementation gap and next checkpoint

At the inspected main revision, `src/sim_load/include/anno/sim_load/fixed_map.hpp`
defaults to `developer_start=true`. The loader creates a free Kontor and an
owner run over the entire starting island. Turning that boolean off omits
that grant; **it does not create starting ships**. The simulation scenario
adapter in `src/sim/src/scenario_formats.cpp` imports cities from state-series
player records and warns on other record types, including ships. Existing
asset/viewer ship rendering is not simulated sailing, cargo or founding.

Before implementation continues in this area:

1. Reconcile the map/start specification with the four owner decisions in §1.
   Keep developer starts explicitly separate. Remove compulsory island
   ownership, north/south pairing and ±5% equality as gameplay acceptance rules.
2. Define a small start manifest: ships, cargo holds, crew, money, navigable
   spawn positions, player identities and seed/version. Choose the baseline
   preset and spawn policy explicitly; use the extracted data as the oracle.
3. Assess candidate maps for legal coast access, usable city space, resource
   opportunities, navigable routes and enough separate viable settlements.
   Measure unequal opportunities as diagnostics. Retain the possibility of
   shared large islands without requiring it.
4. Trace the original founding transaction before implementing it: legal
   coastline/ship distance, territorial conflicts, costs and cargo source,
   ownership expansion, cancellation and simultaneous attempts. This is the
   existing W6.1 scope, not a side effect of disabling developer mode.

Still unproven here: exact initial fog-of-war/reveal behaviour; the complete
startup effect of all indirect calls; full original founding predicates and
transaction ordering; and runtime confirmation of the parsed values after
the first tick. In particular, do not infer fog-of-war from the player `DISCO`
word alone. Those require focused RE or captures on an original-game host,
not repeated expensive renderer suites.
