# npm install -g typescript
esbuild:
	esbuild --format=esm --external:pak:wifi --bundle src/buffer.ts | xxd -i -n buffer_js > ../bindings/buffer_js.h
	esbuild src/http.js | xxd -i -n http_js > ../bindings/http_js.h
