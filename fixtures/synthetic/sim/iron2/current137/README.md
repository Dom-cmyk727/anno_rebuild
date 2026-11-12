# MAP-EXTRACT-2 current137 fixtures

Identity 137 / ECONBK1 / PRNG timeline 5 / replay 4 / command wire 1 / save 1 (MAP-EXTRACT-2 part T,
`docs/sim/iron-tools.md` T1.5). 136 belongs to the route lane and is not part of this identity. The tag, the PRNG timeline and the draw sites are unchanged from 135, and the field order is unchanged except for the two fields added inside 137 by part Q (the `Quarry` payload, tag 9, and `HarvestState::goal_col/goal_row` for job 0x2c; see `docs/sim/quarry-bricks.md` Q3.1 and the addendum below); the stream moves because accepted-command
outcomes change (iron mines place; a mine step's re-lookup binds an ore no mine has bound yet, T2). The
registry digest is unchanged. All 133, 134 and 135 native states and recordings are refused with their own
diagnostic; `econbook1/current133`, `expand1/current134`, `salt1/current135` and the extracted natives
`expand1/legacy133/g2.save`, `salt1/legacy134/g2.save`, `salt1/legacy135/g2.save` stay byte-identical history.

Every recording here was generated anew by the current C++ Recorder (tests run with ANNO_UPDATE_GOLDEN=1);
nothing was relabelled. The numeric content of the 135 fixtures is unchanged: only the identity-dependent
hashes moved (the non-hash assertions of every test pass unchanged). The goldens of G0/G1/S5/S6 agree with
the independent `tools/sim_stream.py --check`; the others are generated evidence. `populated_fields.txt` comes
from `tools/integration_stream_ref.py --write`. Generation is not acceptance.

## Q3 addendum (MAP-EXTRACT-2 part Q, 2026-11-08)

The quarry claim set (payload tag 9) and the mason worker's goal sub-cell are two further saved fields inside
the same identity 137 (`docs/sim/quarry-bricks.md` Q3.1). The class table and the registry digest are
unchanged, so only worlds with a quarry or a job-0x2c figure move: `scenario_corpus.txt` was regenerated once
(`ANNO_UPDATE_GOLDEN=1`, test `scenario corpus original-data components: every .sze loads, creates and runs five
minutes`); the goldens, logs and loop checkpoints of the other worlds are byte-identical.
