# Whole-binary order matrix

Canonical outer result: 36 selected / 32 executed passes / 4 failed / 0 skipped. Five failed native case visits belong to four distinct cases, one known.

| Binary | Order | Framework cases | Executed passes | Failed | Prerequisite skips | Filter excluded | CTest wall s |
|---|---|---:|---:|---:|---:|---:|---:|
| anno_test_support_tests | default | 100 | 92 | 0 | 8 | 0 | 41.48 |
| anno_test_support_tests | 1 | 100 | 92 | 0 | 8 | 0 | 40.99 |
| anno_test_support_tests | 2 | 100 | 92 | 0 | 8 | 0 | 40.94 |
| anno_test_support_tests | 3 | 100 | 92 | 0 | 8 | 0 | 40.97 |
| anno_formats_tests | default | 83 | 83 | 0 | 0 | 0 | 6.10 |
| anno_formats_tests | 1 | 83 | 83 | 0 | 0 | 0 | 5.79 |
| anno_formats_tests | 2 | 83 | 83 | 0 | 0 | 0 | 5.79 |
| anno_formats_tests | 3 | 83 | 83 | 0 | 0 | 0 | 5.79 |
| anno_assets_tests | default | 14 | 14 | 0 | 0 | 0 | 10.41 |
| anno_assets_tests | 1 | 14 | 14 | 0 | 0 | 0 | 10.37 |
| anno_assets_tests | 2 | 14 | 14 | 0 | 0 | 0 | 10.39 |
| anno_assets_tests | 3 | 14 | 14 | 0 | 0 | 0 | 10.42 |
| anno_render_tests | default | 213 | 212 | 1 | 0 | 0 | 344.74 |
| anno_render_tests | 1 | 203 | 203 | 0 | 0 | 10 | 147.17 |
| anno_render_tests | 2 | 203 | 203 | 0 | 0 | 10 | 147.55 |
| anno_render_tests | 3 | 203 | 203 | 0 | 0 | 10 | 148.06 |
| anno_render_gpu_tests | default | 118 | 116 | 0 | 2 | 0 | 152.61 |
| anno_render_gpu_tests | 1 | 118 | 116 | 0 | 2 | 0 | 151.53 |
| anno_render_gpu_tests | 2 | 118 | 116 | 0 | 2 | 0 | 152.05 |
| anno_render_gpu_tests | 3 | 118 | 116 | 0 | 2 | 0 | 150.97 |
| anno_app_tests | default | 326 | 322 | 1 | 3 | 0 | 232.13 |
| anno_app_tests | 1 | 324 | 319 | 2 | 3 | 2 | 156.26 |
| anno_app_tests | 2 | 324 | 320 | 1 | 3 | 2 | 135.86 |
| anno_app_tests | 3 | 324 | 321 | 0 | 3 | 2 | 136.28 |
| anno_app_alloc_tests | default | 6 | 6 | 0 | 0 | 0 | 0.06 |
| anno_app_alloc_tests | 1 | 6 | 6 | 0 | 0 | 0 | 0.05 |
| anno_app_alloc_tests | 2 | 6 | 6 | 0 | 0 | 0 | 0.05 |
| anno_app_alloc_tests | 3 | 6 | 6 | 0 | 0 | 0 | 0.05 |
| anno_app_input_tests | default | 50 | 50 | 0 | 0 | 0 | 16.85 |
| anno_app_input_tests | 1 | 50 | 50 | 0 | 0 | 0 | 16.86 |
| anno_app_input_tests | 2 | 50 | 50 | 0 | 0 | 0 | 16.86 |
| anno_app_input_tests | 3 | 50 | 50 | 0 | 0 | 0 | 16.82 |
| anno_sim_tests | default | 1655 | 1637 | 0 | 18 | 0 | 733.14 |
| anno_sim_tests | 1 | 1651 | 1633 | 0 | 18 | 4 | 556.09 |
| anno_sim_tests | 2 | 1651 | 1633 | 0 | 18 | 4 | 555.59 |
| anno_sim_tests | 3 | 1651 | 1633 | 0 | 18 | 4 | 556.65 |

Prerequisite skips are 8 opt-in test-support cases, 2 GPU adapter cases, 3 app opt-ins and 18 sim opt-ins per order. Doctest filter exclusions are a different column. Exact names/reasons, ordering, prefixes and executable hashes are retained in stage2-matrix.json.
