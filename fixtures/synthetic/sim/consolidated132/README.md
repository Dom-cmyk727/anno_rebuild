# Combined stream 132 fixtures

Current fixtures for main’s composition of Windows/app work, UNIT/SCRIPT/SUPPLY
integration at cafc1a7c and bounded NEEDS N2 at 8ff6d1ca. State 132, SIMINT1
schema, PRNG timeline 5, replay frame 4. Current generated recordings live here;
the exact stream131 fixtures in ../integrate1 remain historical evidence.

`goldens.txt` has independently transcribed synthetic scopes from
`tools/sim_stream.py` plus explicitly generated scenario/replay checkpoints.
`populated_fields.txt` comes from `tools/integration_stream_ref.py`.
`scenario_corpus.txt` and recordings are generated C++ evidence, not independent
oracles. Their non-hash ledgers and outcomes remain asserted in the engine tests.
Both independent stream130 lineages and the combined131 log/native save are
retained under ../integrate1/legacy and indexed in ../integrate1/predecessors.json.
