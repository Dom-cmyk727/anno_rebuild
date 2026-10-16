//! Deterministic ANNO 1503 game simulation.
//!
//! Rules for this crate (see `CLAUDE.md`): integer/fixed-point arithmetic only,
//! a seeded PRNG owned by the simulation, no wall-clock time, no `HashMap`
//! iteration order, no floats in checksummed state, no I/O. It must build and
//! run headless without any rendering or audio dependency. Empty until M3.
#![deny(unsafe_code)]
