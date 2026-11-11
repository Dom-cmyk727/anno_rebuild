# Combined state 138 fixtures

These are newly executed current fixtures for the state-138 integration of automatic freight
(136) with iron/tool, quarry/brick and salt extraction (137/135). They are not renamed copies.
`ANNO_UPDATE_GOLDEN=1` produced the replay sessions and scenario corpus on the combined binary;
`tools/sim_stream.py --check` and `tools/integration_stream_ref.py` are independent
transcriptions, not C++ hash generators.

The populated stream oracle covers a sailing freight ship, a mason goal, a quarry with claims,
and a deposit-bound mine with two distinct 256-unit reservations. `scenario_corpus.txt` has 50 of
50 changed `create_hash` fields; after replacing those fields and removing the version header,
every physical, warning and movement column is identical to the accepted state-137 corpus.

## Changed-golden table

SHA-256 is the value of the complete committed fixture. The old column names the accepted input
identity used for comparison; it remains in its historical directory.

| File | Old value | New value | Cause |
|---|---|---|---|
| `cargo_load.log` | route136 `9fec27373110213f287c366094071b8b0dab6d63ed0ec6e63850eca81918114c` | combined138 `d37215c69213ab419a99f7bf91b06c6ac513405f0feb34eb9ca6eb7e4dedef95` | Re-recorded route load under the 138 replay/native envelope and combined FreightRules identity. |
| `cargo_load_wrong.log` | route136 `806d1175d62824f9f1d02269791cd12545aaf6bcc23a8dd27c52f09a9efce355` | combined138 `07ecfc91e33bbdc932e9d87a66766c525aeb603ebe57b07c30879dae7b569e17` | Mutant was re-derived from the new recording; the replacement is checked by the divergence test. |
| `cargo_unload.log` | route136 `deebfc35efe613a43c6fc59c0a94099f6bfb404a1f6d1bf9926928e5a23cbd08` | combined138 `d376d5de9ad1c33c4ee85d58b914749fc2cc34e96a2858793a3ebbda19bd3ed3` | Re-recorded route unload under the 138 replay/native envelope and combined FreightRules identity. |
| `cargo_unload_wrong.log` | route136 `1697a2a81642c750dcac9b6bd113e725592ce2af19a727fe4a514ed24e255310` | combined138 `fc525604091e54707aa03159ba857e104fe4085af0ee68c0b6c92b466a783346` | Mutant was re-derived from the new recording; the replacement is checked by the divergence test. |
| `g2.log` | iron137 `6660b5694c411aae8b6916256fb66985d26048a6ec6dd90b6bde5ca648ff2184` | combined138 `98a150db252058a8a65b45de3894e5bdecf3efdaa63703671c003ef532831960` | State header, rules digest and combined quarry/mine/freight stream layout moved to 138. |
| `g3.log` | iron137 `5de0d9efdffddf6a5fa84429b0c4f81d9da32658978d78946ad10b431b49a909` | combined138 `94b4ba99a7214e189eb16efe7b5dce02e8e8f4aec7499655714c040098302a1e` | Newly executed production-class session on the combined layout. |
| `g4.log` | iron137 `d0eadd5786bc9fdd2972aca091c3f1113f7e74c5958106f339a4052cd071dd19` | combined138 `fc031ccfe31a197ae5ad886c42db0dcc664411f40de1d28fedd3d4dc995e9615` | Newly executed players/timers/events session on the combined layout. |
| `g5.log` | iron137 `2929e3257ae216fe08265e977777368a5c938f963187af575cf95389438a899e` | combined138 `dfc9baceb959adcd9085956c4ffe4a1b08a6f20c6ec63592de6a4e66aaea5f64` | Newly executed growth/drought session on the combined layout. |
| `g6.log` | iron137 `a5d3f866be003e9859682312ecefde9fcf6bdb74db8206653bcaf52537e85aec` | combined138 `f8ee7b94731bfe1a33ba01208d9271498d62e71e02fca4360583e7494041db65` | Newly executed transport session on the combined layout. |
| `goldens.txt` | iron137 `6233a3761d8c6536aed652f9f0b23f91d84c3e32431bc32d3f26097228125003` | combined138 `7bf1f30525f90ca75876e57cc869416d1cdd54fea35deb668735009d9d3f8a8d` | Independent G0/G1/S5/S6 roots and freshly executed semantic checkpoints use stream 138; event-only roots that did not change remain equal. |
| `loop_checkpoints.txt` | economy134 `c8b01dfd73dad7a4f009d16d6022651ca708901ac3e0d83393d46535e9d18c63` | combined138 `dfbe9d454d589b04ce9404ea0d5408aa3d4b52386ba8f07ac25bdaa7a97621ee` | The unchanged real Tutorial economic loop was re-recorded with the combined state identity. |
| `m2_found.log` | iron137 `7125b31ed7c25b9b3b5a5b2760a90c72841e2f9e6ae914cf8fbf91e0d9d0d232` | combined138 `e662e8790d55e35c997c00694b11d41035debbe086e38797d5caaa97e9112b14` | Founding session replay/native envelope and state roots moved to 138. |
| `m2_found_wrong.log` | iron137 `a572c7a6ba594ca0d2b8d6027d8918630f2a1bef74becbd1828e20af90c0b550` | combined138 `ef37eade318836a297ab19fced41beaabc39206ae995c2d7bd6abaa36ece4099` | Mutant was rebuilt from the 138 founding session and still proves divergence. |
| `populated_fields.txt` | iron137 `9fdd8b498558e015b8236118a1281f9471cab3d9f3513af7681a4aef823561e2` | combined138 `37937f0c06b237a9654ab2c89e3a4bd78f3cd902dd55befa6990dc1755cd8add` | Independent oracle expanded to the required combined populated ship/quarry/mine world. |
| `scenario_corpus.txt` | iron137 `191580d126f23befc76f1bafb7fbbdbbdd223ac6aea3c3f85445920f93d145d3` | combined138 `4462f95367ced3fbb9000ed209e6d0eafb5f4a0f1ee4ca9600d09d7db0a7e9d9` | All 50 creation hashes were regenerated at 138; all non-identity corpus columns are unchanged. |

`provenance.json` records the generation commands and complete new-file digests.
