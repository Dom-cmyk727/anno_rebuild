# W4-WIN-VERIFY evidence subset (text only)

Generated from the Windows checkout at `3c808780c4e5e59d26375151c3ff8215c4c68602` by `cache/v/make_evidence_v.py` (outside git). Everything under `extracts/` is a copy
or an **EXTRACT** (marked) of a git-ignored original under `cache/` of `E:\anno1503-worktrees\anno1503-W4-3`; the sha256 is of the
Windows file as it is on disk (CRLF where a text file has it), not the git blob id of the committed copy. No game data, screenshots, binaries or
archives are in git; the files contain local paths and the local user name and no credentials.

| extract | original (under the checkout) | original sha256 | how |
|---|---|---|---|
| `extracts/repro-before-dad27da-tools.txt` | `cache/v/repro-before.txt` | `e223c098890b09084b0790bd7d86f81a0bfbef46bdf87f2d34f9ed5b40271e58` | whole file |
| `extracts/repro-after-corrected-tools.txt` | `cache/v/repro-after.txt` | `fe927f981cb26ca0e390b194f878593483bf2d9cb1035b6163bd12328c986bd6` | whole file |
| `extracts/repro-harness.py` | `cache/v/repro.py` | `6e6f956c5515b389ce464270d48944c6daf2f12708556f34fe6870eab217dfaa` | whole file |
| `extracts/mutation-python-summary.txt` | `cache/v/mutation-py-summary.txt` | `7599b41f311b3a09f324df4a878e3a0a04cf935938c75948cc09e3fc6723d03c` | whole file |
| `extracts/mutation-python-harness.py` | `cache/v/mutate_py.py` | `50acc083732310c722d4c9b3342c93d71f5b3bf944d61e9c0fff8dd2f94fc62b` | whole file |
| `extracts/mutation-cpp-summary.txt` | `cache/v/mutation-cpp-summary.txt` | `0e828af2abd49255a265a0af657db3ae36e8e745960029035a660fbde3738964` | whole file |
| `extracts/mutation-cpp-harness.py` | `cache/v/mutate_cpp.py` | `6b59d94263bda3cedf36c71678a85f48a69d2b6e84a98f26f2a777f696b7e7bd` | whole file |
| `extracts/gate-native-identities.txt` | `cache/gates/native-verify-a.txt` | `92941b41ba260c1137ea907bca7647ade72a9695265ca56589b2a6afffe59fec` | whole file |
| `extracts/gate-native-verdict.txt` | `cache/gates/native-verify-a-focused.out` | `08a1f719fe98a7cb735c7342027a8c16749674b8ca7dcf04a4d3dc475120c675` | EXTRACT: lines matching /COUNTS|VERDICT|Evidence/ (3 lines) |
| `extracts/gate-native-tests.txt` | `cache/gates/native-verify-a-focused.log` | `4527b1bafbaad2a3a75273d0049c5c095d55b1ed27a62c7f5984a199fd6bc298` | EXTRACT: lines matching /Test +#\d+:|tests passed|COMMAND:/ (56 lines) |
| `extracts/gate-native-b-identities.txt` | `cache/gates/native-verify-b.txt` | `c0bd44d10ce3433fc2a249c7a99ebf76b67e1099a5553b0215225663667debb9` | whole file |
| `extracts/gate-native-b-verdict.txt` | `cache/gates/native-verify-b-focused.out` | `18a44bca8e309166d97d36b0e2bedd1d4ea7da11ffe5ead3a8b027ae0285d488` | EXTRACT: lines matching /COUNTS|VERDICT|Evidence/ (3 lines) |
| `extracts/gate-wsl-b-contract-identities.txt` | `cache/gates/wsl-verify-b.txt` | `e7daf65b442753f800c72101eca1071ed825d5264d01e385993b6e89b34d761b` | whole file |
| `extracts/gate-wsl-b-contract-verdict.txt` | `cache/gates/wsl-verify-b-contract.out` | `3cc461a2c117baef41339dc17d66217cf07e832bf91a3056777663d3ca354975` | EXTRACT: lines matching /COUNTS|VERDICT|Evidence/ (3 lines) |
| `extracts/gate-wsl-identities.txt` | `cache/gates/wsl-verify.txt` | `5b9e3e66dc1882248e4b51c9ba9f4ef0ed14f3784d9801feac659275c119d210` | whole file |
| `extracts/gate-wsl-verdict.txt` | `cache/gates/wsl-verify-a-focused.out` | `2b1c7bbcc4cedb0aa61d2785790ae559e577ced02d352cd256f47825b00d3e92` | EXTRACT: lines matching /COUNTS|VERDICT|Evidence/ (3 lines) |
| `extracts/gate-wsl-tests.txt` | `cache/gates/wsl-verify-a-focused.log` | `69aa959a847f20b87c8bbf3e9874d7ce79773c22fcdfa3588bcc9c4cb6896052` | EXTRACT: lines matching /Test +#\d+:|tests passed|COMMAND:/ (60 lines) |
| `extracts/gate-wsl-warnings.txt` | `cache/gates/unix-warnings-a.log` | `05b9a57ee066c2189423871c6fe772d43490e4fa74bfb67d261b6aa1fa0f4449` | EXTRACT: lines matching /^== / (8 lines) |
| `extracts/canary-rtx-exit-in-hold.out` | `cache/v/canary/exit_in_hold.out` | `eac7f050da55371df270ad3c3d4f7dd8c0272f8bfa27438a13c87e377b726a30` | whole file |
| `extracts/canary-rtx-exit-in-hold-script.log` | `cache/v/canary/exit_in_hold/script.log` | `38d233527ffc921f80445a42b761db10030619794ef110dc55642a3a100f0cc6` | whole file |
| `extracts/canary-rtx-exit-in-hold-stderr.txt` | `cache/v/canary/exit_in_hold.stderr.txt` | `6fe6b6d188d9115f14f9f71f7659d4a6d1e07372a207e7bb265fd4f82ee6ee39` | whole file |
| `extracts/canary-rtx-identity.txt` | `cache/v/canary/exe.sha256` | `9e3ffd9ebe5c1f16138bdb727b0cbc766bcc16e4294891431c27871c71a0bdf8` | executable hash and the source head/dirty count the canary ran at |
| `extracts/stream-reconciliation.txt` | `cache/gates/wsl-verify.txt` | `5b9e3e66dc1882248e4b51c9ba9f4ef0ed14f3784d9801feac659275c119d210` | generated from the retained <out>.stderr/stdout.txt and script.log of the gate runs |
| `extracts/revalidation-of-w4-win-final-traces.txt` | `docs/orchestration/evidence/w4-win-final/extracts/canary-stages-script.log` | `4cd3ceeb0f13f6e66349cc4d0d0573c1d72b15362875bea46a421e7979a95962` | viewer_session.py --revalidate over the three W4-WIN-FINAL raw traces (the committed extracts) |
| `extracts/revalidation-of-new-traces.txt` | `cache/gates/native-verify-a.txt` | `92941b41ba260c1137ea907bca7647ade72a9695265ca56589b2a6afffe59fec` | viewer_session.py <script> <session> over this package's gate and canary sessions |
