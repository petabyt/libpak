test: build
	cmake --build build
	build/veecar

build:
	cmake -G Ninja -B build -DPAK_INCLUDE_TEST=ON
