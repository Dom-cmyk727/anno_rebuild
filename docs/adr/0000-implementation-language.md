# ADR 0000: Implementation language — Rust now, C++20 kept open

- **Status:** accepted, to be revisited at the M1 → M3 gate (see below)
- **Date:** 2026-10-16

## Context
The project owner chose Rust at the start. A co-developer who will continue the work after AI-assisted sessions end is more familiar with C++ and recommends C++20. The owner wants to keep Rust for now but keep a migration to C++20 (or a C++20 rebuild from scratch) possible.

What matters for this project, regardless of language:
- byte-exact readers and writers for the original formats;
- a deterministic simulation for lockstep multiplayer (integer/fixed-point only, controlled PRNG, no UB-dependent behaviour);
- Windows and Linux x86_64 builds from one codebase;
- maintainability by the people who will actually keep working on it.

## Options
| | Rust (current) | C++20 |
|---|---|---|
| Team fit | owner + AI sessions | co-developer's main language |
| Determinism | no UB in safe code, integer overflow checked in debug, `BTreeMap` order fixed | achievable, but needs discipline: UB, sanitizers, `-ffp-contract=off`/`/fp:strict`, no `std::unordered_map` iteration in sim |
| Cross-platform build | `cargo`, one toolchain, CI already green on Windows/Linux | CMake + vcpkg/Conan; more CI setup |
| Formats / RE | explicit readers port 1:1 either way | closer to Ghidra pseudocode (C-like), easier to transcribe recovered functions |
| Ecosystem | wgpu, SDL2 bindings, laminar/quinn | SDL2/3, bgfx, ENet/GameNetworkingSockets, mature and direct |

Mixing both languages through FFI is **not** recommended: the sim is the bulk of the work, and splitting it across a boundary costs more than either choice alone.

## Decision
Continue in Rust. Make every artefact that is not code portable, so that a C++20 port only has to re-implement code and can be checked against the same evidence:

1. **Specs are the source of truth, not code.** `docs/formats/*.md`, `docs/re/*.md` and `docs/open-questions.md` describe layouts, formulas and evidence completely enough to implement from without reading Rust.
2. **Language-neutral oracles.** `tools/chunks.py` and the text output of `anno-dump` define expected results. Any implementation (Rust or C++) must reproduce them byte-for-byte; the agreement test pattern (`crates/tools/tests/agree_python.rs`) is the template. New format work adds a dump subcommand with a documented text format.
3. **Corpus facts are written down.** Counts and invariants asserted by corpus tests (e.g. 2,954 container files, 17,529 chunks, 22 non-zero name paddings) are also in the specs, so a C++ test suite can assert the same numbers.
4. **No Rust-only magic in formats or sim.** Explicit field-by-field readers, no serde derive for original formats, no macros that hide layout. Sim state and commands get a documented, versioned binary encoding and a documented state-hash definition, so a Rust and a C++ sim could even be cross-checked tick by tick.
5. **Crate boundaries map 1:1 to C++ libraries** (`formats`, `assets`, `sim`, `net`, `render`, `ui`, `app`, `tools`), with the same dependency direction.

## When to revisit
The cheapest point to switch is **after M1 (formats + viewer) and before M3 (simulation)**. By then the formats layer is small and fully specified, while sim and net, the expensive parts, are not started. Questions for that review:
- Who will write most of the M3/M4 code?
- Has the Rust code been a barrier for the co-developer in practice?
- Does recovered pseudocode from M2 favour a near-literal C++ transcription?

If the answer is C++20, port `formats` first against the existing oracles and corpus facts, then continue M1.6/M3 in C++. This ADR is then superseded by a new one.
