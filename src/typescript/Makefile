# npm install -g typescript
FLAGS := --format=esm --external:pak:wifi
esbuild:
	esbuild $(FLAGS) --bundle src/buffer.ts > /dev/null
	esbuild $(FLAGS) --bundle src/buffer.ts | xxd -i -n buffer_js > ../bindings/buffer_js.h
	esbuild src/http.js > /dev/null
	esbuild src/http.js | xxd -i -n http_js > ../bindings/http_js.h
