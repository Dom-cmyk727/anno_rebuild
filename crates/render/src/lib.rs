//! Isometric renderer and sprite batching. Consumes simulation state and
//! [`anno_assets`]; never mutates simulation state.

pub mod fluid;
pub mod frame;
#[cfg(feature = "gpu")]
pub mod gpu;
#[cfg(feature = "gpu")]
pub mod gpu_fluid;
pub mod iso;
pub mod scene;
pub mod sea;
