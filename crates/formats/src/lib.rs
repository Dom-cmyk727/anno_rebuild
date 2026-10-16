//! Readers and writers for the original ANNO 1503 file formats.
//!
//! Every layout implemented here must trace to a parsed original file, a
//! disassembled function or a cited community source; specs live in
//! `docs/formats/`.
#![deny(unsafe_code)]

pub mod chunk;
pub mod colors;
pub mod inc;
pub mod tex;
