//! Raw, unsafe bindings to libpak's public C headers.
//!
//! These declarations do not provide a libpak implementation or link it for
//! you. Callers must uphold the ownership, lifetime and threading requirements
//! of the C API. See the crate README for target and regeneration details.
#![allow(non_camel_case_types, non_snake_case, non_upper_case_globals)]

include!("bindings.rs");
