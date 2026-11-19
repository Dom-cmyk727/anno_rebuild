# WILDLIFE-PREP-PROVENANCE-1 evidence

2026-11-19. Base 4054d93aa42f777c49c9c4dd035bd3cdce23f64b; exact dispatch
b5e00e2a54723c33d2437121518750c6bbc88fce. [Contract and addressed ledger](../../sim/wildlife-prep-provenance.md).
Independent orchestrator review pending. Original listings/data remain private.

## Scope and receipts

- Read-only pefile 2024.8.26 + Capstone decoding of GOG AnnoWelt.dll;
  SHA256 f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2.
  No original binary execution. Full local handler dump is `local-full.private`;
  the earlier `Welt-10089130.private` filename now contains a shorter focused dump.
  Only aligned function/instruction addresses in the ledger support claims.
- Commands: `python3 -I cache/wildlife-prep-provenance-1/source_probe.py`
  redirected to `source-probe-final.log`. Preserved `source_probe_first.py`
  establishes the missing-Tundra-DUMMY selection path before payload reads.
  `source-probe-first.log` and `source-probe.log` are empty stdout files;
  the first runtime diagnostic and exit status are unavailable. No failure
  was rerun or recreated; the corrected fallback is explicit.
- `python3 -I cache/wildlife-prep-provenance-1/authored_probe.py`
  redirected to `authored-probe.log`:
  `PRIVATE_PROBES field_order_geometry=12 passed=12 failed=0 future_cpp=NOT_RUN original_execution=NOT_RUN`.
  These are field/order/arithmetic checks, not C++ acceptance gates or dynamic
  original callbacks. No COUNTS/VERDICT suite receipt is manufactured for them.
- Original sample: 24 files, 24 islands, 24 completed FRUCHTBAR, 0 absent,
  0 all-zero-completed. Complete HEIGHT/GFX 24; INFO count one per island.
  Five STAEDTE-bearing DUMMY files: each WARETYP-only, zero ENTRY.
- Tutorial raw header offsets 1927/80723 and payload offsets 1943/80739 identify
  INFO/FRUCHTBAR respectively; local header ordinals 0/49, total 73.
- Frozen preservation check: 12 predecessor source/doc files plus 2 binaries
  match their prior SHA receipts; 292 tracked synthetic working files frozen.
  Code/tools/fixtures diff against base empty; no fixture/pin/state/default move.
  No build or repeated C++ gate was run for this docs/private-probe packet.
- Exact-head GROUND-FACTS CI is separate orchestrator closeout; its pending state
  is not waived. New publication CI is read after push and reported directly.

## Original sample identities

Selection is fixed and bounded as described in the contract. Paths are relative
to the original source root, which remains read-only. Hashes identify whole files;
no original payloads, listings or tables are committed.

| Path | SHA256 |
|---|---|
| `game/app/Scenes/Tutorial/01Discover01.sze` | `225ed50b005743e00d8d3ffe18667d4565dc781ef45bd6d9b767fd2fe056c0f6` |
| `game/app/Scenes/Islands/Dschungel/D_064x064_0001_0B.scp` | `a9a3429a23c451c576f23941aabc62cab60b006bcf1d3f24e9681c696dd809a8` |
| `game/app/Scenes/Islands/Dschungel/D_064x064_0002_0B.scp` | `a25bd443c9b2617a817b25838899c6824f6339c378a06c2a864bcb13384a88a4` |
| `game/app/Scenes/Islands/Dschungel/D_064x064_0003_0B.scp` | `7f892faee563e8228b3d2c8ae76d6186d5871fcf1834702ed7a1f256f2ebd11e` |
| `game/app/Scenes/Islands/Dschungel/D_064x064_0004_0B.scp` | `0085e35a298f8aa6a7de02b081071556f54eb07e2ac562c55eb0ed470d4bca38` |
| `game/app/Scenes/Islands/Dschungel/D_064x064_0005_0B.scp` | `5bb42820b939c6578c91f47a0524d3a2879d26cccf3df61fd9ba5a761f534c2b` |
| `game/app/Scenes/Islands/Dschungel/D_064x064_0006_0B.scp` | `64b6951b203f040f01486267091c07df9c339aa1189ef1676b7aa016c307033e` |
| `game/app/Scenes/Islands/Dschungel/D_064x064_0007_0B.scp` | `ab5f0ffc3fe25a7980d59f01939503be1bc82cbe127c7e26182c821951d1883b` |
| `game/app/Scenes/Islands/Dschungel/D_064x064_0008_0B.scp` | `8ebda34c21e1a5382b3ce71ae74936aefb2a05f433ee5eb4d988264c6c5321cb` |
| `game/app/Scenes/Islands/Dschungel/D_064x064_0009_0B.scp` | `49aed4cd1fc578fb368b232c0d554263a469e17267bc5407a8b149b9c5a38a88` |
| `game/app/Scenes/Islands/Dschungel/D_064x064_0010_0B.scp` | `6fb163e13fb1a48f7cc415689193f6b328e56b12cc8719e1e4e71c343dc47908` |
| `game/app/Scenes/Islands/Dschungel/D_064x064_0011_0B.scp` | `85b50f35ec5c3fdb4e43a7ad7161cb6d2c62239d5b639494350780fc4cdb0607` |
| `game/app/Scenes/Islands/Dschungel/D_064x064_0012_0B_Suedsee.scp` | `4f7c6a4dbcbdaa220120105ede18491b60e33468eada3446b881137f5674b789` |
| `game/app/Scenes/Islands/Nord/N_064x064_0001_0B.scp` | `cd8e9a3a41a9d9bd69c72a893a76dfc04fb245c06c94a805af25f695b6bfdbc0` |
| `game/app/Scenes/Islands/Polar/Po_064x064_0001.scp` | `173b0533f15cad3455f9fcafcb762d9704b9a426ae6e8bb407058e02f60eb8b8` |
| `game/app/Scenes/Islands/Praerie/Pr_064x064_0001_0B.scp` | `5fe15b578f856180485ac70c05c2c3e348cf18a56cdb99e230e6d745145c74bc` |
| `game/app/Scenes/Islands/Steppe/S_064x064_0001_1B.scp` | `f617e95534c29499451da668ce00daf6bdc35401e896ba9974bf064ec40ef247` |
| `game/app/Scenes/Islands/Tundra/T_080x080_0001_0B_Po.scp` | `abb13b39ff4f212cea7ee4b41b958614e733ed40337c85c814a1a542deadf21f` |
| `game/app/Scenes/Islands/Dschungel/D_064x064_DUMMY.scp` | `ef1d322a58d842bcce82f576682b0f861ac28a88e47d8ab244bfede0079fca4f` |
| `game/app/Scenes/Islands/Nord/N_064x064_DUMMY.scp` | `f3a59387f8cbb28b9add1a5dd925f94df923684a8a882be39fbcbb8e1aaa06ea` |
| `game/app/Scenes/Islands/Polar/Po_064x064_DUMMY.scp` | `6936f4115df12c3c10cfcde43328b3286ed5e192cc419e6a0cd395b6ec91299e` |
| `game/app/Scenes/Islands/Praerie/Pr_064x064_DUMMY.scp` | `dc0c21eeb6961c216f095f9cd9f591b09991f69a10a08b4d97a7e34ea2fd1083` |
| `game/app/Scenes/Islands/Steppe/S_064x064_DUMMY.scp` | `da91b4f774092106ed069adc2a79589194de3986f54a35c5bd9eb61e82de2d60` |
| `game/app/Scenes/Islands/Tundra/T_080x080_0002_1B_Po.scp` | `b9500e08521fc1a445fded4d537d1e609e6997a6b910b99322a461aa628736e7` |

## Private artifact manifest

`cache/wildlife-prep-provenance-1/manifest.json` SHA256
`647fcdf59447db3d25c0f26bded80c4421bf9624bf6272f07bcfe6ce3a7534ba`.
It freezes the private research, scripts, actual parsed identities and authored
probe results. No later overwrite is authorized by this report.

| Private artifact | SHA256 |
|---|---|
| `cache/wildlife-prep-provenance-1/Welt-10084370.private` | `6833d80e0136c9870eca8babe594bf6e5d329c55947c8060aab32a1abab8b2ac` |
| `cache/wildlife-prep-provenance-1/Welt-10089130.private` | `01ed13d867d1b2e84343327c1d77dbfd2ce26c66510f7624f7f099b089a55064` |
| `cache/wildlife-prep-provenance-1/Welt-100894ca.private` | `76b416762c3bbc3cfa7e4b15209432a09053b52bfb2b7eea1979cee4d4947ee1` |
| `cache/wildlife-prep-provenance-1/Welt-10089790.private` | `787d64e777cfdb34de5ed8aba7c2b6adc1d0e3db74e9d06f5a6033c4e062abaf` |
| `cache/wildlife-prep-provenance-1/Welt-1008c020.private` | `da8eefe1521b0ef9ffdd1445e688e9cdd942a0914255f2f4de8508f644e42f35` |
| `cache/wildlife-prep-provenance-1/Welt-1008c0b0.private` | `0bfeb007b0e40c4a16bf9fdad65477073de13c816de64381f60787b35eed5c69` |
| `cache/wildlife-prep-provenance-1/Welt-1008c720.private` | `69ea93ca721a33686768db0ca2b448536654059344ade0d06794cde56337c4ef` |
| `cache/wildlife-prep-provenance-1/Welt-1008c745.private` | `6427a6716830aa444d4fa73913e7e7e2356ea9e25a1db6dd39b366e3a3dc522f` |
| `cache/wildlife-prep-provenance-1/Welt-1008cb00.private` | `c7825b1e57186c0817ba940d0bc1f1540324720f9b5545365b51c07bdb5322c2` |
| `cache/wildlife-prep-provenance-1/Welt-1008cb30.private` | `21a7822ee5f463dd3ba43d17f5b5f9e9985ab7e0a9a59e8606b430ae335d63c6` |
| `cache/wildlife-prep-provenance-1/Welt-1008f460.private` | `841893a467d6bceb57755dc4420264f220808ef0b771fab3d5aff3a30a8da259` |
| `cache/wildlife-prep-provenance-1/Welt-100946c0.private` | `11672c103d456713cc3356f8ae0f86ee2cf8a2893cd32c21caeb64af448c2487` |
| `cache/wildlife-prep-provenance-1/Welt-10094c60.private` | `b1105dab1b78d95ea3230f1957582cb917f84bfe0a291c6a0a2d1be5d1a8ff09` |
| `cache/wildlife-prep-provenance-1/Welt-10096da0.private` | `3638627598a86221d72c1f532bbb243cc62557fbc32c141a223c6debb1bdbadc` |
| `cache/wildlife-prep-provenance-1/Welt-100a1810.private` | `2f7d3319f41356e317eee63f7878b4e33d96de3910abd9bc764af4273a93e948` |
| `cache/wildlife-prep-provenance-1/Welt-100a19a0.private` | `81e0af3d617eefd263064299d0807fd64190c3b170aecc8d8e1bd22b5b29e854` |
| `cache/wildlife-prep-provenance-1/Welt-100a24d0.private` | `df16dc21d4a92dee7a42e74db59e5e8c849f829b53aae5361d20cc501e24734f` |
| `cache/wildlife-prep-provenance-1/Welt-100a2e90.private` | `c0c52944e5fa9afea70245e811b130f6061a3d56dbe19755dc10c2141712cb91` |
| `cache/wildlife-prep-provenance-1/Welt-100b4c40.private` | `c5e569fde7598865f44db8116062056258fdfb6bef17a147077993f27058fbad` |
| `cache/wildlife-prep-provenance-1/authored-probe.json` | `13775334b49c8c24d1494012a5ec3a86c69967ecf7dc8195164ceaf3586f0123` |
| `cache/wildlife-prep-provenance-1/authored-probe.log` | `29abea0dc808a148ea93c53808a4e7fc3496d17f20e716edbbd3ca3f04569101` |
| `cache/wildlife-prep-provenance-1/authored_probe.py` | `58d7b5c9cbe18dfef7c6717efbe67f7549709b97707edb6e32eb7582870075d7` |
| `cache/wildlife-prep-provenance-1/bounded-1002f990.private` | `196ac024e92ab5d7150a094ebea6ef196391717f8f694a46838fd08884bb33c8` |
| `cache/wildlife-prep-provenance-1/bounded-1008438f.private` | `33febd1bf0c89c6927f198a925a7cece5ee12602bac8aa6299c5fcef11deab51` |
| `cache/wildlife-prep-provenance-1/bounded-1008bdf0.private` | `5b0d2f8636adfab42c7dcbd00930442fa2ce6acd56df805ff9130a205dc9e0ae` |
| `cache/wildlife-prep-provenance-1/bounded-1008f2d0.private` | `0b51a82854c79b063c0cc154e2b44a22aa62009507cc6a6cdf7ed79bc8e14111` |
| `cache/wildlife-prep-provenance-1/bounded-1008f2f0.private` | `a4fae8fa2817f24bf21e043c21a8e23df75bbdd9813d5ec50ccaecca81bc53b3` |
| `cache/wildlife-prep-provenance-1/bounded-10096da0.private` | `0d83f70c6b48090485e78d005975e800014731420a513003918e4f16782158c8` |
| `cache/wildlife-prep-provenance-1/bounded-sample.json` | `a507b093ca20778113bc96928d555d14dd375f9477141fc3847605cc0f2c029d` |
| `cache/wildlife-prep-provenance-1/city-proof-1008bdf0.private` | `5b0d2f8636adfab42c7dcbd00930442fa2ce6acd56df805ff9130a205dc9e0ae` |
| `cache/wildlife-prep-provenance-1/city-proof-1008c370.private` | `01ba4719c80b6fe911b091a7c05124b64eeece964e09c058ef8f9805daca546b` |
| `cache/wildlife-prep-provenance-1/city-proof-100967e0.private` | `26ec02bc851dfb809a26e9333562a709ba93bc7c00bbd24deb2d486adf70ad1c` |
| `cache/wildlife-prep-provenance-1/city-tail.private` | `d9fe81334d6e1ddabcd6a9039ee4228840b071e894daf5ea122a615f33b8e933` |
| `cache/wildlife-prep-provenance-1/focused-10062820.private` | `9ba5332ae1bd658a51a1f52a31fc0245a8a6996ca99ca8fff00d0cbf07237d32` |
| `cache/wildlife-prep-provenance-1/focused-100861c0.private` | `9383bf36555524e197dec8e37d0c22334161f2da2b803a37489d653e768e931b` |
| `cache/wildlife-prep-provenance-1/focused-1008bdf0.private` | `5b0d2f8636adfab42c7dcbd00930442fa2ce6acd56df805ff9130a205dc9e0ae` |
| `cache/wildlife-prep-provenance-1/focused-1008c090.private` | `298922711218a287d834a1718904b46aca6e673699adc27e1d57682792c581b5` |
| `cache/wildlife-prep-provenance-1/focused-1008f360.private` | `2229cd609744c1fb9f5e54eb0af453ae0de2d30436d08d32ee1f4387eb66e667` |
| `cache/wildlife-prep-provenance-1/focused-10096a60.private` | `236fce280f52ebb7c5a43b31f1acf7a906f66837bb937513e1346dc4224ca002` |
| `cache/wildlife-prep-provenance-1/focused-100a1da0.private` | `de952de5c08aaea28b4b41f0695e930b90ddb6f8a4c3cba4a2471f24702da51a` |
| `cache/wildlife-prep-provenance-1/fresh-arm.private` | `5a88638844003fa0fb25a7f589aeeafb6028c1be08f724260b2512a951ce80a7` |
| `cache/wildlife-prep-provenance-1/full-1008c090.private` | `e0d2c99524da6709300705a646dccb74ce4198fd0d066910a7d60ae0e44a98ab` |
| `cache/wildlife-prep-provenance-1/full-100967b0.private` | `f5618d9e5c3f311e473c5ab6cbeb32628579f79e84d2ff0c122d2ce86491e8fd` |
| `cache/wildlife-prep-provenance-1/full-10096a60.private` | `2cd085e224d45d2e28181a0bc5029b74bf6b5a855a9819980f447719cbfaabbc` |
| `cache/wildlife-prep-provenance-1/full-100a19a0.private` | `7a0011676bf88a9a184b2c872b5065187eb07b1b4a8ae669f7de5ec2fb1d6320` |
| `cache/wildlife-prep-provenance-1/full-100aed70.private` | `65d9cdafeea6879b0b2ec5013f9d6139a92b56b30b8618a39f3b744e1a2201dd` |
| `cache/wildlife-prep-provenance-1/init-full.private` | `b7bc487386e82a5b78673e370f99ddc6bf3d31cd432bca30f8355fa9e53b6e96` |
| `cache/wildlife-prep-provenance-1/island-targets.json` | `ff207b1ecd2e4af0f035da2eb2f2f76e06e77b90292602320948b25a84d42216` |
| `cache/wildlife-prep-provenance-1/local-full.private` | `8e10844cbf3ed9b7019f94f2d6138a62bdf74332fe83de718da31e19bff463dc` |
| `cache/wildlife-prep-provenance-1/preservation.json` | `c6d87b4a5c5448ac70fce1b0b02b6df2de598f9001a3603be42dff23d9a1a175` |
| `cache/wildlife-prep-provenance-1/source-module.json` | `f9854ac763bca55443aafb7bda63f1ecb7dc5eaa03bf7ae80480801afe9de52e` |
| `cache/wildlife-prep-provenance-1/source-probe-final.log` | `990b279f1b4061631211531c68b5a59d125f3077b1a0fb2d08ee9006fd2edcbf` |
| `cache/wildlife-prep-provenance-1/source-probe-first.log` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `cache/wildlife-prep-provenance-1/source-probe.log` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `cache/wildlife-prep-provenance-1/source-sample.json` | `e88479463d2b9c32e2d44ca7f4690609890c80d1958b9f0f83c28c00bdd18024` |
| `cache/wildlife-prep-provenance-1/source_probe.py` | `435f501568db004d7e4ea6ead310be0cc857da5bb5b0468cdd9bcdc6e91c7e7e` |
| `cache/wildlife-prep-provenance-1/source_probe_first.py` | `a6e1f39a28fce53e9f158f84fc8be6e6d38d01776861a2b49f09fd2aff8e0c06` |
| `cache/wildlife-prep-provenance-1/staedte-probe.json` | `4e58ebf900026290a4873d48aa453fe44d638ced2a558e20e5f8483d9abf64a0` |
| `cache/wildlife-prep-provenance-1/tutorial-source-offsets.json` | `596101c34c95e230e3c9aa70d2822c7a2390225fbdea1d456c2b6005a37ca930` |

## Acceptance disposition

| Claim | Positive / wrong control | Actual proof | Disposition |
|---|---|---|---|
| INFO order and safe local lengths | 2 then 6 vs reversed; zero-length vs absent | addressed original stores and 5 private cases | review pending; C++ composition NOT_RUN |
| Completion not nonzero | four zeros vs absent vs short three | c2 store/wrapper plus private cases | review pending; original malformed I/O not certified |
| Full geometry | Tutorial aligned vs authored unaligned | factory edge arithmetic, 2 private cases | review pending; arbitrary geometry not admitted |
| Raw/effective distinction | raw 0x20, old 0xa7 ->0xa0 | nibble store plus hand check | review pending |
| Selected real completed path | 73 headers, source climate 2 and completed payload | raw parser offsets/hash, direct wrapper fork | evidenced proposal, no new C++ run |
| Empty city at absent prep | initial zero vs reachable STAEDTE ENTRY | constructor and insertion/removal chain | U after general admitted local callbacks; not an invented prerequisite for completed path |
| Ownership/refusal/full-output capability | stale slot/prefix/source/lifetime siblings | future contract only | NOT_RUN |

NOT_RUN: future C++ capability/composition, original/HE execution, World/native
integration, absent-ground shipped positive witness, full corpus, substituted
city getters/reused worlds, Windows navigation/raster, repeated C++/pins/heavy
campaigns. No claim of cross-platform completion or original draw-history parity.
