# libpak toolchain

WASM toolchain based on [WASI](https://github.com/WebAssembly/wasi-sdk) that enables compiling modules for libpak compatible runtimes

This toolchain enables libusb-1.0 and POSIX socket APIs, among others

### Dummy WASM integration test

With WAMR, the Linux development dependencies, and a WASI SDK installed, build
the guest and run the host-side integration test (set `WASI_SDK_PATH` if the
SDK is not at `/opt/wasi-sdk`):

```sh
cmake -S examples/dummy -B build/dummy-wasm \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/toolchain/toolchain.cmake" \
  -DCMAKE_PROJECT_INCLUDE="$PWD/toolchain/pakrt.cmake" -DMAKE_EXECUTABLE=ON
cmake --build build/dummy-wasm
cmake -S . -B build/host -DLINUX=ON -DPAK_QUICKJS=OFF \
  -DPAK_WASM_TEST_MODULE="$PWD/build/dummy-wasm/dummy"
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

The test uses an in-process mock host and does not require Bluetooth, Wi-Fi,
or a running system D-Bus. `pakit --test-wasm <module>` can also run the CLI
smoke test, but the CLI's normal module factory initializes system D-Bus.

### Bluetooth fake backend and WASM test

On Linux, configure with `-DPAK_FAKE_BLUETOOTH=ON` to replace BlueZ with a
deterministic fake BLE adapter and device. This is a single-threaded test
backend, not a radio simulator. It offers one service and characteristic;
connect and GATT operations complete immediately regardless of the blocking
flag, and `watch_characteristic` returns immediately. Writing a watched
characteristic emits a write event followed by a change event. RFCOMM is not
implemented. The fake backend invokes native callbacks synchronously.

The WASM bridge queues those native events, then delivers them on the same
execution environment before the originating import returns. Guest callbacks
may call Bluetooth imports again; nested events are drained after the current
callback. Unregistering or releasing an object discards its queued events.
This is currently supported only for the fake backend: registering a guest
callback with BlueZ returns `PAK_ERR_UNSUPPORTED`, because asynchronous BlueZ
dispatch and module teardown are not yet coordinated. Guest Bluetooth context
unref is deferred until module teardown; an externally supplied host context
remains owned by its caller. Neither the fake backend nor this WASM bridge is
safe for concurrent calls.

To build and run the native and WASM Bluetooth tests alongside the dummy test:

```sh
"${WASI_SDK_PATH:-/opt/wasi-sdk}/bin/clang" --target=wasm32-wasip1 \
  --sysroot="${WASI_SDK_PATH:-/opt/wasi-sdk}/share/wasi-sysroot" \
  -I include -D_WASI_EMULATED_GETPID -include toolchain/include/shim_sockets.h \
  -nostartfiles -Wl,--export-table,--no-entry,--export=get_module,--allow-undefined-file=toolchain/symbols.txt \
  tests/wasm_bluetooth_guest.c -o build/wasm-bluetooth-test.wasm
cmake -S . -B build/host -DLINUX=ON -DPAK_QUICKJS=OFF -DPAK_FAKE_BLUETOOTH=ON \
  -DPAK_WASM_TEST_MODULE="$PWD/build/dummy-wasm/dummy" \
  -DPAK_WASM_BT_TEST_MODULE="$PWD/build/wasm-bluetooth-test.wasm"
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

The CLI module factory also initializes Wi-Fi via system D-Bus, even when the
fake Bluetooth backend is selected. The tests instantiate Bluetooth directly
and do not require D-Bus.

### Wi-Fi fake backend and WASM test

`-DPAK_FAKE_WIFI=ON` replaces NetworkManager with a deterministic,
single-threaded fake: one `fake-wifi0` adapter, an open 2.4 GHz `Fake Open`
AP, and a 5 GHz `Fake Camera` AP with password `camera123`. Scans and
connections complete immediately, including nonblocking requests. Failed
authentication leaves the previous connection unchanged. Socket binding is
unsupported in both the fake backend and WASM bridge: guest descriptor
numbers are not safely interchangeable with host file descriptors.

The direct WASM Wi-Fi imports provide enumeration, scanning, connection, and
connected-AP lookup. AP snapshots retain their native adapter until released,
even if the guest releases the adapter first. Guest context unref is deferred
until module teardown; a host-supplied context remains caller-owned. The
bridge is not safe for concurrent calls. `pak_wifi_request_connection` and
the host-to-guest `on_try_connect_wifi` callback are not implemented by this
bridge.

Build the guest and enable the native and WASM Wi-Fi tests:

```sh
"${WASI_SDK_PATH:-/opt/wasi-sdk}/bin/clang" --target=wasm32-wasip1 \
  --sysroot="${WASI_SDK_PATH:-/opt/wasi-sdk}/share/wasi-sysroot" \
  -I include -D_WASI_EMULATED_GETPID -include toolchain/include/shim_sockets.h \
  -nostartfiles -Wl,--export-table,--no-entry,--export=get_module,--allow-undefined-file=toolchain/symbols.txt \
  tests/wasm_wifi_guest.c -o build/wasm-wifi-test.wasm
cmake -S . -B build/host -DLINUX=ON -DPAK_QUICKJS=OFF -DPAK_FAKE_WIFI=ON \
  -DPAK_WASM_WIFI_TEST_MODULE="$PWD/build/wasm-wifi-test.wasm"
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

To run the CLI without system D-Bus, select both `PAK_FAKE_WIFI=ON` and
`PAK_FAKE_BLUETOOTH=ON` when configuring the host build.
