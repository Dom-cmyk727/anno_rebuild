# MAP-EXTRACT-2 current137 fixtures

Identity 137 / ECONBK1 / PRNG timeline 5 / replay 4 / command wire 1 / save 1 (MAP-EXTRACT-2 part T,
`docs/sim/iron-tools.md` T1.5). 136 belongs to the route lane and is not part of this identity. Fields,
field order, the tag and the draw sites are unchanged from 135; the stream moves because accepted-command
outcomes change (iron mines place; a mine step's re-lookup binds an ore no mine has bound yet, T2). The
registry digest is unchanged. All 133, 134 and 135 native states and recordings are refused with their own
diagnostic; `econbook1/current133`, `expand1/current134`, `salt1/current135` and the extracted natives
`expand1/legacy133/g2.save`, `salt1/legacy134/g2.save`, `salt1/legacy135/g2.save` stay byte-identical history.

Every recording here was generated anew by the current C++ Recorder (tests run with ANNO_UPDATE_GOLDEN=1);
nothing was relabelled. The numeric content of the 135 fixtures is unchanged: only the identity-dependent
hashes moved (the non-hash assertions of every test pass unchanged). The goldens of G0/G1/S5/S6 agree with
the independent `tools/sim_stream.py --check`; the others are generated evidence. `populated_fields.txt` comes
from `tools/integration_stream_ref.py --write`. Generation is not acceptance.
