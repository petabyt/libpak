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
