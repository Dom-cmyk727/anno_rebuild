| extract | original (under cache/gates of the checkout) | original sha256 | how |
|---|---|---|---|
| `extracts/gate-native-app-identities.txt` | `native-n7a.txt` | `53fb8bf3268f29cf8e7e55b2df1a817ed3c20e3eee8bcbecd3eb1bb8602a776e` | whole file |
| `extracts/gate-native-app-verdict.txt` | `native-n7a.out` | `9db851622ac2cf75058ef4a98c4154521215fd370a296e96866fb832c70bf5a2` | EXTRACT: lines matching /COUNTS|VERDICT|SKIPPED|Evidence/ (5 lines) |
| `extracts/gate-native-sim-identities.txt` | `native-n7c.txt` | `d146c4ff77ea57ca788643a5069f12df6506998d444ca31e7a1e471be3a02263` | whole file |
| `extracts/gate-native-sim-verdict.txt` | `native-n7c.out` | `60d537e11a07fd1ee45b428dc7e7ca0f63d873f234df25eb6c897bb7cddb1006` | EXTRACT: lines matching /COUNTS|VERDICT|SKIPPED|Evidence/ (8 lines) |
| `extracts/gate-native-rtx-identities.txt` | `native-n7g.txt` | `9bc506cbb2466bc8dd5dde6c318218edbd0fbf2fa7fa24fb038360ffe8c3a862` | whole file |
| `extracts/gate-native-rtx-verdict.txt` | `native-n7g.out` | `3c34c3d66d4deec836cedec3a14ecd4c8e41a76147d96abfeafb252e620add00` | EXTRACT: lines matching /COUNTS|VERDICT|SKIPPED|Evidence/ (3 lines) |
| `extracts/gate-wsl-identities.txt` | `wsl-w12.txt` | `0255d694190e81ebfa58532a50d8ed6c5925742be647b5ae36c8af5b48454b50` | whole file |
| `extracts/gate-wsl-app-verdict.txt` | `wsl-w12-app.out` | `e8439fa74119e17370ad3f942a5c06935f081dff04084d5c136055c84eca6e4e` | EXTRACT: lines matching /COUNTS|VERDICT|SKIPPED|Evidence/ (5 lines) |
| `extracts/gate-wsl-sim-verdict.txt` | `wsl-w12-sim.out` | `18bf375a1f36975c60e09d4f1bd0be9910f21046b251a97530da762de4acca6e` | EXTRACT: lines matching /COUNTS|VERDICT|SKIPPED|Evidence/ (3 lines) |
| `extracts/gate-wsl-warnings.txt` | `unix-warnings-w12.log` | `388e65d623fc5c02e66d63bde014acf2c375ae53a4bdad3c6eea4d5480f1e02d` | EXTRACT: lines matching /^== |rc=/ (8 lines) |
| `extracts/rtx-session.txt` | `opening-session-gpu-d129.log` | `54fb5bb29eac81c683132680f26dd0f6fa3dd9e359dd9b14c289cc2cd3064e25` | EXTRACT: lines matching /^(ok|FAIL|session:|opening_session:)|viewer_wiring: (PASS|FAIL)/ (26 lines) |
| `extracts/rtx-wiring-scripts.txt` | `gpuwin-d129.log` | `8ef0f49dde12b6097e82453276d743a4a5aed086af36cba8585fe1f5183bba58` | EXTRACT: lines matching /viewer_wiring: (PASS|FAIL)/ (2 lines) |
| `extracts/cpu-session-checks.txt` | `opening-session-cpu-chk2.log` | `0f6042c4ff3764f944d25ec6c5ff730e3e16df2735024f0d457049522ed5c05d` | EXTRACT: lines matching /^(ok|FAIL|opening_session:)|viewer_wiring: (PASS|FAIL)/ (25 lines) |
| `extracts/mutation-check.txt` | `mutation-final.log` | `41a60fa06f51fcb48ab0fe718b09e6f1412ba5a1edae12d4fc6cc40787f5a8c2` | whole file |
| `extracts/exe-identity-gated.txt` | `exe-sha-d129bba.txt` | `67feb0d03e43c7d25a9508024c3a89c4210d214ce978504998126aa8a435c62d` | whole file |
| `extracts/exe-identity-before-mutation.txt` | `mut-exe-before.txt` | `1efdf1baa53db1d4e2df301ae3533fe78cf12aeee92cf9970c8637f8d707681d` | whole file |
| `extracts/wsl-build-config.txt` | `wsl-w12-cmake-cache-lines.txt` | `4711a7d57f1aee9053014917f3cc080e59a4143e72a10ad73b4d3f7d5f10080c` | whole file |
| `extracts/caption-check.txt` | `caption-check.txt` | `78e8b9ebbcd8c342e34762a65bfd479ee9f7df4f7d065ea70034d213e294eaf4` | whole file |
| `extracts/post-mutation-wiring-rerun.txt` | `post-mutation-ctest.log` | `08897fc5f297e8f36d261088047206c8a020cf605cf99b303d776ce28f71b7d9` | whole file |
