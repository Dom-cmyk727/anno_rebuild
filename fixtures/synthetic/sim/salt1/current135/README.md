# MAP-EXTRACT-1 current135 fixtures

Identity 135 / ECONBK1 / PRNG timeline 5 / replay 4 / command wire 1 / save 1 (decision acbe5a24, X1 contract
approved at 0d52feee). Fields, field order, tag and draw sites are unchanged from 134: the stream moves
because the salt slice changes accepted-command outcomes (class-0x3000 placement and deposit binding, not
yet landed in this commit) and `BuildingType::highmap` enters the registry digest (conditionally, only for a
type that declares a table). All 133 and 134 native states and recordings are refused with their own
diagnostic; the files under `econbook1/current133` and `expand1/current134` and the extracted natives
`expand1/legacy133/g2.save` and `salt1/legacy134/g2.save` stay byte-identical history.

Every recording here was generated anew by the current C++ Recorder (tests run with ANNO_UPDATE_GOLDEN=1);
nothing was relabelled. The 41 goldens include the independent `tools/sim_stream.py` transcription of
G0/G1/S5/S6 (`sim_stream.py --check` agrees); the other session hashes are generated evidence supported by
the semantic and replay assertions of their tests, not an independent original-game oracle. `populated_fields.txt`
comes from `tools/integration_stream_ref.py --write` (header bytes `8700000045434f4e424b310005000000`).
The registry digest of a type with a table is pinned by `tools/highmap_digest_ref.py` (independent) in
`types_test.cpp`. `provenance.json` pins the sha256 of every file here and of the older files kept unchanged.
Generation is not acceptance; the focused canonical gate follows.

**Not run for this identity (stated, not implied):** the full simulation suite, a Release build, ASan/UBSan at 135,
Windows and Linux builds; only the focused Debug gate named in docs/sim/salt-extraction.md section 16 ran.
