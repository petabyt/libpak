# libpak-sys

Raw Rust FFI declarations generated from the public libpak headers. This
crate does not build, link, or implement libpak; consumers provide the native
implementation and are responsible for all C API safety requirements. It is
not a safe wrapper and does not implement a runtime or the variadic logging
callbacks.

`src/bindings.rs` is checked in, so normal builds do not require bindgen or
libclang. It was generated and ABI-tested on x86-64 Linux. The headers include
platform-sensitive C types and enums: **do not assume the generated ABI is
correct on other targets**. Generate and validate target-specific bindings
before using them there.

Regenerate on x86-64 Linux with libclang and bindgen-cli 0.71.1 installed:

```sh
cargo install bindgen-cli --version 0.71.1 --locked
sh generate.sh
cargo test
```

The test compiles a small C layout probe using `cc` (or the `CC` environment
variable) and compares key layouts and values against the Rust declarations.
It runs on the native host; it is not a cross-compilation test. Re-run it after
header or generator changes. Static inline C helpers are not exported symbols
and are not represented as callable bindings.
