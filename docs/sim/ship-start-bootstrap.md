# Unchanged ship-start bootstrap audit — MAP-BOOTSTRAP-1

This is a construction/dependency audit at accepted basea50aec3a plus the local
availability-facts candidate. It is not a complete settlement demonstration.
Authoritative availability enforcement is BLOCKED by the owner's explicit
scenario-permission decision; see build-availability.md. MAP-SUPPLY-1 and
ECON-BOOK-1 remain separately owned, unaccepted dependencies of this candidate.

## Reproduction and source

`tools/bootstrap_budget.py --game-dir /absolute/game/app --check
fixtures/synthetic/sim/bootstrap_budget.json --out /ignored/ledger.json` decodes
only original chunk records and HEAD costs. `--self-test` exercises the public
reader/arithmetic contract and a wrong-offset negative control. CTest names are
bootstrap_budget_contract and bootstrap_budget_original. No original bytes are
written to fixtures. JSON fixture costs are independently transcribed from the
read-only budget review, not generated from simulator registry values.

Anlagen SHA256b2317a5c24b8c9fe796cf40f3fedaefcfa30ad3f185df9e69b2dca6a03d8dd75.
Baseline03_1024_med_256.sze SHA25622a6ccbe68b1e21f8610f494866cb1affd693d7e833106ab73de586e80967719.
Human HEAD+4=30000;HEAD48=1;HEAD5c=0. Ship1502 WAREN holds, in order:
wood42/50t,food26/10t,tools41/50t,tools41/30t. Runtime holds use raw1/256t:
12800,2560,12800,7680. Two tools holds stay distinct.

The unchanged ship-start loader creates five human seats with30000gold and
mask1, no cities or warehouse stock. Empty diplomacy rows give peace/no
alliances; visibility is shared. Island sharing remains optional. No map
selection/installation, relocation of the accepted world, resource grant or
stage injection is part of the accepted prefix. Resource differences between
islands remain intentional.

## Construction bill and causal dependencies

Amounts below are whole tons; all listed rows have zero marble. Ordinary land
buildings still need an owned, clear, supported level footprint, including
rotation and overlap checks. Availability does not waive those checks.
No universal climate guarantee is inferred: crops, grass, trees, wildlife and
mineral sources must exist/reach the building under their own contracts. This
package does not prove every climate band can supply every chain.

| Type / original behavior | Stage | Gold | Wood | Tools | Bricks | Input/source and prior link; current boundary |
|---|---|---:|---:|---:|---:|---|
|1302 KONTOR|EVER|350|12|5|0|Supported coastal founding geometry, nearby owned ship pays from holds; creates city/store|
|2520 STRASSE dirt road|EVER|5/tile|0|0|0|Supported owned land path; connects producers/warehouse; no building serial|
|801 PLANTAGE forester|EVER|150|0|2|0|Existing cuttable BAUM in worker area; supplies wood without spending starting wood; accepted earlier harvest/delivery scope retained|
|809 UNIVERSAL small farm|EVER|250|4|2|0|KARTOFFEL+GRAS -> ALKOHOL+NAHRUNG; worker/animal harvesting/delivery incomplete at accepted base, MAP-SUPPLY-1 owns it|
|807 SCHAFFARM|EVER|220|4|2|0|GRAS -> WOLLE; requires sheep WEIDETIER job0x2e body not present at base|
|1001 HANDWERK weaver|EVER|300|4|3|0|807 wool -> STOFFE; generic workshop exists, player-start wool supply not demonstrated|
|805 JAGDHAUS|EVER|140|3|1|0|FLEISCHTIER wildlife -> NAHRUNG+TIERHAEUTE; Hunter TickKindNone/Inert at base; resource renewal not assumed|
|1023 HANDWERK tannery|EVER|300|5|2|0|805 hides -> LEDER; workshop cannot manufacture its missing hides|
|1310 MARKTSTAND|EVER|50|1|1|0|Warehouse food/salt -> residents; initial food finite, initial salt0|
|1314 MARKTSTAND|EVER|50|1|1|0|Warehouse cloth/leather -> residents; both initial stocks0|
|1335 WIRT|1B|500|9|5|0|809 alcohol -> tavern service; Inn carrier/stock exists, new alcohol supply separately dependent|
|1320 KAPELLE|1C|700|10|6|0|Infrastructure service; no material producer; placement/reach/supplier checks still apply|
|1203 MINE|1C|700|15|5|0|Matching local SALZSTEIN source/mountain placement; flags0x3096 explicitly Unsupported through normal placement|
|1022 HANDWERK saltworks|1C|400|5|5|0|1203 saltstone -> SALZ ->1310; upstream legal extraction/delivery missing|
|501 PIONIER house|EVER|0|3|0|0|Residence and need/service delivery,8 resident cap; House updates explicitly enabled only in progression tests|
|520 settler base upgrade|EVER|0|4|1|0|Automatic transition after population predicate, not a shortcut player-build proof; costs per upgraded house|
|1301 MARKT optional extension|EVER|250|7|3|0|Extra capacity/reach/logistics if layout needs it; excluded from narrow bill|
|1201 STEINBRUCH|2B|300|3|3|0|Stone source and attachment; quarry stub is not proved active production; mineral placement incomplete|
|804 STEINMETZ|2B|250|3|3|0|Stone-worker/source interaction must be established before claiming ZIEGEL replenishment|
|1205 MINE|2C|1200|15|5|0|Matching iron deposit and legal mountain placement; cannot bootstrap tools before unlock|
|1017 early smelter|2C|800|4|10|10|1205 EISENERZ +801 HOLZ -> EISEN|
|1019 toolmaker|2C|500|4|3|8|1017 EISEN +801 HOLZ -> WERKZEUG; later than initial supply budget|

Cotton819 requires3A; it is not an interchangeable early sheep substitute.
Charcoal1035/1034 require3D/3F; the early smelting bill uses wood instead.
Stone road2500 requires2B and costs10gold+1brick/tile; dirt roads avoid that
unavailable material in the opening audit.

## Finite supplies and possible cycles

One of each chain row1302,801,809,807,1001,805,1023,1310,1314,1335,1320,1203,1022
costs4110gold,73wood,40tools. It is an explicit audit set, not an optimized layout.
Original BAUSTUFEN:1B needs80 pioneers;1C125. At cap8,16 full pioneer houses are
a capacity lower bound for1C before the first upgrade. Add16 houses and one
upgrade:4110gold,125wood,41tools,plus5gold per dirt-road tile. Any necessary
market extension adds250gold,7wood,3tools.

Against starting30000gold/50wood/80tools this leaves25890gold-5R and39tools,
but needs75t newly supplied wood. The forester costs zero wood: no intrinsic
wood-construction cycle follows from this bill. No timing/sufficiency claim
follows either: startingfood10t is finite; initial alcohol/cloth/leather/salt
are zero; consumption and deliveries must be reconciled in an integrated run.
The upgrade formula combines satisfaction values and forced infrastructure;
this list does not invent an independent strict minimum for every consumer good.

Tools remain finite even though80t covers this narrow bill. Iron mine/smelter/
toolmaker alone cost2500gold,23wood,18tools,18bricks at2C, before quarry,
stoneworker and logistics. Constructing them early by ignoring INFRAMIN does
not solve the bootstrap. Income/upkeep and imports are not assumed to close a
shortfall; ECON-BOOK and later resupply integration must establish them.

If a needed source is on another island, the dependency is a second warehouse,
manual cargo voyage and its costs; it is not permission to rebalance the island.

## Executed prefix and remaining limits

Fresh prefix at the frozen working source recorded in cache/bootstrap/B4/prefix-02.log:
prefix case PASS,17238assertions,192.30seconds. Its enclosing gate is FAIL:
9selected,8executed passes,1failed (the separate synthetic unlock/build fixture
had unsupported placement flags),0skipped/empty/incomplete. That failure is
preserved, not erased by this prefix's success. Exact recorded prefix:25dirt
roads,29125gold,8704wood units=34t,11776citytools=46t,6400shiptools=25t,
2560food=10t. State hash13756429881164965641. A newly constructed small farm's
raw/output vectors are0,0; this is a creation observation, not dynamic proof of
inertness. Missing worker/animal bodies are established separately from the
accepted source's callers. The salt command returned Unsupported without any
save-byte change; its pure availability query separately returned StageLocked.
In-memory replay and save/reload matched. Encoded exported-log replay is a
separate check below; the original prefix test replays rec.log() in memory. The new test starts from the unchanged ship
manifest, sails/founds/unloads, constructs a forester and connecting dirt roads,
then a small farm. Planning copies may search placements; recorded execution
never teleports, grants resources or edits stage flags. It separately records
salt content-query StageLocked and the existing placement Unsupported refusal.
The budget after construction is not a claim the small farm has produced goods.

Retained accepted forestry evidence at its original pin demonstrates448rawwood
units=1.75t newly delivered, with replay checkpoints. It proves neither food/
alcohol nor cloth/leather/salt production. The fresh prefix does not repeat that
long harvesting measurement merely to compute costs.

Next concrete chain recommendation: cloth807 ->1001, after the relevant grazing
work is accepted and independently matched to the sheep contract. Leather needs
a separate hunter/wildlife lifecycle; salt placement/extraction is independently
needed at1C. A complete ship-start settler town awaits accepted supply and economy
packages plus these missing chains. This package does not start the next bundle.

The upgrade bill uses original base520 (`501 - 501%20 +20`); type522 is one
possible settler variant and happens to have the same material costs. The budget
review corrected the attribution without changing any totals.
