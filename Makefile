test: build
	cmake --build build
	build/test

build:
	cmake -G Ninja -B build -DPAK_INCLUDE_TEST=ON
