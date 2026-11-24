wasm:
	cmake --build build && build/pakit --test-wasm /home/daniel/Documents/fantasyfudge/modules/build/dummy/dummy

test: build
	cmake --build build
	build/pakit --js examples/x.js

dump-bt: build
	cmake --build build
	build/pakit --dump-bt

config:
	cmake -G Ninja -B build -DPAK_INCLUDE_TEST=ON

dump-bluez:
	dbus-send --system --print-reply --dest=org.bluez / org.freedesktop.DBus.ObjectManager.GetManagedObjects

clean:
	rm -rf .cache build cmake-build-debug .idea

rust-bindgen:
	bindgen rust/wrapper.h \
	    --allowlist-type 'Pak.*|WidgetGroup|WidgetType|SettingUnion|Container.*|GcContext|libusb_device|pak_bt_listen_device' \
	    --allowlist-function 'pak_.*|setup_quickjs_module|setup_wasm_module' \
	    --allowlist-var 'PAK_.*|UUID_STR_LENGTH|BT_DEVICE|BT_ADAPTER|GATT_.*|WIFI_.*' \
	    --formatter prettyplease \
	    --no-layout-tests \
	    --output rust/src/bindings.rs \
	    -- --target=x86_64-unknown-linux-gnu -Iinclude
