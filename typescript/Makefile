# npm install -g typescript
FLAGS := --format=esm --external:pak:wifi --external:c:socket --external:pak:runtime
esbuild:
	esbuild $(FLAGS) --bundle src/buffer.ts > /dev/null
	esbuild $(FLAGS) --bundle src/buffer.ts | xxd -i -n buffer_js > ../src/bindings/buffer_js.h
	esbuild $(FLAGS) --bundle src/http.ts > /dev/null
	esbuild $(FLAGS) --bundle src/http.ts | xxd -i -n http_js > ../src/bindings/http_js.h
