# Ghidra scripts (static analysis of the original modules)

Our own headless scripts for Ghidra 12.1.4; outputs go to `cache/ghidra/` (git-ignored, never committed: they are derived from the original binaries). Findings are written up in `docs/re/` with addresses; no decompiled code goes into the project.

| Script | Does |
|---|---|
| `Decompile.java <out.c> <addr>...` | decompiles the functions containing the given addresses |
| `DecompileAll.java <out.c>` | decompiles every function Ghidra's auto-analysis found |
| `DecompileAllPlus.java <out.c>` | first creates functions at every pointer from data into code (vtables, jump tables), then decompiles all; finds the virtual methods that auto-analysis misses (AnnoDisplay: 1,534 functions instead of 934) |

Run from the repository root (JDK 17+; on the dev Mac Homebrew's `openjdk@21`):

```sh
export JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home
/opt/homebrew/Cellar/ghidra/12.1.4/libexec/support/analyzeHeadless cache/ghidra/plus proj_AnnoDisplay \
  -import game/app/AnnoDisplay.dll -scriptPath tools/ghidra \
  -postScript DecompileAllPlus.java $PWD/cache/ghidra/plus/AnnoDisplay.c -deleteProject
```
