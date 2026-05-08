wasm:
	cmake --build build && build/pakit --test-wasm /home/daniel/Documents/fantasyfudge/modules/build/dummy/dummy

veecarjs:
	cmake --build build
	build/pakit --js examples/veecar.js

veecar:
	cmake --build build
	build/veecar

test: build
	cmake --build build
	build/pakit --js examples/x.js

bt: build
	cmake --build build
	build/pakit --dump-bt

mod:
	cmake --build build
	build/pakit --test-dummy-mod

build:
	cmake -G Ninja -B build -DPAK_INCLUDE_TEST=ON

list:
	dbus-send --system      --print-reply   --dest=org.bluez        /       org.freedesktop.DBus.ObjectManager.GetManagedObjects

clean:
	rm -rf .cache build cmake-build-debug .idea
