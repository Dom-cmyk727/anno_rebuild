# PINS-1 literal inventory (2026-11-13)

Base `f9e6ea0c`: exact lowercase 16-hex scan finds103 occurrences in24 files
(the packet’s126/30 count describes an earlier inventory). Lines below are **base lines**.
25 active hash occurrences moved into22 unique names in `sim/current/goldens.txt`;
2 obsolete hash comments removed;76 fixed inputs/format or primitive-algorithm constants kept.
No moving state/registry/replay hash remains literal. Existing named pins stay in the same file.
Seeds and FNV primitive constants deliberately stay independent of regenerated sim expectations.

| Base source | Literal | Disposition | Reason |
|---|---|---|---|
| src/sim/tests/archive_test.cpp:147 | `0x0102030405060708` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/archive_test.cpp:259 | `0x8000000000000000` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/archive_test.cpp:376 | `0x0101010101010101` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/archive_test.cpp:388 | `0x8000000000000000` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/archive_test.cpp:750 | `0x0102030405060708` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/archive_test.cpp:784 | `0xcbf29ce484222325` | kept | FNV algorithm basis/primitive known-answer constant, independent of state layout. |
| src/sim/tests/archive_test.cpp:785 | `0xaf63dc4c8601ec8c` | kept | FNV algorithm basis/primitive known-answer constant, independent of state layout. |
| src/sim/tests/archive_test.cpp:786 | `0x85944171f73967e8` | kept | FNV algorithm basis/primitive known-answer constant, independent of state layout. |
| src/sim/tests/archive_test.cpp:790 | `0x0807060504030201` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/archive_test.cpp:797 | `0x0807060504030201` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/build_terrain_test.cpp:798 | `0x2545f4914f6cdd1d` | kept | Test-generator seed/algorithm input, not a measured simulation hash. |
| src/sim/tests/build_terrain_test.cpp:1324 | `0x9e3779b97f4a7c15` | kept | Test-generator seed/algorithm input, not a measured simulation hash. |
| src/sim/tests/build_terrain_test.cpp:1575 | `0x51ed270b0123456f` | kept | Test-generator seed/algorithm input, not a measured simulation hash. |
| src/sim/tests/build_terrain_test.cpp:1743 | `0x2f9be6cc44d1a7e3` | kept | Test-generator seed/algorithm input, not a measured simulation hash. |
| src/sim/tests/building_state_test.cpp:57 | `0x0102030405060708` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/building_state_test.cpp:72 | `0x0102030405060708` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/building_state_test.cpp:314 | `0xffffffffffffffff` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/building_state_test.cpp:1158 | `0x77d2dd9ed3f484b1` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/command_codec_test.cpp:53 | `0x0000000700000009` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/command_codec_test.cpp:54 | `0x0000000200000005` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/command_codec_test.cpp:57 | `0x0000000300000011` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/command_codec_test.cpp:172 | `0x0000000700000009` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/command_codec_test.cpp:173 | `0x0000000200000005` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/command_codec_test.cpp:209 | `0x0000000300000011` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/command_codec_test.cpp:538 | `0x1122334455667788` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/command_codec_test.cpp:541 | `0x1122334455667788` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/config_test.cpp:1546 | `0x00314b424e4f4345` | kept | Literal ECONBK1 wire/schema marker; moving it must break format checks. |
| src/sim/tests/config_test.cpp:1576 | `0xffffffffffffffff` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/construction_identity.hpp:33 | `0x1bd52ba547c3878a` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/container_probe_test.cpp:42 | `0xcbf29ce484222325` | kept | FNV algorithm basis/primitive known-answer constant, independent of state layout. |
| src/sim/tests/economy_facts_test.cpp:172 | `0x0102030405060708` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/economy_facts_test.cpp:182 | `0x0102030405060708` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/founding_fixture.hpp:18 | `0xcbf29ce484222325` | kept | FNV algorithm basis/primitive known-answer constant, independent of state layout. |
| src/sim/tests/harvest_state_test.cpp:763 | `0x1112131415161718` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/harvest_state_test.cpp:764 | `0x0000000200000001` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/harvest_state_test.cpp:768 | `0x1112131415161718` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/harvest_state_test.cpp:770 | `0x0000000200000001` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/integration_stream_test.cpp:114 | `0x0102030405060708` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/island_helpers.hpp:103 | `0xffffffffffffffff` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/leather_fixture_test.cpp:156 | `0x87619a7f52e615e7` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/leather_persistence_test.cpp:138 | `0x87619a7f52e615e7` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/leather_persistence_test.cpp:234 | `0x91c8d851f00c27ff` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/leather_persistence_test.cpp:234 | `0xc6db20b1828754d9` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/leather_persistence_test.cpp:234 | `0xfd4ff1cf99dc3db4` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/leather_persistence_test.cpp:235 | `0x7e9f74cbeb56b66d` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/leather_persistence_test.cpp:235 | `0xa5dd5d2595a23492` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/leather_persistence_test.cpp:235 | `0xcdf4c8924f0745d6` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/leather_persistence_test.cpp:236 | `0x78d3929de1a0d745` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/leather_persistence_test.cpp:236 | `0xf145dbe210490640` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/leather_persistence_test.cpp:236 | `0xb0076a66ad4b8575` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/leather_persistence_test.cpp:320 | `0x1859faa6b20efc08` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/leather_persistence_test.cpp:447 | `0xcdf4c8924f0745d6` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/replay_test.cpp:152 | `0x1111111111111111` | kept | Self-authored replay codec input/expected wire bytes; not a measured world identity. |
| src/sim/tests/replay_test.cpp:153 | `0x2222222222222222` | kept | Self-authored replay codec input/expected wire bytes; not a measured world identity. |
| src/sim/tests/replay_test.cpp:154 | `0x3333333333333333` | kept | Self-authored replay codec input/expected wire bytes; not a measured world identity. |
| src/sim/tests/replay_test.cpp:155 | `0x3434343434343434` | kept | Self-authored replay codec input/expected wire bytes; not a measured world identity. |
| src/sim/tests/replay_test.cpp:156 | `0x4444444444444444` | kept | Self-authored replay codec input/expected wire bytes; not a measured world identity. |
| src/sim/tests/replay_test.cpp:167 | `0x5555555555555555` | kept | Self-authored replay codec input/expected wire bytes; not a measured world identity. |
| src/sim/tests/replay_test.cpp:183 | `0x00314b424e4f4345` | kept | Literal ECONBK1 wire/schema marker; moving it must break format checks. |
| src/sim/tests/replay_test.cpp:185 | `0x1111111111111111` | kept | Self-authored replay codec input/expected wire bytes; not a measured world identity. |
| src/sim/tests/replay_test.cpp:186 | `0x2222222222222222` | kept | Self-authored replay codec input/expected wire bytes; not a measured world identity. |
| src/sim/tests/replay_test.cpp:187 | `0x3333333333333333` | kept | Self-authored replay codec input/expected wire bytes; not a measured world identity. |
| src/sim/tests/replay_test.cpp:188 | `0x3434343434343434` | kept | Self-authored replay codec input/expected wire bytes; not a measured world identity. |
| src/sim/tests/replay_test.cpp:189 | `0x4444444444444444` | kept | Self-authored replay codec input/expected wire bytes; not a measured world identity. |
| src/sim/tests/replay_test.cpp:202 | `0x5555555555555555` | kept | Self-authored replay codec input/expected wire bytes; not a measured world identity. |
| src/sim/tests/resident_job_test.cpp:1581 | `0x7777000000000001` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/resident_state_test.cpp:104 | `0x0000000100000002` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/resident_state_test.cpp:105 | `0xffffffffffffffff` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/resident_state_test.cpp:109 | `0xffffffffffffffff` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/resident_state_test.cpp:110 | `0x0000000100000002` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/resident_state_test.cpp:123 | `0x0000000100000002` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/resident_state_test.cpp:148 | `0x0000000300000004` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/resident_state_test.cpp:176 | `0x0000000300000004` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/resident_state_test.cpp:304 | `0x1112131415161718` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/route_objects_test.cpp:687 | `0x9e3779b97f4a7c15` | kept | Test-generator seed/algorithm input, not a measured simulation hash. |
| src/sim/tests/ship_orders_test.cpp:125 | `0x0000000100000002` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/ship_orders_test.cpp:127 | `0x0000000100000002` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/sim_test.cpp:188 | `0xcbf29ce484222325` | kept | FNV algorithm basis/primitive known-answer constant, independent of state layout. |
| src/sim/tests/sim_test.cpp:190 | `0xaf63dc4c8601ec8c` | kept | FNV algorithm basis/primitive known-answer constant, independent of state layout. |
| src/sim/tests/sim_test.cpp:424 | `0x977a5cb1c2ae725d` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/sim_test.cpp:425 | `0x65e3e2696c0396b0` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/sim_test.cpp:426 | `0xc3ae91a5ed6087ff` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/sim_test.cpp:428 | `0x1bf8daace9f69fab` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/sim_test.cpp:512 | `0x00314b424e4f4345` | kept | Literal ECONBK1 wire/schema marker; moving it must break format checks. |
| src/sim/tests/sim_test.cpp:561 | `0xffffffffffffffff` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/sim_test.cpp:619 | `0xfca81f99ec77b062` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/sim_test.cpp:755 | `0x9e3779b97f4a7c15` | kept | Test-generator seed/algorithm input, not a measured simulation hash. |
| src/sim/tests/sim_test.cpp:757 | `0xe220a8397b1dcdaf` | kept | Test-generator seed/algorithm input, not a measured simulation hash. |
| src/sim/tests/sim_test.cpp:762 | `0x123456789abcdef0` | kept | Test-generator seed/algorithm input, not a measured simulation hash. |
| src/sim/tests/sim_test.cpp:774 | `0x123456789abcdef0` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/territory_test.cpp:44 | `0xcbf29ce484222325` | kept | FNV algorithm basis/primitive known-answer constant, independent of state layout. |
| src/sim/tests/tile_hash_test.cpp:692 | `0x0102030405060708` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/tile_hash_test.cpp:720 | `0x0102030405060708` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/types_test.cpp:1565 | `0x804f19da7cae34ee` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/types_test.cpp:1579 | `0x77d2dd9ed3f484b1` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/types_test.cpp:1585 | `0x96fbf9c80cb07372` | removed_comment | Obsolete historical digest in comment; no active assertion. |
| src/sim/tests/types_test.cpp:1928 | `0x986b10f94d3725b4` | removed_comment | Obsolete historical digest in comment; no active assertion. |
| src/sim/tests/types_test.cpp:1929 | `0xf0c98731e34fca02` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/types_test.cpp:2033 | `0x7adcb2c0d02b1a70` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/types_test.cpp:2035 | `0x4cbafff0a755efe4` | moved | Named simulation/registry/replay pin; oracle-owned names remain read-only. |
| src/sim/tests/world_state_test.cpp:466 | `0xffffffffffffffff` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/world_state_test.cpp:482 | `0xffffffffffffffff` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |
| src/sim/tests/world_state_test.cpp:495 | `0xffffffffffffffff` | kept | Fixed archive/codec field value, sentinel, id or its explanatory comment; tests byte layout/range, not a measured hash. |

## Uppercase spelling cross-check

An additional case-insensitive scan finds18 uppercase occurrences, all fixed inputs
or range/sentinel values; these are also retained (94 kept in the broader121-occurrence scan).
They introduce no additional measured hash pin.

| Base source | Literal | Reason |
|---|---|---|
| src/sim/tests/archive_test.cpp:257 | `0xFFFFFFFFFFFFFFFF` | Fixed input seed, wire/field value or all-ones range/sentinel, not a measured hash. |
| src/sim/tests/archive_test.cpp:333 | `0xFFFFFFFFFFFFFFFF` | Fixed input seed, wire/field value or all-ones range/sentinel, not a measured hash. |
| src/sim/tests/archive_test.cpp:385 | `0xFFFFFFFFFFFFFFFF` | Fixed input seed, wire/field value or all-ones range/sentinel, not a measured hash. |
| src/sim/tests/archive_test.cpp:752 | `0xFFFFFFFFFFFFFFFD` | Fixed input seed, wire/field value or all-ones range/sentinel, not a measured hash. |
| src/sim/tests/command_codec_test.cpp:55 | `0xFFFFFFFFFFFFFFFE` | Fixed input seed, wire/field value or all-ones range/sentinel, not a measured hash. |
| src/sim/tests/command_codec_test.cpp:58 | `0x000000010000002C` | Fixed input seed, wire/field value or all-ones range/sentinel, not a measured hash. |
| src/sim/tests/command_codec_test.cpp:59 | `0xFFFFFFFFFFFFFFFE` | Fixed input seed, wire/field value or all-ones range/sentinel, not a measured hash. |
| src/sim/tests/command_codec_test.cpp:210 | `0x000000010000002C` | Fixed input seed, wire/field value or all-ones range/sentinel, not a measured hash. |
| src/sim/tests/config_test.cpp:1273 | `0xFFFFFFFFFFFFFFFF` | Fixed input seed, wire/field value or all-ones range/sentinel, not a measured hash. |
| src/sim/tests/game_setup_test.cpp:33 | `0x0123456789ABCDEF` | Fixed input seed, wire/field value or all-ones range/sentinel, not a measured hash. |
| src/sim/tests/island_state_test.cpp:1725 | `0xFFFFFFFFFFFFFFFF` | Fixed input seed, wire/field value or all-ones range/sentinel, not a measured hash. |
| src/sim/tests/island_state_test.cpp:1725 | `0xFFFFFFFFFFFFFFFF` | Fixed input seed, wire/field value or all-ones range/sentinel, not a measured hash. |
| src/sim/tests/island_state_test.cpp:2221 | `0xFFFFFFFFFFFFFFFF` | Fixed input seed, wire/field value or all-ones range/sentinel, not a measured hash. |
| src/sim/tests/sim_test.cpp:301 | `0xFFFFFFFFFFFFFFFF` | Fixed input seed, wire/field value or all-ones range/sentinel, not a measured hash. |
| src/sim/tests/sim_test.cpp:763 | `0xFFFFFFFFFFFFFFFF` | Fixed input seed, wire/field value or all-ones range/sentinel, not a measured hash. |
| src/sim/tests/tile_hash_test.cpp:93 | `0xFFFFFFFFFFFFFFFF` | Fixed input seed, wire/field value or all-ones range/sentinel, not a measured hash. |
| src/sim/tests/tile_hash_test.cpp:270 | `0xFFFFFFFFFFFFFFFF` | Fixed input seed, wire/field value or all-ones range/sentinel, not a measured hash. |
| src/sim/tests/tile_hash_test.cpp:295 | `0xFFFFFFFFFFFFFFFF` | Fixed input seed, wire/field value or all-ones range/sentinel, not a measured hash. |
