//! ANNO 1503 rebuild entry point. Wires the crates together; the asset viewer
//! (`--view`) arrives in M1.6.

// Link every crate so the workspace wiring is exercised from the start.
use anno_assets as _;
use anno_formats as _;
use anno_net as _;
use anno_render as _;
use anno_sim as _;
use anno_ui as _;

fn main() {
    eprintln!("anno1503: nothing to run yet (milestone M0). See docs/PLAN.md.");
}
