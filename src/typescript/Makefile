esbuild:
	esbuild --format=esm --external:pak:wifi --bundle src/buffer.ts | xxd -i -n buffer_js > ../bindings/buffer_js.h

check:
	bun x tsgo
