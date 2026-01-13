veecarjs:
	cmake --build build
	build/pakcli --js examples/veecar.js

veecar:
	cmake --build build
	build/veecar

test: build
	cmake --build build
	build/pakcli --js examples/x.js

bt: build
	cmake --build build
	build/pakcli --dump-bt

mod:
	cmake --build build
	build/pakcli --test-dummy-mod

build:
	cmake -G Ninja -B build -DPAK_INCLUDE_TEST=ON

list:
	dbus-send --system      --print-reply   --dest=org.bluez        /       org.freedesktop.DBus.ObjectManager.GetManagedObjects
